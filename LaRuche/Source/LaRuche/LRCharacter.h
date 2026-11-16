#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LRTypes.h"
#include "LRCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

/** Le glaneur : personnage joueur en vue a la troisieme personne. */
UCLASS()
class ALRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALRCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Composants
	UPROPERTY()
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Cam;

	UPROPERTY()
	TObjectPtr<USceneComponent> Visual;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pack;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Gun;

	// ---- Etat replique
	UPROPERTY(Replicated)
	float Health = 100.f;

	UPROPERTY(Replicated)
	float MaxHealth = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Life)
	ELRLife Life = ELRLife::Vivant;

	/** Equipement et sac pendant le raid (replique au proprietaire seulement). */
	UPROPERTY(Replicated)
	FLRLoadout Inv;

	UPROPERTY(Replicated)
	bool bInRaid = false;

	UPROPERTY(Replicated)
	bool bSprinting = false;

	UPROPERTY(Replicated)
	float BleedEnd = 0.f;

	UPROPERTY(Replicated)
	float ChannelStart = 0.f;

	UPROPERTY(Replicated)
	float ChannelEnd = 0.f;

	UPROPERTY(Replicated)
	float ReloadEnd = 0.f;

	UPROPERTY(Replicated)
	float PheroEnd = 0.f;

	UPROPERTY(Replicated)
	int32 BeaconCount = 0;

	UPROPERTY(Replicated)
	int32 Infection = 0;

	UFUNCTION()
	void OnRep_Life();

	// ---- Appels client -> serveur
	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Start, FVector_NetQuantizeNormal Dir);

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bNewSprint);

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerUseItem(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerDropItem(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerEquip(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerUnequip(int32 Slot);

	UFUNCTION(Server, Reliable)
	void ServerPocket(int32 Index, bool bToPocket);

	UFUNCTION(Client, Unreliable)
	void ClientHitMarker(bool bKill);

	// ---- Serveur
	void Hurt(float Damage);
	void GoDown();
	void Die();
	void ReviveFromDown();
	void ReviveAt(const FVector& Loc, float NewHealth);
	void Extract();
	void EnterRaid(const FLRLoadout& Loadout, const FVector& Loc);
	void ReturnToPlanque(const FVector& Loc);
	void Notify(const FString& Msg);
	void CancelChannel();

	UPROPERTY()
	TArray<TObjectPtr<ALRCharacter>> CarriedDead;

	// ---- Local
	void LocalFire(bool bJustPressed);
	void TryInteract();
	/** Utilise le premier objet correspondant. Id vide = n'importe quel objet de soin. */
	void QuickUse(FName Id);
	void SetSprint(bool bNewSprint);
	FString GetDisplayName() const;
	void ApplyLife();

	TWeakObjectPtr<AActor> Focus;
	FString FocusPrompt;
	float HitMarkerTime = -10.f;
	bool bHitMarkerKill = false;

private:
	void ServerTick(float DeltaSeconds);
	void UpdateFocus();
	void FinishInteract(AActor* Target);

	TWeakObjectPtr<AActor> ChannelTarget;
	FVector ChannelOrigin = FVector::ZeroVector;
	float NextServerShot = 0.f;
	float NextLocalShot = 0.f;
	float NextStep = 0.f;
	float NextRegen = 0.f;
	float NextFocus = 0.f;
};
