#include "LRGameMode.h"
#include "LRCharacter.h"
#include "LRCity.h"
#include "LREnemy.h"
#include "LRGameState.h"
#include "LRHUD.h"
#include "LRItems.h"
#include "LRPlayerController.h"
#include "LRWorldActors.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ALRGameMode::ALRGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ALRCharacter::StaticClass();
	PlayerControllerClass = ALRPlayerController::StaticClass();
	GameStateClass = ALRGameState::StaticClass();
	HUDClass = ALRHUD::StaticClass();
}

ALRGameState* ALRGameMode::LRGS() const
{
	return GetGameState<ALRGameState>();
}

APawn* ALRGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// Pas de PlayerStart dans la carte : tout le monde apparait dans la planque.
	FActorSpawnParameters Params;
	Params.Instigator = GetInstigator();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<APawn>(DefaultPawnClass, LRWorld::PlanqueSpawn(SpawnCounter++), FRotator::ZeroRotator, Params);
}

void ALRGameMode::Say(const FString& Msg)
{
	if (ALRGameState* GS = LRGS())
	{
		GS->MulticastMessage(Msg);
	}
}

// ---------------------------------------------------------------- Boucle

void ALRGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ALRGameState* GS = LRGS();
	if (!GS || GS->Phase != ELRPhase::Raid || bEnding)
	{
		return;
	}
	const float Now = LR::Now(this);

	// La Ruche se reveille toute seule avec le temps, moins vite si l'equipe est silencieuse.
	const bool bQuiet = (Now - LastNoise) > 15.f;
	AddEveil((bQuiet ? 0.045f : 0.085f) * DeltaSeconds);

	SecondTimer += DeltaSeconds;
	if (SecondTimer >= 1.f)
	{
		SecondTimer = 0.f;

		// Leurres sonores
		for (int32 i = Lures.Num() - 1; i >= 0; --i)
		{
			if (Now >= Lures[i].End)
			{
				Lures.RemoveAt(i);
			}
			else
			{
				EmitNoise(Lures[i].Loc, 3500.f, 0.f);
			}
		}

		// Etat de l'equipe
		TArray<ALRCharacter*> Raiders;
		for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
		{
			if (It->bInRaid)
			{
				Raiders.Add(*It);
			}
		}
		for (ALRCharacter* C : Raiders)
		{
			if (C->Life == ELRLife::ATerre && !AnyoneStanding(C))
			{
				C->Die();
			}
		}
		int32 Active = 0;
		for (ALRCharacter* C : Raiders)
		{
			if (C->Life == ELRLife::Vivant || C->Life == ELRLife::ATerre)
			{
				++Active;
			}
		}
		UpdateSpectators();

		if (Active == 0)
		{
			bEnding = true;
			Say(TEXT("Fin du raid. Retour à la planque…"));
			GetWorldTimerManager().SetTimer(EndTimer, this, &ALRGameMode::EndRaid, 3.f, false);
			return;
		}

		// Evenement : largage de quarantaine
		if (NextEvent > 0.f && Now >= NextEvent)
		{
			NextEvent = 0.f;
			FVector Loc;
			if (PickOpenCell(Loc, 1500.f, 0.f, true))
			{
				SpawnInit<ALRContainer>(Loc, [](ALRContainer* Crate) { Crate->Kind = 1; });
				Say(TEXT("Largage de quarantaine ! Une caisse militaire vient de tomber dans la zone."));
				EmitNoise(Loc, 4000.f, 0.f);
			}
		}
	}

	DirectorTimer += DeltaSeconds;
	if (DirectorTimer >= 1.5f)
	{
		DirectorTimer = 0.f;
		Director();
	}
}

// ---------------------------------------------------------------- Raid

void ALRGameMode::StartRaid()
{
	ALRGameState* GS = LRGS();
	if (!GS || GS->Phase != ELRPhase::Planque || bStarting || bEnding)
	{
		return;
	}
	bStarting = true;
	++RaidCounter;
	GS->RaidSeed = RaidCounter * 100003 + FMath::RandRange(1, 99999);
	GS->RebuildCity();
	GS->Eveil = 0.f;
	GS->Tier = 1;
	GS->MissionProgress = 0;
	GS->bMissionDone = false;

	PopulateRaid();
	Say(TEXT("Insertion dans la zone de quarantaine…"));
	// Petit delai : le temps que chaque machine construise la ville.
	GetWorldTimerManager().SetTimer(InsertTimer, this, &ALRGameMode::InsertPlayers, 1.5f, false);
}

