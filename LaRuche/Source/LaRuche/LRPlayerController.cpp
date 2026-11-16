#include "LRPlayerController.h"
#include "LRCharacter.h"
#include "LRGameInstance.h"
#include "LRGameMode.h"
#include "LRGameState.h"
#include "LRHUD.h"
#include "LRItems.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"

ALRPlayerController::ALRPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

ULRSaveGame* ALRPlayerController::GetSave() const
{
	const ULRGameInstance* GI = GetGameInstance<ULRGameInstance>();
	return GI ? GI->Save.Get() : nullptr;
}

void ALRPlayerController::Commit()
{
	ULRGameInstance* GI = GetGameInstance<ULRGameInstance>();
	if (GI && GI->Save)
	{
		FLRItems::EnsureMods(GI->Save->Loadout);
		GI->SaveNow();
		ServerSubmitLoadout(GI->Save->Loadout);
	}
}

bool ALRPlayerController::IsHost() const
{
	return GetNetMode() != NM_Client;
}

bool ALRPlayerController::GetMouse(FVector2D& Out) const
{
	const ULocalPlayer* LP = GetLocalPlayer();
	return LP && LP->ViewportClient && LP->ViewportClient->GetMousePosition(Out);
}

// ---------------------------------------------------------------- Reseau

void ALRPlayerController::ServerSubmitLoadout_Implementation(const FLRLoadout& Loadout)
{
	ServerLoadout = Loadout;
	FLRItems::EnsureMods(ServerLoadout);
	const ALRGameState* GS = GetWorld()->GetGameState<ALRGameState>();
	ALRCharacter* C = Cast<ALRCharacter>(GetPawn());
	if (C && GS && GS->Phase == ELRPhase::Planque && !C->bInRaid)
	{
		C->Inv = ServerLoadout;
	}
}

void ALRPlayerController::ServerStartRaid_Implementation()
{
	// Seul l'hote (le joueur qui heberge la partie) peut lancer le raid.
	if (!IsLocalController())
	{
		return;
	}
	if (ALRGameMode* GM = GetWorld()->GetAuthGameMode<ALRGameMode>())
	{
		GM->StartRaid();
	}
}

void ALRPlayerController::ServerSetConfig_Implementation(uint8 Mission, int32 StartTier)
{
	if (!IsLocalController())
	{
		return;
	}
	ALRGameState* GS = GetWorld()->GetGameState<ALRGameState>();
	if (GS && GS->Phase == ELRPhase::Planque)
	{
		GS->Mission = ELRMission(FMath::Clamp<int32>(Mission, 0, 3));
		GS->StartTier = FMath::Clamp(StartTier, 1, 3);
	}
}

