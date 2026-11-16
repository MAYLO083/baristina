#include "LRWorldActors.h"
#include "LRCharacter.h"
#include "LRGameMode.h"
#include "LRGameState.h"
#include "LRItems.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

namespace
{
	UStaticMesh* LoadShape(TWeakObjectPtr<UStaticMesh>& Cache, const TCHAR* Path)
	{
		if (!Cache.IsValid())
		{
			Cache = LoadObject<UStaticMesh>(nullptr, Path);
		}
		return Cache.Get();
	}

	ALRGameMode* GetGM(const AActor* Actor)
	{
		return Actor && Actor->GetWorld() ? Actor->GetWorld()->GetAuthGameMode<ALRGameMode>() : nullptr;
	}
}

UStaticMesh* LR::Cube()
{
	static TWeakObjectPtr<UStaticMesh> Cache;
	return LoadShape(Cache, TEXT("/Engine/BasicShapes/Cube.Cube"));
}

UStaticMesh* LR::Sphere()
{
	static TWeakObjectPtr<UStaticMesh> Cache;
	return LoadShape(Cache, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
}

UStaticMesh* LR::Cylinder()
{
	static TWeakObjectPtr<UStaticMesh> Cache;
	return LoadShape(Cache, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
}

UStaticMesh* LR::Cone()
{
	static TWeakObjectPtr<UStaticMesh> Cache;
	return LoadShape(Cache, TEXT("/Engine/BasicShapes/Cone.Cone"));
}

UMaterialInterface* LR::ColorMaterial()
{
	static TWeakObjectPtr<UMaterialInterface> Cache;
	if (Cache.IsValid())
	{
		return Cache.Get();
	}
	const FString PackageName = TEXT("/Game/LaRuche/M_LRColor");
	UMaterialInterface* Result = nullptr;
	if (FPackageName::DoesPackageExist(PackageName))
	{
		Result = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LaRuche/M_LRColor.M_LRColor"));
	}

#if WITH_EDITOR
	if (!Result)
	{
		// Premier lancement dans l'editeur : on fabrique le materiau (couleur + lueur) et on l'enregistre,
		// pour qu'il soit aussi present dans le jeu empaquete.
		UPackage* Package = CreatePackage(*PackageName);
		UMaterial* NewMat = NewObject<UMaterial>(Package, TEXT("M_LRColor"), RF_Public | RF_Standalone);

		UMaterialExpressionVectorParameter* ColorParam = NewObject<UMaterialExpressionVectorParameter>(NewMat);
		ColorParam->ParameterName = TEXT("Color");
		ColorParam->DefaultValue = FLinearColor(0.5f, 0.5f, 0.5f);

		UMaterialExpressionScalarParameter* GlowParam = NewObject<UMaterialExpressionScalarParameter>(NewMat);
		GlowParam->ParameterName = TEXT("Glow");
		GlowParam->DefaultValue = 0.f;

		UMaterialExpressionMultiply* Mul = NewObject<UMaterialExpressionMultiply>(NewMat);
		Mul->A.Expression = ColorParam;
		Mul->B.Expression = GlowParam;

		NewMat->GetExpressionCollection().AddExpression(ColorParam);
		NewMat->GetExpressionCollection().AddExpression(GlowParam);
		NewMat->GetExpressionCollection().AddExpression(Mul);
		NewMat->GetEditorOnlyData()->BaseColor.Expression = ColorParam;
		NewMat->GetEditorOnlyData()->EmissiveColor.Expression = Mul;
		NewMat->PostEditChange();

		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		UPackage::SavePackage(Package, NewMat, *FileName, SaveArgs);
		Result = NewMat;
	}
#endif

	Cache = Result;
	return Result;
}

void LR::Tint(UStaticMeshComponent* Comp, const FLinearColor& Color, float Glow)
{
	if (!Comp)
	{
		return;
	}
	UMaterialInterface* Base = ColorMaterial();
	UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
	if (!Mat || (Base && Mat->Parent != Base))
	{
		Mat = Base ? Comp->CreateDynamicMaterialInstance(0, Base) : Comp->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		Mat->SetScalarParameterValue(TEXT("Glow"), Glow);
	}
}

UStaticMeshComponent* LR::Part(AActor* Owner, FName Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale)
{
	UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetupAttachment(Parent);
	Comp->SetStaticMesh(Mesh);
	Comp->SetRelativeLocation(Loc);
	Comp->SetRelativeScale3D(Scale);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCanEverAffectNavigation(false);
	return Comp;
}

float LR::Now(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return 0.f;
	}
	const AGameStateBase* GS = World->GetGameState();
	return GS ? float(GS->GetServerWorldTimeSeconds()) : float(World->GetTimeSeconds());
}

// ---------------------------------------------------------------- Bloc

ALRBlock::ALRBlock()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetStaticMesh(LR::Cube());
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCanEverAffectNavigation(false);
}

