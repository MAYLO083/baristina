#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "LRTypes.h"
#include "LRGameInstance.generated.h"

/** Sauvegarde locale d'un joueur : son equipement, son coffre et ses plans. */
UCLASS()
class ULRSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FLRLoadout Loadout;

	UPROPERTY()
	TArray<FLRStack> Stash;

	UPROPERTY()
	TArray<FName> Plans;

	UPROPERTY()
	FString LastIP;

	UPROPERTY()
	float Sensitivity = 1.f;

	UPROPERTY()
	int32 Raids = 0;

	UPROPERTY()
	int32 Extractions = 0;
};

UCLASS()
class ULRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	void SaveNow();

	UPROPERTY()
	TObjectPtr<ULRSaveGame> Save;

	/** Vrai quand le joueur a choisi "Jouer seul" : le menu principal ne s'affiche plus. */
	bool bSolo = false;

	FString Slot;
};
