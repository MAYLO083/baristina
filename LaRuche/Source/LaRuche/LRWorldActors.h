#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LRTypes.h"
#include "LRWorldActors.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class ALRCharacter;

namespace LR
{
	UStaticMesh* Cube();
	UStaticMesh* Sphere();
	UStaticMesh* Cylinder();
	UStaticMesh* Cone();

	/**
	 * Materiau colorable du jeu. Dans l'editeur il est cree et enregistre automatiquement
	 * (Content/LaRuche/M_LRColor) s'il n'existe pas encore.
	 */
	UMaterialInterface* ColorMaterial();

	/** Colore un mesh. Glow > 0 le rend lumineux. */
	void Tint(UStaticMeshComponent* Comp, const FLinearColor& Color, float Glow = 0.f);

	/** Cree une piece visuelle sans collision (a appeler dans un constructeur). */
	UStaticMeshComponent* Part(AActor* Owner, FName Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale);

	/** Heure du serveur, identique sur toutes les machines. */
	float Now(const UObject* WorldContext);
}

/** Bloc de decor (batiment, mur, sol). Cree localement sur chaque machine a partir de la graine. */
UCLASS()
class ALRBlock : public AActor
{
	GENERATED_BODY()

public:
	ALRBlock();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;
};

/** Soleil, ciel et brouillard. Cree localement sur chaque machine. */
UCLASS()
class ALREnvironment : public AActor
{
	GENERATED_BODY()

public:
	ALREnvironment();
	virtual void BeginPlay() override;

	/** Change l'ambiance selon le palier d'Eveil (1 a 4). */
	void SetTier(int32 Tier);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY()
	TObjectPtr<USkyLightComponent> Sky;

	UPROPERTY()
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY()
	TObjectPtr<UExponentialHeightFogComponent> Fog;
};

/** Petit effet visuel local et ephemere (trait de tir, explosion). */
UCLASS()
class ALRFx : public AActor
{
	GENERATED_BODY()

public:
	ALRFx();
	virtual void Tick(float DeltaSeconds) override;

	static void Tracer(UWorld* World, const FVector& A, const FVector& B, const FLinearColor& Color);
	static void Burst(UWorld* World, const FVector& Loc, float Radius, const FLinearColor& Color);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	float Grow = 0.f;
};

/** Base de tout ce avec quoi un joueur peut interagir (touche E). */
UCLASS(Abstract)
class ALRInteractable : public AActor
{
	GENERATED_BODY()

public:
	ALRInteractable();

	virtual bool CanInteract(const ALRCharacter* By) const { return true; }
	virtual FString GetPrompt(const ALRCharacter* By) const { return TEXT("Utiliser"); }
	/** Duree pendant laquelle il faut rester sur place. 0 = immediat. */
	virtual float GetChannelTime(const ALRCharacter* By) const { return 0.f; }
	/** Appele sur le serveur quand l'interaction aboutit. */
	virtual void OnInteract(ALRCharacter* By) {}
	/** Marqueur affiche a l'ecran pour tous les joueurs. */
	virtual bool GetMarker(FString& OutLabel, FLinearColor& OutColor) const { return false; }

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	float Range = 300.f;
};

/** Objet au sol. */
UCLASS()
class ALRPickup : public ALRInteractable
{
	GENERATED_BODY()

public:
	ALRPickup();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FString GetPrompt(const ALRCharacter* By) const override;
	virtual void OnInteract(ALRCharacter* By) override;

	UFUNCTION()
	void OnRep_Stack();

	UPROPERTY(ReplicatedUsing = OnRep_Stack)
	FLRStack Stack;
};

/** Conteneur de butin. Peut etre un cocon piege. */
UCLASS()
class ALRContainer : public ALRInteractable
{
	GENERATED_BODY()

public:
	ALRContainer();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract(const ALRCharacter* By) const override { return !bOpened; }
	virtual FString GetPrompt(const ALRCharacter* By) const override;
	virtual float GetChannelTime(const ALRCharacter* By) const override { return Kind == 1 ? 4.f : 1.2f; }
	virtual void OnInteract(ALRCharacter* By) override;
	virtual bool GetMarker(FString& OutLabel, FLinearColor& OutColor) const override;

	UFUNCTION()
	void OnRep_Visual();

	void ApplyVisual();