void ALRGameMode::PopulateRaid()
{
	ALRGameState* GS = LRGS();
	if (!GS)
	{
		return;
	}
	const int32 N = FLRCityLayout::N;
	UsedCells.Reset();
	Lures.Reset();

	// Les coins du boulevard peripherique sont toujours libres.
	InsertionLoc = FLRCityLayout::CellToWorld(1, 1) + FVector(200.f, 200.f, 0.f);
	UsedCells.Add(FIntPoint(1, 1));
	UsedCells.Add(FIntPoint(2, 2));

	auto MakeExtraction = [this](ELRExtract Kind, int32 X, int32 Y)
	{
		UsedCells.Add(FIntPoint(X, Y));
		SpawnInit<ALRExtraction>(FLRCityLayout::CellToWorld(X, Y), [Kind](ALRExtraction* E) { E->Kind = Kind; });
	};
	MakeExtraction(ELRExtract::Egout, N - 3, 2);
	MakeExtraction(ELRExtract::Sas, N - 3, N - 3);
	MakeExtraction(ELRExtract::Helico, 2, N - 3);

	FVector Loc;
	for (int32 i = 0; i < 2; ++i)
	{
		if (PickOpenCell(Loc, 0.f, 3000.f, true))
		{
			SpawnInit<ALRMedStation>(Loc, [](ALRMedStation*) {});
		}
	}

	const int32 NumNests = 5 + FMath::Clamp(GS->StartTier, 1, 3);
	for (int32 i = 0; i < NumNests; ++i)
	{
		if (PickOpenCell(Loc, 0.f, 3500.f, true))
		{
			SpawnInit<ALRNest>(Loc, [](ALRNest*) {});
		}
	}

	for (int32 i = 0; i < 6; ++i)
	{
		if (PickOpenCell(Loc, 0.f, 2500.f, true))
		{
			SpawnEnemy(ELREnemy::Sentinelle, Loc);
		}
	}

	for (int32 i = 0; i < 34; ++i)
	{
		if (PickOpenCell(Loc, 0.f, 1200.f, true))
		{
			const bool bTrap = FMath::FRand() < 0.08f;
			SpawnInit<ALRContainer>(Loc, [bTrap](ALRContainer* C) { C->bBreathing = bTrap; });
		}
	}

	for (int32 i = 0; i < 8; ++i)
	{
		if (PickOpenCell(Loc, 0.f, 800.f, false))
		{
			if (FMath::FRand() < 0.5f)
			{
				SpawnPickup(LRId::Munitions, FMath::RandRange(8, 20), Loc);
			}
			else
			{
				SpawnPickup(LRId::Ferraille, FMath::RandRange(1, 2), Loc);
			}
		}
	}

	// Contrat
	switch (GS->Mission)
	{
	case ELRMission::Prelevement:
		GS->MissionGoal = 6;
		break;
	case ELRMission::Purge:
		GS->MissionGoal = 3;
		break;
	case ELRMission::BoiteNoire:
		GS->MissionGoal = 1;
		if (PickOpenCell(Loc, 0.f, 6000.f, false))
		{
			if (ALREnemy* Host = SpawnEnemy(ELREnemy::Hote, Loc))
			{
				Host->bBlackBox = true;
				Host->MaxHealth *= 2.f;
				Host->Health = Host->MaxHealth;
			}
		}
		break;
	default:
		GS->MissionGoal = 3;
		for (int32 i = 0; i < 3; ++i)
		{
			if (PickOpenCell(Loc, 0.f, 4000.f, true))
			{
				SpawnInit<ALRSensorSite>(Loc, [](ALRSensorSite*) {});
			}
		}
		break;
	}

	// Premieres creatures
	for (int32 i = 0; i < 8; ++i)
	{
		if (PickOpenCell(Loc, 0.f, 3000.f, false))
		{
			SpawnEnemy(RollEnemyType(1), Loc);
		}
	}
}