void ALRPlayerController::ClientRaidResult_Implementation(const FLRLoadout& Loadout, const TArray<FLRStack>& Rewards, bool bSurvived, const FString& Summary)
{
	ULRSaveGame* Save = GetSave();
	if (!Save)
	{
		return;
	}
	Save->Loadout = Loadout;
	FLRItems::EnsureMods(Save->Loadout);
	Save->Raids++;
	if (bSurvived)
	{
		Save->Extractions++;
	}

	TArray<FString> Lines;
	Lines.Add(Summary);

	for (const FLRStack& Reward : Rewards)
	{
		FLRItems::AddToList(Save->Stash, Reward.Id, Reward.Count);
		Lines.Add(FString::Printf(TEXT("Prime : %s x%d"), *FLRItems::Name(Reward.Id), Reward.Count));
	}

	// Les objets de mission ne servent plus a rien une fois rentre.
	auto StripMission = [](TArray<FLRStack>& List)
	{
		for (int32 i = List.Num() - 1; i >= 0; --i)
		{
			const FLRItemDef* Def = FLRItems::Find(List[i].Id);
			if (!Def || Def->Type == ELRItem::Mission)
			{
				List.RemoveAt(i);
			}
		}
	};
	StripMission(Save->Loadout.Items);
	StripMission(Save->Loadout.Pocket);

	// Les plans ramenes sont appris definitivement.
	auto LearnPlans = [Save, &Lines](TArray<FLRStack>& List)
	{
		for (int32 i = List.Num() - 1; i >= 0; --i)
		{
			const FLRItemDef* Def = FLRItems::Find(List[i].Id);
			if (Def && Def->Type == ELRItem::Plan)
			{
				if (!Save->Plans.Contains(Def->Id))
				{
					Save->Plans.Add(Def->Id);
					Lines.Add(FString::Printf(TEXT("Nouveau plan appris : %s"), *Def->Name));
				}
				else
				{
					// Plan deja connu : revendu en ferraille
					FLRItems::AddToList(Save->Stash, LRId::Ferraille, Def->Value);
					Lines.Add(FString::Printf(TEXT("%s déjà connu : revendu %d ferraille"), *Def->Name, Def->Value));
				}
				List.RemoveAt(i);
			}
		}
	};
	LearnPlans(Save->Loadout.Items);
	LearnPlans(Save->Loadout.Pocket);
	LearnPlans(Save->Stash);

	Commit();

	if (ALRHUD* HUD = Cast<ALRHUD>(GetHUD()))
	{
		HUD->ShowResult(bSurvived, Lines);
	}
}

void ALRPlayerController::ClientNotify_Implementation(const FString& Msg)
{
	if (ALRHUD* HUD = Cast<ALRHUD>(GetHUD()))
	{
		HUD->AddLocalMessage(Msg);
	}
}

// ---------------------------------------------------------------- Clavier et souris

void ALRPlayerController::HandleTyping(ALRHUD* HUD)
{
	static const FKey Digits[10] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	static const FKey Pad[10] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	for (int32 i = 0; i < 10; ++i)
	{
		if ((WasInputKeyJustPressed(Digits[i]) || WasInputKeyJustPressed(Pad[i])) && HUD->IPText.Len() < 15)
		{
			HUD->IPText.AppendInt(i);
		}
	}
	// Sur un clavier francais le point demande Maj : la barre d'espace fait aussi un point.
	if ((WasInputKeyJustPressed(EKeys::Period) || WasInputKeyJustPressed(EKeys::Decimal) || WasInputKeyJustPressed(EKeys::Comma)
		|| WasInputKeyJustPressed(EKeys::Semicolon) || WasInputKeyJustPressed(EKeys::SpaceBar)) && HUD->IPText.Len() < 15)
	{
		HUD->IPText += TEXT(".");
	}
	if (WasInputKeyJustPressed(EKeys::BackSpace) && HUD->IPText.Len() > 0)
	{
		HUD->IPText.LeftChopInline(1);
	}
	if (WasInputKeyJustPressed(EKeys::Enter))
	{
		HUD->DoJoin();
	}
}

void ALRPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsLocalController())
	{
		return;
	}
	ALRHUD* HUD = Cast<ALRHUD>(GetHUD());
	if (!HUD || !PlayerInput)
	{
		return;
	}

	// Premier envoi de l'equipement au serveur
	if (!bPushed && GetPawn())
	{
		bPushed = true;
		Commit();
	}

	if (WasInputKeyJustPressed(EKeys::Tab))
	{
		HUD->ToggleSac();
	}
	if (WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::P))
	{
		HUD->TogglePause();
	}

	// Curseur visible dans les menus, cache en jeu
	const bool bCursor = HUD->WantsCursor();
	if (bCursor != bCursorMode)
	{
		bCursorMode = bCursor;
		bShowMouseCursor = bCursor;
		if (bCursor)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(Mode);
		}
		else
		{
			SetInputMode(FInputModeGameOnly());
		}
	}

	ALRCharacter* C = Cast<ALRCharacter>(GetPawn());

	if (bCursor)
	{
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			HUD->bClick = true;
		}
		if (HUD->IsTypingIP())
		{
			HandleTyping(HUD);
		}
		if (C)
		{
			C->SetSprint(false);
		}
		return;
	}

	// ---- Regard
	ULRSaveGame* Save = GetSave();
	if (Save)
	{
		if (WasInputKeyJustPressed(EKeys::PageUp))
		{
			Save->Sensitivity = FMath::Clamp(Save->Sensitivity + 0.1f, 0.2f, 4.f);
			HUD->AddLocalMessage(FString::Printf(TEXT("Sensibilité souris : %.1f"), Save->Sensitivity));
		}
		if (WasInputKeyJustPressed(EKeys::PageDown))
		{
			Save->Sensitivity = FMath::Clamp(Save->Sensitivity - 0.1f, 0.2f, 4.f);
			HUD->AddLocalMessage(FString::Printf(TEXT("Sensibilité souris : %.1f"), Save->Sensitivity));
		}
	}
	const float Sens = 2.5f * (Save ? Save->Sensitivity : 1.f);
	const float MouseX = PlayerInput->GetKeyValue(EKeys::MouseX);
	const float MouseY = PlayerInput->GetKeyValue(EKeys::MouseY);
	const float KeyYaw = (IsInputKeyDown(EKeys::Right) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::Left) ? 1.f : 0.f);
	const float KeyPitch = (IsInputKeyDown(EKeys::Up) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::Down) ? 1.f : 0.f);

	FRotator Rot = GetControlRotation();
	Rot.Yaw += MouseX * Sens + KeyYaw * 120.f * DeltaSeconds;
	Rot.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rot.Pitch) + MouseY * Sens + KeyPitch * 90.f * DeltaSeconds, -75.f, 75.f);
	Rot.Roll = 0.f;
	SetControlRotation(Rot);

	if (!C || C->Life != ELRLife::Vivant)
	{
		if (C)
		{
			C->SetSprint(false);
		}
		return;
	}

	// ---- Deplacement : ZQSD (clavier francais) et WASD fonctionnent tous les deux
	const float Forward = ((IsInputKeyDown(EKeys::Z) || IsInputKeyDown(EKeys::W)) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
	const float Strafe = (IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - ((IsInputKeyDown(EKeys::Q) || IsInputKeyDown(EKeys::A)) ? 1.f : 0.f);
	const FRotationMatrix YawMatrix(FRotator(0.f, Rot.Yaw, 0.f));
	if (Forward != 0.f)
	{
		C->AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Forward);
	}
	if (Strafe != 0.f)
	{
		C->AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Strafe);
	}

	if (WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		C->Jump();
	}
	if (WasInputKeyJustReleased(EKeys::SpaceBar))
	{
		C->StopJumping();
	}
	if (WasInputKeyJustPressed(EKeys::C) || WasInputKeyJustPressed(EKeys::LeftControl))
	{
		if (C->bIsCrouched)
		{
			C->UnCrouch();
		}
		else
		{
			C->Crouch();
		}
	}
	C->SetSprint(IsInputKeyDown(EKeys::LeftShift) && Forward > 0.f && !C->bIsCrouched);

	// ---- Actions
	if (IsInputKeyDown(EKeys::LeftMouseButton))
	{
		C->LocalFire(WasInputKeyJustPressed(EKeys::LeftMouseButton));
	}
	if (WasInputKeyJustPressed(EKeys::R))
	{
		C->ServerReload();
	}
	if (WasInputKeyJustPressed(EKeys::E))
	{
		C->TryInteract();
	}
	if (WasInputKeyJustPressed(EKeys::One))
	{
		C->QuickUse(NAME_None);
	}
	if (WasInputKeyJustPressed(EKeys::Two))
	{
		C->QuickUse(LRId::Leurre);
	}
	if (WasInputKeyJustPressed(EKeys::Three))
	{
		C->QuickUse(LRId::Pheromones);
	}
	if (WasInputKeyJustPressed(EKeys::Four))
	{
		C->QuickUse(FName(TEXT("serum")));
	}
}
