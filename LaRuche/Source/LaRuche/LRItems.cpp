#include "LRItems.h"

namespace
{
	FLRItemDef& AddDef(TArray<FLRItemDef>& T, const TCHAR* Id, const TCHAR* Name, ELRItem Type, int32 Rarity, int32 Value, int32 MaxStack)
	{
		FLRItemDef D;
		D.Id = FName(Id);
		D.Name = Name;
		D.Type = Type;
		D.Rarity = Rarity;
		D.Value = Value;
		D.MaxStack = MaxStack;
		T.Add(D);
		return T.Last();
	}

	void AddWeapon(TArray<FLRItemDef>& T, const TCHAR* Id, const TCHAR* Name, int32 Rarity, int32 Value,
		float Damage, float Interval, float Spread, float Noise, float Reload, int32 Mag, int32 Pellets)
	{
		FLRItemDef& D = AddDef(T, Id, Name, ELRItem::Arme, Rarity, Value, 1);
		D.A = Damage;
		D.B = Interval;
		D.C = Spread;
		D.D = Noise;
		D.E = Reload;
		D.I1 = Mag;
		D.I2 = Pellets;
	}

	void AddMod(TArray<FLRItemDef>& T, const TCHAR* Id, const TCHAR* Name, int32 Rarity, int32 Value, int32 Slot)
	{
		FLRItemDef& D = AddDef(T, Id, Name, ELRItem::Mod, Rarity, Value, 1);
		D.I1 = Slot;
	}

	const TCHAR* PlanTargets[] = {
		TEXT("medkit"), TEXT("serum"), TEXT("silencieux"), TEXT("carabine"), TEXT("pm"),
		TEXT("pompe"), TEXT("gilet_lourd"), TEXT("sac_grand"), TEXT("greffe_acide"), TEXT("greffe_vivante")
	};

	struct FLootWeight
	{
		const TCHAR* Id;
		int32 Min;
		int32 Max;
		int32 Weight;
	};

	const FLootWeight LootTable[] = {
		{ TEXT("munitions"), 12, 30, 30 },
		{ TEXT("ferraille"), 1, 3, 26 },
		{ TEXT("electronique"), 1, 2, 12 },
		{ TEXT("chimie"), 1, 2, 12 },
		{ TEXT("bandage"), 1, 2, 12 },
		{ TEXT("medkit"), 1, 1, 5 },
		{ TEXT("serum"), 1, 1, 3 },
		{ TEXT("leurre"), 1, 2, 5 },
		{ TEXT("pheromones"), 1, 1, 3 },
		{ TEXT("canon_long"), 1, 1, 3 },
		{ TEXT("chargeur_etendu"), 1, 1, 3 },
		{ TEXT("crosse_stable"), 1, 1, 3 },
		{ TEXT("silencieux"), 1, 1, 2 },
		{ TEXT("gilet_leger"), 1, 1, 3 },
		{ TEXT("sac_moyen"), 1, 1, 3 },
		{ TEXT("pm"), 1, 1, 2 },
		{ TEXT("pompe"), 1, 1, 2 },
		{ TEXT("carabine"), 1, 1, 1 },
		{ TEXT("gilet_lourd"), 1, 1, 1 },
		{ TEXT("sac_grand"), 1, 1, 1 },
		{ TEXT("PLAN"), 1, 1, 2 }
	};

	FLRStack RollOne()
	{
		int32 Total = 0;
		for (const FLootWeight& W : LootTable)
		{
			Total += W.Weight;
		}
		int32 Pick = FMath::RandRange(0, Total - 1);
		for (const FLootWeight& W : LootTable)
		{
			if (Pick < W.Weight)
			{
				if (FCString::Strcmp(W.Id, TEXT("PLAN")) == 0)
				{
					return FLRStack(FLRItems::RandomPlan(), 1);
				}
				return FLRStack(FName(W.Id), FMath::RandRange(W.Min, W.Max));
			}
			Pick -= W.Weight;
		}
		return FLRStack(LRId::Ferraille, 1);
	}
}