void ALRGameMode::InsertPlayers()
{
	bStarting = false;
	ALRGameState* GS = LRGS();
	if (!GS)
	{
		return;
	}
	GS->Phase = ELRPhase::Raid;
	GS->RaidStart = LR::Now(this);
	LastNoise = GS->RaidStart;
	SecondTimer = 0.f;
	DirectorTimer = 0.f;
	NextEvent = GS->RaidStart + FMath::FRandRange(150.f, 300.f);

	int32 Index = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ALRPlayerController* PC = Cast<ALRPlayerController>(It->Get());
		ALRCharacter* C = PC ? Cast<ALRCharacter>(PC->GetPawn()) : nullptr;
		if (!C)
		{
			continue;
		}
		C->EnterRaid(PC->ServerLoadout, InsertionLoc + FVector((Index % 2) * 160.f, (Index / 2) * 160.f, 120.f));
		++Index;
	}

	// Palier de depart choisi par l'hote
	const int32 Start = FMath::Clamp(GS->StartTier, 1, 3);
	if (Start > 1)
	{
		GS->Eveil = (Start - 1) * 25.f;
		while (GS->Tier < Start)
		{
			GS->Tier++;
			OnTierReached(GS->Tier);
		}
	}
	GS->ApplyTier();
	Say(FString::Printf(TEXT("Contrat « %s » : %s."), *ALRGameState::MissionName(GS->Mission), *ALRGameState::MissionText(GS->Mission)));
}

void ALRGameMode::EndRaid()
{
	ALRGameState* GS = LRGS();
	if (!GS)
	{
		return;
	}

	int32 Index = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ALRPlayerController* PC = Cast<ALRPlayerController>(It->Get());
		ALRCharacter* C = PC ? Cast<ALRCharacter>(PC->GetPawn()) : nullptr;
		if (!C)
		{
			continue;
		}
		if (C->bInRaid)
		{
			const bool bOut = (C->Life == ELRLife::Extrait);
			FLRLoadout Result;
			TArray<FLRStack> Rewards;
			FString Summary;
			if (bOut)
			{
				Result = C->Inv;
				Summary = TEXT("Extraction réussie : tout ce que vous portez est conservé.");
				if (GS->bMissionDone)
				{
					Rewards.Add(FLRStack(LRId::Ferraille, 10 + 5 * GS->StartTier));
					Rewards.Add(FLRStack(FLRItems::RandomPlan(), 1));
					Summary += TEXT(" Contrat rempli : la prime est dans le coffre.");
				}
				else
				{
					Summary += TEXT(" Contrat non rempli.");
				}
			}
			else
			{
				Result.Pocket = C->Inv.Pocket;
				Summary = TEXT("Vous n'êtes pas sorti de la zone. Seule la poche sécurisée est conservée.");
			}
			FLRItems::EnsureMods(Result);
			PC->ServerLoadout = Result;
			C->Inv = Result;
			PC->ClientRaidResult(Result, Rewards, bOut, Summary);
		}
		C->ReturnToPlanque(LRWorld::PlanqueSpawn(Index++));
		PC->SetViewTargetWithBlend(C, 0.f);
	}

	CleanupRaid();
	GS->Phase = ELRPhase::Planque;
	GS->Eveil = 0.f;
	GS->Tier = 1;
	GS->RaidSeed = 0;
	GS->RebuildCity();
	GS->ApplyTier();
	bEnding = false;
}

void ALRGameMode::CleanupRaid()
{
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->IsA<ALREnemy>() || Actor->IsA<ALRInteractable>() || Actor->IsA<ALRDestructible>())
		{
			ToDestroy.Add(Actor);
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		Actor->Destroy();
	}
	Lures.Reset();
	UsedCells.Reset();
}

void ALRGameMode::UpdateSpectators()
{
	ALRCharacter* Alive = nullptr;
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->bInRaid && (It->Life == ELRLife::Vivant || It->Life == ELRLife::ATerre))
		{
			Alive = *It;
			break;
		}
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ALRCharacter* C = PC ? Cast<ALRCharacter>(PC->GetPawn()) : nullptr;
		if (!C || !C->bInRaid)
		{
			continue;
		}
		const bool bGone = (C->Life == ELRLife::Mort || C->Life == ELRLife::Extrait);
		AActor* Want = (bGone && Alive) ? static_cast<AActor*>(Alive) : static_cast<AActor*>(C);
		if (PC->GetViewTarget() != Want)
		{
			PC->SetViewTargetWithBlend(Want, 0.4f);
		}
	}
}

// ---------------------------------------------------------------- Eveil et bruit

