#include "LRCity.h"

FVector LRWorld::PlanqueSpawn(int32 Index)
{
	const int32 I = FMath::Abs(Index) % 8;
	return PlanqueCenter + FVector(-300.f + (I / 4) * 200.f, -300.f + (I % 4) * 200.f, 120.f);
}

void FLRCityLayout::Clear()
{
	bValid = false;
	Blocked.Reset();
	Blocks.Reset();
	Open.Reset();
}

FVector FLRCityLayout::CellToWorld(int32 X, int32 Y)
{
	return FVector((X - N * 0.5f + 0.5f) * Cell, (Y - N * 0.5f + 0.5f) * Cell, 0.f);
}

FIntPoint FLRCityLayout::WorldToCell(const FVector& W)
{
	return FIntPoint(
		FMath::Clamp(FMath::FloorToInt(W.X / Cell + N * 0.5f), 0, N - 1),
		FMath::Clamp(FMath::FloorToInt(W.Y / Cell + N * 0.5f), 0, N - 1));
}

void FLRCityLayout::SetBlocked(const FIntPoint& C, bool bBlocked)
{
	if (bValid && InGrid(C.X, C.Y))
	{
		Blocked[Idx(C.X, C.Y)] = bBlocked ? 1 : 0;
	}
}

void FLRCityLayout::Generate(int32 Seed)
{
	Clear();
	Blocked.Init(0, N * N);
	FRandomStream R(Seed);

	auto AddBlock = [this](const FVector& Loc, const FVector& Size, const FLinearColor& Color)
	{
		FLRBlockDef B;
		B.Loc = Loc;
		B.Scale = Size / 100.f;
		B.Color = Color;
		Blocks.Add(B);
	};

	// Mur d'enceinte de la zone de quarantaine
	for (int32 i = 0; i < N; ++i)
	{
		Blocked[Idx(i, 0)] = 1;
		Blocked[Idx(i, N - 1)] = 1;
		Blocked[Idx(0, i)] = 1;
		Blocked[Idx(N - 1, i)] = 1;
	}
	const float Half = N * Cell * 0.5f;
	const float WallH = 1400.f;
	const FLinearColor WallCol(0.07f, 0.08f, 0.07f);
	AddBlock(FVector(0.f, -Half + Cell * 0.5f, WallH * 0.5f), FVector(N * Cell, Cell, WallH), WallCol);
	AddBlock(FVector(0.f, Half - Cell * 0.5f, WallH * 0.5f), FVector(N * Cell, Cell, WallH), WallCol);
	AddBlock(FVector(-Half + Cell * 0.5f, 0.f, WallH * 0.5f), FVector(Cell, N * Cell, WallH), WallCol);
	AddBlock(FVector(Half - Cell * 0.5f, 0.f, WallH * 0.5f), FVector(Cell, N * Cell, WallH), WallCol);

	// Decoupage en parcelles separees par des rues de 2 cases.
	// Les cases 1 et 2 forment un boulevard peripherique toujours libre.
	TArray<FIntPoint> Cols;
	TArray<FIntPoint> Rows;
	auto Split = [&R](TArray<FIntPoint>& Out)
	{
		int32 P = 3;
		while (P < N - 5)
		{
			int32 W = R.RandRange(3, 6);
			if (P + W > N - 3)
			{
				W = N - 3 - P;
			}
			if (W < 2)
			{
				break;
			}
			Out.Add(FIntPoint(P, W));
			P += W + 2;
		}
	};
	Split(Cols);
	Split(Rows);

	TArray<uint8> Plaza;
	Plaza.Init(0, N * N);

	for (const FIntPoint& Col : Cols)
	{
		for (const FIntPoint& Row : Rows)
		{
			int32 X0 = Col.X;
			int32 W = Col.Y;
			int32 Y0 = Row.X;
			int32 H = Row.Y;

			if (R.FRand() < 0.14f)
			{
				// Place ouverte
				for (int32 X = X0; X < X0 + W; ++X)
				{
					for (int32 Y = Y0; Y < Y0 + H; ++Y)
					{
						Plaza[Idx(X, Y)] = 1;
					}
				}
				continue;
			}

			// Parfois une ruelle d'une case le long du batiment
			if (W >= 5 && R.FRand() < 0.4f)
			{
				W -= 1;
				if (R.FRand() < 0.5f)
				{
					X0 += 1;
				}
			}
			if (H >= 5 && R.FRand() < 0.4f)
			{
				H -= 1;
				if (R.FRand() < 0.5f)
				{
					Y0 += 1;
				}
			}

			const float Height = R.FRandRange(500.f, 2600.f);
			const float G = R.FRandRange(0.05f, 0.16f);
			const bool bInfected = R.FRand() < 0.2f;
			const FLinearColor Color = bInfected ? FLinearColor(0.13f, 0.05f, 0.15f) : FLinearColor(G, G, G * 1.1f);

			for (int32 X = X0; X < X0 + W; ++X)
			{
				for (int32 Y = Y0; Y < Y0 + H; ++Y)
				{
					Blocked[Idx(X, Y)] = 1;
				}
			}
			const FVector A = CellToWorld(X0, Y0);
			const FVector B = CellToWorld(X0 + W - 1, Y0 + H - 1);
			AddBlock(FVector((A.X + B.X) * 0.5f, (A.Y + B.Y) * 0.5f, Height * 0.5f), FVector(W * Cell, H * Cell, Height), Color);
		}
	}

	// Couverts (epaves, debris). Uniquement sur les cases paires : deux couverts ne sont
	// jamais voisins, donc une rue de 2 cases n'est jamais bouchee.
	for (int32 X = 4; X < N - 4; X += 2)
	{
		for (int32 Y = 4; Y < N - 4; Y += 2)
		{
			if (Blocked[Idx(X, Y)] != 0)
			{
				continue;
			}
			const float Chance = Plaza[Idx(X, Y)] != 0 ? 0.35f : 0.12f;
			if (R.FRand() < Chance)
			{
				Blocked[Idx(X, Y)] = 1;
				const bool bTurn = R.FRand() < 0.5f;
				const FVector Size = bTurn ? FVector(170.f, 340.f, 140.f) : FVector(340.f, 170.f, 140.f);
				const float G = R.FRandRange(0.1f, 0.25f);
				AddBlock(CellToWorld(X, Y) + FVector(0.f, 0.f, 70.f), Size, FLinearColor(G * 1.3f, G, G * 0.7f));
			}
		}
	}

	for (int32 X = 1; X < N - 1; ++X)
	{
		for (int32 Y = 1; Y < N - 1; ++Y)
		{
			if (Blocked[Idx(X, Y)] == 0)
			{
				Open.Add(FIntPoint(X, Y));
			}
		}
	}

	bValid = true;
}

