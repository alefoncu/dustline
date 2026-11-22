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
	};

	FBodyDef MakeDef(int32 St)
	{
		FBodyDef D;
		switch (St)
		{
		case 0: // Clase G
			D.Top = { FVector2D(0, 0.93), FVector2D(0.012, 0.99), FVector2D(0.04, 1.0), FVector2D(0.68, 1.0), FVector2D(0.695, 0.985), FVector2D(0.745, 0.59), FVector2D(0.757, 0.556), FVector2D(0.80, 0.545), FVector2D(0.955, 0.54), FVector2D(0.985, 0.525), FVector2D(1.0, 0.47) };
			D.Win = { FVector2D(0.445, 0.655), FVector2D(0.215, 0.425), FVector2D(0.07, 0.195) }; D.Gaps = { 0.675f, 0.435f, 0.205f };
			D.Belt = 0.60f; D.CabIn = 0.93f; D.RoofIn = 0.90f; D.Crown = 2.f; D.RoofCrown = 1.5f; D.Round = 3.f; D.SmoothDeg = 24.f; D.TaperF = 0.02f; D.TaperR = 0.f; break;
		case 1: // Urus
			D.Top = { FVector2D(0, 0.46), FVector2D(0.012, 0.56), FVector2D(0.05, 0.68), FVector2D(0.11, 0.84), FVector2D(0.20, 0.97), FVector2D(0.30, 1.0), FVector2D(0.46, 1.0), FVector2D(0.56, 0.985), FVector2D(0.64, 0.93), FVector2D(0.70, 0.78), FVector2D(0.735, 0.665), FVector2D(0.76, 0.63), FVector2D(0.83, 0.605), FVector2D(0.92, 0.575), FVector2D(0.975, 0.52), FVector2D(1.0, 0.40) };
			D.Win = { FVector2D(0.43, 0.67), FVector2D(0.255, 0.415), FVector2D(0.17, 0.24) }; D.Gaps = { 0.675f, 0.42f, 0.245f };
			D.Belt = 0.60f; D.CabIn = 0.86f; D.RoofIn = 0.72f; D.Crown = 5.f; D.RoofCrown = 5.f; D.Round = 7.f; D.SmoothDeg = 70.f; D.TaperF = 0.09f; D.TaperR = 0.08f; D.bSmoothOutline = true; break;
		case 2: // Panda 4x4
			D.Top = { FVector2D(0, 0.66), FVector2D(0.015, 0.92), FVector2D(0.05, 1.0), FVector2D(0.60, 1.0), FVector2D(0.625, 0.985), FVector2D(0.70, 0.65), FVector2D(0.725, 0.60), FVector2D(0.77, 0.58), FVector2D(0.955, 0.55), FVector2D(0.985, 0.52), FVector2D(1.0, 0.40) };
			D.Win = { FVector2D(0.385, 0.60), FVector2D(0.14, 0.345) }; D.Gaps = { 0.61f, 0.365f, 0.125f };
			D.Belt = 0.60f; D.CabIn = 0.90f; D.RoofIn = 0.82f; D.Crown = 4.f; D.RoofCrown = 3.f; D.Round = 5.f; D.SmoothDeg = 40.f; D.TaperF = 0.05f; D.TaperR = 0.04f; break;
		case 3: // Defender clásico
			D.Top = { FVector2D(0, 0.92), FVector2D(0.01, 0.985), FVector2D(0.03, 1.0), FVector2D(0.69, 1.0), FVector2D(0.70, 0.985), FVector2D(0.725, 0.62), FVector2D(0.735, 0.585), FVector2D(0.76, 0.58), FVector2D(0.965, 0.575), FVector2D(0.985, 0.56), FVector2D(1.0, 0.44) };
			D.Win = { FVector2D(0.455, 0.665), FVector2D(0.265, 0.435), FVector2D(0.075, 0.215) }; D.Gaps = { 0.67f, 0.445f, 0.235f };
			D.Belt = 0.62f; D.CabIn = 0.93f; D.RoofIn = 0.90f; D.Crown = 0.f; D.RoofCrown = 0.5f; D.Round = 2.f; D.SmoothDeg = 18.f; D.TaperF = 0.01f; D.TaperR = 0.f; break;
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
	if (GetModel(Setup.Model).Style == 4) BuildBuggyBody(B); else BuildShell(B);
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
		const float zTop = ZTopAt(u), zbelt = FMath::Min(zTop, zBeltMax), zb = gc;
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
	auto ZB = [&](float u) { return FMath::Min(ZTopAt(u), zBeltMax); };
	const float wF = WA(1.f), wB = WA(0.f), zTopF = ZTopAt(1.f), zTopB = ZTopAt(0.f);
	const bool bOpen = (St == 6);

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
		Round(wF * 0.72f, zTopF + 1.5f, 9.f);
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
		B.SuperBox(SDark, FVector(xF - 6.f, 0, gc + 14.f), FVector(8.f, wF * 0.92f, 13.f), 0.4f, 10);
		B.SuperBox(SCarbon, FVector(xF - 3.f, 0, gc + 8.f), FVector(7.f, wF * 0.80f, 3.f), 0.4f, 8);                    // splitter
		B.SuperBox(SDark, FVector(xF - 1.8f, 0, gc + 20.f), FVector(1.5f, wF * 0.62f, 13.f), 0.3f, 10);                 // gran entrada central
		for (int32 k = -4; k <= 4; ++k) B.Box(SMetal, FVector(xF - 0.5f, k * wF * 0.135f, gc + 20.f), FVector(0.4f, 0.5f, 12.f));
		for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SDark, FVector(xF - 3.f, s * wF * 0.78f, gc + 20.f), FVector(1.5f, 15.f, 9.f), 0.3f, 8);
			B.SuperBox(SLight, FVector(xF - 3.f, s * wF * 0.70f, zTopF - 7.f), FVector(2.f, 17.f, 3.6f), 0.4f, 8); B.SuperBox(SLight, FVector(xF - 3.f, s * wF * 0.70f, zTopF - 14.f), FVector(1.6f, 9.f, 1.6f), 0.4f, 6); }
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
		for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SLight, FVector(xF - 0.5f, s * wF * 0.66f, zTopF - 11.f), FVector(1.8f, 15.f, 6.f), 0.4f, 6); B.SuperBox(SAccent, FVector(xF - 0.5f, s * wF * 0.40f, gc + 22.f), FVector(1.2f, 4.f, 4.f), 0.8f, 6); }
		Tail(wB * 0.83f, zTopB - 26.f, 3.5f, 11.f); Plate(xB - 0.5f, gc + 25.f, -1.f);
		B.Box(SGlass, FVector(xB - 0.35f + 4.f, 0, 0.5f * (ZTopAt(0.03f) - 8.f + zBeltMax + 8.f)), FVector(0.3f, wB * 0.70f, 0.5f * (ZTopAt(0.03f) - 8.f - zBeltMax - 8.f)));
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SDark, { FVector(-0.40f * L, s * (w0 * 0.82f), Ht + 0.5f), FVector(0.22f * L, s * (w0 * 0.82f), Ht + 0.5f) }, 1.0f, 6);
		break;
	}
	case 3: // Defender
	{
		B.Box(SMetal, FVector(xF - 6.f, 0, gc + 12.f), FVector(8.f, wF * 0.99f, 8.f));                                  // parachoques de acero
		B.Box(SMetal, FVector(xB + 6.f, 0, gc + 12.f), FVector(8.f, wB * 0.99f, 8.f));
		B.Box(SDark, FVector(xF + 0.3f, 0, zTopF - 17.f), FVector(1.f, wF * 0.62f, 13.f));                              // panel de parrilla
		for (int32 k = -3; k <= 3; ++k) B.Box(SMetal, FVector(xF + 0.9f, 0, zTopF - 17.f + k * 3.4f), FVector(0.5f, wF * 0.56f, 0.9f));
		Round(wF * 0.60f, zTopF - 17.f, 8.5f);
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
	if (St == 6) { /* Crawler: jaula y accesorios en BuildBuggyBody-like extras */
		const float zt = zBeltMax, hw = w0 - 6.f;
		B.SuperBox(SMetal, FVector(xF - 6.f, 0, gc + 10.f), FVector(8.f, wF * 0.98f, 8.f), 0.3f, 8);                       // parachoques con cabrestante
		B.SuperBox(SDark, FVector(xF - 12.f, 0, gc + 24.f), FVector(8.f, 18.f, 8.f), 0.4f, 8);
		B.Cylinder(SMetal, FVector(xF - 18.f, -12.f, gc + 24.f), FVector(0, 1, 0), 5.f, 24.f, 14, true);
		B.SuperBox(SMetal, FVector(xB + 6.f, 0, gc + 12.f), FVector(8.f, wB * 0.98f, 8.f), 0.3f, 8);
		Round(wF * 0.62f, zTopF + 4.f, 8.f);
		B.SuperBox(SDark, FVector(xF - 0.8f, 0, zTopF - 8.f), FVector(1.f, wF * 0.46f, 9.f), 0.3f, 6);
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

// ---------------------------------------------------------------- buggy (chasis tubular)
void ADustlineCar::BuildBuggyBody(FDustlineMeshBuilder& B)
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const float L = M.L, W = M.W, Ht = BodyH(), gc = GroundClearance(), R = Setup.TireRadiusM * 100.f, Wd = Setup.TireWidthM * 100.f;
	const float hw = W * 0.5f - 38.f, zt = Ht, belt = gc + 0.42f * (Ht - gc), xF = FrontX(), xR = RearX();
	// chasis y suelo
	B.SuperBox(SDark, FVector(0, 0, gc + 6.f), FVector(L * 0.36f, W * 0.20f, 3.5f), 0.3f, 6);
	for (int32 s = -1; s <= 1; s += 2) B.Tube(SPaint, { FVector(xR - 20.f, s * hw * 0.78f, gc + 12.f), FVector(xF + 30.f, s * hw * 0.78f, gc + 12.f) }, 2.8f, 10);
	// jaula
	auto Hoop = [&](float x) { B.Tube(SPaint, FDustlineMeshBuilder::Smooth({ FVector(x, -hw, gc + 12.f), FVector(x, -hw * 0.92f, belt + 10.f), FVector(x - 4.f, -hw * 0.72f, zt - 6.f), FVector(x - 5.f, 0, zt), FVector(x - 4.f, hw * 0.72f, zt - 6.f), FVector(x, hw * 0.92f, belt + 10.f), FVector(x, hw, gc + 12.f) }, 5), 2.5f, 8); };
	Hoop(-5.f); Hoop(62.f);
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.Tube(SPaint, { FVector(-10.f, s * hw * 0.72f, zt - 6.f), FVector(62.f, s * hw * 0.72f, zt - 6.f) }, 2.4f, 8);
		B.Tube(SPaint, { FVector(-10.f, s * hw * 0.72f, zt - 6.f), FVector(xR + 5.f, s * hw * 0.55f, belt + 6.f) }, 2.2f, 8);
		B.Tube(SPaint, { FVector(62.f, s * hw * 0.92f, belt + 10.f), FVector(xF - 20.f, s * hw * 0.80f, belt - 4.f), FVector(xF, s * hw * 0.78f, gc + 14.f) }, 2.2f, 8);
		B.Tube(SPaint, { FVector(-5.f, s * hw, gc + 12.f), FVector(62.f, s * hw * 0.92f, belt + 10.f) }, 2.0f, 8);
		B.SuperBox(SInterior, FVector(10.f, s * 26.f, belt + 4.f), FVector(17.f, 19.f, 5.f), 0.4f, 8);
		B.SuperBox(SInterior, FVector(-8.f, s * 26.f, belt + 26.f), FVector(5.f, 19.f, 24.f), 0.4f, 8);
		B.Tube(SMetal, { FVector(-10.f, s * 26.f, zt - 6.f), FVector(-10.f, s * 26.f, belt + 40.f) }, 1.0f, 6);
	}
	B.Tube(SPaint, { FVector(62.f, -hw * 0.92f, belt + 10.f), FVector(62.f, hw * 0.92f, belt + 10.f) }, 2.2f, 8);
	// volante y cuadro
	B.Cylinder(SDark, FVector(36.f, -26.f, belt + 20.f), FVector(-0.5f, 0, 0.8f), 1.6f, 26.f, 8, true);
	B.Torus(SDark, FVector(32.f, -26.f, belt + 30.f), FVector(0.7f, 0, 0.7f), 10.f, 1.6f, 20, 6);
	B.SuperBox(SDark, FVector(50.f, 0, belt + 14.f), FVector(10.f, hw * 0.7f, 6.f), 0.4f, 8);
	// morro y aletas
	B.SuperBox(SPaint, FVector(xF + 36.f, 0, gc + 30.f), FVector(44.f, W * 0.30f, 20.f), 0.35f, 12);
	B.SuperBox(SDark, FVector(xF + 80.f, 0, gc + 22.f), FVector(2.f, W * 0.20f, 10.f), 0.35f, 8);
	for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SLight, FVector(xF + 76.f, s * W * 0.22f, gc + 38.f), FVector(3.f, 9.f, 6.f), 0.5f, 8); }
	B.SuperBox(SPaint, FVector(xR - 6.f, 0, gc + 26.f), FVector(34.f, W * 0.28f, 15.f), 0.35f, 10);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f;
		ArcBand(B, SPaint, WP.X, R, side, R + 6.f, R + 8.5f, FMath::Abs(WP.Y) - Wd * 0.5f - 6.f, FMath::Abs(WP.Y) + Wd * 0.5f + 5.f, 0.15f, PI - 0.15f, 18);
	}
	B.SuperBox(SMetal, FVector(xF + 80.f, 0, gc + 6.f), FVector(4.f, W * 0.34f, 2.f), 0.3f, 6);
	B.Quad(SDark, FVector(xR - 30.f, -W * 0.12f, gc + 30.f), FVector(xR - 30.f, W * 0.12f, gc + 30.f), FVector(xR - 30.f, W * 0.12f, gc + 8.f), FVector(xR - 30.f, -W * 0.12f, gc + 8.f));
	B.Cylinder(SChrome, FVector(xR - 38.f, 18.f, gc + 14.f), FVector(-1, 0, 0), 3.4f, 14.f, 12, true);
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
