#include "DustlineCar.h"
#include "DustlineCarParts.h"

// Carrocerías procedurales: cada modelo tiene su perfil lateral (cofre, parabrisas, techo, portón), ventanas, pasos de rueda reales,
// y detalles propios (faros, parrilla, parachoques, retrovisores, manillas, rueda de repuesto, barras de techo...).

namespace
{
	float Sm(float x) { x = FMath::Clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }

	struct FBodyDef
	{
		TArray<FVector2D> Top;      // (u, v): u 0=trasera..1=delantera; v 0=suelo de carrocería..1=techo
		TArray<FVector2D> Win;      // ventanas laterales (u0,u1)
		TArray<float> Gaps;         // juntas de puertas (u)
		float Belt = 0.6f, CabIn = 0.9f, RoofIn = 0.82f, Crown = 2.f, RoofCrown = 2.f, Round = 3.f, SmoothDeg = 30.f, TaperF = 0.04f, TaperR = 0.03f;
		bool bSmoothOutline = false;
		TArray<FVector2D> BeltPts;   // (u, v) cintura variable (opcional)
	};

	FBodyDef MakeDef(int32 St)
	{
		FBodyDef D;
		switch (St)
		{
		case 0: // Clase G
			D.Top = { FVector2D(0, 0.93), FVector2D(0.012, 0.99), FVector2D(0.04, 1.0), FVector2D(0.68, 1.0), FVector2D(0.695, 0.985), FVector2D(0.745, 0.625), FVector2D(0.757, 0.592), FVector2D(0.80, 0.58), FVector2D(0.955, 0.575), FVector2D(0.985, 0.555), FVector2D(1.0, 0.50) };
			D.Win = { FVector2D(0.445, 0.655), FVector2D(0.215, 0.425), FVector2D(0.07, 0.195) }; D.Gaps = { 0.675f, 0.435f, 0.205f };
			D.Belt = 0.635f; D.CabIn = 0.93f; D.RoofIn = 0.90f; D.Crown = 1.2f; D.RoofCrown = 1.5f; D.Round = 2.4f; D.SmoothDeg = 20.f; D.TaperF = 0.02f; D.TaperR = 0.f; break;
		case 1: // Urus: morro bajo, parabrisas muy inclinado, techo de coupé y cola alta
			D.Top = { FVector2D(0, 0.74), FVector2D(0.015, 0.80), FVector2D(0.05, 0.84), FVector2D(0.11, 0.875), FVector2D(0.19, 0.93), FVector2D(0.27, 0.975), FVector2D(0.34, 1.0), FVector2D(0.47, 1.0), FVector2D(0.55, 0.985), FVector2D(0.61, 0.93), FVector2D(0.665, 0.80), FVector2D(0.715, 0.655), FVector2D(0.745, 0.605), FVector2D(0.80, 0.585), FVector2D(0.90, 0.53), FVector2D(0.96, 0.45), FVector2D(0.99, 0.39), FVector2D(1.0, 0.35) };
			D.BeltPts = { FVector2D(0, 0.74), FVector2D(0.30, 0.69), FVector2D(0.70, 0.61), FVector2D(1.0, 0.53) };
			D.Win = { FVector2D(0.46, 0.665), FVector2D(0.30, 0.445), FVector2D(0.205, 0.285) }; D.Gaps = { 0.675f, 0.455f, 0.29f };
			D.Belt = 0.74f; D.CabIn = 0.88f; D.RoofIn = 0.70f; D.Crown = 5.f; D.RoofCrown = 5.f; D.Round = 7.f; D.SmoothDeg = 70.f; D.TaperF = 0.10f; D.TaperR = 0.07f; D.bSmoothOutline = true; break;
		case 2: // Panda 4x4: alto, corto y cuadrado-redondeado
			D.Top = { FVector2D(0, 0.62), FVector2D(0.012, 0.90), FVector2D(0.05, 1.0), FVector2D(0.62, 1.0), FVector2D(0.665, 0.975), FVector2D(0.765, 0.64), FVector2D(0.79, 0.605), FVector2D(0.84, 0.595), FVector2D(0.965, 0.58), FVector2D(0.992, 0.55), FVector2D(1.0, 0.44) };
			D.Win = { FVector2D(0.455, 0.745), FVector2D(0.195, 0.435), FVector2D(0.115, 0.18) }; D.Gaps = { 0.75f, 0.445f, 0.19f };
			D.Belt = 0.62f; D.CabIn = 0.90f; D.RoofIn = 0.84f; D.Crown = 3.f; D.RoofCrown = 3.f; D.Round = 7.f; D.SmoothDeg = 45.f; D.TaperF = 0.04f; D.TaperR = 0.03f; break;
		case 3: // Defender clásico
			D.Top = { FVector2D(0, 0.92), FVector2D(0.008, 0.99), FVector2D(0.02, 1.0), FVector2D(0.685, 1.0), FVector2D(0.695, 0.99), FVector2D(0.722, 0.635), FVector2D(0.734, 0.595), FVector2D(0.765, 0.585), FVector2D(0.988, 0.58), FVector2D(0.996, 0.565), FVector2D(1.0, 0.50) };
			D.Win = { FVector2D(0.455, 0.665), FVector2D(0.265, 0.435), FVector2D(0.075, 0.215) }; D.Gaps = { 0.67f, 0.445f, 0.235f };
			D.Belt = 0.62f; D.CabIn = 0.97f; D.RoofIn = 0.95f; D.Crown = 0.f; D.RoofCrown = 0.4f; D.Round = 1.6f; D.SmoothDeg = 18.f; D.TaperF = 0.01f; D.TaperR = 0.f; break;
		case 5: // Pick-up
			D.Top = { FVector2D(0, 0.54), FVector2D(0.01, 0.56), FVector2D(0.335, 0.56), FVector2D(0.35, 0.62), FVector2D(0.358, 0.96), FVector2D(0.385, 1.0), FVector2D(0.64, 1.0), FVector2D(0.665, 0.985), FVector2D(0.74, 0.62), FVector2D(0.76, 0.595), FVector2D(0.80, 0.585), FVector2D(0.965, 0.575), FVector2D(0.99, 0.55), FVector2D(1.0, 0.46) };
			D.Win = { FVector2D(0.50, 0.655), FVector2D(0.385, 0.485) }; D.Gaps = { 0.67f, 0.49f, 0.37f };
			D.Belt = 0.60f; D.CabIn = 0.90f; D.RoofIn = 0.82f; D.Crown = 2.f; D.RoofCrown = 2.f; D.Round = 4.f; D.SmoothDeg = 35.f; D.TaperF = 0.03f; D.TaperR = 0.f; break;
		default: // 6 Crawler (tina abierta con jaula)
			D.Top = { FVector2D(0, 0.50), FVector2D(0.46, 0.50), FVector2D(0.52, 0.52), FVector2D(0.62, 0.56), FVector2D(0.65, 0.57), FVector2D(0.90, 0.56), FVector2D(0.98, 0.54), FVector2D(1.0, 0.44) };
			D.Belt = 0.60f; D.Crown = 1.f; D.Round = 3.f; D.SmoothDeg = 28.f; D.TaperF = 0.02f; D.TaperR = 0.f; break;
		}
		return D;
	}

	float TopV(const TArray<FVector2D>& P, float u)
	{
		if (u <= P[0].X) return P[0].Y;
		for (int32 i = 0; i < P.Num() - 1; ++i) if (u <= P[i + 1].X) { const float d = P[i + 1].X - P[i].X; return d < 1e-6f ? P[i + 1].Y : FMath::Lerp((float)P[i].Y, (float)P[i + 1].Y, (u - P[i].X) / d); }
		return P.Last().Y;
	}