// ---------------------------------------------------------------- Environnement

ALREnvironment::ALREnvironment()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-22.f, 40.f, 0.f));

	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->bRealTimeCapture = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
}

void ALREnvironment::BeginPlay()
{
	Super::BeginPlay();
	SetTier(1);
}

void ALREnvironment::SetTier(int32 Tier)
{
	Tier = FMath::Clamp(Tier, 1, 4);
	static const FLinearColor FogColors[4] = {
		FLinearColor(0.10f, 0.16f, 0.12f),
		FLinearColor(0.16f, 0.16f, 0.08f),
		FLinearColor(0.22f, 0.10f, 0.06f),
		FLinearColor(0.30f, 0.04f, 0.04f)
	};
	static const FLinearColor SunColors[4] = {
		FLinearColor(1.f, 0.95f, 0.85f),
		FLinearColor(1.f, 0.9f, 0.7f),
		FLinearColor(1.f, 0.7f, 0.5f),
		FLinearColor(1.f, 0.45f, 0.35f)
	};
	if (Fog)
	{
		Fog->SetFogDensity(0.045f + 0.012f * Tier);
		Fog->SetFogInscatteringColor(FogColors[Tier - 1]);
	}
	if (Sun)
	{
		Sun->SetLightColor(SunColors[Tier - 1]);
		Sun->SetIntensity(2.5f);
	}
	if (Sky)
	{
		Sky->SetIntensity(1.f);
	}
}

// ---------------------------------------------------------------- Effets

ALRFx::ALRFx()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = LR::Part(this, TEXT("Mesh"), Root, LR::Cube(), FVector::ZeroVector, FVector(0.1f));
	Mesh->SetCastShadow(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(700.f);
}

void ALRFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Grow > 0.f && Mesh)
	{
		Mesh->SetWorldScale3D(Mesh->GetComponentScale() + FVector(Grow * DeltaSeconds));
	}
}

void ALRFx::Tracer(UWorld* World, const FVector& A, const FVector& B, const FLinearColor& Color)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FVector Delta = B - A;
	const float Len = Delta.Size();
	if (Len < 1.f)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALRFx* Fx = World->SpawnActor<ALRFx>(ALRFx::StaticClass(), (A + B) * 0.5f, Delta.Rotation(), Params);
	if (!Fx)
	{
		return;
	}
	Fx->Mesh->SetWorldScale3D(FVector(Len / 100.f, 0.025f, 0.025f));
	LR::Tint(Fx->Mesh, Color, 30.f);
	Fx->Light->SetWorldLocation(A);
	Fx->Light->SetLightColor(Color);
	Fx->Light->SetIntensity(6000.f);
	Fx->SetLifeSpan(0.07f);
}

void ALRFx::Burst(UWorld* World, const FVector& Loc, float Radius, const FLinearColor& Color)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALRFx* Fx = World->SpawnActor<ALRFx>(ALRFx::StaticClass(), Loc, FRotator::ZeroRotator, Params);
	if (!Fx)
	{
		return;
	}
	const float Life = 0.3f;
	const float Final = Radius / 50.f;
	Fx->Mesh->SetStaticMesh(LR::Sphere());
	Fx->Mesh->SetWorldScale3D(FVector(Final * 0.25f));
	Fx->Grow = Final * 0.75f / Life;
	LR::Tint(Fx->Mesh, Color, 8.f);
	Fx->Light->SetLightColor(Color);
	Fx->Light->SetIntensity(12000.f);
	Fx->Light->SetAttenuationRadius(FMath::Max(700.f, Radius * 2.f));
	Fx->SetLifeSpan(Life);
}

// ---------------------------------------------------------------- Interactible

ALRInteractable::ALRInteractable()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = LR::Part(this, TEXT("Mesh"), Root, LR::Cube(), FVector(0.f, 0.f, 30.f), FVector(0.5f));
}

// ---------------------------------------------------------------- Objet au sol

