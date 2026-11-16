#include "LRHUD.h"
#include "LRCharacter.h"
#include "LREnemy.h"
#include "LRGameInstance.h"
#include "LRItems.h"
#include "LRPlayerController.h"
#include "LRWorldActors.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SocketSubsystem.h"

namespace
{
	const float FontMul = 1.5f;
	const FLinearColor ColText(0.9f, 0.92f, 0.88f);
	const FLinearColor ColDim(0.55f, 0.58f, 0.55f);
	const FLinearColor ColGreen(0.35f, 0.95f, 0.45f);
	const FLinearColor ColRed(1.f, 0.3f, 0.25f);
	const FLinearColor ColYellow(1.f, 0.85f, 0.3f);
}

// ---------------------------------------------------------------- Dessin de base

void ALRHUD::Txt(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	DrawText(Text, Color, X, Y, nullptr, Scale * S * FontMul, false);
}

float ALRHUD::TxtW(const FString& Text, float Scale) const
{
	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	return Font ? Font->GetStringSize(*Text) * Scale * S * FontMul : 0.f;
}

void ALRHUD::TxtC(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	Txt(Text, CenterX - TxtW(Text, Scale) * 0.5f, Y, Color, Scale);
}

bool ALRHUD::Btn(const FString& Label, float X, float Y, float W, float H, bool bEnabled)
{
	const bool bHover = bEnabled && Mouse.X >= X && Mouse.X <= X + W && Mouse.Y >= Y && Mouse.Y <= Y + H;
	FLinearColor Back(0.1f, 0.1f, 0.1f, 0.6f);
	if (bEnabled)
	{
		Back = bHover ? FLinearColor(0.2f, 0.5f, 0.28f, 0.95f) : FLinearColor(0.1f, 0.22f, 0.14f, 0.92f);
	}
	DrawRect(Back, X, Y, W, H);
	TxtC(Label, X + W * 0.5f, Y + H * 0.5f - 11.f * S, bEnabled ? ColText : ColDim, 0.9f);
	if (bHover && bClick)
	{
		bClick = false;
		return true;
	}
	return false;
}

void ALRHUD::Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X, Y, W, H);
	DrawRect(Color, X + 2.f, Y + 2.f, FMath::Max(0.f, (W - 4.f) * FMath::Clamp(Pct, 0.f, 1.f)), H - 4.f);
}

FLinearColor ALRHUD::TierColor(int32 Tier)
{
	switch (Tier)
	{
	case 1: return FLinearColor(0.3f, 0.85f, 0.4f);
	case 2: return FLinearColor(0.95f, 0.85f, 0.2f);
	case 3: return FLinearColor(1.f, 0.5f, 0.1f);
	default: return FLinearColor(1.f, 0.15f, 0.15f);
	}
}

FString ALRHUD::LocalIP()
{
	static FString Cached;
	if (Cached.IsEmpty())
	{
		if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			bool bCanBind = false;
			const TSharedRef<FInternetAddr> Addr = Sockets->GetLocalHostAddr(*GLog, bCanBind);
			Cached = Addr->ToString(false);
		}
		if (Cached.IsEmpty())
		{
			Cached = TEXT("inconnue");
		}
	}
	return Cached;
}

// ---------------------------------------------------------------- Commandes des menus

void ALRHUD::ToggleSac()
{
	if (Menu == EMenu::None)
	{
		Menu = EMenu::Sac;
	}
	else if (Menu == EMenu::Sac)
	{
		Menu = EMenu::None;
	}
}

void ALRHUD::TogglePause()
{
	if (Menu == EMenu::None)
	{
		Menu = EMenu::Pause;
	}
	else if (Menu == EMenu::Join)
	{
		Menu = EMenu::Main;
	}
	else if (Menu != EMenu::Main)
	{
		Menu = EMenu::None;
	}
}

bool ALRHUD::WantsCursor() const
{
	return Menu != EMenu::None;
}

bool ALRHUD::IsTypingIP() const
{
	return Menu == EMenu::Join;
}

void ALRHUD::ShowResult(bool bSurvived, const TArray<FString>& Lines)
{
	bResultSurvived = bSurvived;
	ResultLines = Lines;
	Menu = EMenu::Result;
}

void ALRHUD::AddLocalMessage(const FString& Msg)
{
	FLRMessage M;
	M.Text = Msg;
	M.Time = GetWorld() ? float(GetWorld()->GetTimeSeconds()) : 0.f;
	LocalMessages.Add(M);
	if (LocalMessages.Num() > 5)
	{
		LocalMessages.RemoveAt(0);
	}
}

void ALRHUD::DoHost()
{
	// Recharge la carte en mode "ecoute" : les amis peuvent alors se connecter a cette machine.
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("listen"));
}

void ALRHUD::DoJoin()
{
	APlayerController* Player = GetOwningPlayerController();
	if (!Player || IPText.IsEmpty())
	{
		return;
	}
	if (ULRGameInstance* Instance = GetGameInstance<ULRGameInstance>())
	{
		if (Instance->Save)
		{
			Instance->Save->LastIP = IPText;
			Instance->SaveNow();
		}
	}
	Player->ClientTravel(IPText, ETravelType::TRAVEL_Absolute);
}

// ---------------------------------------------------------------- Boucle de dessin

