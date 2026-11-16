#pragma once

#include "CoreMinimal.h"
#include "LRTypes.generated.h"

/** Phase de la partie : dans la planque ou en raid. */
UENUM()
enum class ELRPhase : uint8
{
	Planque,
	Raid
};

/** Etat d'un joueur pendant un raid. */
UENUM()
enum class ELRLife : uint8
{
	Vivant,
	ATerre,
	Mort,
	Extrait
};

UENUM()
enum class ELREnemy : uint8
{
	Larve,
	Pisteur,
	Sentinelle,
	Carapace,
	Glaneuse,
	Hote,
	Cocon,
	Matriarche
};

UENUM()
enum class ELRMission : uint8
{
	Prelevement,
	Purge,
	BoiteNoire,
	Balise
};

UENUM()
enum class ELRExtract : uint8
{
	Egout,
	Sas,
	Helico
};

/** Une pile d'objets : identifiant + quantite. */
USTRUCT()
struct FLRStack
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	int32 Count = 0;

	FLRStack() {}
	FLRStack(FName InId, int32 InCount) : Id(InId), Count(InCount) {}
};

/** Tout ce qu'un joueur porte sur lui. */
USTRUCT()
struct FLRLoadout
{
	GENERATED_BODY()

	UPROPERTY()
	FName Weapon;

	/** Toujours 3 entrees : canon, chargeur, crosse. */
	UPROPERTY()
	TArray<FName> Mods;

	UPROPERTY()
	FName Armor;

	UPROPERTY()
	FName Bag;

	UPROPERTY()
	TArray<FLRStack> Items;

	/** Poche securisee : conservee meme en cas de mort. */
	UPROPERTY()
	TArray<FLRStack> Pocket;

	/** Balles dans le chargeur. */
	UPROPERTY()
	int32 Mag = 0;
};