ALRPickup::ALRPickup()
{
	Mesh->SetRelativeScale3D(FVector(0.28f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
	Mesh->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
	Range = 260.f;
}

void ALRPickup::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Stack();
}

void ALRPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALRPickup, Stack);
}

void ALRPickup::OnRep_Stack()
{
	const FLRItemDef* Def = FLRItems::Find(Stack.Id);
	LR::Tint(Mesh, FLRItems::RarityColor(Def ? Def->Rarity : 0), 2.f);
}

FString ALRPickup::GetPrompt(const ALRCharacter* By) const
{
	return FString::Printf(TEXT("Ramasser : %s x%d"), *FLRItems::Name(Stack.Id), Stack.Count);
}

void ALRPickup::OnInteract(ALRCharacter* By)
{
	if (!By || Stack.Count <= 0)
	{
		return;
	}
	const int32 Left = FLRItems::Add(By->Inv, Stack.Id, Stack.Count);
	const int32 Taken = Stack.Count - Left;
	if (Taken <= 0)
	{
		By->Notify(TEXT("Sac plein."));
		return;
	}
	if (ALRGameMode* GM = GetGM(this))
	{
		GM->OnItemPicked(Stack.Id, Taken, By);
	}
	if (Left <= 0)
	{
		Destroy();
	}
	else
	{
		Stack.Count = Left;
	}
}

// ---------------------------------------------------------------- Conteneur

ALRContainer::ALRContainer()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.6f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
}

void ALRContainer::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisual();
}

void ALRContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALRContainer, Kind);
	DOREPLIFETIME(ALRContainer, bOpened);
	DOREPLIFETIME(ALRContainer, bBreathing);
}

void ALRContainer::OnRep_Visual()
{
	ApplyVisual();
}

void ALRContainer::ApplyVisual()
{
	const float Size = (Kind == 1) ? 1.5f : 1.f;
	const float Height = bOpened ? 0.25f : 0.6f;
	Mesh->SetRelativeScale3D(FVector(0.9f * Size, 0.6f * Size, Height * Size));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, Height * Size * 50.f));
	FLinearColor Color = (Kind == 1) ? FLinearColor(0.9f, 0.45f, 0.05f) : FLinearColor(0.32f, 0.24f, 0.12f);
	if (bOpened)
	{
		Color *= 0.35f;
	}
	LR::Tint(Mesh, Color);
}

void ALRContainer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bBreathing && !bOpened && GetWorld())
	{
		const float Pulse = 1.f + 0.035f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.2f);
		Mesh->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.6f) * Pulse);
	}
}

FString ALRContainer::GetPrompt(const ALRCharacter* By) const
{
	return Kind == 1 ? TEXT("Ouvrir le largage (bruyant)") : TEXT("Fouiller");
}

void ALRContainer::OnInteract(ALRCharacter* By)
{
	ALRGameMode* GM = GetGM(this);
	if (!GM || bOpened)
	{
		return;
	}
	if (bBreathing)
	{
		GM->SpawnEnemy(ELREnemy::Cocon, GetActorLocation());
		GM->Say(TEXT("C'était un cocon piégé !"));
		Destroy();
		return;
	}
	bOpened = true;
	ApplyVisual();
	GM->SpawnLoot(GetActorLocation(), Kind);
	if (Kind == 1)
	{
		GM->OnCrateOpened(this);
	}
}

bool ALRContainer::GetMarker(FString& OutLabel, FLinearColor& OutColor) const
{
	if (Kind == 1 && !bOpened)
	{
		OutLabel = TEXT("Largage");
		OutColor = FLinearColor(1.f, 0.6f, 0.1f);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- Extraction

ALRExtraction::ALRExtraction()
{
	Mesh->SetStaticMesh(LR::Cylinder());
	Mesh->SetRelativeScale3D(FVector(3.5f, 3.5f, 0.08f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 4.f));
	Range = 380.f;
}

void ALRExtraction::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisual();
}

void ALRExtraction::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALRExtraction, Kind);
	DOREPLIFETIME(ALRExtraction, bOpen);
	DOREPLIFETIME(ALRExtraction, HeliArrive);
	DOREPLIFETIME(ALRExtraction, HeliLeave);
}

void ALRExtraction::OnRep_Visual()
{
	ApplyVisual();
}