void ALRHUD::DrawHUD()
{
	Super::DrawHUD();
	PC = Cast<ALRPlayerController>(GetOwningPlayerController());
	if (!Canvas || !PC || !GetWorld())
	{
		return;
	}
	S = Canvas->ClipY / 1080.f;
	OX = (Canvas->ClipX - 1920.f * S) * 0.5f;
	Mouse = FVector2D(-1.f, -1.f);
	PC->GetMouse(Mouse);
	GS = GetWorld()->GetGameState<ALRGameState>();
	Me = Cast<ALRCharacter>(PC->GetPawn());
	GI = GetGameInstance<ULRGameInstance>();

	// Le menu principal s'affiche tant qu'on n'est ni en solo, ni hote, ni connecte chez un ami.
	const bool bNeedMain = GetNetMode() == NM_Standalone && GI && !GI->bSolo;
	if (bNeedMain && Menu != EMenu::Main && Menu != EMenu::Join)
	{
		Menu = EMenu::Main;
	}
	else if (!bNeedMain && (Menu == EMenu::Main || Menu == EMenu::Join))
	{
		Menu = EMenu::None;
	}

	if (Menu != EMenu::Main && Menu != EMenu::Join)
	{
		DrawGame();
	}

	switch (Menu)
	{
	case EMenu::Main: DrawMain(); break;
	case EMenu::Join: DrawJoin(); break;
	case EMenu::Sac: DrawSac(); break;
	case EMenu::Pause: DrawPause(); break;
	case EMenu::Result: DrawResult(); break;
	default: break;
	}

	bClick = false;
}

// ---------------------------------------------------------------- Ecran de jeu

void ALRHUD::DrawGame()
{
	if (!GS)
	{
		return;
	}
	if (GS->Phase == ELRPhase::Raid && Me && Me->bInRaid)
	{
		DrawRaidHUD();
	}
	else
	{
		DrawPlanqueHints();
	}
	DrawMessages();
}

void ALRHUD::DrawPlanqueHints()
{
	Txt(TEXT("LA PLANQUE"), PX(40.f), PY(30.f), ColGreen, 1.5f);
	if (GS->Phase == ELRPhase::Raid)
	{
		Txt(TEXT("Un raid est en cours. Vous partirez avec l'équipe au prochain."), PX(40.f), PY(80.f), ColYellow);
	}
	else
	{
		Txt(TEXT("Tab : ouvrir le coffre, l'équipement et l'atelier"), PX(40.f), PY(80.f), ColText);
		Txt(PC->IsHost() ? TEXT("Vous êtes l'hôte : lancez le raid depuis cet écran (Tab).") : TEXT("L'hôte lance le raid quand tout le monde est prêt."),
			PX(40.f), PY(112.f), ColDim);
	}

	if (GetNetMode() == NM_ListenServer)
	{
		Txt(FString::Printf(TEXT("Partie ouverte. Adresse de cette machine : %s"), *LocalIP()), PX(40.f), PY(160.f), ColYellow);
		Txt(TEXT("Vos amis choisissent « Rejoindre » et tapent cette adresse (ou votre adresse Tailscale)."), PX(40.f), PY(190.f), ColDim, 0.85f);
	}

	static const TCHAR* Help[] = {
		TEXT("ZQSD : se déplacer      Souris : viser      Clic gauche : tirer"),
		TEXT("R : recharger      E : interagir      Maj : courir (bruyant)"),
		TEXT("C ou Ctrl : s'accroupir (silencieux)      Espace : sauter"),
		TEXT("1 : soin      2 : leurre      3 : phéromones      4 : sérum"),
		TEXT("Tab : sac      P ou Échap : pause      Page haut / bas : sensibilité souris")
	};
	float Y = PY(900.f);
	for (const TCHAR* Line : Help)
	{
		Txt(Line, PX(40.f), Y, ColDim, 0.85f);
		Y += 30.f * S;
	}
}

void ALRHUD::DrawMessages()
{
	const float Now = GetWorld()->GetTimeSeconds();
	float Y = PY(120.f);
	const float CX = Canvas->ClipX * 0.5f;
	for (const FLRMessage& M : GS->Messages)
	{
		if (Now - M.Time < 7.f)
		{
			TxtC(M.Text, CX, Y, ColYellow);
			Y += 30.f * S;
		}
	}
	for (const FLRMessage& M : LocalMessages)
	{
		if (Now - M.Time < 5.f)
		{
			TxtC(M.Text, CX, Y, ColText);
			Y += 30.f * S;
		}
	}
}

void ALRHUD::DrawMarker(const FVector& Loc, const FString& Label, const FLinearColor& Color)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const int32 Meters = FMath::RoundToInt(FVector::Dist(CamLoc, Loc) / 100.f);
	const FString Text = FString::Printf(TEXT("%s (%d m)"), *Label, Meters);

	FVector2D P;
	const bool bOnScreen = PC->ProjectWorldLocationToScreen(Loc, P) && P.X > 0.f && P.X < W && P.Y > 0.f && P.Y < H;
	if (bOnScreen)
	{
		DrawRect(Color, P.X - 4.f * S, P.Y - 4.f * S, 8.f * S, 8.f * S);
		TxtC(Text, P.X, P.Y - 30.f * S, Color, 0.8f);
		return;
	}
	// Hors champ : affiche sur le bord gauche ou droit, du cote ou il faut tourner.
	const float Delta = FMath::FindDeltaAngleDegrees(CamRot.Yaw, (Loc - CamLoc).Rotation().Yaw);
	if (Delta > 0.f)
	{
		Txt(Text + TEXT(" >"), W - TxtW(Text, 0.8f) - 50.f * S, H * 0.3f + MarkersRight * 26.f * S, Color, 0.8f);
		++MarkersRight;
	}
	else
	{
		Txt(TEXT("< ") + Text, 20.f * S, H * 0.3f + MarkersLeft * 26.f * S, Color, 0.8f);
		++MarkersLeft;
	}
}

