#include "LRCharacter.h"
#include "LREnemy.h"
#include "LRGameMode.h"
#include "LRGameState.h"
#include "LRItems.h"
#include "LRPlayerController.h"
#include "LRWorldActors.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

ALRCharacter::ALRCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 88.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->MaxWalkSpeed = 450.f;
	Move->MaxWalkSpeedCrouched = 220.f;
	Move->JumpZVelocity = 480.f;
	Move->AirControl = 0.3f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Boom->TargetArmLength = 360.f;
	Boom->SocketOffset = FVector(0.f, 65.f, 60.f);
	Boom->bUsePawnControlRotation = true;

	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	Cam->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Cam->bUsePawnControlRotation = false;

	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);

	Body = LR::Part(this, TEXT("Body"), Visual, LR::Cylinder(), FVector(0.f, 0.f, -25.f), FVector(0.6f, 0.6f, 1.2f));
	Head = LR::Part(this, TEXT("Head"), Visual, LR::Sphere(), FVector(0.f, 0.f, 62.f), FVector(0.5f));
	Pack = LR::Part(this, TEXT("Pack"), Visual, LR::Cube(), FVector(-34.f, 0.f, 5.f), FVector(0.25f, 0.45f, 0.5f));
	Gun = LR::Part(this, TEXT("Gun"), Visual, LR::Cube(), FVector(38.f, 28.f, 20.f), FVector(0.7f, 0.1f, 0.14f));
}

void ALRCharacter::BeginPlay()
{
	Super::BeginPlay();
	const bool bMine = IsLocallyControlled();
	LR::Tint(Body, bMine ? FLinearColor(0.1f, 0.5f, 0.55f) : FLinearColor(0.85f, 0.45f, 0.1f));
	LR::Tint(Head, FLinearColor(0.85f, 0.7f, 0.55f));
	LR::Tint(Pack, FLinearColor(0.15f, 0.2f, 0.12f));
	LR::Tint(Gun, FLinearColor(0.03f, 0.03f, 0.03f));
	ApplyLife();
}

void ALRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALRCharacter, Health);
	DOREPLIFETIME(ALRCharacter, MaxHealth);
	DOREPLIFETIME(ALRCharacter, Life);
	DOREPLIFETIME_CONDITION(ALRCharacter, Inv, COND_OwnerOnly);
	DOREPLIFETIME(ALRCharacter, bInRaid);
	DOREPLIFETIME(ALRCharacter, bSprinting);
	DOREPLIFETIME(ALRCharacter, BleedEnd);
	DOREPLIFETIME(ALRCharacter, ChannelStart);
	DOREPLIFETIME(ALRCharacter, ChannelEnd);
	DOREPLIFETIME(ALRCharacter, ReloadEnd);
	DOREPLIFETIME(ALRCharacter, PheroEnd);
	DOREPLIFETIME(ALRCharacter, BeaconCount);
	DOREPLIFETIME(ALRCharacter, Infection);
}

FString ALRCharacter::GetDisplayName() const
{
	const APlayerState* PS = GetPlayerState();
	return PS ? PS->GetPlayerName() : FString(TEXT("Glaneur"));
}

void ALRCharacter::OnRep_Life()
{
	ApplyLife();
}

void ALRCharacter::ApplyLife()
{
	const bool bGone = (Life == ELRLife::Mort || Life == ELRLife::Extrait);
	SetActorHiddenInGame(bGone);
	// Un joueur mort ou extrait ne doit plus bloquer personne.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bGone ? ECR_Ignore : ECR_Block);
	if (Visual)
	{
		Visual->SetRelativeRotation(Life == ELRLife::ATerre ? FRotator(-85.f, 0.f, 0.f) : FRotator::ZeroRotator);
	}
}

void ALRCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Vitesse : identique sur le serveur et chez le proprietaire
	float Speed = bSprinting ? 680.f : 450.f;
	if (const FLRItemDef* Armor = FLRItems::Find(Inv.Armor))
	{
		if (Armor->Type == ELRItem::Armure && Armor->B > 0.f)
		{
			Speed *= Armor->B;
		}
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	if (Visual && Life != ELRLife::ATerre)
	{
		Visual->SetRelativeScale3D(FVector(1.f, 1.f, bIsCrouched ? 0.5f : 1.f));
	}

	if (HasAuthority())
	{
		ServerTick(DeltaSeconds);
	}
	if (IsLocallyControlled())
	{
		UpdateFocus();
	}
}

void ALRCharacter::ServerTick(float DeltaSeconds)
{
	const float Now = LR::Now(this);
	ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>();

	// Fin du rechargement
	if (ReloadEnd > 0.f && Now >= ReloadEnd)
	{
		ReloadEnd = 0.f;
		const FLRWeaponStats Stats = FLRItems::WeaponStats(Inv);
		if (Stats.bValid)
		{
			const int32 N = FMath::Min(Stats.MagSize - Inv.Mag, FLRItems::CountOf(Inv, LRId::Munitions));
			if (N > 0 && FLRItems::Remove(Inv, LRId::Munitions, N))
			{
				Inv.Mag += N;
			}
		}
	}

	// Chargeur vivant : regenere une balle toutes les 2 secondes
	if (bInRaid && Life == ELRLife::Vivant && Now >= NextRegen)
	{
		NextRegen = Now + 2.f;
		const FLRWeaponStats Stats = FLRItems::WeaponStats(Inv);
		if (Stats.bValid && Stats.bRegen && Inv.Mag < Stats.MagSize)
		{
			Inv.Mag++;
		}
	}

	// Interaction en cours
	if (ChannelEnd > 0.f)
	{
		AActor* Target = ChannelTarget.Get();
		if (!Target || Life != ELRLife::Vivant || FVector::Dist(GetActorLocation(), ChannelOrigin) > 130.f)
		{
			CancelChannel();
		}
		else if (Now >= ChannelEnd)
		{
			CancelChannel();
			FinishInteract(Target);
		}
	}

	// Hemorragie
	if (Life == ELRLife::ATerre && Now >= BleedEnd)
	{
		Die();
	}

	// Bruits de pas : marcher s'entend de pres, courir de loin, s'accroupir est silencieux
	if (GM && bInRaid && Life == ELRLife::Vivant && Now >= NextStep)
	{
		NextStep = Now + 0.5f;
		if (!bIsCrouched && GetVelocity().Size2D() > 120.f)
		{
			GM->EmitNoise(GetActorLocation(), bSprinting ? 1200.f : 500.f, bSprinting ? 0.03f : 0.f);
		}
	}
}

// ---------------------------------------------------------------- Tir

void ALRCharacter::LocalFire(bool bJustPressed)
{
	if (Life != ELRLife::Vivant || !bInRaid || !Controller)
	{
		return;
	}
	const FLRWeaponStats Stats = FLRItems::WeaponStats(Inv);
	if (!Stats.bValid || (!Stats.bAuto && !bJustPressed))
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextLocalShot || ReloadEnd > 0.f)
	{
		return;
	}
	if (Inv.Mag <= 0)
	{
		if (bJustPressed)
		{
			ServerReload();
		}
		return;
	}
	NextLocalShot = Now + Stats.Interval;

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
	ServerFire(ViewLoc, ViewRot.Vector());
}

