#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LRTypes.h"
#include "LREnemy.generated.h"

class UStaticMeshComponent;
class ALRCharacter;
class ALRNest;

struct FLREnemyStats
{
	float Health;
	float Speed;
	float Damage;
	float Range;
	float Cooldown;
	float Sight;
	float Radius;
	float HalfHeight;
	FLinearColor Color;
};

/** Creature de la Ruche. Une seule classe, le comportement depend du type. */
UCLASS()
class ALREnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ALREnemy();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static const FLREnemyStats& StatsFor(ELREnemy InType);
	static FString TypeName(ELREnemy InType);

	UPROPERTY(ReplicatedUsing = OnRep_Type)
	ELREnemy Type = ELREnemy::Larve;

	UPROPERTY(Replicated)
	float Health = 30.f;

	UPROPERTY(Replicated)
	float MaxHealth = 30.f;

	UFUNCTION()
	void OnRep_Type();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Extra;

	/** Applique des degats. Renvoie true si la creature meurt. */
	bool ApplyHit(float Damage, const FVector& ShotDir, ALRCharacter* From, bool bAcid);
	void HearNoise(const FVector& Loc);
	float HearingMult() const;

	bool bDead = false;
	/** Multiplicateur de vie et de degats selon le palier d'Eveil a l'apparition. */
	float Power = 1.f;
	/** Hote qui porte la boite noire (contrat). */
	bool bBlackBox = false;
	/** Objet vole par une Glaneuse. */
	FLRStack Carried;

private:
	void ApplyType();
	void Think();
	ALRCharacter* FindTarget() const;
	bool HasLineOfSight(const AActor* Other) const;
	void GoTo(const FVector& Dest, bool bRun);
	void StopMoving();
	void FaceTo(const FVector& Loc);
	void Wander();
	void Die(ALRCharacter* Killer);

	TArray<FVector> Path;
	int32 PathIdx = 0;
	FVector PathGoal = FVector::ZeroVector;
	FVector LastKnown = FVector::ZeroVector;
	FVector LastPos = FVector::ZeroVector;
	bool bInvestigating = false;
	bool bFleeing = false;
	float ThinkTimer = 0.f;
	float NextRepath = 0.f;
	float NextAttack = 0.f;
	float NextWander = 0.f;
	float NextSpecial = 0.f;
	float SeenTime = 0.f;
	float StuckTime = 0.f;
	TWeakObjectPtr<ALRNest> FleeNest;
};