void ALRHUD::DrawMarkers()
{
	MarkersLeft = 0;
	MarkersRight = 0;

	for (TActorIterator<ALRInteractable> It(GetWorld()); It; ++It)
	{
		FString Label;
		FLinearColor Color;
		if (It->GetMarker(Label, Color))
		{
			DrawMarker(It->GetActorLocation() + FVector(0.f, 0.f, 120.f), Label, Color);
		}
	}

	if (GS->Mission == ELRMission::Purge && !GS->bMissionDone)
	{
		for (TActorIterator<ALRNest> It(GetWorld()); It; ++It)
		{
			DrawMarker(It->GetActorLocation() + FVector(0.f, 0.f, 150.f), TEXT("Nid"), FLinearColor(0.8f, 0.3f, 1.f));
		}
	}

	// Coequipiers
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		ALRCharacter* C = *It;
		if (C == Me || !C->bInRaid || C->Life == ELRLife::Mort || C->Life == ELRLife::Extrait)
		{
			continue;
		}
		const bool bDown = (C->Life == ELRLife::ATerre);
		DrawMarker(C->GetActorLocation() + FVector(0.f, 0.f, 120.f), bDown ? C->GetDisplayName() + TEXT(" (à terre)") : C->GetDisplayName(),
			bDown ? ColRed : FLinearColor(0.4f, 0.8f, 1.f));
	}

	// Barres de vie des creatures blessees
	const FVector MyLoc = Me ? Me->GetActorLocation() : FVector::ZeroVector;
	for (TActorIterator<ALREnemy> It(GetWorld()); It; ++It)
	{
		ALREnemy* E = *It;
		if (E->MaxHealth <= 0.f || E->Health >= E->MaxHealth || FVector::Dist(MyLoc, E->GetActorLocation()) > 3500.f)
		{
			continue;
		}
		FVector2D P;
		const FVector Top = E->GetActorLocation() + FVector(0.f, 0.f, E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 30.f);
		if (PC->ProjectWorldLocationToScreen(Top, P))
		{
			Bar(P.X - 35.f * S, P.Y, 70.f * S, 9.f * S, E->Health / E->MaxHealth, ColRed);
		}
	}
}