void ALRGameMode::AddEveil(float Amount)
{
	ALRGameState* GS = LRGS();
	if (!GS || GS->Phase != ELRPhase::Raid || Amount <= 0.f)
	{
		return;
	}
	GS->Eveil = FMath::Clamp(GS->Eveil + Amount, 0.f, 100.f);
	const int32 NewTier = FMath::Clamp(1 + FMath::FloorToInt(GS->Eveil / 25.f), 1, 4);
	while (GS->Tier < NewTier)
	{
		GS->Tier++;
		GS->ApplyTier();
		OnTierReached(GS->Tier);
	}
}

void ALRGameMode::EmitNoise(const FVector& Loc, float Radius, float EveilAmount)
{
	ALRGameState* GS = LRGS();
	if (!GS || GS->Phase != ELRPhase::Raid)
	{
		return;
	}
	if (EveilAmount > 0.f)
	{
		LastNoise = LR::Now(this);
		AddEveil(EveilAmount);
	}
	for (TActorIterator<ALREnemy> It(GetWorld()); It; ++It)
	{
		const float R = Radius * It->HearingMult();
		if (R > 0.f && FVector::DistSquared(Loc, It->GetActorLocation()) < R * R)
		{
			It->HearNoise(Loc);
		}
	}
}

void ALRGameMode::AddLure(const FVector& Loc)
{
	FLRLure Lure;
	Lure.Loc = Loc;
	Lure.End = LR::Now(this) + 12.f;
	Lures.Add(Lure);
	EmitNoise(Loc, 3500.f, 0.f);
	if (ALRGameState* GS = LRGS())
	{
		GS->MulticastFx(5, Loc, Loc);
	}
}

void ALRGameMode::OnTierReached(int32 NewTier)
{
	FVector Loc;
	if (NewTier == 2)
	{
		SpawnBioWalls(6);
		Say(TEXT("La Ruche s'agite : des cloisons organiques poussent dans les rues."));
	}
	else if (NewTier == 3)
	{
		for (TActorIterator<ALRExtraction> It(GetWorld()); It; ++It)
		{
			if (It->Kind == ELRExtract::Egout)
			{
				It->bOpen = false;
				It->ApplyVisual();
			}
		}
		SpawnBioWalls(5);
		for (int32 i = 0; i < 3; ++i)
		{
			if (PickOpenCell(Loc, 0.f, 2000.f, false))
			{
				SpawnPickup(LRId::Nodule, 1, Loc, false);
			}
		}
		Say(TEXT("La Ruche est éveillée : les égouts sont bouchés, des nodules rares ont éclos."));
	}
	else if (NewTier == 4)
	{
		for (TActorIterator<ALRExtraction> It(GetWorld()); It; ++It)
		{
			if (It->Kind == ELRExtract::Sas)
			{
				It->bOpen = false;
				It->ApplyVisual();
			}
		}
		if (PickOpenCell(Loc, 3000.f, 0.f, false))
		{
			SpawnEnemy(ELREnemy::Matriarche, Loc);
		}
		Say(TEXT("FRÉNÉSIE ! La Matriarche est sortie. Seul l'hélico peut encore vous extraire."));
	}
}

void ALRGameMode::SpawnBioWalls(int32 Count)
{
	ALRGameState* GS = LRGS();
	if (!GS || !GS->Layout.bValid)
	{
		return;
	}
	const int32 N = FLRCityLayout::N;
	int32 Made = 0;
	for (int32 Try = 0; Try < 200 && Made < Count; ++Try)
	{
		const FIntPoint Cell = GS->Layout.Open[FMath::RandRange(0, GS->Layout.Open.Num() - 1)];
		// Jamais sur le boulevard peripherique : les extractions restent accessibles.
		if (Cell.X < 3 || Cell.Y < 3 || Cell.X > N - 4 || Cell.Y > N - 4)
		{
			continue;
		}
		if (GS->Layout.IsBlocked(Cell.X, Cell.Y) || UsedCells.Contains(Cell))
		{
			continue;
		}
		const FVector Loc = FLRCityLayout::CellToWorld(Cell);
		if (!FarFromPlayers(Loc, 900.f))
		{
			continue;
		}
		bool bEnemyNear = false;
		for (TActorIterator<ALREnemy> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(Loc, It->GetActorLocation()) < 450.f)
			{
				bEnemyNear = true;
				break;
			}
		}
		if (bEnemyNear)
		{
			continue;
		}
		UsedCells.Add(Cell);
		GS->Layout.SetBlocked(Cell, true);
		SpawnInit<ALRBioWall>(Loc, [Cell](ALRBioWall* Wall) { Wall->CellPos = Cell; });
		++Made;
	}
}

