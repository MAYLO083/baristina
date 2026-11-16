#include "LREnemy.h"
#include "LRCharacter.h"
#include "LRGameMode.h"
#include "LRGameState.h"
#include "LRItems.h"
#include "LRWorldActors.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

const FLREnemyStats& ALREnemy::StatsFor(ELREnemy InType)
{
	// Vie, vitesse, degats, portee, delai d'attaque, vue, rayon, demi-hauteur, couleur
	static const FLREnemyStats Table[] = {
		{ 30.f, 520.f, 7.f, 110.f, 0.8f, 1800.f, 30.f, 30.f, FLinearColor(0.55f, 0.75f, 0.1f) },      // Larve
		{ 130.f, 600.f, 24.f, 150.f, 1.1f, 280.f, 38.f, 80.f, FLinearColor(0.85f, 0.8f, 0.8f) },      // Pisteur
		{ 60.f, 0.f, 0.f, 0.f, 1.f, 2200.f, 34.f, 50.f, FLinearColor(0.9f, 0.1f, 0.1f) },             // Sentinelle
		{ 320.f, 260.f, 34.f, 190.f, 1.6f, 1500.f, 60.f, 90.f, FLinearColor(0.2f, 0.12f, 0.06f) },    // Carapace
		{ 70.f, 640.f, 0.f, 130.f, 1.f, 2000.f, 30.f, 50.f, FLinearColor(0.95f, 0.8f, 0.1f) },        // Glaneuse
		{ 150.f, 380.f, 10.f, 2200.f, 1.3f, 2400.f, 36.f, 88.f, FLinearColor(0.3f, 0.4f, 0.3f) },     // Hote
		{ 140.f, 420.f, 28.f, 150.f, 0.9f, 1500.f, 45.f, 45.f, FLinearColor(0.5f, 0.15f, 0.6f) },     // Cocon
		{ 2600.f, 300.f, 55.f, 380.f, 2.2f, 100000.f, 130.f, 200.f, FLinearColor(0.6f, 0.02f, 0.08f) } // Matriarche
	};
	return Table[FMath::Clamp(int32(InType), 0, 7)];
}

FString ALREnemy::TypeName(ELREnemy InType)
{
	switch (InType)
	{
	case ELREnemy::Larve: return TEXT("Larve");
	case ELREnemy::Pisteur: return TEXT("Pisteur");
	case ELREnemy::Sentinelle: return TEXT("Sentinelle");
	case ELREnemy::Carapace: return TEXT("Carapace");
	case ELREnemy::Glaneuse: return TEXT("Glaneuse");
	case ELREnemy::Hote: return TEXT("Hôte");
	case ELREnemy::Cocon: return TEXT("Cocon");
	default: return TEXT("Matriarche");
	}
}

ALREnemy::ALREnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->MaxWalkSpeed = 400.f;

	Body = LR::Part(this, TEXT("Body"), RootComponent, LR::Sphere(), FVector::ZeroVector, FVector(0.6f));
	Extra = LR::Part(this, TEXT("Extra"), RootComponent, LR::Cone(), FVector::ZeroVector, FVector(0.3f));
}

void ALREnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALREnemy, Type);
	DOREPLIFETIME(ALREnemy, Health);
	DOREPLIFETIME(ALREnemy, MaxHealth);
}

void ALREnemy::BeginPlay()
{
	Super::BeginPlay();
	ApplyType();
	if (HasAuthority())
	{
		const FLREnemyStats& S = StatsFor(Type);
		MaxHealth = S.Health * Power * (bBlackBox ? 2.f : 1.f);
		Health = MaxHealth;
		LastPos = GetActorLocation();
		ThinkTimer = FMath::FRandRange(0.f, 0.3f);
	}
}

void ALREnemy::OnRep_Type()
{
	ApplyType();
}