void ALRHUD::DrawRaidHUD()
{
	const float Now = LR::Now(this);
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;

	DrawMarkers();

	// ---- Jauge d'Eveil
	const float BarW = 600.f * S;
	const float BarX = CX - BarW * 0.5f;
	const float BarY = PY(26.f);
	const FLinearColor TierCol = TierColor(GS->Tier);
	Bar(BarX, BarY, BarW, 22.f * S, GS->Eveil / 100.f, TierCol);
	for (int32 i = 1; i < 4; ++i)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.9f), BarX + BarW * 0.25f * i - 1.f, BarY, 2.f, 22.f * S);
	}
	TxtC(FString::Printf(TEXT("ÉVEIL %d/4 : %s"), GS->Tier, *ALRGameState::TierName(GS->Tier)), CX, BarY + 28.f * S, TierCol);
	const int32 Elapsed = FMath::Max(0, FMath::FloorToInt(Now - GS->RaidStart));
	Txt(FString::Printf(TEXT("%02d:%02d"), Elapsed / 60, Elapsed % 60), BarX + BarW + 16.f * S, BarY - 2.f * S, ColDim);

	// ---- Contrat
	Txt(FString::Printf(TEXT("Contrat : %s"), *ALRGameState::MissionName(GS->Mission)), PX(30.f), PY(26.f), ColText);
	Txt(ALRGameState::MissionText(GS->Mission), PX(30.f), PY(56.f), ColDim, 0.8f);
	Txt(GS->bMissionDone ? FString(TEXT("Rempli : extrayez-vous !")) : FString::Printf(TEXT("Progression : %d / %d"), GS->MissionProgress, GS->MissionGoal),
		PX(30.f), PY(82.f), GS->bMissionDone ? ColGreen : ColYellow);

	// ---- Equipe
	float TeamY = PY(130.f);
	for (TActorIterator<ALRCharacter> It(GetWorld()); It; ++It)
	{
		ALRCharacter* C = *It;
		if (!C->bInRaid)
		{
			continue;
		}
		FString State;
		FLinearColor Col = ColText;
		switch (C->Life)
		{
		case ELRLife::Vivant: State = FString::Printf(TEXT("%d PV"), FMath::CeilToInt(C->Health)); break;
		case ELRLife::ATerre: State = TEXT("À TERRE"); Col = ColRed; break;
		case ELRLife::Mort: State = TEXT("mort"); Col = ColDim; break;
		default: State = TEXT("extrait"); Col = ColGreen; break;
		}
		Txt(FString::Printf(TEXT("%s : %s"), *C->GetDisplayName(), *State), PX(30.f), TeamY, Col, 0.85f);
		TeamY += 26.f * S;
	}

	// ---- Etats particuliers
	if (Me->Life == ELRLife::ATerre)
	{
		TxtC(FString::Printf(TEXT("À TERRE : un coéquipier doit vous relever (%d s)"), FMath::Max(0, FMath::CeilToInt(Me->BleedEnd - Now))), CX, H * 0.45f, ColRed, 1.4f);
		return;
	}
	if (Me->Life == ELRLife::Mort)
	{
		TxtC(TEXT("MORT"), CX, H * 0.4f, ColRed, 2.f);
		TxtC(TEXT("Un coéquipier peut ramasser votre balise et la porter à une station médicale."), CX, H * 0.4f + 60.f * S, ColText);
		return;
	}
	if (Me->Life == ELRLife::Extrait)
	{
		TxtC(TEXT("EXTRAIT"), CX, H * 0.4f, ColGreen, 2.f);
		TxtC(TEXT("En attente du reste de l'équipe…"), CX, H * 0.4f + 60.f * S, ColText);
		return;
	}

	// ---- Vie
	const float HX = PX(30.f);
	const float HY = H - 70.f * S;
	Bar(HX, HY, 320.f * S, 26.f * S, Me->Health / FMath::Max(1.f, Me->MaxHealth), Me->Health > 35.f ? ColGreen : ColRed);
	Txt(FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Me->Health), FMath::RoundToInt(Me->MaxHealth)), HX + 330.f * S, HY, ColText);
	if (Me->Infection > 0)
	{
		Txt(FString::Printf(TEXT("Infecté x%d (sérum : touche 4)"), Me->Infection), HX, HY - 30.f * S, ColYellow, 0.85f);
	}
	if (Me->BeaconCount > 0)
	{
		Txt(FString::Printf(TEXT("Balises portées : %d"), Me->BeaconCount), HX, HY - 58.f * S, FLinearColor(0.1f, 0.9f, 1.f), 0.85f);
	}
	if (Me->PheroEnd > Now)
	{
		TxtC(FString::Printf(TEXT("Phéromones : %d s"), FMath::CeilToInt(Me->PheroEnd - Now)), CX, H - 190.f * S, FLinearColor(0.8f, 0.5f, 1.f));
	}

	// ---- Arme
	const FLRWeaponStats Stats = FLRItems::WeaponStats(Me->Inv);
	const float WX = W - 420.f * S;
	if (Stats.bValid)
	{
		Txt(FLRItems::Name(Me->Inv.Weapon), WX, H - 104.f * S, ColText);
		Txt(FString::Printf(TEXT("%d / %d"), Me->Inv.Mag, FLRItems::CountOf(Me->Inv, LRId::Munitions)), WX, H - 74.f * S, Me->Inv.Mag > 0 ? ColText : ColRed, 1.5f);
		if (Me->ReloadEnd > 0.f)
		{
			Txt(TEXT("Rechargement…"), WX + 190.f * S, H - 66.f * S, ColYellow);
		}
	}
	else
	{
		Txt(TEXT("Aucune arme équipée"), WX, H - 74.f * S, ColRed);
	}

	// ---- Raccourcis
	int32 Heals = 0;
	for (const FLRStack& Stack : Me->Inv.Items)
	{
		const FLRItemDef* Def = FLRItems::Find(Stack.Id);
		if (Def && Def->Type == ELRItem::Soin)
		{
			Heals += Stack.Count;
		}
	}
	TxtC(FString::Printf(TEXT("1 Soin (%d)    2 Leurre (%d)    3 Phéromones (%d)    4 Sérum (%d)    Sac %d/%d"),
		Heals, FLRItems::CountOf(Me->Inv, LRId::Leurre), FLRItems::CountOf(Me->Inv, LRId::Pheromones), FLRItems::CountOf(Me->Inv, FName(TEXT("serum"))),
		Me->Inv.Items.Num(), FLRItems::Capacity(Me->Inv)), CX, H - 40.f * S, ColDim, 0.8f);

	if (Menu != EMenu::None)
	{
		return;
	}

	// ---- Viseur
	const float Gap = 7.f * S;
	const float Len = 9.f * S;
	const FLinearColor Cross(1.f, 1.f, 1.f, 0.85f);
	DrawRect(Cross, CX - Gap - Len, H * 0.5f - 1.f, Len, 2.f);
	DrawRect(Cross, CX + Gap, H * 0.5f - 1.f, Len, 2.f);
	DrawRect(Cross, CX - 1.f, H * 0.5f - Gap - Len, 2.f, Len);
	DrawRect(Cross, CX - 1.f, H * 0.5f + Gap, 2.f, Len);
	if (GetWorld()->GetTimeSeconds() - Me->HitMarkerTime < 0.18f)
	{
		const FLinearColor HitCol = Me->bHitMarkerKill ? ColRed : FLinearColor::White;
		const float D = 14.f * S;
		DrawLine(CX - D, H * 0.5f - D, CX - D * 0.4f, H * 0.5f - D * 0.4f, HitCol, 2.f);
		DrawLine(CX + D, H * 0.5f - D, CX + D * 0.4f, H * 0.5f - D * 0.4f, HitCol, 2.f);
		DrawLine(CX - D, H * 0.5f + D, CX - D * 0.4f, H * 0.5f + D * 0.4f, HitCol, 2.f);
		DrawLine(CX + D, H * 0.5f + D, CX + D * 0.4f, H * 0.5f + D * 0.4f, HitCol, 2.f);
	}

	// ---- Interaction
	if (Me->ChannelEnd > 0.f && Me->ChannelEnd > Me->ChannelStart)
	{
		const float Pct = (Now - Me->ChannelStart) / (Me->ChannelEnd - Me->ChannelStart);
		Bar(CX - 150.f * S, H * 0.62f, 300.f * S, 18.f * S, Pct, ColYellow);
		TxtC(TEXT("Ne bougez pas…"), CX, H * 0.62f + 24.f * S, ColText, 0.85f);
	}
	else if (!Me->FocusPrompt.IsEmpty())
	{
		TxtC(FString::Printf(TEXT("[E] %s"), *Me->FocusPrompt), CX, H * 0.62f, ColText, 1.1f);
	}
}

