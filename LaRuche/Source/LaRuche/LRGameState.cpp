#include "LRGameState.h"
#include "LRWorldActors.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ALRGameState::ALRGameState()
{
}

void ALRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALRGameState, Phase);
	DOREPLIFETIME(ALRGameState, Eveil);
	DOREPLIFETIME(ALRGameState, Tier);
	DOREPLIFETIME(ALRGameState, RaidSeed);
	DOREPLIFETIME(ALRGameState, RaidStart);
	DOREPLIFETIME(ALRGameState, Mission);
	DOREPLIFETIME(ALRGameState, StartTier);
	DOREPLIFETIME(ALRGameState, MissionProgress);
	DOREPLIFETIME(ALRGameState, MissionGoal);
	DOREPLIFETIME(ALRGameState, bMissionDone);
}

void ALRGameState::BeginPlay()
{
	Super::BeginPlay();
	BuildStatic();
	RebuildCity();
	ApplyTier();
}

FString ALRGameState::TierName(int32 InTier)
{
	switch (InTier)
	{
	case 1: return TEXT("Dormante");
	case 2: return TEXT("Agitée");
	case 3: return TEXT("Éveillée");
	default: return TEXT("FRÉNÉSIE");
	}
}

FString ALRGameState::MissionName(ELRMission InMission)
{
	switch (InMission)
	{
	case ELRMission::Prelevement: return TEXT("Prélèvement");
	case ELRMission::Purge: return TEXT("Purge");
	case ELRMission::BoiteNoire: return TEXT("Boîte noire");
	default: return TEXT("Balise");
	}
}

FString ALRGameState::MissionText(ELRMission InMission)
{
	switch (InMission)
	{
	case ELRMission::Prelevement: return TEXT("Ramassez les échantillons laissés par les créatures tuées");
	case ELRMission::Purge: return TEXT("Détruisez des nids de la Ruche");
	case ELRMission::BoiteNoire: return TEXT("Tuez l'Hôte qui porte la boîte noire et ramassez-la");
	default: return TEXT("Posez les capteurs sur les sites marqués");
	}
}

ALRBlock* ALRGameState::SpawnBlock(const FString& Name, const FVector& Loc, const FVector& Scale, const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	// Meme nom sur toutes les machines : le reseau peut ainsi designer ce bloc
	// (utile quand un personnage se tient dessus).
	FActorSpawnParameters Params;
	Params.Name = FName(*Name);
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALRBlock* Block = World->SpawnActor<ALRBlock>(ALRBlock::StaticClass(), FTransform(FRotator::ZeroRotator, Loc, Scale), Params);
	if (Block)
	{
		Block->SetNetAddressable();
		LR::Tint(Block->Mesh, Color);
	}
	return Block;
}