void ALREnemy::ApplyType()
{
	const FLREnemyStats& S = StatsFor(Type);
	GetCapsuleComponent()->SetCapsuleSize(S.Radius, S.HalfHeight);
	GetCharacterMovement()->MaxWalkSpeed = S.Speed;

	const float R = S.Radius / 50.f;
	const float H = S.HalfHeight / 50.f;
	UStaticMesh* BodyMesh = LR::Sphere();
	FVector BodyScale(R, R, H);
	UStaticMesh* ExtraMesh = LR::Cone();
	FVector ExtraLoc(S.Radius * 0.8f, 0.f, S.HalfHeight * 0.4f);
	FVector ExtraScale(R * 0.4f);
	FRotator ExtraRot(-90.f, 0.f, 0.f);
	FLinearColor ExtraColor = S.Color * 0.5f;

	switch (Type)
	{
	case ELREnemy::Pisteur:
		BodyMesh = LR::Cylinder();
		BodyScale = FVector(R * 0.8f, R * 0.8f, H);
		break;
	case ELREnemy::Sentinelle:
		BodyMesh = LR::Cone();
		ExtraMesh = LR::Sphere();
		ExtraLoc = FVector(0.f, 0.f, S.HalfHeight);
		ExtraRot = FRotator::ZeroRotator;
		ExtraColor = FLinearColor(1.f, 0.9f, 0.2f);
		break;
	case ELREnemy::Carapace:
		BodyMesh = LR::Cube();
		BodyScale = FVector(R * 0.9f, R * 0.9f, H);
		// La plaque de chitine a l'avant
		ExtraMesh = LR::Cube();
		ExtraLoc = FVector(S.Radius * 0.95f, 0.f, 0.f);
		ExtraScale = FVector(0.15f, R * 1.1f, H * 1.05f);
		ExtraRot = FRotator::ZeroRotator;
		ExtraColor = FLinearColor(0.02f, 0.02f, 0.02f);
		break;
	case ELREnemy::Hote:
		BodyMesh = LR::Cylinder();
		BodyScale = FVector(R * 0.8f, R * 0.8f, H);
		ExtraMesh = LR::Cube();
		ExtraLoc = FVector(S.Radius, 20.f, 20.f);
		ExtraScale = FVector(0.7f, 0.1f, 0.14f);
		ExtraRot = FRotator::ZeroRotator;
		ExtraColor = FLinearColor(0.02f, 0.02f, 0.02f);
		break;
	default:
		break;
	}

	Body->SetStaticMesh(BodyMesh);
	Body->SetRelativeScale3D(BodyScale);
	Extra->SetStaticMesh(ExtraMesh);
	Extra->SetRelativeLocation(ExtraLoc);
	Extra->SetRelativeScale3D(ExtraScale);
	Extra->SetRelativeRotation(ExtraRot);
	LR::Tint(Body, S.Color);
	LR::Tint(Extra, ExtraColor);
}

float ALREnemy::HearingMult() const
{
	switch (Type)
	{
	case ELREnemy::Pisteur: return 2.5f;
	case ELREnemy::Sentinelle: return 0.f;
	case ELREnemy::Matriarche: return 0.f;
	default: return 1.f;
	}
}

void ALREnemy::HearNoise(const FVector& Loc)
{
	if (bDead || bFleeing)
	{
		return;
	}
	LastKnown = Loc;
	bInvestigating = true;
}

bool ALREnemy::ApplyHit(float Damage, const FVector& ShotDir, ALRCharacter* From, bool bAcid)
{
	if (!HasAuthority() || bDead)
	{
		return false;
	}
	if (Type == ELREnemy::Carapace)
	{
		// Touchee dans le dos : degats augmentes. De face : presque rien, sauf avec la greffe acide.
		const float Dot = float(FVector::DotProduct(ShotDir.GetSafeNormal2D(), GetActorForwardVector()));
		if (Dot > 0.25f)
		{
			Damage *= 1.6f;
		}
		else if (!bAcid)
		{
			Damage *= 0.15f;
		}
	}
	Health -= Damage;
	if (From && !bFleeing)
	{
		LastKnown = From->GetActorLocation();
		bInvestigating = true;
	}
	if (Health <= 0.f)
	{
		Die(From);
		return true;
	}
	return false;
}

void ALREnemy::Die(ALRCharacter* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	if (ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>())
	{
		GM->OnEnemyKilled(this, Killer);
	}
	Destroy();
}

bool ALREnemy::HasLineOfSight(const AActor* Other) const
{
	if (!Other)
	{
		return false;
	}
	FCollisionQueryParams Query;
	Query.AddIgnoredActor(this);
	Query.AddIgnoredActor(Other);
	const FVector From = GetActorLocation() + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f);
	return !GetWorld()->LineTraceTestByChannel(From, Other->GetActorLocation(), ECC_Visibility, Query);
}

ALRCharacter* ALREnemy::FindTarget() const
{
	const FLREnemyStats& S = StatsFor(Type);
	const float Now = LR::Now(this);
	const FVector Me = GetActorLocation();
	ALRCharacter* Best = nullptr;
	float BestDist = MAX_flt;
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		ALRCharacter* C = *It;
		if (!C->bInRaid || C->Life != ELRLife::Vivant || C->PheroEnd > Now)
		{
			continue;
		}
		const float Dist = FVector::Dist(Me, C->GetActorLocation());
		float Sight = S.Sight;
		if (C->bIsCrouched && Type != ELREnemy::Pisteur && Type != ELREnemy::Matriarche)
		{
			Sight *= 0.6f;
		}
		if (Dist > Sight || Dist >= BestDist)
		{
			continue;
		}
		// Le Pisteur est aveugle (il ne sent que ce qui le touche) et la Matriarche sait toujours ou vous etes.
		if (Type != ELREnemy::Pisteur && Type != ELREnemy::Matriarche && !HasLineOfSight(C))
		{
			continue;
		}
		Best = C;
		BestDist = Dist;
	}
	return Best;
}