// ---------------------------------------------------------------- Menus

void ALRHUD::DrawMain()
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	DrawRect(FLinearColor(0.01f, 0.03f, 0.02f, 0.94f), 0.f, 0.f, W, H);
	TxtC(TEXT("LA RUCHE"), CX, PY(180.f), ColGreen, 4.f);
	TxtC(TEXT("Entrez. Récupérez. Sortez avant qu'elle ne se réveille."), CX, PY(310.f), ColDim);

	const float BW = 460.f * S;
	const float BH = 60.f * S;
	const float BX = CX - BW * 0.5f;
	float Y = PY(420.f);
	if (Btn(TEXT("Jouer seul"), BX, Y, BW, BH) && GI)
	{
		GI->bSolo = true;
		Menu = EMenu::None;
	}
	Y += 80.f * S;
	if (Btn(TEXT("Héberger une partie"), BX, Y, BW, BH))
	{
		DoHost();
	}
	Y += 80.f * S;
	if (Btn(TEXT("Rejoindre une partie"), BX, Y, BW, BH))
	{
		Menu = EMenu::Join;
		if (!bIPLoaded && GI && GI->Save)
		{
			bIPLoaded = true;
			IPText = GI->Save->LastIP;
		}
	}
	Y += 80.f * S;
	if (Btn(TEXT("Quitter"), BX, Y, BW, BH))
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
	TxtC(TEXT("V1 prototype"), CX, H - 50.f * S, ColDim, 0.8f);
}

void ALRHUD::DrawJoin()
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	DrawRect(FLinearColor(0.01f, 0.03f, 0.02f, 0.94f), 0.f, 0.f, W, H);
	TxtC(TEXT("REJOINDRE UNE PARTIE"), CX, PY(200.f), ColGreen, 2.2f);
	TxtC(TEXT("Tapez l'adresse IP de l'hôte (elle s'affiche sur son écran dans la planque)."), CX, PY(300.f), ColText);
	TxtC(TEXT("Chiffres du clavier, Espace pour le point, Retour arrière pour effacer."), CX, PY(335.f), ColDim, 0.85f);

	const float BoxW = 460.f * S;
	const float BoxX = CX - BoxW * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.9f), BoxX, PY(400.f), BoxW, 60.f * S);
	const bool bBlink = FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.f) < 0.5f;
	TxtC(IPText + (bBlink ? TEXT("_") : TEXT(" ")), CX, PY(414.f), ColYellow, 1.4f);

	float Y = PY(490.f);
	if (Btn(TEXT("Coller l'adresse copiée"), BoxX, Y, BoxW, 54.f * S))
	{
		FString Clip;
		FPlatformApplicationMisc::ClipboardPaste(Clip);
		FString Clean;
		for (const TCHAR Ch : Clip)
		{
			if ((FChar::IsDigit(Ch) || Ch == TEXT('.')) && Clean.Len() < 15)
			{
				Clean.AppendChar(Ch);
			}
		}
		if (!Clean.IsEmpty())
		{
			IPText = Clean;
		}
	}
	Y += 70.f * S;
	if (Btn(TEXT("Rejoindre (Entrée)"), BoxX, Y, BoxW, 54.f * S, !IPText.IsEmpty()))
	{
		DoJoin();
	}
	Y += 70.f * S;
	if (Btn(TEXT("Retour"), BoxX, Y, BoxW, 54.f * S))
	{
		Menu = EMenu::Main;
	}
}

void ALRHUD::DrawPause()
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), 0.f, 0.f, W, H);
	TxtC(TEXT("PAUSE"), CX, PY(240.f), ColGreen, 2.5f);
	TxtC(TEXT("La partie continue pendant la pause."), CX, PY(330.f), ColDim, 0.85f);
	if (GetNetMode() == NM_ListenServer)
	{
		TxtC(FString::Printf(TEXT("Adresse de cette machine : %s"), *LocalIP()), CX, PY(365.f), ColYellow);
	}

	const float BW = 460.f * S;
	const float BX = CX - BW * 0.5f;
	float Y = PY(430.f);
	if (Btn(TEXT("Reprendre"), BX, Y, BW, 56.f * S))
	{
		Menu = EMenu::None;
	}
	Y += 76.f * S;
	if (Btn(TEXT("Retour au menu principal"), BX, Y, BW, 56.f * S))
	{
		if (GI)
		{
			GI->bSolo = false;
		}
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true);
	}
	Y += 76.f * S;
	if (Btn(TEXT("Quitter le jeu"), BX, Y, BW, 56.f * S))
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}

void ALRHUD::DrawResult()
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.88f), 0.f, 0.f, W, H);
	TxtC(bResultSurvived ? TEXT("EXTRACTION RÉUSSIE") : TEXT("RAID PERDU"), CX, PY(220.f), bResultSurvived ? ColGreen : ColRed, 2.5f);
	float Y = PY(340.f);
	for (const FString& Line : ResultLines)
	{
		TxtC(Line, CX, Y, ColText);
		Y += 36.f * S;
	}
	const float BW = 360.f * S;
	if (Btn(TEXT("Continuer"), CX - BW * 0.5f, Y + 40.f * S, BW, 56.f * S))
	{
		Menu = EMenu::None;
	}
}