void ALRGameState::BuildStatic()
{
	UWorld* World = GetWorld();
	if (bStaticBuilt || !World)
	{
		return;
	}
	bStaticBuilt = true;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Env = World->SpawnActor<ALREnvironment>(ALREnvironment::StaticClass(), FVector(0.f, 0.f, 3000.f), FRotator::ZeroRotator, Params);

	// Sol de la ville (toujours present, meme hors raid)
	const float CitySize = FLRCityLayout::N * FLRCityLayout::Cell;
	SpawnBlock(TEXT("LRS_CityFloor"), FVector(0.f, 0.f, -50.f), FVector(CitySize / 100.f, CitySize / 100.f, 1.f), FLinearColor(0.045f, 0.05f, 0.045f));

	// La planque : un poste de quarantaine desaffecte
	const FVector C = LRWorld::PlanqueCenter;
	const float Size = 2400.f;
	const float WallH = 500.f;
	const FLinearColor WallCol(0.12f, 0.13f, 0.11f);
	SpawnBlock(TEXT("LRS_PlanqueFloor"), C + FVector(0.f, 0.f, -50.f), FVector(Size / 100.f, Size / 100.f, 1.f), FLinearColor(0.09f, 0.09f, 0.08f));
	SpawnBlock(TEXT("LRS_PlanqueWallN"), C + FVector(Size * 0.5f, 0.f, WallH * 0.5f), FVector(1.f, Size / 100.f, WallH / 100.f), WallCol);
	SpawnBlock(TEXT("LRS_PlanqueWallS"), C + FVector(-Size * 0.5f, 0.f, WallH * 0.5f), FVector(1.f, Size / 100.f, WallH / 100.f), WallCol);
	SpawnBlock(TEXT("LRS_PlanqueWallE"), C + FVector(0.f, Size * 0.5f, WallH * 0.5f), FVector(Size / 100.f, 1.f, WallH / 100.f), WallCol);
	SpawnBlock(TEXT("LRS_PlanqueWallW"), C + FVector(0.f, -Size * 0.5f, WallH * 0.5f), FVector(Size / 100.f, 1.f, WallH / 100.f), WallCol);
	// Coffre, etabli et table du labo (decor)
	SpawnBlock(TEXT("LRS_Coffre"), C + FVector(900.f, -600.f, 60.f), FVector(2.f, 1.2f, 1.2f), FLinearColor(0.1f, 0.3f, 0.12f));
	SpawnBlock(TEXT("LRS_Etabli"), C + FVector(900.f, 0.f, 45.f), FVector(1.2f, 3.f, 0.9f), FLinearColor(0.3f, 0.2f, 0.08f));
	SpawnBlock(TEXT("LRS_Labo"), C + FVector(900.f, 600.f, 45.f), FVector(1.2f, 2.f, 0.9f), FLinearColor(0.3f, 0.3f, 0.35f));
}

void ALRGameState::RebuildCity()
{
	if (BuiltSeed == RaidSeed || !GetWorld() || !bStaticBuilt)
	{
		return;
	}
	BuiltSeed = RaidSeed;

	for (ALRBlock* Block : CityBlocks)
	{
		if (Block)
		{
			Block->Destroy();
		}
	}
	CityBlocks.Reset();
	Layout.Clear();

	if (RaidSeed == 0)
	{
		return;
	}

	Layout.Generate(RaidSeed);
	for (int32 i = 0; i < Layout.Blocks.Num(); ++i)
	{
		const FLRBlockDef& Def = Layout.Blocks[i];
		// La graine fait partie du nom : aucun conflit avec les blocs du raid precedent.
		const FString Name = FString::Printf(TEXT("LRB_%d_%d"), RaidSeed, i);
		if (ALRBlock* Block = SpawnBlock(Name, Def.Loc, Def.Scale, Def.Color))
		{
			CityBlocks.Add(Block);
		}
	}
}

void ALRGameState::OnRep_Seed()
{
	RebuildCity();
}

void ALRGameState::OnRep_Tier()
{
	ApplyTier();
}

void ALRGameState::ApplyTier()
{
	if (Env)
	{
		Env->SetTier(Phase == ELRPhase::Raid ? Tier : 1);
	}
}

void ALRGameState::MulticastMessage_Implementation(const FString& Msg)
{
	FLRMessage M;
	M.Text = Msg;
	M.Time = GetWorld() ? float(GetWorld()->GetTimeSeconds()) : 0.f;
	Messages.Add(M);
	if (Messages.Num() > 6)
	{
		Messages.RemoveAt(0);
	}
}

void ALRGameState::MulticastFx_Implementation(uint8 Kind, FVector_NetQuantize A, FVector_NetQuantize B)
{
	UWorld* World = GetWorld();
	switch (Kind)
	{
	case 0: ALRFx::Tracer(World, A, B, FLinearColor(1.f, 0.85f, 0.4f)); break;
	case 1: ALRFx::Tracer(World, A, B, FLinearColor(0.4f, 1.f, 0.4f)); break;
	case 2: ALRFx::Burst(World, A, FMath::Max(100.f, float(B.X)), FLinearColor(0.5f, 1.f, 0.1f)); break;
	case 3: ALRFx::Burst(World, A, 90.f, FLinearColor(0.5f, 0.1f, 0.6f)); break;
	case 4: ALRFx::Burst(World, A, 600.f, FLinearColor(1.f, 0.1f, 0.1f)); break;
	default: ALRFx::Burst(World, A, 200.f, FLinearColor(0.2f, 0.5f, 1.f)); break;
	}
}