void ALRExtraction::ApplyVisual()
{
	LR::Tint(Mesh, bOpen ? FLinearColor(0.1f, 0.9f, 0.3f) : FLinearColor(0.3f, 0.03f, 0.03f), bOpen ? 3.f : 0.f);
}

int32 ALRExtraction::HeliState() const
{
	if (HeliArrive <= 0.f)
	{
		return 0;
	}
	const float Now = LR::Now(this);
	if (Now < HeliArrive)
	{
		return 1;
	}
	return Now < HeliLeave ? 2 : 0;
}

bool ALRExtraction::CanInteract(const ALRCharacter* By) const
{
	if (!bOpen)
	{
		return false;
	}
	return Kind != ELRExtract::Helico || HeliState() != 1;
}

FString ALRExtraction::GetPrompt(const ALRCharacter* By) const
{
	switch (Kind)
	{
	case ELRExtract::Egout: return TEXT("S'extraire par les égouts");
	case ELRExtract::Sas: return TEXT("S'extraire par le sas (coûte 3 ferraille)");
	default: return HeliState() == 2 ? TEXT("Monter dans l'hélico") : TEXT("Appeler l'hélico (très bruyant, 60 s)");
	}
}

float ALRExtraction::GetChannelTime(const ALRCharacter* By) const
{
	switch (Kind)
	{
	case ELRExtract::Egout: return 6.f;
	case ELRExtract::Sas: return 3.f;
	default: return HeliState() == 2 ? 2.f : 1.f;
	}
}

void ALRExtraction::OnInteract(ALRCharacter* By)
{
	ALRGameMode* GM = GetGM(this);
	if (!By || !GM || !bOpen)
	{
		return;
	}
	if (Kind == ELRExtract::Egout)
	{
		By->Extract();
	}
	else if (Kind == ELRExtract::Sas)
	{
		if (FLRItems::Remove(By->Inv, LRId::Ferraille, 3))
		{
			By->Extract();
		}
		else
		{
			By->Notify(TEXT("Il faut 3 ferraille dans le sac pour ouvrir le sas."));
		}
	}
	else
	{
		const int32 State = HeliState();
		if (State == 2)
		{
			By->Extract();
		}
		else if (State == 0)
		{
			const float Now = LR::Now(this);
			HeliArrive = Now + 60.f;
			HeliLeave = HeliArrive + 75.f;
			GM->EmitNoise(GetActorLocation(), 7000.f, 10.f);
			GM->Say(TEXT("Hélico appelé : arrivée dans 60 secondes. Toute la zone l'a entendu."));
		}
	}
}

bool ALRExtraction::GetMarker(FString& OutLabel, FLinearColor& OutColor) const
{
	if (!bOpen)
	{
		return false;
	}
	OutColor = FLinearColor(0.2f, 1.f, 0.4f);
	switch (Kind)
	{
	case ELRExtract::Egout: OutLabel = TEXT("Extraction : égouts"); break;
	case ELRExtract::Sas: OutLabel = TEXT("Extraction : sas"); break;
	default:
	{
		const int32 State = HeliState();
		if (State == 1)
		{
			OutLabel = FString::Printf(TEXT("Hélico dans %d s"), FMath::CeilToInt(HeliArrive - LR::Now(this)));
		}
		else if (State == 2)
		{
			OutLabel = FString::Printf(TEXT("Hélico sur place (%d s)"), FMath::CeilToInt(HeliLeave - LR::Now(this)));
		}
		else
		{
			OutLabel = TEXT("Extraction : hélico");
		}
		break;
	}
	}
	return true;
}

// ---------------------------------------------------------------- Balise

ALRBeacon::ALRBeacon()
{
	Mesh->SetStaticMesh(LR::Cone());
	Mesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.8f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
}

void ALRBeacon::BeginPlay()
{
	Super::BeginPlay();
	LR::Tint(Mesh, FLinearColor(0.1f, 0.9f, 1.f), 4.f);
}

void ALRBeacon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALRBeacon, Dead);
}

FString ALRBeacon::GetPrompt(const ALRCharacter* By) const
{
	return FString::Printf(TEXT("Récupérer la balise de %s"), Dead ? *Dead->GetDisplayName() : TEXT("?"));
}

void ALRBeacon::OnInteract(ALRCharacter* By)
{
	if (!By || !Dead)
	{
		return;
	}
	By->CarriedDead.AddUnique(Dead);
	By->BeaconCount = By->CarriedDead.Num();
	By->Notify(TEXT("Balise récupérée : portez-la à une station médicale, ou extrayez-vous avec."));
	Destroy();
}