// ---------------------------------------------------------------- Sac, coffre, atelier

void ALRHUD::DrawSac()
{
	if (!GS || !PC->GetSave())
	{
		return;
	}
	const bool bRaid = GS->Phase == ELRPhase::Raid && Me && Me->bInRaid;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.86f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	if (bRaid)
	{
		Txt(TEXT("SAC"), PX(650.f), PY(24.f), ColGreen, 1.5f);
		Txt(TEXT("Tab : fermer"), PX(1100.f), PY(34.f), ColDim, 0.85f);
		DrawInventory(true, PX(650.f), PY(80.f));
		return;
	}
	if (GS->Phase == ELRPhase::Raid)
	{
		TxtC(TEXT("Un raid est en cours : le coffre rouvrira au retour de l'équipe."), Canvas->ClipX * 0.5f, PY(500.f), ColYellow);
		return;
	}

	Txt(TEXT("LA PLANQUE"), PX(30.f), PY(20.f), ColGreen, 1.5f);
	Txt(TEXT("Tab : fermer"), PX(330.f), PY(32.f), ColDim, 0.85f);
	DrawStash();
	DrawInventory(false, PX(650.f), PY(80.f));
	DrawCraft();
	DrawRaidPanel();
}

void ALRHUD::DrawStash()
{
	ULRSaveGame* Save = PC->GetSave();
	const float X = PX(30.f);
	const float ColW = 590.f * S;
	const float RowH = 30.f * S;
	float Y = PY(80.f);

	Txt(FString::Printf(TEXT("COFFRE   (Ferraille : %d)"), FLRItems::CountIn(Save->Stash, LRId::Ferraille)), X, Y, ColGreen, 1.1f);
	Y += 40.f * S;

	const int32 PerPage = 22;
	const int32 NumPages = FMath::Max(1, (Save->Stash.Num() + PerPage - 1) / PerPage);
	StashPage = FMath::Clamp(StashPage, 0, NumPages - 1);

	int32 TakeIndex = -1;
	int32 SellIndex = -1;
	const int32 First = StashPage * PerPage;
	for (int32 i = First; i < FMath::Min(Save->Stash.Num(), First + PerPage); ++i)
	{
		const FLRStack& Stack = Save->Stash[i];
		const FLRItemDef* Def = FLRItems::Find(Stack.Id);
		if (!Def)
		{
			continue;
		}
		Txt(FString::Printf(TEXT("%s x%d"), *Def->Name, Stack.Count), X, Y + 3.f * S, FLRItems::RarityColor(Def->Rarity), 0.85f);
		if (Btn(TEXT("Prendre"), X + ColW - 250.f * S, Y, 100.f * S, RowH - 4.f * S))
		{
			TakeIndex = i;
		}
		const int32 Gain = Def->Value * Stack.Count;
		const bool bCanSell = Def->Value > 0 && Stack.Id != LRId::Ferraille;
		if (Btn(bCanSell ? FString::Printf(TEXT("Vendre +%d"), Gain) : FString(TEXT("-")), X + ColW - 140.f * S, Y, 140.f * S, RowH - 4.f * S, bCanSell))
		{
			SellIndex = i;
		}
		Y += RowH;
	}

	if (NumPages > 1)
	{
		Y = PY(80.f) + 40.f * S + PerPage * RowH + 6.f * S;
		if (Btn(TEXT("< Page"), X, Y, 120.f * S, RowH, StashPage > 0))
		{
			--StashPage;
		}
		Txt(FString::Printf(TEXT("%d / %d"), StashPage + 1, NumPages), X + 140.f * S, Y + 3.f * S, ColDim, 0.85f);
		if (Btn(TEXT("Page >"), X + 220.f * S, Y, 120.f * S, RowH, StashPage < NumPages - 1))
		{
			++StashPage;
		}
	}

	// Kit de secours : evite de rester bloque sans arme ni ferraille apres un raid perdu.
	bool bHasWeapon = !Save->Loadout.Weapon.IsNone();
	auto HasWeaponIn = [](const TArray<FLRStack>& List)
	{
		for (const FLRStack& Stack : List)
		{
			const FLRItemDef* Def = FLRItems::Find(Stack.Id);
			if (Def && Def->Type == ELRItem::Arme)
			{
				return true;
			}
		}
		return false;
	};
	bHasWeapon = bHasWeapon || HasWeaponIn(Save->Stash) || HasWeaponIn(Save->Loadout.Items) || HasWeaponIn(Save->Loadout.Pocket);
	if (!bHasWeapon)
	{
		if (Btn(TEXT("Kit de secours gratuit (pistolet + 24 balles)"), X, PY(830.f), ColW, 44.f * S))
		{
			FLRItems::AddToList(Save->Stash, LRId::Pistolet, 1);
			FLRItems::AddToList(Save->Stash, LRId::Munitions, 24);
			PC->Commit();
		}
	}

	if (TakeIndex >= 0 && Save->Stash.IsValidIndex(TakeIndex))
	{
		const FLRStack Stack = Save->Stash[TakeIndex];
		const FLRItemDef* Def = FLRItems::Find(Stack.Id);
		const int32 Want = Def ? FMath::Min(Stack.Count, Def->MaxStack) : Stack.Count;
		const int32 Left = FLRItems::Add(Save->Loadout, Stack.Id, Want);
		const int32 Taken = Want - Left;
		if (Taken > 0)
		{
			FLRItems::RemoveFromList(Save->Stash, Stack.Id, Taken);
			PC->Commit();
		}
		else
		{
			AddLocalMessage(TEXT("Sac plein."));
		}
	}
	else if (SellIndex >= 0 && Save->Stash.IsValidIndex(SellIndex))
	{
		const FLRStack Stack = Save->Stash[SellIndex];
		const FLRItemDef* Def = FLRItems::Find(Stack.Id);
		if (Def && Def->Value > 0)
		{
			Save->Stash.RemoveAt(SellIndex);
			FLRItems::AddToList(Save->Stash, LRId::Ferraille, Def->Value * Stack.Count);
			PC->Commit();
		}
	}
}