void ALRCharacter::ServerFire_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Dir)
{
	if (Life != ELRLife::Vivant || !bInRaid || ReloadEnd > 0.f)
	{
		return;
	}
	const FLRWeaponStats Stats = FLRItems::WeaponStats(Inv);
	if (!Stats.bValid || Inv.Mag <= 0)
	{
		return;
	}
	const float Now = LR::Now(this);
	if (Now < NextServerShot)
	{
		return;
	}
	NextServerShot = Now + Stats.Interval * 0.75f;
	Inv.Mag--;
	CancelChannel();

	ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>();
	ALRGameState* GS = GetWorld()->GetGameState<ALRGameState>();

	const FVector StartLoc = Start;
	const FVector AimDir = FVector(Dir).GetSafeNormal();
	// Le tir part de la camera, mais on avance le depart au niveau du personnage
	// pour ne rien toucher derriere lui.
	const float Advance = FMath::Max(0.f, float(FVector::DotProduct(GetActorLocation() - StartLoc, AimDir)));
	const FVector From = StartLoc + AimDir * Advance;
	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 40.f) + AimDir * 50.f;

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams Query;
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}

	bool bHitSomething = false;
	bool bKill = false;
	for (int32 i = 0; i < Stats.Pellets; ++i)
	{
		const FVector ShotDir = FMath::VRandCone(AimDir, FMath::DegreesToRadians(Stats.Spread));
		const FVector End = From + ShotDir * 12000.f;
		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, From, End, ObjParams, Query);
		const FVector Impact = bHit ? FVector(Hit.ImpactPoint) : End;
		if (bHit)
		{
			AActor* HitActor = Hit.GetActor();
			if (ALREnemy* Enemy = Cast<ALREnemy>(HitActor))
			{
				bHitSomething = true;
				if (Enemy->ApplyHit(Stats.Damage, ShotDir, this, Stats.bAcid))
				{
					bKill = true;
				}
			}
			else if (ALRDestructible* Destructible = Cast<ALRDestructible>(HitActor))
			{
				bHitSomething = true;
				Destructible->ApplyHit(Stats.Damage, this);
			}
		}
		if (GS)
		{
			GS->MulticastFx(0, Muzzle, Impact);
		}
	}

	if (bHitSomething)
	{
		ClientHitMarker(bKill);
	}
	if (GM)
	{
		GM->EmitNoise(GetActorLocation(), Stats.Noise * 300.f, Stats.Noise * 0.02f);
	}
}

void ALRCharacter::ClientHitMarker_Implementation(bool bKill)
{
	HitMarkerTime = GetWorld()->GetTimeSeconds();
	bHitMarkerKill = bKill;
}

void ALRCharacter::ServerReload_Implementation()
{
	if (Life != ELRLife::Vivant || ReloadEnd > 0.f)
	{
		return;
	}
	const FLRWeaponStats Stats = FLRItems::WeaponStats(Inv);
	if (!Stats.bValid || Inv.Mag >= Stats.MagSize)
	{
		return;
	}
	if (FLRItems::CountOf(Inv, LRId::Munitions) <= 0)
	{
		Notify(TEXT("Plus de munitions."));
		return;
	}
	ReloadEnd = LR::Now(this) + Stats.Reload;
}

void ALRCharacter::SetSprint(bool bNewSprint)
{
	if (bSprinting != bNewSprint)
	{
		bSprinting = bNewSprint;
		ServerSetSprint(bNewSprint);
	}
}

void ALRCharacter::ServerSetSprint_Implementation(bool bNewSprint)
{
	bSprinting = bNewSprint;
}

// ---------------------------------------------------------------- Interaction

void ALRCharacter::UpdateFocus()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextFocus)
	{
		return;
	}
	NextFocus = Now + 0.1f;

	Focus = nullptr;
	FocusPrompt.Reset();
	if (Life != ELRLife::Vivant)
	{
		return;
	}

	const FVector Me = GetActorLocation();
	float Best = MAX_flt;
	for (TActorIterator<ALRInteractable> It(GetWorld()); It; ++It)
	{
		ALRInteractable* Item = *It;
		const float Dist = FVector::Dist(Me, Item->GetActorLocation());
		if (Dist > Item->Range || Dist >= Best || !Item->CanInteract(this))
		{
			continue;
		}
		Best = Dist;
		Focus = Item;
		FocusPrompt = Item->GetPrompt(this);
	}
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		ALRCharacter* Other = *It;
		if (Other == this || Other->Life != ELRLife::ATerre)
		{
			continue;
		}
		const float Dist = FVector::Dist(Me, Other->GetActorLocation());
		if (Dist < 260.f && Dist < Best)
		{
			Best = Dist;
			Focus = Other;
			FocusPrompt = FString::Printf(TEXT("Relever %s"), *Other->GetDisplayName());
		}
	}
}

