#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LRCity.h"
#include "LRTypes.h"
#include "LRGameState.generated.h"

class ALRBlock;
class ALREnvironment;

struct FLRMessage
{
	FString Text;
	float Time = 0.f;
};

/** Etat de la partie visible par tous les joueurs. Construit aussi le decor sur chaque machine. */
UCLASS()
class ALRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALRGameState();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	ELRPhase Phase = ELRPhase::Planque;

	/** Jauge d'Eveil de la Ruche, de 0 a 100. */
	UPROPERTY(Replicated)
	float Eveil = 0.f;

	/** Palier d'Eveil, de 1 (Dormante) a 4 (Frenesie). */
	UPROPERTY(ReplicatedUsing = OnRep_Tier)
	int32 Tier = 1;

	/** Graine de la ville du raid en cours. 0 = pas de ville. */
	UPROPERTY(ReplicatedUsing = OnRep_Seed)
	int32 RaidSeed = 0;

	UPROPERTY(Replicated)
	float RaidStart = 0.f;

	UPROPERTY(Replicated)
	ELRMission Mission = ELRMission::Prelevement;

	/** Palier de depart choisi par l'hote (1 a 3). */
	UPROPERTY(Replicated)
	int32 StartTier = 1;

	UPROPERTY(Replicated)
	int32 MissionProgress = 0;

	UPROPERTY(Replicated)
	int32 MissionGoal = 0;

	UPROPERTY(Replicated)
	bool bMissionDone = false;

	UFUNCTION()
	void OnRep_Tier();

	UFUNCTION()
	void OnRep_Seed();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMessage(const FString& Msg);

	/** Kind : 0 tir joueur, 1 tir ennemi, 2 explosion acide (rayon B.X), 3 mort, 4 hurlement, 5 leurre. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFx(uint8 Kind, FVector_NetQuantize A, FVector_NetQuantize B);

	void RebuildCity();
	void ApplyTier();

	static FString TierName(int32 InTier);
	static FString MissionName(ELRMission InMission);
	static FString MissionText(ELRMission InMission);

	FLRCityLayout Layout;
	TArray<FLRMessage> Messages;

private:
	void BuildStatic();
	ALRBlock* SpawnBlock(const FString& Name, const FVector& Loc, const FVector& Scale, const FLinearColor& Color);

	UPROPERTY()
	TArray<TObjectPtr<ALRBlock>> CityBlocks;

	UPROPERTY()
	TObjectPtr<ALREnvironment> Env;

	int32 BuiltSeed = 0;
	bool bStaticBuilt = false;
};