// ---------------------------------------------------------------- Apparitions

bool ALRGameMode::FarFromPlayers(const FVector& Loc, float MinDist) const
{
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->bInRaid && It->Life != ELRLife::Extrait && FVector::Dist2D(Loc, It->GetActorLocation()) < MinDist)
		{
			return false;
		}
	}
	return true;
}

int32 ALRGameMode::NumRaiders() const
{
	int32 Num = 0;
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->bInRaid && (It->Life == ELRLife::Vivant || It->Life == ELRLife::ATerre))
		{
			++Num;
		}
	}
	return Num;
}

bool ALRGameMode::AnyoneStanding(const ALRCharacter* Except) const
{
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != Except && It->bInRaid && It->Life == ELRLife::Vivant)
		{
			return true;
		}
	}
	return false;
}

ALRNest* ALRGameMode::NearestNest(const FVector& Loc) const
{
	ALRNest* Best = nullptr;
	float BestDist = MAX_flt;
	for (TActorIterator<ALRNest> It(GetWorld()); It; ++It)
	{
		const float Dist = FVector::Dist2D(Loc, It->GetActorLocation());
		if (!It->bBroken && Dist < BestDist)
		{
			Best = *It;
			BestDist = Dist;
		}
	}
	return Best;
}

bool ALRGameMode::PickOpenCell(FVector& OutLoc, float MinDistFromPlayers, float MinDistFromInsertion, bool bReserve)
{
	ALRGameState* GS = LRGS();
	if (!GS || !GS->Layout.bValid || GS->Layout.Open.Num() == 0)
	{
		return false;
	}
	for (int32 Try = 0; Try < 60; ++Try)
	{
		const FIntPoint Cell = GS->Layout.Open[FMath::RandRange(0, GS->Layout.Open.Num() - 1)];
		if (UsedCells.Contains(Cell) || GS->Layout.IsBlocked(Cell.X, Cell.Y))
		{
			continue;
		}
		const FVector Loc = FLRCityLayout::CellToWorld(Cell);
		if (MinDistFromInsertion > 0.f && FVector::Dist2D(Loc, InsertionLoc) < MinDistFromInsertion)
		{
			continue;
		}
		if (MinDistFromPlayers > 0.f && !FarFromPlayers(Loc, MinDistFromPlayers))
		{
			continue;
		}
		if (bReserve)
		{
			UsedCells.Add(Cell);
		}
		OutLoc = Loc;
		return true;
	}
	return false;
}

ELREnemy ALRGameMode::RollEnemyType(int32 Tier) const
{
	static const ELREnemy Types[5] = { ELREnemy::Larve, ELREnemy::Pisteur, ELREnemy::Glaneuse, ELREnemy::Hote, ELREnemy::Carapace };
	static const int32 Weights[4][5] = {
		{ 60, 15, 10, 10, 5 },
		{ 50, 18, 10, 12, 10 },
		{ 42, 20, 8, 15, 15 },
		{ 40, 20, 5, 15, 20 }
	};
	const int32 Row = FMath::Clamp(Tier, 1, 4) - 1;
	int32 Pick = FMath::RandRange(0, 99);
	for (int32 i = 0; i < 5; ++i)
	{
		if (Pick < Weights[Row][i])
		{
			return Types[i];
		}
		Pick -= Weights[Row][i];
	}
	return ELREnemy::Larve;
}

void ALRGameMode::Director()
{
	ALRGameState* GS = LRGS();
	if (!GS)
	{
		return;
	}
	int32 Count = 0;
	for (TActorIterator<ALREnemy> It(GetWorld()); It; ++It)
	{
		if (!It->bDead && It->Type != ELREnemy::Sentinelle)
		{
			++Count;
		}
	}
	static const int32 Targets[4] = { 10, 18, 28, 40 };
	const int32 Tier = FMath::Clamp(GS->Tier, 1, 4);
	const int32 Target = FMath::RoundToInt(Targets[Tier - 1] * (0.7f + 0.3f * NumRaiders()));

	for (int32 i = 0; i < 2 && Count < Target; ++i)
	{
		FVector Loc;
		bool bFound = false;
		ELREnemy Type = RollEnemyType(Tier);

		// Une fois sur deux, la creature sort d'un nid eloigne des joueurs.
		if (FMath::FRand() < 0.5f)
		{
			TArray<ALRNest*> Nests;
			for (TActorIterator<ALRNest> It(GetWorld()); It; ++It)
			{
				if (!It->bBroken && FarFromPlayers(It->GetActorLocation(), 1800.f))
				{
					Nests.Add(*It);
				}
			}
			if (Nests.Num() > 0)
			{
				const FVector Dir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
				Loc = Nests[FMath::RandRange(0, Nests.Num() - 1)]->GetActorLocation() + Dir * 170.f;
				bFound = true;
				if (FMath::FRand() < 0.7f)
				{
					Type = ELREnemy::Larve;
				}
			}
		}
		if (!bFound)
		{
			bFound = PickOpenCell(Loc, 2200.f, 0.f, false);
		}
		if (!bFound)
		{
			return;
		}
		SpawnEnemy(Type, Loc);
		++Count;
	}
}