void ALRCharacter::TryInteract()
{
	if (Focus.IsValid())
	{
		ServerInteract(Focus.Get());
	}
}

void ALRCharacter::ServerInteract_Implementation(AActor* Target)
{
	if (Life != ELRLife::Vivant || !Target || ChannelEnd > 0.f)
	{
		return;
	}
	const float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	float Duration = 0.f;

	if (ALRCharacter* Other = Cast<ALRCharacter>(Target))
	{
		if (Other == this || Other->Life != ELRLife::ATerre || Dist > 320.f)
		{
			return;
		}
		Duration = 3.f;
	}
	else if (ALRInteractable* Item = Cast<ALRInteractable>(Target))
	{
		if (Dist > Item->Range + 80.f || !Item->CanInteract(this))
		{
			return;
		}
		Duration = Item->GetChannelTime(this);
	}
	else
	{
		return;
	}

	if (Duration <= 0.f)
	{
		FinishInteract(Target);
		return;
	}
	const float Now = LR::Now(this);
	ChannelTarget = Target;
	ChannelOrigin = GetActorLocation();
	ChannelStart = Now;
	ChannelEnd = Now + Duration;
}

void ALRCharacter::CancelChannel()
{
	ChannelTarget = nullptr;
	ChannelStart = 0.f;
	ChannelEnd = 0.f;
}

void ALRCharacter::FinishInteract(AActor* Target)
{
	if (ALRCharacter* Other = Cast<ALRCharacter>(Target))
	{
		if (Other->Life == ELRLife::ATerre)
		{
			Other->ReviveFromDown();
		}
		return;
	}
	if (ALRInteractable* Item = Cast<ALRInteractable>(Target))
	{
		if (Item->CanInteract(this))
		{
			Item->OnInteract(this);
		}
	}
}

// ---------------------------------------------------------------- Objets

void ALRCharacter::QuickUse(FName Id)
{
	for (int32 i = 0; i < Inv.Items.Num(); ++i)
	{
		const FLRItemDef* Def = FLRItems::Find(Inv.Items[i].Id);
		if (!Def)
		{
			continue;
		}
		const bool bMatch = Id.IsNone() ? (Def->Type == ELRItem::Soin) : (Def->Id == Id);
		if (bMatch)
		{
			ServerUseItem(i);
			return;
		}
	}
}

void ALRCharacter::ServerUseItem_Implementation(int32 Index)
{
	if (Life != ELRLife::Vivant || !Inv.Items.IsValidIndex(Index))
	{
		return;
	}
	const FName Id = Inv.Items[Index].Id;
	const FLRItemDef* Def = FLRItems::Find(Id);
	if (!Def)
	{
		return;
	}

	bool bUsed = false;
	if (Def->Type == ELRItem::Soin)
	{
		if (Health < MaxHealth)
		{
			Health = FMath::Min(MaxHealth, Health + Def->A);
			bUsed = true;
		}
	}
	else if (Def->Type == ELRItem::Serum)
	{
		if (Infection > 0)
		{
			Infection = 0;
			MaxHealth = 100.f;
			bUsed = true;
			Notify(TEXT("Infection soignée."));
		}
	}
	else if (Def->Type == ELRItem::Gadget && bInRaid)
	{
		ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>();
		if (Id == LRId::Leurre && GM)
		{
			// Le leurre atterrit devant le joueur, ou contre le premier mur
			FVector Dir = GetBaseAimRotation().Vector();
			Dir.Z = 0.f;
			Dir = Dir.GetSafeNormal();
			const FVector From = GetActorLocation();
			FVector To = From + Dir * 1400.f;
			FHitResult Hit;
			FCollisionQueryParams Query;
			Query.AddIgnoredActor(this);
			if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query))
			{
				To = FVector(Hit.ImpactPoint) - Dir * 60.f;
			}
			To.Z = 20.f;
			GM->AddLure(To);
			bUsed = true;
		}
		else if (Id == LRId::Pheromones)
		{
			PheroEnd = LR::Now(this) + 10.f;
			bUsed = true;
			Notify(TEXT("Phéromones : les créatures vous ignorent pendant 10 secondes."));
		}
	}

	if (bUsed)
	{
		if (--Inv.Items[Index].Count <= 0)
		{
			Inv.Items.RemoveAt(Index);
		}
	}
}