const TArray<FLRItemDef>& FLRItems::All()
{
	static TArray<FLRItemDef> T;
	if (T.Num() > 0)
	{
		return T;
	}

	// Armes : degats, intervalle, dispersion, bruit, rechargement, chargeur, projectiles
	AddWeapon(T, TEXT("pistolet"), TEXT("Pistolet"), 0, 3, 22.f, 0.28f, 1.5f, 6.f, 1.4f, 12, 1);
	AddWeapon(T, TEXT("pm"), TEXT("Pistolet-mitrailleur"), 1, 8, 13.f, 0.09f, 3.2f, 5.f, 2.0f, 30, 1);
	AddWeapon(T, TEXT("pompe"), TEXT("Fusil à pompe"), 1, 8, 10.f, 0.85f, 6.5f, 12.f, 2.6f, 6, 8);
	AddWeapon(T, TEXT("carabine"), TEXT("Carabine"), 2, 12, 58.f, 0.65f, 0.4f, 10.f, 2.2f, 8, 1);

	// Mods : emplacement 0 canon, 1 chargeur, 2 crosse
	AddMod(T, TEXT("canon_long"), TEXT("Canon long (+20% dégâts)"), 1, 4, 0);
	AddMod(T, TEXT("silencieux"), TEXT("Silencieux (-70% bruit)"), 2, 6, 0);
	AddMod(T, TEXT("greffe_acide"), TEXT("Greffe acide (perce les carapaces)"), 3, 15, 0);
	AddMod(T, TEXT("chargeur_etendu"), TEXT("Chargeur étendu (+50%)"), 1, 4, 1);
	AddMod(T, TEXT("greffe_vivante"), TEXT("Chargeur vivant (régénère, bruyant)"), 3, 15, 1);
	AddMod(T, TEXT("crosse_stable"), TEXT("Crosse stable (-40% dispersion)"), 1, 4, 2);

	AddDef(T, TEXT("munitions"), TEXT("Munitions"), ELRItem::Munitions, 0, 0, 120);

	AddDef(T, TEXT("bandage"), TEXT("Bandage (+30 PV)"), ELRItem::Soin, 0, 1, 5).A = 30.f;
	AddDef(T, TEXT("medkit"), TEXT("Trousse de soin (+75 PV)"), ELRItem::Soin, 1, 3, 3).A = 75.f;
	AddDef(T, TEXT("serum"), TEXT("Sérum (soigne l'infection)"), ELRItem::Serum, 1, 3, 3);

	{
		FLRItemDef& D = AddDef(T, TEXT("gilet_leger"), TEXT("Gilet léger (-25% dégâts)"), ELRItem::Armure, 0, 4, 1);
		D.A = 0.25f;
		D.B = 1.f;
	}
	{
		FLRItemDef& D = AddDef(T, TEXT("gilet_lourd"), TEXT("Gilet lourd (-45% dégâts, plus lent)"), ELRItem::Armure, 2, 9, 1);
		D.A = 0.45f;
		D.B = 0.9f;
	}

	AddDef(T, TEXT("sac_moyen"), TEXT("Sac moyen (12 places)"), ELRItem::Sac, 1, 4, 1).I1 = 12;
	AddDef(T, TEXT("sac_grand"), TEXT("Grand sac (16 places)"), ELRItem::Sac, 2, 9, 1).I1 = 16;

	AddDef(T, TEXT("leurre"), TEXT("Leurre sonore"), ELRItem::Gadget, 0, 2, 4);
	AddDef(T, TEXT("pheromones"), TEXT("Phéromones (invisible 10 s)"), ELRItem::Gadget, 1, 3, 3);

	AddDef(T, TEXT("ferraille"), TEXT("Ferraille"), ELRItem::Ressource, 0, 1, 50);
	AddDef(T, TEXT("electronique"), TEXT("Électronique"), ELRItem::Ressource, 0, 2, 20);
	AddDef(T, TEXT("chimie"), TEXT("Produits chimiques"), ELRItem::Ressource, 0, 2, 20);

	AddDef(T, TEXT("chitine"), TEXT("Chitine"), ELRItem::Biomasse, 0, 2, 20);
	AddDef(T, TEXT("glande"), TEXT("Glande"), ELRItem::Biomasse, 0, 3, 20);
	AddDef(T, TEXT("seve"), TEXT("Sève"), ELRItem::Biomasse, 1, 4, 20);
	AddDef(T, TEXT("nodule"), TEXT("Nodule"), ELRItem::Biomasse, 2, 10, 5);
	AddDef(T, TEXT("coeur_matriarche"), TEXT("Cœur de Matriarche"), ELRItem::Biomasse, 3, 60, 1);

	AddDef(T, TEXT("echantillon"), TEXT("Échantillon de tissu"), ELRItem::Mission, 1, 1, 20);
	AddDef(T, TEXT("boite_noire"), TEXT("Boîte noire"), ELRItem::Mission, 2, 0, 1);

	// Plans : un par objet a debloquer
	for (const TCHAR* Target : PlanTargets)
	{
		FString TargetName = Target;
		for (const FLRItemDef& D : T)
		{
			if (D.Id == FName(Target))
			{
				TargetName = D.Name;
				break;
			}
		}
		const FString PlanId = FString(TEXT("plan_")) + Target;
		const FString PlanName = FString(TEXT("Plan : ")) + TargetName;
		AddDef(T, *PlanId, *PlanName, ELRItem::Plan, 2, 5, 1);
	}

	return T;
}

