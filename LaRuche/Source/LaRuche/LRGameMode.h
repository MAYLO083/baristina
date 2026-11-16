#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"
#include "LRTypes.h"
#include "LRGameMode.generated.h"

class ALRGameState;
class ALRCharacter;
class ALREnemy;
class ALRNest;
class ALRBioWall;
class ALRContainer;
class ALRSensorSite;

struct FLRLure
{
	FVector Loc = FVector::ZeroVector;
	float End = 0.f;
};

/** Regles du jeu (serveur uniquement) : deroulement du raid, Eveil, apparitions, missions. */
UCLASS()
class ALRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALRGameMode();
	virtual void Tick(float DeltaSeconds) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	void StartRaid();

	void Say(const FString& Msg);
	void AddEveil(float Amount);
	/** Bruit entendu par les creatures dans le rayon, et qui fait monter l'Eveil. */
	void EmitNoise(const FVector& Loc, float Radius, float EveilAmount);
	void AddLure(const FVector& Loc);
	bool AnyoneStanding(const ALRCharacter* Except) const;
	ALRNest* NearestNest(const FVector& Loc) const;

	ALREnemy* SpawnEnemy(ELREnemy Type, const FVector& Loc);
	void SpawnPickup(FName Id, int32 Count, const FVector& Loc, bool bScatter = true);
	void SpawnLoot(const FVector& Loc, int32 Kind);
	void SpawnBeacon(ALRCharacter* Dead, const FVector& Loc);

	void OnEnemyKilled(ALREnemy* Enemy, ALRCharacter* Killer);
	void OnNestDestroyed(ALRNest* Nest, ALRCharacter* By);
	void OnWallDestroyed(ALRBioWall* Wall);
	void OnItemPicked(FName Id, int32 Count, ALRCharacter* By);
	void OnSensorPlaced(ALRSensorSite* Site);
	void OnCrateOpened(ALRContainer* Crate);

private:
	ALRGameState* LRGS() const;
	void PopulateRaid();
	void InsertPlayers();
	void EndRaid();
	void CleanupRaid();
	void Director();
	void OnTierReached(int32 NewTier);
	void SpawnBioWalls(int32 Count);
	void MissionProgress(int32 Amount);
	void UpdateSpectators();
	bool PickOpenCell(FVector& OutLoc, float MinDistFromPlayers, float MinDistFromInsertion, bool bReserve);
	ELREnemy RollEnemyType(int32 Tier) const;
	bool FarFromPlayers(const FVector& Loc, float MinDist) const;
	int32 NumRaiders() const;

	/** Cree un acteur et laisse le regler avant qu'il n'entre en jeu. */
	template <typename T>
	T* SpawnInit(const FVector& Loc, TFunctionRef<void(T*)> Init)
	{
		const FTransform Transform(FRotator::ZeroRotator, Loc);
		T* Actor = GetWorld()->SpawnActorDeferred<T>(T::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Actor)
		{
			Init(Actor);
			Actor->FinishSpawning(Transform);
		}
		return Actor;
	}

	TArray<FLRLure> Lures;
	TSet<FIntPoint> UsedCells;
	FVector InsertionLoc = FVector::ZeroVector;
	int32 SpawnCounter = 0;
	int32 RaidCounter = 0;
	bool bStarting = false;
	bool bEnding = false;
	float LastNoise = 0.f;
	float SecondTimer = 0.f;
	float DirectorTimer = 0.f;
	float NextEvent = 0.f;
	FTimerHandle InsertTimer;
	FTimerHandle EndTimer;
};