void ALREnemy::StopMoving()
{
	Path.Reset();
	PathIdx = 0;
}

void ALREnemy::FaceTo(const FVector& Loc)
{
	FVector Dir = Loc - GetActorLocation();
	Dir.Z = 0.f;
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}
}

void ALREnemy::GoTo(const FVector& Dest, bool bRun)
{
	const FLREnemyStats& S = StatsFor(Type);
	GetCharacterMovement()->MaxWalkSpeed = bRun ? S.Speed : S.Speed * 0.4f;

	const float Now = LR::Now(this);
	if (Now < NextRepath && PathIdx < Path.Num() && FVector::Dist2D(Dest, PathGoal) < 200.f)
	{
		return;
	}
	NextRepath = Now + 0.5f + FMath::FRandRange(0.f, 0.3f);
	PathGoal = Dest;
	Path.Reset();
	PathIdx = 0;

	// Ligne droite si rien ne gene, sinon chemin par la grille de la ville
	const FVector Me = GetActorLocation();
	FCollisionQueryParams Query;
	Query.AddIgnoredActor(this);
	const FVector A(Me.X, Me.Y, 60.f);
	const FVector B(Dest.X, Dest.Y, 60.f);
	const bool bBlockedLine = GetWorld()->LineTraceTestByChannel(A, B, ECC_Visibility, Query);
	if (bBlockedLine)
	{
		const ALRGameState* GS = GetWorld()->GetGameState<ALRGameState>();
		if (GS)
		{
			GS->Layout.FindPath(Me, Dest, Path);
		}
	}
	if (Path.Num() == 0)
	{
		Path.Add(Dest);
	}
}

void ALREnemy::Wander()
{
	const float Now = LR::Now(this);
	if (PathIdx < Path.Num() || Now < NextWander)
	{
		return;
	}
	NextWander = Now + FMath::FRandRange(4.f, 9.f);
	const ALRGameState* GS = GetWorld()->GetGameState<ALRGameState>();
	if (!GS || GS->Layout.Open.Num() == 0)
	{
		return;
	}
	const FVector Me = GetActorLocation();
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const FIntPoint Cell = GS->Layout.Open[FMath::RandRange(0, GS->Layout.Open.Num() - 1)];
		const FVector Dest = FLRCityLayout::CellToWorld(Cell);
		if (FVector::Dist2D(Me, Dest) < 3500.f)
		{
			NextRepath = 0.f;
			GoTo(Dest, false);
			return;
		}
	}
}

void ALREnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bDead)
	{
		return;
	}

	ThinkTimer -= DeltaSeconds;
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = 0.2f + FMath::FRandRange(0.f, 0.1f);
		Think();
	}

	if (PathIdx < Path.Num())
	{
		FVector To = Path[PathIdx] - GetActorLocation();
		To.Z = 0.f;
		if (To.SizeSquared() < FMath::Square(70.f))
		{
			++PathIdx;
		}
		else
		{
			AddMovementInput(To.GetSafeNormal(), 1.f);
		}
	}
}