FIntPoint FLRCityLayout::NearestOpen(const FIntPoint& C) const
{
	if (!IsBlocked(C.X, C.Y))
	{
		return C;
	}
	for (int32 Radius = 1; Radius <= 4; ++Radius)
	{
		for (int32 DX = -Radius; DX <= Radius; ++DX)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				if (!IsBlocked(C.X + DX, C.Y + DY))
				{
					return FIntPoint(C.X + DX, C.Y + DY);
				}
			}
		}
	}
	return FIntPoint(-1, -1);
}

namespace
{
	struct FLRPathNode
	{
		int32 I = 0;
		float F = 0.f;
		bool operator<(const FLRPathNode& Other) const { return F < Other.F; }
	};
}

bool FLRCityLayout::FindPath(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
	Out.Reset();
	if (!bValid)
	{
		return false;
	}
	const FIntPoint S = NearestOpen(WorldToCell(From));
	const FIntPoint G = NearestOpen(WorldToCell(To));
	if (S.X < 0 || G.X < 0)
	{
		return false;
	}

	const int32 Total = N * N;
	const int32 StartI = Idx(S.X, S.Y);
	const int32 GoalI = Idx(G.X, G.Y);

	TArray<float> Cost;
	Cost.Init(MAX_flt, Total);
	TArray<int32> Parent;
	Parent.Init(-1, Total);
	TArray<uint8> Closed;
	Closed.Init(0, Total);
	TArray<FLRPathNode> Heap;

	auto Heuristic = [&G](int32 X, int32 Y)
	{
		const float DX = float(X - G.X);
		const float DY = float(Y - G.Y);
		return FMath::Sqrt(DX * DX + DY * DY);
	};

	Cost[StartI] = 0.f;
	FLRPathNode First;
	First.I = StartI;
	First.F = Heuristic(S.X, S.Y);
	Heap.HeapPush(First);

	bool bFound = false;
	while (Heap.Num() > 0)
	{
		FLRPathNode Cur;
		Heap.HeapPop(Cur);
		if (Closed[Cur.I] != 0)
		{
			continue;
		}
		Closed[Cur.I] = 1;
		if (Cur.I == GoalI)
		{
			bFound = true;
			break;
		}
		const int32 CX = Cur.I % N;
		const int32 CY = Cur.I / N;
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				if (DX == 0 && DY == 0)
				{
					continue;
				}
				const int32 NX = CX + DX;
				const int32 NY = CY + DY;
				if (IsBlocked(NX, NY))
				{
					continue;
				}
				// Pas de passage en diagonale a travers un coin
				if (DX != 0 && DY != 0 && (IsBlocked(CX + DX, CY) || IsBlocked(CX, CY + DY)))
				{
					continue;
				}
				const int32 NI = Idx(NX, NY);
				if (Closed[NI] != 0)
				{
					continue;
				}
				const float NewCost = Cost[Cur.I] + ((DX != 0 && DY != 0) ? 1.4142f : 1.f);
				if (NewCost < Cost[NI])
				{
					Cost[NI] = NewCost;
					Parent[NI] = Cur.I;
					FLRPathNode Next;
					Next.I = NI;
					Next.F = NewCost + Heuristic(NX, NY);
					Heap.HeapPush(Next);
				}
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	TArray<int32> Chain;
	for (int32 I = GoalI; I != -1 && I != StartI; I = Parent[I])
	{
		Chain.Add(I);
	}
	for (int32 K = Chain.Num() - 1; K >= 0; --K)
	{
		Out.Add(CellToWorld(Chain[K] % N, Chain[K] / N));
	}
	// La destination exacte, si sa case est libre
	const FIntPoint ToCell = WorldToCell(To);
	if (!IsBlocked(ToCell.X, ToCell.Y))
	{
		Out.Add(FVector(To.X, To.Y, 0.f));
	}
	return Out.Num() > 0;
}