	/** 0 conteneur normal, 1 largage de quarantaine. */
	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	int32 Kind = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	bool bOpened = false;

	/** Un cocon piege "respire" : c'est le seul indice. */
	UPROPERTY(Replicated)
	bool bBreathing = false;
};

/** Point d'extraction. */
UCLASS()
class ALRExtraction : public ALRInteractable
{
	GENERATED_BODY()

public:
	ALRExtraction();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract(const ALRCharacter* By) const override;
	virtual FString GetPrompt(const ALRCharacter* By) const override;
	virtual float GetChannelTime(const ALRCharacter* By) const override;
	virtual void OnInteract(ALRCharacter* By) override;
	virtual bool GetMarker(FString& OutLabel, FLinearColor& OutColor) const override;

	UFUNCTION()
	void OnRep_Visual();

	void ApplyVisual();
	/** Helico : 0 pas appele, 1 en route, 2 sur place. */
	int32 HeliState() const;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	ELRExtract Kind = ELRExtract::Egout;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	bool bOpen = true;

	/** Heure serveur d'arrivee et de depart de l'helico (0 = pas appele). */
	UPROPERTY(Replicated)
	float HeliArrive = 0.f;

	UPROPERTY(Replicated)
	float HeliLeave = 0.f;
};

/** Balise laissee par un joueur mort. */
UCLASS()
class ALRBeacon : public ALRInteractable
{
	GENERATED_BODY()

public:
	ALRBeacon();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FString GetPrompt(const ALRCharacter* By) const override;
	virtual float GetChannelTime(const ALRCharacter* By) const override { return 2.f; }
	virtual void OnInteract(ALRCharacter* By) override;
	virtual bool GetMarker(FString& OutLabel, FLinearColor& OutColor) const override;

	UPROPERTY(Replicated)
	TObjectPtr<ALRCharacter> Dead;
};

/** Station medicale : ramene les joueurs dont on porte la balise. */
UCLASS()
class ALRMedStation : public ALRInteractable
{
	GENERATED_BODY()

public:
	ALRMedStation();
	virtual void BeginPlay() override;

	virtual bool CanInteract(const ALRCharacter* By) const override;
	virtual FString GetPrompt(const ALRCharacter* By) const override { return TEXT("Réanimer les coéquipiers (balises)"); }
	virtual float GetChannelTime(const ALRCharacter* By) const override { return 4.f; }
	virtual void OnInteract(ALRCharacter* By) override;
	virtual bool GetMarker(FString& OutLabel, FLinearColor& OutColor) const override;
};

/** Site ou poser un capteur (contrat Balise). */
UCLASS()
class ALRSensorSite : public ALRInteractable
{
	GENERATED_BODY()

public:
	ALRSensorSite();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract(const ALRCharacter* By) const override { return !bDone; }
	virtual FString GetPrompt(const ALRCharacter* By) const override { return TEXT("Poser le capteur (bruyant)"); }
	virtual float GetChannelTime(const ALRCharacter* By) const override { return 5.f; }
	virtual void OnInteract(ALRCharacter* By) override;
	virtual bool GetMarker(FString& OutLabel, FLinearColor& OutColor) const override;

	UFUNCTION()
	void OnRep_Done();

	UPROPERTY(ReplicatedUsing = OnRep_Done)
	bool bDone = false;
};

/** Base de ce qui peut etre detruit par les tirs. */
UCLASS(Abstract)
class ALRDestructible : public AActor
{
	GENERATED_BODY()

public:
	ALRDestructible();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ApplyHit(float Damage, ALRCharacter* From);
	virtual void Broken(ALRCharacter* From) {}

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Replicated)
	float Health = 100.f;

	bool bBroken = false;
};

/** Nid de la Ruche : fait sortir des larves, stocke le butin vole. */
UCLASS()
class ALRNest : public ALRDestructible
{
	GENERATED_BODY()

public:
	ALRNest();
	virtual void BeginPlay() override;
	virtual void Broken(ALRCharacter* From) override;

	TArray<FLRStack> Stored;
};

/** Cloison organique qui bouche une rue. */
UCLASS()
class ALRBioWall : public ALRDestructible
{
	GENERATED_BODY()

public:
	ALRBioWall();
	virtual void BeginPlay() override;
	virtual void Broken(ALRCharacter* From) override;

	FIntPoint CellPos = FIntPoint(-1, -1);
};