	// banda en arco alrededor de una rueda (aletines): eje Y, arco en el plano XZ
	void ArcBand(FDustlineMeshBuilder& B, int32 Id, float cx, float cz, float side, float rIn, float rOut, float y0, float y1, float a0, float a1, int32 Segs)
	{
		for (int32 s = 0; s < Segs; ++s)
		{
			const float aa = a0 + (a1 - a0) * s / Segs, ab = a0 + (a1 - a0) * (s + 1) / Segs;
			const FVector da(FMath::Cos(aa), 0, FMath::Sin(aa)), db(FMath::Cos(ab), 0, FMath::Sin(ab));
			auto P = [&](const FVector& d, float r, float y) { return FVector(cx + d.X * r, side * y, cz + d.Z * r); };
			B.QuadN(Id, P(da, rOut, y0), P(db, rOut, y0), P(db, rOut, y1), P(da, rOut, y1), da, db, db, da);
			B.QuadN(Id, P(da, rIn, y0), P(da, rIn, y1), P(db, rIn, y1), P(db, rIn, y0), -da, -da, -db, -db);
			const FVector Ns(0, side, 0);
			B.QuadN(Id, P(da, rIn, y1), P(da, rOut, y1), P(db, rOut, y1), P(db, rIn, y1), Ns, Ns, Ns, Ns);
		}
	}
}

void ADustlineCar::BuildBody()
{
	if (bKen)
	{
		BuildBodyKen(); return;
	}
	KBody->SetStaticMesh(nullptr);
	FDustlineMeshBuilder B;
	const int32 St = GetModel(Setup.Model).Style;
	if (St == 4) BuildBuggyBody(B); else BuildShellV2(B);
	B.Build(Body, Mats, false);
}