void ALREnemy::Think()
{
	ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>();
	ALRGameState* GS = GetWorld()->GetGameState<ALRGameState>();
	if (!GM || !GS)
	{
		return;
	}
	const FLREnemyStats& S = StatsFor(Type);
	const float Now = LR::Now(this);
	const FVector Me = GetActorLocation();
	const float Step = 0.25f;

	// Coince contre un obstacle : on recalcule le chemin
	if (PathIdx < Path.Num())
	{
		if (FVector::Dist2D(Me, LastPos) < 15.f)
		{
			StuckTime += Step;
			if (StuckTime > 1.5f)
			{
				StuckTime = 0.f;
				NextRepath = 0.f;
				const FVector Dest = PathGoal;
				Path.Reset();
				PathIdx = 0;
				if (GS->Layout.FindPath(Me, Dest, Path) == false)
				{
					bInvestigating = false;
				}
			}
		}
		else
		{
			StuckTime = 0.f;
		}
	}
	LastPos = Me;

	ALRCharacter* Target = FindTarget();

	// ---- Sentinelle : immobile, hurle si elle voit un joueur trop longtemps
	if (Type == ELREnemy::Sentinelle)
	{
		if (Target)
		{
			FaceTo(Target->GetActorLocation());
			SeenTime += Step;
			if (SeenTime >= 1.2f && Now >= NextSpecial)
			{
				NextSpecial = Now + 20.f;
				SeenTime = 0.f;
				GM->Say(TEXT("Une Sentinelle hurle !"));
				GM->EmitNoise(Target->GetActorLocation(), 5000.f, 8.f);
				GS->MulticastFx(4, Me, FVector(600.f, 0.f, 0.f));
			}
		}
		else
		{
			SeenTime = FMath::Max(0.f, SeenTime - Step);
		}
		return;
	}

	// ---- Glaneuse : vole un objet puis fuit vers un nid
	if (Type == ELREnemy::Glaneuse)
	{
		if (bFleeing)
		{
			ALRNest* Nest = FleeNest.Get();
			const FVector Dest = Nest ? Nest->GetActorLocation() : LastKnown;
			if (FVector::Dist2D(Me, Dest) < 300.f)
			{
				if (Nest && Carried.Count > 0)
				{
					Nest->Stored.Add(Carried);
					Carried = FLRStack();
				}
				bFleeing = false;
				NextSpecial = Now + 12.f;
				StopMoving();
			}
			else
			{
				GoTo(Dest, true);
			}
			return;
		}
		if (Target && Carried.Count <= 0 && Now >= NextSpecial)
		{
			if (FVector::Dist2D(Me, Target->GetActorLocation()) < S.Range + 40.f)
			{
				// Vol d'un objet au hasard (jamais un objet de mission)
				TArray<int32> Candidates;
				for (int32 i = 0; i < Target->Inv.Items.Num(); ++i)
				{
					const FLRItemDef* Def = FLRItems::Find(Target->Inv.Items[i].Id);
					if (Def && Def->Type != ELRItem::Mission)
					{
						Candidates.Add(i);
					}
				}
				if (Candidates.Num() > 0)
				{
					const int32 Pick = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
					Carried = Target->Inv.Items[Pick];
					Target->Inv.Items.RemoveAt(Pick);
					Target->Notify(FString::Printf(TEXT("Une Glaneuse vous a volé : %s x%d !"), *FLRItems::Name(Carried.Id), Carried.Count));
				}
				bFleeing = true;
				FleeNest = GM->NearestNest(Me);
				LastKnown = Me + (Me - Target->GetActorLocation()).GetSafeNormal2D() * 3000.f;
				NextRepath = 0.f;
			}
			else
			{
				GoTo(Target->GetActorLocation(), true);
			}
			return;
		}
		Wander();
		return;
	}

	// ---- Hote : tire a distance
	if (Type == ELREnemy::Hote && Target)
	{
		const FVector TargetLoc = Target->GetActorLocation();
		const float Dist = FVector::Dist2D(Me, TargetLoc);
		LastKnown = TargetLoc;
		bInvestigating = true;
		if (Dist > 900.f)
		{
			GoTo(TargetLoc, true);
		}
		else
		{
			StopMoving();
			FaceTo(TargetLoc);
		}
		if (Now >= NextAttack)
		{
			NextAttack = Now + S.Cooldown;
			const bool bHit = FMath::FRand() < 0.55f;
			const FVector Miss(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(20.f, 110.f));
			GS->MulticastFx(1, Me + FVector(0.f, 0.f, 30.f), bHit ? TargetLoc : TargetLoc + Miss);
			if (bHit)
			{
				Target->Hurt(S.Damage * Power);
			}
		}
		return;
	}

	// ---- Matriarche : pond des larves
	if (Type == ELREnemy::Matriarche && Now >= NextSpecial)
	{
		NextSpecial = Now + 7.f;
		for (int32 i = 0; i < 2; ++i)
		{
			GM->SpawnEnemy(ELREnemy::Larve, Me + FVector(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f), 0.f));
		}
	}

	// ---- Corps a corps (Larve, Pisteur, Carapace, Cocon, Matriarche, Hote sans cible)
	if (Target)
	{
		const FVector TargetLoc = Target->GetActorLocation();
		LastKnown = TargetLoc;
		bInvestigating = true;
		if (FVector::Dist2D(Me, TargetLoc) < S.Range + S.Radius)
		{
			StopMoving();
			FaceTo(TargetLoc);
			if (Now >= NextAttack)
			{
				NextAttack = Now + S.Cooldown;
				if (Type == ELREnemy::Matriarche)
				{
					// Coup de zone
					GS->MulticastFx(2, Me, FVector(S.Range + S.Radius, 0.f, 0.f));
					for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
					{
						if (FVector::Dist2D(Me, It->GetActorLocation()) < S.Range + S.Radius + 60.f)
						{
							It->Hurt(S.Damage * Power);
						}
					}
				}
				else
				{
					Target->Hurt(S.Damage * Power);
				}
			}
		}
		else
		{
			GoTo(TargetLoc, true);
		}
	}
	else if (bInvestigating)
	{
		if (FVector::Dist2D(Me, LastKnown) < 160.f)
		{
			bInvestigating = false;
			StopMoving();
		}
		else
		{
			GoTo(LastKnown, true);
		}
	}
	else
	{
		Wander();
	}
}