bool ALRBeacon::GetMarker(FString& OutLabel, FLinearColor& OutColor) const
{
	OutLabel = TEXT("Balise");
	OutColor = FLinearColor(0.1f, 0.9f, 1.f);
	return true;
}

// ---------------------------------------------------------------- Station medicale

ALRMedStation::ALRMedStation()
{
	Mesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.7f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 85.f));
}

void ALRMedStation::BeginPlay()
{
	Super::BeginPlay();
	LR::Tint(Mesh, FLinearColor(0.9f, 0.9f, 0.95f));
}

bool ALRMedStation::CanInteract(const ALRCharacter* By) const
{
	return By && By->BeaconCount > 0;
}

void ALRMedStation::OnInteract(ALRCharacter* By)
{
	if (!By)
	{
		return;
	}
	int32 Index = 0;
	for (ALRCharacter* Dead : By->CarriedDead)
	{
		if (Dead && Dead->Life == ELRLife::Mort)
		{
			const FVector Spot = GetActorLocation() + FVector(150.f * (Index + 1), 0.f, 120.f);
			Dead->ReviveAt(Spot, 50.f);
			++Index;
		}
	}
	By->CarriedDead.Reset();
	By->BeaconCount = 0;
}

bool ALRMedStation::GetMarker(FString& OutLabel, FLinearColor& OutColor) const
{
	OutLabel = TEXT("Station médicale");
	OutColor = FLinearColor(0.9f, 0.9f, 1.f);
	return true;
}

// ---------------------------------------------------------------- Site capteur

ALRSensorSite::ALRSensorSite()
{
	Mesh->SetStaticMesh(LR::Cylinder());
	Mesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 2.2f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
}

void ALRSensorSite::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Done();
}

void ALRSensorSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALRSensorSite, bDone);
}

void ALRSensorSite::OnRep_Done()
{
	LR::Tint(Mesh, bDone ? FLinearColor(0.1f, 0.8f, 0.2f) : FLinearColor(0.15f, 0.4f, 1.f), 3.f);
}

void ALRSensorSite::OnInteract(ALRCharacter* By)
{
	if (bDone)
	{
		return;
	}
	bDone = true;
	OnRep_Done();
	if (ALRGameMode* GM = GetGM(this))
	{
		GM->OnSensorPlaced(this);
	}
}

bool ALRSensorSite::GetMarker(FString& OutLabel, FLinearColor& OutColor) const
{
	if (bDone)
	{
		return false;
	}
	OutLabel = TEXT("Site du capteur");
	OutColor = FLinearColor(0.3f, 0.6f, 1.f);
	return true;
}

// ---------------------------------------------------------------- Destructibles

ALRDestructible::ALRDestructible()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetupAttachment(Root);
	Mesh->SetStaticMesh(LR::Cube());
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetCanEverAffectNavigation(false);
}

void ALRDestructible::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALRDestructible, Health);
}

void ALRDestructible::ApplyHit(float Damage, ALRCharacter* From)
{
	if (!HasAuthority() || bBroken)
	{
		return;
	}
	Health -= Damage;
	if (Health <= 0.f)
	{
		bBroken = true;
		Broken(From);
	}
}

ALRNest::ALRNest()
{
	Mesh->SetStaticMesh(LR::Sphere());
	Mesh->SetRelativeScale3D(FVector(2.4f, 2.4f, 1.6f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	Health = 260.f;
}

void ALRNest::BeginPlay()
{
	Super::BeginPlay();
	LR::Tint(Mesh, FLinearColor(0.35f, 0.05f, 0.4f));
}

void ALRNest::Broken(ALRCharacter* From)
{
	if (ALRGameMode* GM = GetGM(this))
	{
		GM->OnNestDestroyed(this, From);
	}
	Destroy();
}

ALRBioWall::ALRBioWall()
{
	Mesh->SetRelativeScale3D(FVector(3.95f, 3.95f, 3.2f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Health = 380.f;
}

void ALRBioWall::BeginPlay()
{
	Super::BeginPlay();
	LR::Tint(Mesh, FLinearColor(0.4f, 0.06f, 0.1f));
}

void ALRBioWall::Broken(ALRCharacter* From)
{
	if (ALRGameMode* GM = GetGM(this))
	{
		GM->OnWallDestroyed(this);
	}
	Destroy();
}
