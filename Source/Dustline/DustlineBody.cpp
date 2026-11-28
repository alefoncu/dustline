#include "DustlineCar.h"
#include "DustlineCarParts.h"

// Carrocerías: el buggy se construye aquí; el resto de modelos, en DustlineShell.cpp (BuildShellV2).

namespace
{
	float Sm(float x) { x = FMath::Clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }

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
	// baca opcional sobre el techo plano: barandilla baja, dos bidones y una bolsa estanca
	if (Setup.bRack)
	{
		const float zk = zT + 3.f, hw = 48.f, xa = -56.f, xb = 2.f;
		for (int32 s = -1; s <= 1; s += 2)
		{
			B.Tube(SRubber, { FVector(xa, s * hw, zk + 7.f), FVector(xb, s * hw, zk + 7.f) }, 1.2f, 6);
			for (float xq : { xa, 0.5f * (xa + xb), xb }) B.Tube(SRubber, { FVector(xq, s * hw, zk), FVector(xq, s * hw, zk + 7.f) }, 1.0f, 6);
			B.SuperBox(SAccent, FVector(xa + 12.f, s * 30.f, zk + 20.f), FVector(11.f, 7.f, 19.f), 0.18f, 6);
		}
		B.SuperBox(SInterior, FVector(-16.f, 0.f, zk + 9.f), FVector(16.f, 38.f, 9.f), 0.35f, 8);
	}
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
