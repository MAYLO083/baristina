#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LRGameState.h"
#include "LRTypes.h"
#include "LRHUD.generated.h"

class ALRPlayerController;
class ALRCharacter;
class ULRGameInstance;

/** Toute l'interface : menu principal, ecran de jeu, sac, coffre, atelier. Dessinee directement a l'ecran. */
UCLASS()
class ALRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	void ToggleSac();
	void TogglePause();
	bool WantsCursor() const;
	bool IsTypingIP() const;
	void DoJoin();
	void ShowResult(bool bSurvived, const TArray<FString>& Lines);
	void AddLocalMessage(const FString& Msg);

	FString IPText;
	bool bClick = false;

private:
	enum class EMenu : uint8
	{
		None,
		Main,
		Join,
		Sac,
		Pause,
		Result
	};

	// Dessin de base
	float PX(float V) const { return OX + V * S; }
	float PY(float V) const { return V * S; }
	void Txt(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.f);
	void TxtC(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale = 1.f);
	float TxtW(const FString& Text, float Scale = 1.f) const;
	bool Btn(const FString& Label, float X, float Y, float W, float H, bool bEnabled = true);
	void Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& Color);

	// Ecrans
	void DrawGame();
	void DrawRaidHUD();
	void DrawPlanqueHints();
	void DrawMessages();
	void DrawMarkers();
	void DrawMarker(const FVector& Loc, const FString& Label, const FLinearColor& Color);
	void DrawMain();
	void DrawJoin();
	void DrawPause();
	void DrawResult();
	void DrawSac();
	void DrawStash();
	void DrawInventory(bool bRaid, float X, float Y);
	void DrawCraft();
	void DrawRaidPanel();

	void DoHost();
	static FString LocalIP();
	static FLinearColor TierColor(int32 Tier);

	EMenu Menu = EMenu::None;

	// Valeurs recalculees a chaque image
	float S = 1.f;
	float OX = 0.f;
	FVector2D Mouse = FVector2D(-1.f, -1.f);
	ALRPlayerController* PC = nullptr;
	ALRGameState* GS = nullptr;
	ALRCharacter* Me = nullptr;
	ULRGameInstance* GI = nullptr;
	int32 MarkersLeft = 0;
	int32 MarkersRight = 0;

	TArray<FLRMessage> LocalMessages;
	TArray<FString> ResultLines;
	bool bResultSurvived = false;
	int32 StashPage = 0;
	bool bIPLoaded = false;
};