const FLRItemDef* FLRItems::Find(FName Id)
{
	if (Id.IsNone())
	{
		return nullptr;
	}
	static TMap<FName, int32> Index;
	const TArray<FLRItemDef>& T = All();
	if (Index.Num() == 0)
	{
		for (int32 i = 0; i < T.Num(); ++i)
		{
			Index.Add(T[i].Id, i);
		}
	}
	const int32* Found = Index.Find(Id);
	return Found ? &T[*Found] : nullptr;
}

FString FLRItems::Name(FName Id)
{
	const FLRItemDef* D = Find(Id);
	return D ? D->Name : FString(TEXT("-"));
}

FLinearColor FLRItems::RarityColor(int32 Rarity)
{
	switch (Rarity)
	{
	case 1: return FLinearColor(0.35f, 0.9f, 0.4f);
	case 2: return FLinearColor(0.35f, 0.6f, 1.f);
	case 3: return FLinearColor(0.85f, 0.4f, 1.f);
	default: return FLinearColor(0.85f, 0.85f, 0.85f);
	}
}

const TArray<FLRRecipe>& FLRItems::Recipes()
{
	static TArray<FLRRecipe> R;
	if (R.Num() > 0)
	{
		return R;
	}

	auto Make = [](const TCHAR* Result, int32 Count, bool bPlan, std::initializer_list<FLRStack> Cost)
	{
		FLRRecipe Rec;
		Rec.Result = FName(Result);
		Rec.Count = Count;
		Rec.Cost = Cost;
		if (bPlan)
		{
			Rec.Plan = FName(*(FString(TEXT("plan_")) + Result));
		}
		R.Add(Rec);
	};

	Make(TEXT("munitions"), 30, false, { FLRStack(LRId::Ferraille, 2) });
	Make(TEXT("bandage"), 1, false, { FLRStack(LRId::Chimie, 1) });
	Make(TEXT("pistolet"), 1, false, { FLRStack(LRId::Ferraille, 3) });
	Make(TEXT("leurre"), 1, false, { FLRStack(LRId::Electronique, 1), FLRStack(LRId::Ferraille, 1) });
	Make(TEXT("pheromones"), 1, false, { FLRStack(LRId::Glande, 2) });
	Make(TEXT("gilet_leger"), 1, false, { FLRStack(LRId::Ferraille, 4) });
	Make(TEXT("sac_moyen"), 1, false, { FLRStack(LRId::Ferraille, 4) });
	Make(TEXT("canon_long"), 1, false, { FLRStack(LRId::Ferraille, 3), FLRStack(LRId::Electronique, 1) });
	Make(TEXT("chargeur_etendu"), 1, false, { FLRStack(LRId::Ferraille, 3), FLRStack(LRId::Electronique, 1) });
	Make(TEXT("crosse_stable"), 1, false, { FLRStack(LRId::Ferraille, 3) });
	Make(TEXT("medkit"), 1, true, { FLRStack(LRId::Chimie, 2), FLRStack(LRId::Seve, 1) });
	Make(TEXT("serum"), 1, true, { FLRStack(LRId::Seve, 1), FLRStack(LRId::Chimie, 1) });
	Make(TEXT("silencieux"), 1, true, { FLRStack(LRId::Ferraille, 4), FLRStack(LRId::Electronique, 2) });
	Make(TEXT("pm"), 1, true, { FLRStack(LRId::Ferraille, 6), FLRStack(LRId::Electronique, 1) });
	Make(TEXT("pompe"), 1, true, { FLRStack(LRId::Ferraille, 6), FLRStack(LRId::Electronique, 1) });
	Make(TEXT("carabine"), 1, true, { FLRStack(LRId::Ferraille, 8), FLRStack(LRId::Electronique, 2) });
	Make(TEXT("gilet_lourd"), 1, true, { FLRStack(LRId::Chitine, 4), FLRStack(LRId::Ferraille, 3) });
	Make(TEXT("sac_grand"), 1, true, { FLRStack(LRId::Ferraille, 6), FLRStack(LRId::Chitine, 2) });
	Make(TEXT("greffe_acide"), 1, true, { FLRStack(LRId::Glande, 3), FLRStack(LRId::Chitine, 2) });
	Make(TEXT("greffe_vivante"), 1, true, { FLRStack(LRId::Seve, 3), FLRStack(LRId::Nodule, 1) });

	return R;
}