void ADustlineCar::BuildShell(FDustlineMeshBuilder& B)
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const FBodyDef D = MakeDef(M.Style);
	const int32 St = M.Style;
	const float L = M.L, Ht = BodyH(), gc = GroundClearance(), R = Setup.TireRadiusM * 100.f, Wd = Setup.TireWidthM * 100.f, Hh = Ht - gc;
	float w0 = M.W * 0.5f;
	const float wyAbs = FMath::Abs(WheelPos(0).Y);
	if (2.f * R + 7.f > gc + 0.58f * Hh - 6.f) w0 = FMath::Min(w0, wyAbs - Wd * 0.5f - 3.f);   // neumáticos altos: la carrocería queda entre ellos
	const float wxF = WheelPos(0).X, wxR = WheelPos(2).X, Ra = R + 7.f;
	const float Rnd = D.Round, yi = FMath::Max(10.f, FMath::Min(w0 - 14.f, wyAbs - Wd * 0.5f - 2.f));

	// ---- perfil superior
	TArray<FVector2D> Top = D.Top;
	if (D.bSmoothOutline)
	{
		TArray<FVector> C; for (const FVector2D& p : D.Top) C.Add(FVector(p.X, p.Y, 0));
		const TArray<FVector> S = FDustlineMeshBuilder::Smooth(C, 8);
		Top.Reset(); for (const FVector& v : S) Top.Add(FVector2D(v.X, v.Y));
	}
	auto ZTopAt = [&](float u) { return gc + TopV(Top, u) * Hh; };
	const float zBeltMax = gc + D.Belt * Hh;
	auto BeltAt = [&](float u) { return gc + (D.BeltPts.Num() ? TopV(D.BeltPts, u) : D.Belt) * Hh; };
	auto WAt = [&](float u) { return w0 * (1.f - D.TaperF * Sm((u - 0.88f) / 0.12f) - D.TaperR * Sm((0.12f - u) / 0.12f)); };

	// ---- cortes en u
	TArray<float> Us;
	auto AddU = [&](float u) { Us.Add(FMath::Clamp(u, 0.f, 1.f)); };
	for (int32 i = 0; i <= 80; ++i) AddU(i / 80.f);
	for (const FVector2D& p : Top) AddU(p.X);
	for (const FVector2D& wv : D.Win) { AddU(wv.X); AddU(wv.Y); }
	for (float g : D.Gaps) { AddU(g - 0.004f); AddU(g + 0.004f); }
	for (float wx : { wxF, wxR }) for (int32 s = 0; s <= 14; ++s) AddU(wx / L + 0.5f - Ra / L + 2.f * Ra / L * s / 14.f);
	Us.Sort();
	{ TArray<float> U2; for (float u : Us) if (U2.Num() == 0 || u - U2.Last() > 0.0007f) U2.Add(u); Us = U2; }
	const int32 NS = Us.Num() - 1;
	constexpr int32 K = 12, NR = 2 * K - 2;

	TArray<TArray<FVector>> P; P.SetNum(NS + 1);
	TArray<float> Kc, ZT; Kc.SetNum(NS + 1); ZT.SetNum(NS + 1);
	for (int32 i = 0; i <= NS; ++i)
	{
		const float u = Us[i], x = (u - 0.5f) * L, w = WAt(u);
		const float zTop = ZTopAt(u), zbelt = FMath::Min(zTop, BeltAt(u)), zb = gc;
		const float hc = zTop - zbelt, k = FMath::Clamp(hc / 12.f, 0.f, 1.f);
		Kc[i] = k; ZT[i] = zTop;
		float za = zb;
		for (float wx : { wxF, wxR }) { const float dx = x - wx; if (FMath::Abs(dx) < Ra) za = FMath::Max(za, R + FMath::Sqrt(Ra * Ra - dx * dx)); }
		const float zs = zbelt - FMath::Min(8.f, 0.15f * (zbelt - zb));
		const float zw = FMath::Clamp(za, zb, zs - 5.f);
		const float c1 = 3.f;
		FVector2D h[K];
		h[0] = FVector2D(0, zb); h[1] = FVector2D(yi, zb); h[2] = FVector2D(yi, zw); h[3] = FVector2D(w - c1, zw); h[4] = FVector2D(w, zw + c1);
		h[5] = FVector2D(w, zs); h[6] = FVector2D(w - Rnd, zbelt);
		const float zr = zTop, ct = w * D.RoofIn, cb = w * D.CabIn, ra = 5.f;
		const FVector2D hood[5] = { FVector2D(w * 0.86f, zTop + 0.3f * D.Crown), FVector2D(w * 0.64f, zTop + 0.7f * D.Crown), FVector2D(w * 0.38f, zTop + 0.93f * D.Crown), FVector2D(w * 0.16f, zTop + 0.99f * D.Crown), FVector2D(0, zTop + D.Crown) };
		const FVector2D cab[5] = { FVector2D(cb, zbelt), FVector2D(ct, zr - ra), FVector2D(ct * 0.88f, zr), FVector2D(ct * 0.48f, zr + 0.7f * D.RoofCrown), FVector2D(0, zr + D.RoofCrown) };
		for (int32 q = 0; q < 5; ++q) h[7 + q] = FMath::Lerp(hood[q], cab[q], (double)k);
		if (k < 1.f) h[6].Y = FMath::Lerp((float)zTop, zbelt, k) ;   // el hombro sigue al capó
		P[i].SetNum(NR);
		for (int32 j = 0; j < K; ++j) P[i][j] = FVector(x, h[j].X, h[j].Y);
		for (int32 j = K; j < NR; ++j) { const int32 src = 2 * K - 2 - j; P[i][j] = FVector(x, -h[src].X, h[src].Y); }
	}

	// ---- material por segmento
	auto SegOf = [&](int32 j) { return j <= 10 ? j : (j == 11 ? 10 : 21 - j); };
	auto InWin = [&](float u) { for (const FVector2D& wv : D.Win) if (u >= wv.X - 1e-4f && u <= wv.Y + 1e-4f) return true; return false; };
	auto MatOf = [&](int32 i, int32 j)
	{
		const int32 sg = SegOf(j);
		const float um = 0.5f * (Us[i] + Us[i + 1]), kmin = FMath::Min(Kc[i], Kc[i + 1]), kmax = FMath::Max(Kc[i], Kc[i + 1]);
		const float slope = FMath::Abs(ZT[i + 1] - ZT[i]) / FMath::Max(0.5f, (Us[i + 1] - Us[i]) * L);
		if (sg <= 3) return (int32)SDark;
		if (sg == 7 && kmin > 0.55f && InWin(um)) return (int32)SGlass;
		if ((sg == 9 || sg == 10) && kmax > 0.15f && slope > 0.30f) return (int32)SGlass;
		return (int32)SPaint;
	};

	// ---- normales (suavizado por ángulo)
	TArray<FVector> FN; FN.SetNum(NS * NR);
	for (int32 i = 0; i < NS; ++i) for (int32 j = 0; j < NR; ++j)
	{
		const int32 j2 = (j + 1) % NR;
		FN[i * NR + j] = FVector::CrossProduct(P[i + 1][j2] - P[i][j], P[i + 1][j] - P[i][j2]).GetSafeNormal();
	}
	const float CosT = FMath::Cos(FMath::DegreesToRadians(D.SmoothDeg));
	auto VN = [&](int32 i, int32 j, int32 di, int32 dj) -> FVector
	{
		const FVector Own = FN[i * NR + j];
		FVector Sum = Own;
		for (int32 a = -1; a <= 0; ++a) for (int32 b = -1; b <= 0; ++b)
		{
			const int32 ii = i + di + a, jj = ((j + dj + b) % NR + NR) % NR;
			if (ii < 0 || ii >= NS || (ii == i && jj == j)) continue;
			const FVector& O = FN[ii * NR + jj];
			if (!O.IsNearlyZero() && FVector::DotProduct(O, Own) > CosT) Sum += O;
		}
		return Sum.GetSafeNormal();
	};
	for (int32 i = 0; i < NS; ++i) for (int32 j = 0; j < NR; ++j)
	{
		const int32 j2 = (j + 1) % NR;
		if (FN[i * NR + j].IsNearlyZero()) continue;
		B.QuadN(MatOf(i, j), P[i][j], P[i][j2], P[i + 1][j2], P[i + 1][j], VN(i, j, 0, 0), VN(i, j, 0, 1), VN(i, j, 1, 1), VN(i, j, 1, 0));
	}
	// tapas delantera/trasera
	for (int32 e = 0; e < 2; ++e)
	{
		const int32 i = e == 0 ? 0 : NS; const FVector N(e == 0 ? -1.f : 1.f, 0, 0);
		FVector C(0); for (const FVector& v : P[i]) C += v; C /= NR;
		for (int32 j = 0; j < NR; ++j) B.Tri(SPaint, C, P[i][j], P[i][(j + 1) % NR], N, N, N, FVector2D(.5, .5), FVector2D(0, 0), FVector2D(1, 0));
	}

	// ---------------------------------------------------------------- detalles
	const float xF = 0.5f * L, xB = -0.5f * L;
	auto WA = [&](float u) { return WAt(u); };
	auto ZB = [&](float u) { return FMath::Min(ZTopAt(u), BeltAt(u)); };
	const float wF = WA(1.f), wB = WA(0.f), zTopF = ZTopAt(1.f), zTopB = ZTopAt(0.f);
	const bool bOpen = (St == 6);
	const FTransform Keep0 = B.Xf;

	// aletines de los pasos de rueda y revestimiento
	if (!bOpen)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f;
			if (i == 1 || i == 3) { /* el otro lado se genera aparte */ }
			const float w = WA(WP.X / L + 0.5f);
			ArcBand(B, SPaint, WP.X, R, side, Ra, Ra + (St == 1 ? 3.5f : 5.5f), w - 0.6f, w + 7.2f, -0.30f, PI + 0.30f, 20);
		}
	}
	// umbral lateral (plástico) entre ruedas
	if (!bOpen) for (int32 s = -1; s <= 1; s += 2)
	{
		const float xa = wxR + Ra + 3.f, xb = wxF - Ra - 3.f;
		B.SuperBox(SDark, FVector(0.5f * (xa + xb), s * (WA(0.5f) + 0.8f), gc + 8.f), FVector(0.5f * (xb - xa), 2.2f, 5.5f), 0.35f, 6);
		if (St == 0 || St == 5 || St == 3) B.SuperBox(SMetal, FVector(0.5f * (xa + xb), s * (WA(0.5f) + 6.f), gc + 2.f), FVector(0.5f * (xb - xa) - 8.f, 6.5f, 1.4f), 0.3f, 6);   // estribos
	}
	// juntas y manillas de puertas
	if (!bOpen)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			for (float g : D.Gaps)
			{
				const float zb0 = gc + 10.f, zb1 = ZB(g) - 1.5f; if (zb1 <= zb0) continue;
				B.Box(SDark, FVector((g - 0.5f) * L, s * (WA(g) + 0.15f), 0.5f * (zb0 + zb1)), FVector(0.28f, 0.28f, 0.5f * (zb1 - zb0)));
			}
			for (int32 gi = 1; gi < D.Gaps.Num(); ++gi)
			{
				const float u = D.Gaps[gi] + 0.045f;
				B.SuperBox(SChrome, FVector((u - 0.5f) * L, s * (WA(u) + 0.9f), ZB(u) - 9.f), FVector(6.f, 1.0f, 1.5f), 0.3f, 6);
			}
		}
	}
	// retrovisores
	if (!bOpen) { const float ub = St == 0 ? 0.745f : St == 1 ? 0.735f : St == 2 ? 0.70f : St == 3 ? 0.725f : 0.74f;
		for (int32 s = -1; s <= 1; s += 2)
		{
			const float xm = (ub - 0.5f) * L, ym = s * (WA(ub) + 11.f), zm = zBeltMax + 5.f;
			B.Box(SDark, FVector(xm, s * (WA(ub) + 5.f), zm - 2.f), FVector(2.f, 6.f, 1.2f));
			B.SuperBox(St == 1 ? SCarbon : SDark, FVector(xm - 2.f, ym, zm), FVector(4.0f, 7.5f, 6.0f), 0.45f, 8);
			B.SuperBox(SChrome, FVector(xm - 5.8f, ym, zm), FVector(0.4f, 6.2f, 4.8f), 0.4f, 6);
		}
	}

	// ---- frontal y trasera por modelo
	auto Plate = [&](float x, float z, float dir) { B.Box(SAlu, FVector(x + dir * 0.4f, 0, z), FVector(0.5f, 26.f, 6.5f)); };
	auto Tail = [&](float y, float z, float hy, float hz) { B.SuperBox(STail, FVector(xB - 0.8f, y, z), FVector(2.0f, hy, hz), 0.35f, 6); B.SuperBox(STail, FVector(xB - 0.8f, -y, z), FVector(2.0f, hy, hz), 0.35f, 6); };
	auto Round = [&](float y, float z, float r) { for (int32 s = -1; s <= 1; s += 2) {
		B.Torus(SChrome, FVector(xF + 0.2f, s * y, z), FVector(1, 0, 0), r, 1.1f, 20, 6);
		B.SuperBox(SLight, FVector(xF - 1.5f, s * y, z), FVector(3.2f, r - 0.4f, r - 0.4f), 1.0f, 8);
		B.Cylinder(SDark, FVector(xF - 5.f, s * y, z), FVector(1, 0, 0), r + 1.5f, 5.f, 18, true); } };
	switch (St)
	{
	case 0: // Clase G
	{
		B.SuperBox(SDark, FVector(xF - 5.f, 0, gc + 15.f), FVector(8.f, wF * 0.98f, 14.f), 0.28f, 8);                 // parachoques
		B.SuperBox(SMetal, FVector(xF - 5.f, 0, gc + 3.f), FVector(8.f, wF * 0.55f, 2.5f), 0.3f, 6);                  // protector
		B.SuperBox(SDark, FVector(xB + 5.f, 0, gc + 15.f), FVector(8.f, wB * 0.98f, 14.f), 0.28f, 8);
		B.Box(SDark, FVector(xF + 0.4f, 0, zTopF - 19.f), FVector(1.f, wF * 0.40f, 12.5f));                         // parrilla
		for (int32 k = -2; k <= 2; ++k) B.Box(SChrome, FVector(xF + 0.9f, 0, zTopF - 19.f + k * 4.6f), FVector(0.5f, wF * 0.38f, 0.8f));
		B.SuperBox(SChrome, FVector(xF + 1.2f, 0, zTopF - 19.f), FVector(0.7f, 5.f, 5.f), 0.6f, 8);                    // estrella
		for (int32 s = -1; s <= 1; s += 2)
		{
			B.SuperBox(SDark, FVector(xF - 0.6f, s * wF * 0.72f, zTopF - 3.f), FVector(1.6f, 12.f, 12.f), 0.25f, 6);       // embellecedor cuadrado
			B.SuperBox(SDark, FVector(xF - 14.f, s * wF * 0.82f, zTopF + 2.4f), FVector(5.f, 3.2f, 1.8f), 0.4f, 6);         // intermitente sobre la aleta
		}
		Round(wF * 0.72f, zTopF - 3.f, 9.f);
		B.SuperBox(SDark, FVector(xF - 2.f, 0, gc + 36.f), FVector(1.2f, wF * 0.62f, 3.f), 0.35f, 6);                  // entrada de aire
		for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SAccent, FVector(xF - 1.f, s * wF * 0.45f, gc + 26.f), FVector(1.f, 4.f, 4.f), 0.8f, 6); B.SuperBox(SLight, FVector(xF - 0.3f, s * wF * 0.88f, zTopF - 4.f), FVector(1.2f, 3.2f, 3.2f), 0.7f, 6); }
		Tail(wB * 0.86f, zTopB - 30.f, 5.f, 12.f); Plate(xB - 2.f, gc + 28.f, -1.f);
		B.Box(SGlass, FVector(xB - 0.35f, 0, 0.5f * (zTopB - 10.f + zBeltMax + 8.f)), FVector(0.3f, wB * 0.66f, 0.5f * (zTopB - 10.f - zBeltMax - 8.f)));   // luneta
		for (int32 s = -1; s <= 1; s += 2) { B.Tube(SMetal, { FVector(-0.40f * L, s * (w0 * 0.84f), Ht + 1.f), FVector(0.30f * L, s * (w0 * 0.84f), Ht + 1.f) }, 1.1f, 6); }                // barras de techo
		{ const FTransform Keep = B.Xf; B.Xf = FTransform(FRotator(0, 90, 0), FVector(xB - 13.f, 0, gc + 0.52f * Hh)) * Keep; DustlineAddWheel(B, FMath::Min(R, 40.f), FMath::Min(Wd, 26.f), 1, 1.f, false); B.Xf = Keep; }  // rueda de repuesto
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SChrome, { FVector(xF - 20.f, s * (wF * 0.62f), zTopF + 2.f), FVector(xF - 40.f, s * wF * 0.62f, zTopF + 22.f) }, 0.4f, 4);   // intermitentes (varilla)
		break;
	}
	case 1: // Urus
	{
		B.SuperBox(SDark, FVector(xF - 6.f, 0, gc + 13.f), FVector(8.f, wF * 0.94f, 12.5f), 0.4f, 10);                 // faldón negro
		B.SuperBox(SCarbon, FVector(xF - 4.f, 0, gc + 5.f), FVector(8.f, wF * 0.86f, 2.6f), 0.4f, 8);                    // splitter
		B.SuperBox(SDark, FVector(xF - 1.5f, 0, gc + 22.f), FVector(1.6f, wF * 0.56f, 9.5f), 0.3f, 10);                 // gran boca central
		B.SuperBox(SMetal, FVector(xF - 0.6f, 0, gc + 31.8f), FVector(1.0f, wF * 0.52f, 0.9f), 0.3f, 6);
		for (int32 k = -3; k <= 3; ++k) B.Box(SMetal, FVector(xF - 0.4f, k * wF * 0.11f, gc + 22.f), FVector(0.35f, 0.45f, 8.6f));
		for (int32 s = -1; s <= 1; s += 2)
		{
			B.SuperBox(SDark, FVector(xF - 2.5f, s * wF * 0.80f, gc + 20.f), FVector(1.6f, 14.f, 8.5f), 0.3f, 8);   // entradas laterales
			for (int32 q = 0; q < 3; ++q) B.Box(SMetal, FVector(xF - 0.9f, s * wF * 0.80f, gc + 15.f + q * 5.f), FVector(0.4f, 12.5f, 0.55f));
			B.Xf = FTransform(FRotator(0.f, s * -10.f, 0.f), FVector(xF - 2.5f, s * wF * 0.67f, zTopF - 4.5f)) * Keep0;       // faros afilados
			B.SuperBox(SDark, FVector(0.f), FVector(2.2f, 19.f, 3.0f), 0.4f, 8);
			B.SuperBox(SLight, FVector(0.8f, 0.f, 0.3f), FVector(2.0f, 17.f, 1.5f), 0.5f, 8);
			B.SuperBox(SLight, FVector(0.8f, s * 6.f, -2.4f), FVector(1.6f, 8.f, 0.6f), 0.5f, 6);
			B.Xf = Keep0;
		}
		B.SuperBox(SDark, FVector(xB + 6.f, 0, gc + 15.f), FVector(8.f, wB * 0.94f, 13.f), 0.4f, 10);
		B.SuperBox(SCarbon, FVector(xB + 2.f, 0, gc + 5.f), FVector(8.f, wB * 0.6f, 3.5f), 0.4f, 8);                    // difusor
		for (int32 s = -1; s <= 1; s += 2) for (int32 q = 0; q < 2; ++q) B.Cylinder(SChrome, FVector(xB - 1.f, s * (wB * 0.34f + q * 11.f), gc + 7.f), FVector(-1, 0, 0), 4.2f, 5.f, 14, true);   // 4 escapes
		B.SuperBox(STail, FVector(xB + 1.2f, 0, 0.5f * (zTopB + zBeltMax) - 2.f), FVector(1.f, wB * 0.84f, 2.2f), 0.5f, 6);    // piloto corrido
		Plate(xB - 0.5f, gc + 30.f, -1.f);
		B.SuperBox(SCarbon, FVector(xB + 6.f, 0, zTopB + 1.f), FVector(8.f, wB * 0.78f, 1.4f), 0.4f, 6);                // alerón
		break;
	}
	case 2: // Panda 4x4
	{
		B.SuperBox(SDark, FVector(xF - 3.f, 0, gc + 14.f), FVector(6.f, wF * 0.97f, 12.f), 0.3f, 8);
		B.SuperBox(SDark, FVector(xB + 3.f, 0, gc + 14.f), FVector(6.f, wB * 0.97f, 12.f), 0.3f, 8);
		B.SuperBox(SDark, FVector(xF + 0.2f, 0, zTopF - 14.f), FVector(1.2f, wF * 0.34f, 9.f), 0.3f, 6);                // parrilla
		for (int32 k = -1; k <= 1; ++k) B.Box(SChrome, FVector(xF + 0.9f, k * 12.f, zTopF - 14.f), FVector(0.4f, 0.8f, 8.f));
		for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SLight, FVector(xF - 0.5f, s * wF * 0.66f, zTopF - 12.f), FVector(2.2f, 17.f, 8.f), 0.5f, 8); B.SuperBox(SAccent, FVector(xF - 0.5f, s * wF * 0.40f, gc + 22.f), FVector(1.2f, 4.f, 4.f), 0.8f, 6); }
		Tail(wB * 0.86f, zTopB - 24.f, 4.5f, 17.f); Plate(xB - 0.5f, gc + 25.f, -1.f);
		B.Box(SGlass, FVector(xB - 0.35f + 4.f, 0, 0.5f * (ZTopAt(0.03f) - 8.f + zBeltMax + 8.f)), FVector(0.3f, wB * 0.70f, 0.5f * (ZTopAt(0.03f) - 8.f - zBeltMax - 8.f)));
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SMetal, { FVector(-0.40f * L, s * (w0 * 0.82f), Ht + 0.5f), FVector(0.22f * L, s * (w0 * 0.82f), Ht + 0.5f) }, 1.2f, 6);
		break;
	}
	case 3: // Defender
	{
		B.Box(SMetal, FVector(xF - 1.f, 0, gc + 12.f), FVector(9.f, wF * 1.0f, 8.5f));                                  // parachoques de acero (sobresale)
		B.Box(SMetal, FVector(xB + 6.f, 0, gc + 12.f), FVector(8.f, wB * 0.99f, 8.f));
		B.Box(SDark, FVector(xF + 0.3f, 0, zTopF - 15.f), FVector(1.f, wF * 0.30f, 11.f));                              // parrilla estrecha
		for (int32 k = -3; k <= 3; ++k) B.Box(SMetal, FVector(xF + 0.8f, 0, zTopF - 15.f + k * 3.2f), FVector(0.4f, wF * 0.28f, 0.45f));
		for (int32 s = -1; s <= 1; s += 2)
		{
			B.SuperBox(SDark, FVector(xF - 0.2f, s * wF * 0.68f, zTopF - 14.f), FVector(1.4f, 12.5f, 12.5f), 0.25f, 6);   // embellecedor cuadrado
			B.Torus(SChrome, FVector(xF + 1.2f, s * wF * 0.68f, zTopF - 14.f), FVector(1, 0, 0), 8.8f, 1.0f, 20, 6);
			B.SuperBox(SLight, FVector(xF - 0.6f, s * wF * 0.68f, zTopF - 14.f), FVector(3.f, 8.f, 8.f), 1.0f, 8);
			B.SuperBox(SAccent, FVector(xF + 0.3f, s * wF * 0.68f, zTopF - 27.f), FVector(1.f, 3.8f, 2.4f), 0.5f, 6);
			B.Box(SMetal, FVector(xF - 18.f, s * wF * 0.60f, zTopF + 0.7f), FVector(13.f, wF * 0.30f, 0.3f));              // chapa estriada en las aletas
		}
		for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SAccent, FVector(xF - 1.f, s * wF * 0.85f, zTopF - 22.f), FVector(1.2f, 3.2f, 3.2f), 0.8f, 6); B.SuperBox(SDark, FVector(xF - 2.f, s * wF * 0.80f, gc + 14.f), FVector(2.f, 5.f, 4.f), 0.4f, 6); }
		Tail(wB * 0.86f, zTopB - 34.f, 4.5f, 10.f); Plate(xB - 2.f, gc + 30.f, -1.f);
		B.Box(SGlass, FVector(xB - 0.35f, 0, 0.5f * (zTopB - 10.f + zBeltMax + 8.f)), FVector(0.3f, wB * 0.70f, 0.5f * (zTopB - 10.f - zBeltMax - 8.f)));
		for (int32 s = -1; s <= 1; s += 2) { B.Box(SMetal, FVector(0.0f, s * (w0 * 0.93f), Ht - 1.f), FVector(0.34f * L, 1.4f, 1.2f)); }                      // canalones
		for (int32 r = 0; r < 4; ++r) B.Tube(SMetal, { FVector(-0.30f * L + r * 0.16f * L, -w0 * 0.78f, Ht + 5.f), FVector(-0.30f * L + r * 0.16f * L, w0 * 0.78f, Ht + 5.f) }, 1.1f, 6);   // baca
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SMetal, { FVector(-0.30f * L, s * w0 * 0.78f, Ht + 5.f), FVector(0.18f * L, s * w0 * 0.78f, Ht + 5.f) }, 1.1f, 6);
		{ const FTransform Keep = B.Xf; B.Xf = FTransform(FRotator(0, 90, 0), FVector(xB - 13.f, 0, gc + 0.50f * Hh)) * Keep; DustlineAddWheel(B, FMath::Min(R, 42.f), FMath::Min(Wd, 26.f), 1, 1.f, false); B.Xf = Keep; }
		B.Tube(SMetal, { FVector(0.20f * L, w0 * 0.88f, zBeltMax + 5.f), FVector(0.20f * L + 3.f, w0 * 0.88f, Ht + 14.f), FVector(0.14f * L, w0 * 0.88f, Ht + 20.f) }, 2.4f, 8);   // snorkel
		break;
	}
	case 5: // Pick-up
	{
		B.SuperBox(SChrome, FVector(xF - 5.f, 0, gc + 14.f), FVector(8.f, wF * 0.98f, 10.f), 0.3f, 8);
		B.SuperBox(SDark, FVector(xF - 5.f, 0, gc + 5.f), FVector(8.f, wF * 0.50f, 4.f), 0.35f, 6);
		B.SuperBox(SChrome, FVector(xB + 5.f, 0, gc + 14.f), FVector(8.f, wB * 0.98f, 10.f), 0.3f, 8);
		B.SuperBox(SDark, FVector(xF + 0.2f, 0, zTopF - 16.f), FVector(1.5f, wF * 0.56f, 13.f), 0.3f, 8);              // gran parrilla
		for (int32 k = -2; k <= 2; ++k) B.Box(SChrome, FVector(xF + 1.2f, 0, zTopF - 16.f + k * 4.8f), FVector(0.5f, wF * 0.54f, 1.0f));
		for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SLight, FVector(xF - 1.f, s * wF * 0.78f, zTopF - 12.f), FVector(2.f, 14.f, 7.f), 0.4f, 8); }
		const float ub = 0.0f; (void)ub;
		Tail(wB * 0.86f, zTopB - 12.f, 3.5f, 9.f); Plate(xB - 1.f, zTopB - 16.f, -1.f);
		const float xa = xB + 4.f, xb = (0.34f - 0.5f) * L, zf = ZTopAt(0.15f);
		B.Box(SInterior, FVector(0.5f * (xa + xb), 0, zf + 0.5f), FVector(0.5f * (xb - xa), WA(0.15f) - 8.f, 0.4f));    // caja
		for (int32 s = -1; s <= 1; s += 2) B.SuperBox(SPaint, FVector(0.5f * (xa + xb), s * (WA(0.15f) - 3.f), zf + 2.5f), FVector(0.5f * (xb - xa), 3.2f, 3.5f), 0.4f, 6);
		B.SuperBox(SPaint, FVector(xa, 0, zf + 2.5f), FVector(3.2f, WA(0.15f) - 3.f, 3.5f), 0.4f, 6);
		B.Tube(SDark, { FVector(xb - 4.f, -w0 * 0.7f, zf + 4.f), FVector(xb - 4.f, -w0 * 0.7f, zf + 38.f), FVector(xb - 4.f, w0 * 0.7f, zf + 38.f), FVector(xb - 4.f, w0 * 0.7f, zf + 4.f) }, 1.8f, 8);  // arco antivuelco
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SMetal, { FVector(xF - 15.f, s * wF * 0.72f, gc + 14.f), FVector(xF - 20.f, s * wF * 0.72f, gc + 60.f) }, 1.6f, 6);   // defensa
		break;
	}
	default: break;
	}
	// pasos de rueda ensanchados (plástico negro) y rasgos propios por modelo
	if (!bOpen)
	{
		const float Flare = St == 0 ? 10.f : (St == 2 ? 7.f : (St == 3 ? 7.f : (St == 5 ? 9.f : (St == 1 ? 8.f : 0.f))));
		if (Flare > 0.f) for (int32 i = 0; i < 4; ++i)
		{
			const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f, w = WA(WP.X / L + 0.5f);
			ArcBand(B, SDark, WP.X, R, side, Ra + 5.6f, Ra + 8.2f, w - 0.6f, w + Flare, -0.28f, PI + 0.28f, 22);
		}
		if (St == 3) for (int32 s = -1; s <= 1; s += 2)   // tragaluces alpinos del techo
		{
			B.Xf = FTransform(FRotator(0.f, 0.f, s * 24.f), FVector(-0.24f * L, s * w0 * 0.74f, Ht - 5.f)) * B.Xf;
			B.SuperBox(SGlass, FVector(0.f), FVector(0.13f * L, 7.f, 1.1f), 0.4f, 6); B.Xf = Keep0;
		}
		if (St == 5) B.SuperBox(SDark, FVector((0.865f - 0.5f) * L, 0.f, ZTopAt(0.865f) + 1.6f), FVector(17.f, 13.f, 2.4f), 0.4f, 8);   // toma del capó
		if (St == 2) for (int32 s = -1; s <= 1; s += 2) B.SuperBox(SDark, FVector(0.f, s * (w0 * 0.995f), gc + 24.f), FVector(0.33f * L, 1.2f, 11.f), 0.3f, 6);   // moldura lateral
	}
	if (St == 6) { /* Crawler: jaula y accesorios en BuildBuggyBody-like extras */
		const float zt = zBeltMax, hw = w0 - 6.f;
		B.SuperBox(SMetal, FVector(xF - 6.f, 0, gc + 10.f), FVector(8.f, wF * 0.98f, 8.f), 0.3f, 8);                       // parachoques con cabrestante
		B.SuperBox(SDark, FVector(xF - 12.f, 0, gc + 24.f), FVector(8.f, 18.f, 8.f), 0.4f, 8);
		B.Cylinder(SMetal, FVector(xF - 18.f, -12.f, gc + 24.f), FVector(0, 1, 0), 5.f, 24.f, 14, true);
		B.SuperBox(SMetal, FVector(xB + 6.f, 0, gc + 12.f), FVector(8.f, wB * 0.98f, 8.f), 0.3f, 8);
		Round(wF * 0.62f, zTopF + 4.f, 8.f);
		B.SuperBox(SDark, FVector(xF - 0.8f, 0, zTopF - 8.f), FVector(1.4f, wF * 0.31f, 10.f), 0.3f, 6);
		for (int32 k = -3; k <= 3; ++k) B.SuperBox(SPaint, FVector(xF + 0.4f, k * wF * 0.082f, zTopF - 8.f), FVector(0.8f, wF * 0.030f, 8.6f), 0.4f, 6);   // 7 ranuras
		for (int32 i = 0; i < 4; ++i) { const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f; ArcBand(B, SDark, WP.X, R, side, R + 6.f, R + 9.f, FMath::Abs(WP.Y) - Wd * 0.5f - 8.f, FMath::Abs(WP.Y) + Wd * 0.5f + 8.f, 0.f, PI, 22); }   // aletas anchas
		const float xw0 = (0.60f - 0.5f) * L, xw1 = (0.52f - 0.5f) * L, zw1 = zt + 54.f;   // marco de parabrisas (sube hacia atrás)
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SDark, { FVector(xw0, s * hw, zt), FVector(xw1, s * hw, zw1) }, 2.4f, 8);
		B.Tube(SDark, { FVector(xw1, -hw, zw1), FVector(xw1, hw, zw1) }, 2.4f, 8);
		B.Quad(SGlass, FVector(xw0 - 0.5f, -hw + 2.f, zt + 1.f), FVector(xw0 - 0.5f, hw - 2.f, zt + 1.f), FVector(xw1 - 0.5f, hw - 2.f, zw1 - 1.f), FVector(xw1 - 0.5f, -hw + 2.f, zw1 - 1.f));
		B.Quad(SGlass, FVector(xw1 - 0.5f, -hw + 2.f, zw1 - 1.f), FVector(xw1 - 0.5f, hw - 2.f, zw1 - 1.f), FVector(xw0 - 0.5f, hw - 2.f, zt + 1.f), FVector(xw0 - 0.5f, -hw + 2.f, zt + 1.f));
		const float xr = (0.12f - 0.5f) * L;   // jaula trasera
		for (int32 s = -1; s <= 1; s += 2) { B.Tube(SDark, { FVector(xw1, s * hw, zw1), FVector(0.5f * (xw1 + xr), s * hw, zw1 + 2.f), FVector(xr, s * hw, zw1 - 4.f), FVector(xr, s * hw, zt) }, 2.4f, 8); }
		B.Tube(SDark, { FVector(xr, -hw, zw1 - 4.f), FVector(xr, hw, zw1 - 4.f) }, 2.4f, 8);
		for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SInterior, FVector(-0.08f * L, s * 22.f, zt + 14.f), FVector(14.f, 17.f, 12.f), 0.4f, 8); B.SuperBox(SInterior, FVector(-0.16f * L, s * 22.f, zt + 32.f), FVector(4.f, 17.f, 20.f), 0.4f, 8); }
		B.SuperBox(SDark, FVector(xw0 - 12.f, 0, zt + 10.f), FVector(10.f, w0 * 0.6f, 5.f), 0.4f, 8);
		Tail(wB * 0.80f, zt - 4.f, 3.f, 6.f);
		{ const FTransform Keep = B.Xf; B.Xf = FTransform(FRotator(0, 90, 0), FVector(xB - 20.f, 0, gc + 0.40f * Hh)) * Keep; DustlineAddWheel(B, 46.f, 28.f, 2, 1.f, false); B.Xf = Keep; }
	}
}

