#pragma once

#include "CoreMinimal.h"

struct FLRBlockDef
{
	FVector Loc = FVector::ZeroVector;
	FVector Scale = FVector::OneVector;
	FLinearColor Color = FLinearColor::Gray;
};

/**
 * Plan de la ville, genere a partir d'une graine.
 * Le serveur et les clients generent exactement le meme plan a partir de la meme graine.
 * Sert aussi de grille de deplacement pour les creatures.
 */
struct FLRCityLayout
{
	static constexpr int32 N = 44;
	static constexpr float Cell = 400.f;

	bool bValid = false;
	TArray<uint8> Blocked;
	TArray<FLRBlockDef> Blocks;
	TArray<FIntPoint> Open;

	void Generate(int32 Seed);
	void Clear();

	static int32 Idx(int32 X, int32 Y) { return Y * N + X; }
	static bool InGrid(int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < N && Y < N; }
	bool IsBlocked(int32 X, int32 Y) const { return !InGrid(X, Y) || Blocked[Idx(X, Y)] != 0; }
	void SetBlocked(const FIntPoint& C, bool bBlocked);

	static FVector CellToWorld(int32 X, int32 Y);
	static FVector CellToWorld(const FIntPoint& C) { return CellToWorld(C.X, C.Y); }
	static FIntPoint WorldToCell(const FVector& W);
	FIntPoint NearestOpen(const FIntPoint& C) const;

	/** Chemin de cases libres entre deux points. Renvoie false si aucun chemin. */
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& Out) const;
};

namespace LRWorld
{
	/** Centre de la planque, loin de la ville. */
	static const FVector PlanqueCenter(-14000.f, 0.f, 0.f);
	FVector PlanqueSpawn(int32 Index);
}