FName FLRItems::RandomPlan()
{
	const int32 Num = UE_ARRAY_COUNT(PlanTargets);
	return PlanFor(FName(PlanTargets[FMath::RandRange(0, Num - 1)]));
}

FName FLRItems::PlanFor(FName Result)
{
	return FName(*(FString(TEXT("plan_")) + Result.ToString()));
}

void FLRItems::EnsureMods(FLRLoadout& L)
{
	if (L.Mods.Num() != 3)
	{
		L.Mods.SetNum(3);
	}
}

int32 FLRItems::Capacity(const FLRLoadout& L)
{
	const FLRItemDef* Bag = Find(L.Bag);
	return (Bag && Bag->Type == ELRItem::Sac) ? Bag->I1 : BaseSlots;
}

int32 FLRItems::CountOf(const FLRLoadout& L, FName Id)
{
	return CountIn(L.Items, Id);
}

int32 FLRItems::Add(FLRLoadout& L, FName Id, int32 Count)
{
	const FLRItemDef* D = Find(Id);
	if (!D || Count <= 0)
	{
		return 0;
	}
	for (FLRStack& S : L.Items)
	{
		if (S.Id == Id && S.Count < D->MaxStack)
		{
			const int32 N = FMath::Min(Count, D->MaxStack - S.Count);
			S.Count += N;
			Count -= N;
			if (Count == 0)
			{
				return 0;
			}
		}
	}
	const int32 Cap = Capacity(L);
	while (Count > 0 && L.Items.Num() < Cap)
	{
		const int32 N = FMath::Min(Count, D->MaxStack);
		L.Items.Add(FLRStack(Id, N));
		Count -= N;
	}
	return Count;
}

bool FLRItems::Remove(FLRLoadout& L, FName Id, int32 Count)
{
	return RemoveFromList(L.Items, Id, Count);
}

bool FLRItems::Equip(FLRLoadout& L, int32 Index)
{
	if (!L.Items.IsValidIndex(Index))
	{
		return false;
	}
	EnsureMods(L);
	const FName Id = L.Items[Index].Id;
	const FLRItemDef* D = Find(Id);
	if (!D)
	{
		return false;
	}

	FName* SlotPtr = nullptr;
	switch (D->Type)
	{
	case ELRItem::Arme: SlotPtr = &L.Weapon; break;
	case ELRItem::Mod: SlotPtr = &L.Mods[FMath::Clamp(D->I1, 0, 2)]; break;
	case ELRItem::Armure: SlotPtr = &L.Armor; break;
	case ELRItem::Sac: SlotPtr = &L.Bag; break;
	default: return false;
	}

	const FName Old = *SlotPtr;
	if (D->Type == ELRItem::Sac && D->I1 < L.Items.Num())
	{
		return false;
	}

	const int32 OldMag = L.Mag;
	*SlotPtr = Id;
	if (Old.IsNone())
	{
		L.Items.RemoveAt(Index);
	}
	else
	{
		L.Items[Index] = FLRStack(Old, 1);
	}
	if (D->Type == ELRItem::Arme)
	{
		// Les balles de l'ancienne arme retournent dans le sac si elles rentrent.
		L.Mag = 0;
		Add(L, LRId::Munitions, OldMag);
	}
	return true;
}

bool FLRItems::Unequip(FLRLoadout& L, int32 Slot)
{
	EnsureMods(L);
	FName* SlotPtr = nullptr;
	if (Slot == 0) SlotPtr = &L.Weapon;
	else if (Slot >= 1 && Slot <= 3) SlotPtr = &L.Mods[Slot - 1];
	else if (Slot == 4) SlotPtr = &L.Armor;
	else if (Slot == 5) SlotPtr = &L.Bag;
	if (!SlotPtr || SlotPtr->IsNone())
	{
		return false;
	}

	const int32 CapAfter = (Slot == 5) ? BaseSlots : Capacity(L);
	if (L.Items.Num() + 1 > CapAfter)
	{
		return false;
	}

	const FName Id = *SlotPtr;
	*SlotPtr = NAME_None;
	L.Items.Add(FLRStack(Id, 1));
	if (Slot == 0)
	{
		const int32 OldMag = L.Mag;
		L.Mag = 0;
		Add(L, LRId::Munitions, OldMag);
	}
	return true;
}