// ---------------------------------------------------------------- buggy: UTV deportivo tipo Can-Am Maverick X3
void ADustlineCar::BuildBuggyBody(FDustlineMeshBuilder& B)
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const float L = M.L, W = M.W, zT = BodyH(), gc = GroundClearance(), R = Setup.TireRadiusM * 100.f, Wd = Setup.TireWidthM * 100.f;
	const float xN = 0.5f * L, xRe = -0.5f * L, zFl = gc + 22.f;
	const FTransform Keep = B.Xf;

	// --- carrocería por secciones (rectángulos redondeados) ---
	struct FSecL { float x, hw, zt, zb, r; };
	auto Loft = [&](int32 Id, const TArray<FSecL>& S)
	{
		constexpr int32 NC = 5, NR = 4 * NC + 2;
		auto Ring = [&](const FSecL& s, FVector* P, FVector* N)
		{
			int32 n = 0;
			auto Put = [&](float y, float z, float ny, float nz) { P[n] = FVector(s.x, y, z); N[n] = FVector(0.f, ny, nz).GetSafeNormal(); ++n; };
			const float r = FMath::Min(s.r, 0.5f * (s.zt - s.zb) - 0.1f), cy = s.hw - r;
			Put(0.f, s.zb, 0.f, -1.f);
			for (int32 i = 0; i < NC; ++i) { const float a = -PI * 0.5f + PI * 0.5f * i / (NC - 1); Put(cy + r * FMath::Cos(a), s.zb + r + r * FMath::Sin(a), FMath::Cos(a), FMath::Sin(a)); }
			for (int32 i = 0; i < NC; ++i) { const float a = PI * 0.5f * i / (NC - 1); Put(cy + r * FMath::Cos(a), s.zt - r + r * FMath::Sin(a), FMath::Cos(a), FMath::Sin(a)); }
			Put(0.f, s.zt, 0.f, 1.f);
			for (int32 i = 0; i < NC; ++i) { const float a = PI * 0.5f + PI * 0.5f * i / (NC - 1); Put(-cy + r * FMath::Cos(a), s.zt - r + r * FMath::Sin(a), FMath::Cos(a), FMath::Sin(a)); }
			for (int32 i = 0; i < NC; ++i) { const float a = PI + PI * 0.5f * i / (NC - 1); Put(-cy + r * FMath::Cos(a), s.zb + r + r * FMath::Sin(a), FMath::Cos(a), FMath::Sin(a)); }
		};
		FVector P0[NR], N0[NR], P1[NR], N1[NR];
		for (int32 k = 0; k + 1 < S.Num(); ++k)
		{
			Ring(S[k], P0, N0); Ring(S[k + 1], P1, N1);
			for (int32 j = 0; j < NR; ++j) { const int32 j2 = (j + 1) % NR; B.QuadN(Id, P0[j], P0[j2], P1[j2], P1[j], N0[j], N0[j2], N1[j2], N1[j]); }
		}
		for (int32 e = 0; e < 2; ++e)
		{
			Ring(e == 0 ? S[0] : S.Last(), P0, N0); FVector C(0); for (int32 j = 0; j < NR; ++j) C += P0[j]; C /= NR;
			const FVector Nc(e == 0 ? -1.f : 1.f, 0.f, 0.f);
			for (int32 j = 0; j < NR; ++j) B.Tri(Id, C, P0[j], P0[(j + 1) % NR], Nc, Nc, Nc, FVector2D(.5, .5), FVector2D(0, 0), FVector2D(1, 0));
		}
	};

	// capó bajo y ancho, cubierta trasera y suelo
	Loft(SPaint, { { 30.f, 70.f, gc + 68.f, gc + 14.f, 16.f }, { 68.f, 75.f, gc + 70.f, gc + 12.f, 16.f }, { 112.f, 73.f, gc + 73.f, gc + 10.f, 16.f }, { 148.f, 65.f, gc + 58.f, gc + 8.f, 14.f }, { xN - 2.f, 55.f, gc + 38.f, gc + 8.f, 11.f } });
	Loft(SPaint, { { xRe + 2.f, 58.f, gc + 42.f, gc + 8.f, 10.f }, { -140.f, 66.f, gc + 58.f, gc + 8.f, 14.f }, { -92.f, 71.f, gc + 64.f, gc + 12.f, 14.f }, { -54.f, 71.f, gc + 62.f, gc + 16.f, 12.f } });
	B.SuperBox(SDark, FVector(-12.f, 0.f, gc + 16.f), FVector(78.f, 62.f, 4.f), 0.3f, 8);                                    // suelo y chasis
	B.SuperBox(SPaint, FVector(86.f, 0.f, gc + 72.f), FVector(44.f, 17.f, 3.2f), 0.4f, 8);                                // cúpula del capó
	for (int32 s = -1; s <= 1; s += 2) for (int32 k = 0; k < 4; ++k) B.Box(SDark, FVector(60.f + k * 7.5f, s * 40.f, gc + 70.6f), FVector(2.6f, 13.f, 0.35f));   // rejillas de ventilación

	// aletas anchas sobre cada rueda + arcos
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f;
		const float y0 = FMath::Abs(WP.Y) - Wd * 0.5f - 9.f, y1 = FMath::Abs(WP.Y) + Wd * 0.5f + 7.f;
		ArcBand(B, SPaint, WP.X, R, side, R + 5.f, R + 8.f, y0, y1, 0.10f, PI - 0.10f, 20);
		B.SuperBox(SPaint, FVector(WP.X, side * 0.5f * (y0 + y1), 2.f * R + 11.f), FVector(R + 5.f, 0.5f * (y1 - y0), 2.2f), 0.35f, 8);
	}
	// umbrales laterales y puertas bajas
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.SuperBox(SDark, FVector(-4.f, s * 66.f, gc + 20.f), FVector(78.f, 4.f, 8.f), 0.35f, 8);
		B.SuperBox(SPaint, FVector(-8.f, s * 66.f, gc + 38.f), FVector(40.f, 2.2f, 13.f), 0.4f, 8);
		B.SuperBox(SDark, FVector(-8.f, s * 67.8f, gc + 40.f), FVector(22.f, 0.6f, 6.f), 0.4f, 6);
	}

	// --- jaula tubular negra y techo plano ---
	const float zr = zT - 3.f;
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.Tube(SRubber, FDustlineMeshBuilder::Smooth({ FVector(38.f, s * 64.f, gc + 50.f), FVector(22.f, s * 58.f, zr - 46.f), FVector(6.f, s * 52.f, zr) }, 5), 2.5f, 8);          // montante A
		B.Tube(SRubber, FDustlineMeshBuilder::Smooth({ FVector(-52.f, s * 64.f, gc + 54.f), FVector(-57.f, s * 58.f, zr - 40.f), FVector(-57.f, s * 52.f, zr) }, 5), 2.5f, 8);      // montante trasero
		B.Tube(SRubber, { FVector(-12.f, s * 63.f, gc + 54.f), FVector(-12.f, s * 55.f, zr) }, 2.0f, 8);                                                                         // montante B
		B.Tube(SRubber, { FVector(6.f, s * 52.f, zr), FVector(-57.f, s * 52.f, zr) }, 2.5f, 8);                                                                                 // larguero de techo
		B.Tube(SRubber, { FVector(38.f, s * 64.f, gc + 66.f), FVector(-52.f, s * 64.f, gc + 66.f) }, 2.2f, 8);                                                                  // barra de puerta
		B.Tube(SRubber, { FVector(38.f, s * 64.f, gc + 78.f), FVector(-12.f, s * 61.f, gc + 90.f) }, 1.0f, 6);                                                                  // red lateral
		B.Tube(SRubber, { FVector(-52.f, s * 64.f, gc + 54.f), FVector(-70.f, s * 66.f, gc + 52.f), FVector(-96.f, s * 62.f, gc + 54.f) }, 2.3f, 8);                          // refuerzo trasero
	}
	B.Tube(SRubber, { FVector(6.f, -52.f, zr), FVector(6.f, 52.f, zr) }, 2.5f, 8);
	B.Tube(SRubber, { FVector(-57.f, -52.f, zr), FVector(-57.f, 52.f, zr) }, 2.5f, 8);
	B.Tube(SRubber, { FVector(-26.f, -52.f, zr), FVector(-26.f, 52.f, zr) }, 2.0f, 8);
	B.SuperBox(SPaint, FVector(-25.f, 0.f, zT + 0.5f), FVector(35.f, 55.f, 1.6f), 0.3f, 8);                                 // techo
	B.SuperBox(SLight, FVector(10.f, 0.f, zT + 6.f), FVector(1.8f, 46.f, 2.2f), 0.5f, 6);                                   // barra LED
	for (int32 s = -1; s <= 1; s += 2) B.Tube(SRubber, { FVector(10.f, s * 38.f, zT + 5.f), FVector(8.f, s * 40.f, zT) }, 1.2f, 6);

	// --- interior: baquets, consola, salpicadero, volante ---
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.SuperBox(SInterior, FVector(-14.f, s * 23.f, zFl + 12.f), FVector(17.f, 16.f, 5.5f), 0.4f, 8);
		B.SuperBox(SInterior, FVector(-33.f, s * 23.f, zFl + 36.f), FVector(5.f, 16.f, 27.f), 0.4f, 8);
		B.SuperBox(SInterior, FVector(-33.f, s * 23.f, zFl + 68.f), FVector(4.f, 11.f, 8.f), 0.5f, 8);
		for (int32 k = -1; k <= 1; k += 2) B.SuperBox(SInterior, FVector(-30.f, s * 23.f + k * 15.f, zFl + 30.f), FVector(6.f, 3.5f, 20.f), 0.4f, 6);   // laterales del baquet
	}
	B.SuperBox(SDark, FVector(-8.f, 0.f, zFl + 12.f), FVector(24.f, 7.f, 9.f), 0.35f, 8);
	B.SuperBox(SDark, FVector(40.f, 0.f, gc + 60.f), FVector(9.f, 62.f, 9.f), 0.35f, 8);
	B.Cylinder(SDark, FVector(32.f, -23.f, gc + 66.f), FVector(-0.7f, 0.f, 0.7f), 1.7f, 24.f, 8, true);
	B.Torus(SDark, FVector(15.f, -23.f, gc + 83.f), FVector(0.7f, 0.f, 0.7f), 9.f, 1.7f, 20, 6);
	B.Tube(SRubber, { FVector(30.f, 40.f, gc + 66.f), FVector(8.f, 44.f, gc + 78.f) }, 1.3f, 6);                                  // asidero del copiloto

	// --- frontal: rejilla de malla, faros LED afilados, defensa tubular y protector ---
	B.SuperBox(SDark, FVector(xN - 3.f, 0.f, gc + 21.f), FVector(2.5f, 38.f, 12.f), 0.3f, 8);
	for (int32 r2 = -4; r2 <= 4; ++r2) for (int32 c = -1; c <= 1; ++c)
		B.Box(SMetal, FVector(xN - 0.2f, r2 * 8.5f, gc + 21.f + c * 7.5f + ((r2 & 1) ? 3.7f : 0.f)), FVector(0.45f, 3.2f, 0.9f));
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.Xf = FTransform(FRotator(0.f, s * 24.f, s * -6.f), FVector(xN - 14.f, s * 50.f, gc + 38.f)) * Keep;
		B.SuperBox(SDark, FVector(0.f), FVector(4.f, 18.f, 5.5f), 0.4f, 8);
		B.SuperBox(SLight, FVector(1.8f, 0.f, 1.5f), FVector(2.8f, 15.f, 2.2f), 0.5f, 8);
		B.SuperBox(SLight, FVector(1.8f, 0.f, -3.2f), FVector(2.4f, 9.f, 1.2f), 0.5f, 6);
		B.Xf = Keep;
		B.SuperBox(SLight, FVector(xN - 6.f, s * 62.f, gc + 22.f), FVector(2.f, 4.f, 3.f), 0.6f, 6);                              // faros auxiliares
	}
	B.Tube(SMetal, FDustlineMeshBuilder::Smooth({ FVector(xN - 6.f, -54.f, gc + 16.f), FVector(xN + 3.f, -28.f, gc + 14.f), FVector(xN + 3.f, 28.f, gc + 14.f), FVector(xN - 6.f, 54.f, gc + 16.f) }, 5), 2.2f, 8);
	for (int32 s = -1; s <= 1; s += 2) B.Tube(SMetal, { FVector(xN + 3.f, s * 28.f, gc + 14.f), FVector(xN - 8.f, s * 24.f, gc + 34.f) }, 1.6f, 6);
	B.SuperBox(SMetal, FVector(xN - 26.f, 0.f, gc + 4.f), FVector(26.f, 52.f, 2.4f), 0.3f, 8);                              // protector inferior
	B.SuperBox(SDark, FVector(xN - 10.f, 0.f, gc + 8.f), FVector(6.f, 46.f, 3.f), 0.4f, 8);

	// --- trasero: pilotos verticales, defensa, escape y protector ---
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.SuperBox(STail, FVector(xRe + 0.5f, s * 55.f, gc + 44.f), FVector(1.8f, 3.2f, 13.f), 0.5f, 6);
		B.SuperBox(STail, FVector(xRe + 0.5f, s * 49.f, gc + 33.f), FVector(1.6f, 4.5f, 2.f), 0.5f, 6);
	}
	B.Tube(SMetal, FDustlineMeshBuilder::Smooth({ FVector(xRe + 4.f, -56.f, gc + 14.f), FVector(xRe - 3.f, -28.f, gc + 12.f), FVector(xRe - 3.f, 28.f, gc + 12.f), FVector(xRe + 4.f, 56.f, gc + 14.f) }, 5), 2.2f, 8);
	B.SuperBox(SMetal, FVector(xRe + 22.f, 0.f, gc + 4.f), FVector(22.f, 46.f, 2.2f), 0.3f, 8);
	B.Cylinder(SChrome, FVector(xRe + 6.f, 28.f, gc + 20.f), FVector(-1.f, 0.f, 0.f), 4.6f, 12.f, 14, true);
	B.Cylinder(SDark, FVector(xRe - 5.f, 28.f, gc + 20.f), FVector(-1.f, 0.f, 0.f), 3.4f, 1.f, 12, true);
	B.Box(SDark, FVector(xRe + 0.3f, 0.f, gc + 26.f), FVector(0.4f, 15.f, 7.f));                                             // matrícula
	(void)L;
}

void ADustlineCar::BuildWheels()
{
	const float R = Setup.TireRadiusM * 100.f, Wd = Setup.TireWidthM * 100.f;
	for (int32 i = 0; i < 4; ++i)
	{
		FDustlineMeshBuilder B;
		DustlineAddWheel(B, R, Wd, Setup.TireType, (i % 2) == 0 ? -1.f : 1.f);
		B.Build(Wheels[i], Mats, false);
		Wheels[i]->SetRelativeLocation(WheelPos(i));
	}
}
