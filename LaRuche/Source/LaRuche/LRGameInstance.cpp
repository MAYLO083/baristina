#include "LRGameInstance.h"
#include "LRItems.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

void ULRGameInstance::Init()
{
	Super::Init();

	// En test dans l'editeur avec plusieurs joueurs, chacun a sa propre sauvegarde.
	Slot = TEXT("LaRuche");
	if (const FWorldContext* Context = GetWorldContext())
	{
		if (Context->PIEInstance > 0)
		{
			Slot += FString::Printf(TEXT("_Test%d"), Context->PIEInstance);
		}
	}

	Save = Cast<ULRSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Save)
	{
		Save = Cast<ULRSaveGame>(UGameplayStatics::CreateSaveGameObject(ULRSaveGame::StaticClass()));
		// Kit de depart
		Save->Loadout.Weapon = LRId::Pistolet;
		Save->Loadout.Mag = 12;
		Save->Loadout.Items.Add(FLRStack(LRId::Munitions, 48));
		Save->Loadout.Items.Add(FLRStack(LRId::Bandage, 2));
		Save->Stash.Add(FLRStack(LRId::Ferraille, 6));
		Save->Stash.Add(FLRStack(LRId::Munitions, 60));
		Save->Stash.Add(FLRStack(LRId::Bandage, 2));
	}
	FLRItems::EnsureMods(Save->Loadout);
}

void ULRGameInstance::SaveNow()
{
	if (Save)
	{
		UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	}
}