bool FLRItems::ToPocket(FLRLoadout& L, int32 Index)
{
	if (!L.Items.IsValidIndex(Index) || L.Pocket.Num() >= PocketSlots)
	{
		return false;
	}
	const FLRItemDef* D = Find(L.Items[Index].Id);
	if (!D || D->Type == ELRItem::Mission)
	{
		return false;
	}
	L.Pocket.Add(L.Items[Index]);
	L.Items.RemoveAt(Index);
	return true;
}

bool FLRItems::FromPocket(FLRLoadout& L, int32 Index)
{
	if (!L.Pocket.IsValidIndex(Index) || L.Items.Num() >= Capacity(L))
	{
		return false;
	}
	L.Items.Add(L.Pocket[Index]);
	L.Pocket.RemoveAt(Index);
	return true;
}

FLRWeaponStats FLRItems::WeaponStats(const FLRLoadout& L)
{
	FLRWeaponStats S;
	const FLRItemDef* W = Find(L.Weapon);
	if (!W || W->Type != ELRItem::Arme)
	{
		return S;
	}
	S.bValid = true;
	S.Damage = W->A;
	S.Interval = W->B;
	S.Spread = W->C;
	S.Noise = W->D;
	S.Reload = W->E;
	S.MagSize = W->I1;
	S.Pellets = FMath::Max(1, W->I2);
	S.bAuto = W->B < 0.15f;

	for (const FName& M : L.Mods)
	{
		if (M == LRId::CanonLong)
		{
			S.Damage *= 1.2f;
		}
		else if (M == LRId::Silencieux)
		{
			S.Noise *= 0.3f;
			S.Damage *= 0.9f;
		}
		else if (M == LRId::GreffeAcide)
		{
			S.Damage *= 1.25f;
			S.bAcid = true;
			S.Reload *= 1.5f;
		}
		else if (M == LRId::ChargeurEtendu)
		{
			S.MagSize = FMath::RoundToInt(S.MagSize * 1.5f);
		}
		else if (M == LRId::GreffeVivante)
		{
			S.bRegen = true;
			S.Noise *= 2.f;
		}
		else if (M == LRId::CrosseStable)
		{
			S.Spread *= 0.6f;
		}
	}
	return S;
}

void FLRItems::AddToList(TArray<FLRStack>& List, FName Id, int32 Count)
{
	if (Id.IsNone() || Count <= 0)
	{
		return;
	}
	for (FLRStack& S : List)
	{
		if (S.Id == Id)
		{
			S.Count += Count;
			return;
		}
	}
	List.Add(FLRStack(Id, Count));
}

bool FLRItems::RemoveFromList(TArray<FLRStack>& List, FName Id, int32 Count)
{
	if (CountIn(List, Id) < Count)
	{
		return false;
	}
	for (int32 i = List.Num() - 1; i >= 0 && Count > 0; --i)
	{
		if (List[i].Id == Id)
		{
			const int32 N = FMath::Min(Count, List[i].Count);
			List[i].Count -= N;
			Count -= N;
			if (List[i].Count <= 0)
			{
				List.RemoveAt(i);
			}
		}
	}
	return true;
}

int32 FLRItems::CountIn(const TArray<FLRStack>& List, FName Id)
{
	int32 Total = 0;
	for (const FLRStack& S : List)
	{
		if (S.Id == Id)
		{
			Total += S.Count;
		}
	}
	return Total;
}

void FLRItems::RollLoot(int32 Kind, TArray<FLRStack>& Out)
{
	const int32 Rolls = (Kind == 1) ? 5 : FMath::RandRange(2, 3);
	for (int32 i = 0; i < Rolls; ++i)
	{
		Out.Add(RollOne());
	}
	if (Kind == 1)
	{
		Out.Add(FLRStack(RandomPlan(), 1));
		Out.Add(FLRStack(FName(TEXT("medkit")), 1));
		static const TCHAR* Rare[] = { TEXT("carabine"), TEXT("pm"), TEXT("pompe"), TEXT("silencieux"), TEXT("gilet_lourd"), TEXT("sac_grand") };
		const int32 RareNum = UE_ARRAY_COUNT(Rare);
		Out.Add(FLRStack(FName(Rare[FMath::RandRange(0, RareNum - 1)]), 1));
	}
}