void ALRHUD::DrawInventory(bool bRaid, float X, float Y)
{
	ULRSaveGame* Save = PC->GetSave();
	// Copie de travail : en raid on affiche l'etat du serveur, dans la planque la sauvegarde locale.
	FLRLoadout L = bRaid ? Me->Inv : Save->Loadout;
	FLRItems::EnsureMods(L);

	const float ColW = 620.f * S;
	const float RowH = 30.f * S;
	const float BtnH = RowH - 4.f * S;
	const float BW = 96.f * S;

	// Action choisie cette image : 1 retirer, 2 equiper, 3 utiliser, 4 poche, 5 ranger/jeter, 6 sortir de la poche
	int32 Action = 0;
	int32 ActionIndex = -1;

	Txt(TEXT("ÉQUIPEMENT"), X, Y, ColGreen, 1.1f);
	Y += 38.f * S;
	static const TCHAR* SlotNames[6] = { TEXT("Arme"), TEXT("Canon"), TEXT("Chargeur"), TEXT("Crosse"), TEXT("Armure"), TEXT("Sac à dos") };
	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		FName Id;
		if (Slot == 0) Id = L.Weapon;
		else if (Slot <= 3) Id = L.Mods[Slot - 1];
		else if (Slot == 4) Id = L.Armor;
		else Id = L.Bag;

		const FLRItemDef* Def = FLRItems::Find(Id);
		Txt(FString::Printf(TEXT("%s : %s"), SlotNames[Slot], Def ? *Def->Name : TEXT("aucun")), X, Y + 3.f * S,
			Def ? FLRItems::RarityColor(Def->Rarity) : ColDim, 0.85f);
		if (Def && Btn(TEXT("Retirer"), X + ColW - BW, Y, BW, BtnH))
		{
			Action = 1;
			ActionIndex = Slot;
		}
		Y += RowH;
	}

	Y += 12.f * S;
	Txt(FString::Printf(TEXT("SAC  (%d / %d)"), L.Items.Num(), FLRItems::Capacity(L)), X, Y, ColGreen, 1.1f);
	Y += 38.f * S;
	for (int32 i = 0; i < L.Items.Num(); ++i)
	{
		const FLRItemDef* Def = FLRItems::Find(L.Items[i].Id);
		if (!Def)
		{
			continue;
		}
		Txt(FString::Printf(TEXT("%s x%d"), *Def->Name, L.Items[i].Count), X, Y + 3.f * S, FLRItems::RarityColor(Def->Rarity), 0.85f);

		const bool bEquipable = Def->Type == ELRItem::Arme || Def->Type == ELRItem::Mod || Def->Type == ELRItem::Armure || Def->Type == ELRItem::Sac;
		const bool bUsable = Def->Type == ELRItem::Soin || Def->Type == ELRItem::Serum || Def->Type == ELRItem::Gadget;
		float BX = X + ColW - BW;
		if (Btn(bRaid ? TEXT("Jeter") : TEXT("Ranger"), BX, Y, BW, BtnH, !(bRaid && Def->Type == ELRItem::Mission)))
		{
			Action = 5;
			ActionIndex = i;
		}
		BX -= BW + 6.f * S;
		if (Btn(TEXT("Poche"), BX, Y, BW, BtnH, Def->Type != ELRItem::Mission))
		{
			Action = 4;
			ActionIndex = i;
		}
		BX -= BW + 6.f * S;
		if (bEquipable)
		{
			if (Btn(TEXT("Équiper"), BX, Y, BW, BtnH))
			{
				Action = 2;
				ActionIndex = i;
			}
		}
		else if (bUsable && bRaid)
		{
			if (Btn(TEXT("Utiliser"), BX, Y, BW, BtnH))
			{
				Action = 3;
				ActionIndex = i;
			}
		}
		Y += RowH;
	}

	Y += 12.f * S;
	Txt(FString::Printf(TEXT("POCHE SÉCURISÉE  (%d / %d) : gardée même en cas de mort"), L.Pocket.Num(), FLRItems::PocketSlots), X, Y, ColGreen, 0.95f);
	Y += 36.f * S;
	for (int32 i = 0; i < L.Pocket.Num(); ++i)
	{
		const FLRItemDef* Def = FLRItems::Find(L.Pocket[i].Id);
		if (!Def)
		{
			continue;
		}
		Txt(FString::Printf(TEXT("%s x%d"), *Def->Name, L.Pocket[i].Count), X, Y + 3.f * S, FLRItems::RarityColor(Def->Rarity), 0.85f);
		if (Btn(TEXT("Sortir"), X + ColW - BW, Y, BW, BtnH))
		{
			Action = 6;
			ActionIndex = i;
		}
		Y += RowH;
	}

	if (Action == 0)
	{
		return;
	}

	if (bRaid)
	{
		switch (Action)
		{
		case 1: Me->ServerUnequip(ActionIndex); break;
		case 2: Me->ServerEquip(ActionIndex); break;
		case 3: Me->ServerUseItem(ActionIndex); break;
		case 4: Me->ServerPocket(ActionIndex, true); break;
		case 5: Me->ServerDropItem(ActionIndex); break;
		case 6: Me->ServerPocket(ActionIndex, false); break;
		default: break;
		}
		return;
	}

	bool bOk = true;
	FLRLoadout& Real = Save->Loadout;
	switch (Action)
	{
	case 1: bOk = FLRItems::Unequip(Real, ActionIndex); break;
	case 2: bOk = FLRItems::Equip(Real, ActionIndex); break;
	case 4: bOk = FLRItems::ToPocket(Real, ActionIndex); break;
	case 5:
		if (Real.Items.IsValidIndex(ActionIndex))
		{
			FLRItems::AddToList(Save->Stash, Real.Items[ActionIndex].Id, Real.Items[ActionIndex].Count);
			Real.Items.RemoveAt(ActionIndex);
		}
		break;
	case 6: bOk = FLRItems::FromPocket(Real, ActionIndex); break;
	default: break;
	}
	if (bOk)
	{
		PC->Commit();
	}
	else
	{
		AddLocalMessage(TEXT("Impossible : pas assez de place."));
	}
}

