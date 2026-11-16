#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LRTypes.h"
#include "LRPlayerController.generated.h"

class ULRSaveGame;
class ALRHUD;

/** Lit le clavier et la souris, et fait le lien entre la sauvegarde locale et le serveur. */
UCLASS()
class ALRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALRPlayerController();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Dernier equipement envoye par ce joueur (cote serveur). */
	UPROPERTY()
	FLRLoadout ServerLoadout;

	UFUNCTION(Server, Reliable)
	void ServerSubmitLoadout(const FLRLoadout& Loadout);

	UFUNCTION(Server, Reliable)
	void ServerStartRaid();

	UFUNCTION(Server, Reliable)
	void ServerSetConfig(uint8 Mission, int32 StartTier);

	UFUNCTION(Client, Reliable)
	void ClientRaidResult(const FLRLoadout& Loadout, const TArray<FLRStack>& Rewards, bool bSurvived, const FString& Summary);

	UFUNCTION(Client, Reliable)
	void ClientNotify(const FString& Msg);

	ULRSaveGame* GetSave() const;
	/** Enregistre la sauvegarde et envoie l'equipement au serveur. */
	void Commit();
	bool IsHost() const;
	bool GetMouse(FVector2D& Out) const;

private:
	void HandleTyping(ALRHUD* HUD);

	bool bCursorMode = false;
	bool bPushed = false;
};