void ALRCharacter::ServerDropItem_Implementation(int32 Index)
{
	if (Life != ELRLife::Vivant || !bInRaid || !Inv.Items.IsValidIndex(Index))
	{
		return;
	}
	const FLRStack Stack = Inv.Items[Index];
	const FLRItemDef* Def = FLRItems::Find(Stack.Id);
	if (Def && Def->Type == ELRItem::Mission)
	{
		Notify(TEXT("Un objet de mission ne se jette pas."));
		return;
	}
	Inv.Items.RemoveAt(Index);
	if (ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>())
	{
		GM->SpawnPickup(Stack.Id, Stack.Count, GetActorLocation() + GetActorForwardVector() * 110.f, false);
	}
}

void ALRCharacter::ServerEquip_Implementation(int32 Index)
{
	if (Life == ELRLife::Vivant && !FLRItems::Equip(Inv, Index))
	{
		Notify(TEXT("Impossible d'équiper cet objet."));
	}
}

void ALRCharacter::ServerUnequip_Implementation(int32 Slot)
{
	if (Life == ELRLife::Vivant && !FLRItems::Unequip(Inv, Slot))
	{
		Notify(TEXT("Pas assez de place dans le sac."));
	}
}

void ALRCharacter::ServerPocket_Implementation(int32 Index, bool bToPocket)
{
	if (Life != ELRLife::Vivant)
	{
		return;
	}
	const bool bOk = bToPocket ? FLRItems::ToPocket(Inv, Index) : FLRItems::FromPocket(Inv, Index);
	if (!bOk)
	{
		Notify(bToPocket ? TEXT("Poche pleine (2 places), ou objet de mission.") : TEXT("Sac plein."));
	}
}

// ---------------------------------------------------------------- Vie et mort

void ALRCharacter::Notify(const FString& Msg)
{
	if (ALRPlayerController* PC = Cast<ALRPlayerController>(Controller))
	{
		PC->ClientNotify(Msg);
	}
}

void ALRCharacter::Hurt(float Damage)
{
	if (!HasAuthority() || Life != ELRLife::Vivant || !bInRaid)
	{
		return;
	}
	if (const FLRItemDef* Armor = FLRItems::Find(Inv.Armor))
	{
		if (Armor->Type == ELRItem::Armure)
		{
			Damage *= (1.f - Armor->A);
		}
	}
	Health -= Damage;
	CancelChannel();
	if (Health <= 0.f)
	{
		Health = 0.f;
		GoDown();
	}
}

void ALRCharacter::GoDown()
{
	ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>();
	// Sans coequipier debout pour vous relever, c'est la mort directe.
	if (!GM || !GM->AnyoneStanding(this))
	{
		Die();
		return;
	}
	Life = ELRLife::ATerre;
	BleedEnd = LR::Now(this) + 45.f;
	ReloadEnd = 0.f;
	GetCharacterMovement()->DisableMovement();
	ApplyLife();
	GM->Say(FString::Printf(TEXT("%s est à terre !"), *GetDisplayName()));
}

