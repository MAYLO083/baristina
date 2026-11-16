#pragma once

#include "CoreMinimal.h"
#include "LRTypes.h"

enum class ELRItem : uint8
{
	Arme,
	Mod,
	Munitions,
	Soin,
	Serum,
	Armure,
	Sac,
	Gadget,
	Ressource,
	Biomasse,
	Plan,
	Mission
};

namespace LRId
{
	static const FName Munitions(TEXT("munitions"));
	static const FName Ferraille(TEXT("ferraille"));
	static const FName Electronique(TEXT("electronique"));
	static const FName Chimie(TEXT("chimie"));
	static const FName Chitine(TEXT("chitine"));
	static const FName Glande(TEXT("glande"));
	static const FName Seve(TEXT("seve"));
	static const FName Nodule(TEXT("nodule"));
	static const FName Coeur(TEXT("coeur_matriarche"));
	static const FName Pistolet(TEXT("pistolet"));
	static const FName Bandage(TEXT("bandage"));
	static const FName Leurre(TEXT("leurre"));
	static const FName Pheromones(TEXT("pheromones"));
	static const FName Echantillon(TEXT("echantillon"));
	static const FName BoiteNoire(TEXT("boite_noire"));
	static const FName CanonLong(TEXT("canon_long"));
	static const FName Silencieux(TEXT("silencieux"));
	static const FName GreffeAcide(TEXT("greffe_acide"));
	static const FName ChargeurEtendu(TEXT("chargeur_etendu"));
	static const FName GreffeVivante(TEXT("greffe_vivante"));
	static const FName CrosseStable(TEXT("crosse_stable"));
}

/**
 * Definition d'un objet.
 * Arme   : A degats, B intervalle de tir, C dispersion (degres), D bruit, E rechargement, I1 chargeur, I2 projectiles
 * Mod    : I1 emplacement (0 canon, 1 chargeur, 2 crosse)
 * Soin   : A points de vie rendus
 * Armure : A reduction des degats (0..1), B multiplicateur de vitesse
 * Sac    : I1 nombre d'emplacements
 */
struct FLRItemDef
{
	FName Id;
	FString Name;
	ELRItem Type = ELRItem::Ressource;
	int32 Rarity = 0;
	int32 Value = 0;
	int32 MaxStack = 1;
	float A = 0.f;
	float B = 0.f;
	float C = 0.f;
	float D = 0.f;
	float E = 0.f;
	int32 I1 = 0;
	int32 I2 = 0;
};

struct FLRRecipe
{
	FName Result;
	int32 Count = 1;
	TArray<FLRStack> Cost;
	/** Plan necessaire (aucun si vide). */
	FName Plan;
};

struct FLRWeaponStats
{
	bool bValid = false;
	float Damage = 0.f;
	float Interval = 0.3f;
	float Spread = 1.f;
	float Noise = 5.f;
	float Reload = 1.5f;
	int32 MagSize = 0;
	int32 Pellets = 1;
	bool bAuto = false;
	bool bAcid = false;
	bool bRegen = false;
};

class FLRItems
{
public:
	static constexpr int32 BaseSlots = 8;
	static constexpr int32 PocketSlots = 2;

	static const TArray<FLRItemDef>& All();
	static const FLRItemDef* Find(FName Id);
	static FString Name(FName Id);
	static FLinearColor RarityColor(int32 Rarity);
	static const TArray<FLRRecipe>& Recipes();
	static FName RandomPlan();
	static FName PlanFor(FName Result);

	static void EnsureMods(FLRLoadout& L);
	static int32 Capacity(const FLRLoadout& L);
	static int32 CountOf(const FLRLoadout& L, FName Id);
	/** Ajoute au sac. Renvoie la quantite qui n'a pas pu entrer. */
	static int32 Add(FLRLoadout& L, FName Id, int32 Count);
	static bool Remove(FLRLoadout& L, FName Id, int32 Count);
	static bool Equip(FLRLoadout& L, int32 Index);
	/** Slot : 0 arme, 1..3 mods, 4 armure, 5 sac. */
	static bool Unequip(FLRLoadout& L, int32 Slot);
	static bool ToPocket(FLRLoadout& L, int32 Index);
	static bool FromPocket(FLRLoadout& L, int32 Index);
	static FLRWeaponStats WeaponStats(const FLRLoadout& L);

	static void AddToList(TArray<FLRStack>& List, FName Id, int32 Count);
	static bool RemoveFromList(TArray<FLRStack>& List, FName Id, int32 Count);
	static int32 CountIn(const TArray<FLRStack>& List, FName Id);

	/** Kind : 0 conteneur normal, 1 largage de quarantaine. */
	static void RollLoot(int32 Kind, TArray<FLRStack>& Out);
};