ALREnemy* ALRGameMode::SpawnEnemy(ELREnemy Type, const FVector& Loc)
{
	const ALRGameState* GS = LRGS();
	const FLREnemyStats& Stats = ALREnemy::StatsFor(Type);
	const FTransform Transform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), FVector(Loc.X, Loc.Y, Stats.HalfHeight + 20.f));
	ALREnemy* Enemy = GetWorld()->SpawnActorDeferred<ALREnemy>(ALREnemy::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Enemy)
	{
		Enemy->Type = Type;
		Enemy->Power = 1.f + 0.15f * ((GS ? FMath::Clamp(GS->Tier, 1, 4) : 1) - 1);
		Enemy->FinishSpawning(Transform);
	}
	return Enemy;
}

void ALRGameMode::SpawnPickup(FName Id, int32 Count, const FVector& Loc, bool bScatter)
{
	const FLRItemDef* Def = FLRItems::Find(Id);
	if (!Def || Count <= 0)
	{
		return;
	}
	while (Count > 0)
	{
		const int32 N = FMath::Min(Count, Def->MaxStack);
		Count -= N;
		FVector Pos(Loc.X, Loc.Y, 5.f);
		if (bScatter)
		{
			Pos += FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), 0.f);
		}
		SpawnInit<ALRPickup>(Pos, [Id, N](ALRPickup* Pickup) { Pickup->Stack = FLRStack(Id, N); });
	}
}

void ALRGameMode::SpawnLoot(const FVector& Loc, int32 Kind)
{
	TArray<FLRStack> Loot;
	FLRItems::RollLoot(Kind, Loot);
	for (const FLRStack& Stack : Loot)
	{
		SpawnPickup(Stack.Id, Stack.Count, Loc);
	}
}

void ALRGameMode::SpawnBeacon(ALRCharacter* Dead, const FVector& Loc)
{
	if (!Dead)
	{
		return;
	}
	SpawnInit<ALRBeacon>(FVector(Loc.X, Loc.Y, 5.f), [Dead](ALRBeacon* Beacon) { Beacon->Dead = Dead; });
}

// ---------------------------------------------------------------- Evenements de jeu