void ALRHUD::DrawCraft()
{
	ULRSaveGame* Save = PC->GetSave();
	const float X = PX(1300.f);
	const float ColW = 590.f * S;
	const float RowH = 42.f * S;
	float Y = PY(80.f);

	Txt(FString::Printf(TEXT("ATELIER   (plans connus : %d)"), Save->Plans.Num()), X, Y, ColGreen, 1.1f);
	Y += 40.f * S;

	int32 CraftIndex = -1;
	const TArray<FLRRecipe>& Recipes = FLRItems::Recipes();
	for (int32 i = 0; i < Recipes.Num(); ++i)
	{
		const FLRRecipe& Recipe = Recipes[i];
		const FLRItemDef* Def = FLRItems::Find(Recipe.Result);
		if (!Def)
		{
			continue;
		}
		const bool bKnown = Recipe.Plan.IsNone() || Save->Plans.Contains(Recipe.Plan);
		bool bAfford = true;
		FString CostText;
		for (const FLRStack& Cost : Recipe.Cost)
		{
			if (FLRItems::CountIn(Save->Stash, Cost.Id) < Cost.Count)
			{
				bAfford = false;
			}
			if (!CostText.IsEmpty())
			{
				CostText += TEXT(", ");
			}
			CostText += FString::Printf(TEXT("%d %s"), Cost.Count, *FLRItems::Name(Cost.Id));
		}

		const FString Title = Recipe.Count > 1 ? FString::Printf(TEXT("%s x%d"), *Def->Name, Recipe.Count) : Def->Name;
		Txt(Title, X, Y, bKnown ? FLRItems::RarityColor(Def->Rarity) : ColDim, 0.8f);
		Txt(bKnown ? CostText : FString(TEXT("Plan à trouver en raid")), X, Y + 19.f * S, bKnown ? (bAfford ? ColText : ColRed) : ColDim, 0.7f);
		if (Btn(TEXT("Fabriquer"), X + ColW - 120.f * S, Y + 4.f * S, 120.f * S, RowH - 12.f * S, bKnown && bAfford))
		{
			CraftIndex = i;
		}
		Y += RowH;
	}

	if (CraftIndex >= 0)
	{
		const FLRRecipe& Recipe = Recipes[CraftIndex];
		for (const FLRStack& Cost : Recipe.Cost)
		{
			FLRItems::RemoveFromList(Save->Stash, Cost.Id, Cost.Count);
		}
		FLRItems::AddToList(Save->Stash, Recipe.Result, Recipe.Count);
		PC->Commit();
		AddLocalMessage(FString::Printf(TEXT("Fabriqué : %s (rangé dans le coffre)"), *FLRItems::Name(Recipe.Result)));
	}
}

void ALRHUD::DrawRaidPanel()
{
	const float X = PX(650.f);
	const float Y = PY(960.f);
	const float H = 50.f * S;
	const FString MissionLabel = FString::Printf(TEXT("Contrat : %s"), *ALRGameState::MissionName(GS->Mission));
	const FString TierLabel = FString::Printf(TEXT("Menace : %d (%s)"), GS->StartTier, *ALRGameState::TierName(GS->StartTier));

	if (!PC->IsHost())
	{
		Txt(MissionLabel + TEXT("     ") + TierLabel, X, Y, ColText);
		Txt(TEXT("En attente : c'est l'hôte qui lance le raid."), X, Y + 34.f * S, ColDim, 0.85f);
		return;
	}

	if (Btn(MissionLabel, X, Y, 230.f * S, H))
	{
		PC->ServerSetConfig(uint8((int32(GS->Mission) + 1) % 4), GS->StartTier);
	}
	if (Btn(TierLabel, X + 240.f * S, Y, 200.f * S, H))
	{
		PC->ServerSetConfig(uint8(GS->Mission), (GS->StartTier % 3) + 1);
	}
	if (Btn(TEXT("LANCER LE RAID"), X + 450.f * S, Y, 170.f * S, H))
	{
		PC->ServerStartRaid();
		Menu = EMenu::None;
	}
	Txt(ALRGameState::MissionText(GS->Mission) + TEXT(". Menace plus haute = meilleure prime."), X, Y + 58.f * S, ColDim, 0.75f);
}