void ALRCharacter::Die()
{
	if (Life == ELRLife::Mort || Life == ELRLife::Extrait)
	{
		return;
	}
	Life = ELRLife::Mort;
	Health = 0.f;
	ReloadEnd = 0.f;
	CancelChannel();
	GetCharacterMovement()->DisableMovement();
	ApplyLife();

	if (ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>())
	{
		GM->Say(FString::Printf(TEXT("%s est mort."), *GetDisplayName()));
		// Les balises portees retombent au sol, avec la sienne.
		for (ALRCharacter* Carried : CarriedDead)
		{
			if (Carried && Carried->Life == ELRLife::Mort)
			{
				GM->SpawnBeacon(Carried, GetActorLocation() + FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), 0.f));
			}
		}
		GM->SpawnBeacon(this, GetActorLocation());
	}
	CarriedDead.Reset();
	BeaconCount = 0;
}

void ALRCharacter::ReviveFromDown()
{
	if (Life != ELRLife::ATerre)
	{
		return;
	}
	Life = ELRLife::Vivant;
	Infection++;
	MaxHealth = FMath::Max(40.f, 100.f - 20.f * Infection);
	Health = FMath::Min(30.f, MaxHealth);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	ApplyLife();
	Notify(TEXT("Relevé, mais infecté : vie maximale réduite jusqu'à la prise d'un sérum."));
}

void ALRCharacter::ReviveAt(const FVector& Loc, float NewHealth)
{
	if (Life != ELRLife::Mort)
	{
		return;
	}
	Life = ELRLife::Vivant;
	Infection++;
	MaxHealth = FMath::Max(40.f, 100.f - 20.f * Infection);
	Health = FMath::Min(NewHealth, MaxHealth);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	TeleportTo(Loc, GetActorRotation(), false, true);
	ApplyLife();
	Notify(TEXT("Réanimé à la station médicale."));
}

void ALRCharacter::Extract()
{
	if (Life != ELRLife::Vivant || !bInRaid)
	{
		return;
	}
	Life = ELRLife::Extrait;
	CancelChannel();
	GetCharacterMovement()->DisableMovement();
	ApplyLife();

	ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>();
	if (GM)
	{
		GM->Say(FString::Printf(TEXT("%s s'est extrait."), *GetDisplayName()));
	}
	// Les coequipiers dont on porte la balise sont sauves avec leur equipement.
	for (ALRCharacter* Carried : CarriedDead)
	{
		if (Carried && Carried->Life == ELRLife::Mort)
		{
			Carried->Life = ELRLife::Extrait;
			Carried->ApplyLife();
			Carried->Notify(TEXT("Votre balise a été extraite : vous gardez votre équipement."));
		}
	}
	CarriedDead.Reset();
	BeaconCount = 0;
}

void ALRCharacter::EnterRaid(const FLRLoadout& Loadout, const FVector& Loc)
{
	Inv = Loadout;
	FLRItems::EnsureMods(Inv);
	MaxHealth = 100.f;
	Health = 100.f;
	Infection = 0;
	Life = ELRLife::Vivant;
	bInRaid = true;
	ReloadEnd = 0.f;
	PheroEnd = 0.f;
	BleedEnd = 0.f;
	CarriedDead.Reset();
	BeaconCount = 0;
	CancelChannel();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	TeleportTo(Loc, FRotator(0.f, 45.f, 0.f), false, true);
	ApplyLife();
}

void ALRCharacter::ReturnToPlanque(const FVector& Loc)
{
	MaxHealth = 100.f;
	Health = 100.f;
	Infection = 0;
	Life = ELRLife::Vivant;
	bInRaid = false;
	ReloadEnd = 0.f;
	PheroEnd = 0.f;
	CarriedDead.Reset();
	BeaconCount = 0;
	CancelChannel();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	TeleportTo(Loc, FRotator::ZeroRotator, false, true);
	ApplyLife();
}