void ALRGameMode::OnEnemyKilled(ALREnemy* Enemy, ALRCharacter* Killer)
{
	ALRGameState* GS = LRGS();
	if (!Enemy || !GS || GS->Phase != ELRPhase::Raid)
	{
		return;
	}
	const FVector Loc = Enemy->GetActorLocation();
	GS->MulticastFx(3, Loc, Loc);

	switch (Enemy->Type)
	{
	case ELREnemy::Larve:
		if (FMath::FRand() < 0.25f)
		{
			SpawnPickup(LRId::Chitine, 1, Loc);
		}
		// A partir du palier 3, les larves explosent en acide.
		if (GS->Tier >= 3)
		{
			GS->MulticastFx(2, Loc, FVector(300.f, 0.f, 0.f));
			TArray<ALRCharacter*> Hit;
			for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
			{
				if (FVector::Dist(Loc, It->GetActorLocation()) < 300.f)
				{
					Hit.Add(*It);
				}
			}
			for (ALRCharacter* C : Hit)
			{
				C->Hurt(15.f);
			}
		}
		break;
	case ELREnemy::Pisteur:
		if (FMath::FRand() < 0.6f)
		{
			SpawnPickup(LRId::Glande, 1, Loc);
		}
		break;
	case ELREnemy::Sentinelle:
		if (FMath::FRand() < 0.4f)
		{
			SpawnPickup(LRId::Glande, 1, Loc);
		}
		break;
	case ELREnemy::Carapace:
		SpawnPickup(LRId::Chitine, FMath::RandRange(2, 3), Loc);
		break;
	case ELREnemy::Glaneuse:
		SpawnPickup(LRId::Ferraille, 2, Loc);
		break;
	case ELREnemy::Hote:
		SpawnPickup(LRId::Munitions, FMath::RandRange(10, 24), Loc);
		if (FMath::FRand() < 0.3f)
		{
			SpawnLoot(Loc, 0);
		}
		if (Enemy->bBlackBox)
		{
			SpawnPickup(LRId::BoiteNoire, 1, Loc, false);
		}
		break;
	case ELREnemy::Cocon:
		SpawnPickup(LRId::Seve, 2, Loc);
		break;
	case ELREnemy::Matriarche:
		SpawnPickup(LRId::Coeur, 1, Loc, false);
		SpawnPickup(LRId::Nodule, 2, Loc);
		SpawnPickup(LRId::Seve, 3, Loc);
		Say(TEXT("La Matriarche est morte ! Son cœur est au sol."));
		break;
	}

	if (Enemy->Carried.Count > 0)
	{
		SpawnPickup(Enemy->Carried.Id, Enemy->Carried.Count, Loc);
	}
	if (GS->Mission == ELRMission::Prelevement && !GS->bMissionDone && FMath::FRand() < 0.4f)
	{
		SpawnPickup(LRId::Echantillon, 1, Loc);
	}
}

void ALRGameMode::OnNestDestroyed(ALRNest* Nest, ALRCharacter* By)
{
	ALRGameState* GS = LRGS();
	if (!Nest || !GS || GS->Phase != ELRPhase::Raid)
	{
		return;
	}
	const FVector Loc = Nest->GetActorLocation();
	GS->MulticastFx(3, Loc, Loc);
	SpawnPickup(LRId::Glande, FMath::RandRange(1, 2), Loc);
	SpawnPickup(LRId::Seve, 1, Loc);
	for (const FLRStack& Stack : Nest->Stored)
	{
		SpawnPickup(Stack.Id, Stack.Count, Loc);
	}
	Nest->Stored.Reset();
	Say(TEXT("Nid détruit. La Ruche réagit."));
	LastNoise = LR::Now(this);
	AddEveil(5.f);
	if (GS->Mission == ELRMission::Purge)
	{
		MissionProgress(1);
	}
}

void ALRGameMode::OnWallDestroyed(ALRBioWall* Wall)
{
	ALRGameState* GS = LRGS();
	if (Wall && GS)
	{
		GS->Layout.SetBlocked(Wall->CellPos, false);
		UsedCells.Remove(Wall->CellPos);
	}
}

void ALRGameMode::OnItemPicked(FName Id, int32 Count, ALRCharacter* By)
{
	const ALRGameState* GS = LRGS();
	if (!GS || GS->Phase != ELRPhase::Raid)
	{
		return;
	}
	if (GS->Mission == ELRMission::Prelevement && Id == LRId::Echantillon)
	{
		MissionProgress(Count);
	}
	else if (GS->Mission == ELRMission::BoiteNoire && Id == LRId::BoiteNoire)
	{
		MissionProgress(1);
	}
}

void ALRGameMode::OnSensorPlaced(ALRSensorSite* Site)
{
	const ALRGameState* GS = LRGS();
	if (!Site || !GS)
	{
		return;
	}
	EmitNoise(Site->GetActorLocation(), 4000.f, 4.f);
	if (GS->Mission == ELRMission::Balise)
	{
		MissionProgress(1);
	}
}

void ALRGameMode::OnCrateOpened(ALRContainer* Crate)
{
	if (Crate)
	{
		EmitNoise(Crate->GetActorLocation(), 5000.f, 6.f);
		Say(TEXT("Le largage est ouvert. Toute la zone l'a entendu."));
	}
}

void ALRGameMode::MissionProgress(int32 Amount)
{
	ALRGameState* GS = LRGS();
	if (!GS || GS->bMissionDone || Amount <= 0)
	{
		return;
	}
	GS->MissionProgress = FMath::Min(GS->MissionGoal, GS->MissionProgress + Amount);
	if (GS->MissionProgress >= GS->MissionGoal)
	{
		GS->bMissionDone = true;
		Say(TEXT("Contrat rempli ! Extrayez-vous pour toucher la prime."));
	}
}
