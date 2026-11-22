#include "DustlineCar.h"
#include "DustlineCarParts.h"

// Ruedas, frenos, suspensión, transmisión, motor y turbos procedurales (alto detalle; se regeneran con cada cambio en el taller).

static FVector2D P2(float a, float r) { return FVector2D(a, r); }

// ---------------------------------------------------------------- rueda completa
void DustlineAddWheel(FDustlineMeshBuilder& B, float R, float Wd, int32 Type, float Side, bool bBrake)
{
	const FVector Ax(0, 1, 0);
	const FTransform Keep = B.Xf;
	const float Rr = R * (Type == 4 ? 0.50f : (Type == 2 ? 0.52f : 0.60f));     // radio de llanta
	const float Bulge = (Type == 4) ? 1.10f : (Type == 0 ? 0.97f : 1.0f);
	auto Mir = [&](TArray<FVector2D> Half) { TArray<FVector2D> Out; for (int32 i = 0; i < Half.Num(); ++i) Out.Add(Half[i]); for (int32 i = Half.Num() - 1; i >= 0; --i) Out.Add(FVector2D(-Half[i].X, Half[i].Y)); return Out; };

	// ---- cubierta: talón, flanco con abombamiento, hombro, banda
	TArray<FVector2D> Half;   // eje negativo -> centro (x<=0)
	Half.Add(P2(-0.43f * Wd, Rr * 0.99f)); Half.Add(P2(-0.49f * Wd, Rr * 1.07f)); Half.Add(P2(-0.515f * Wd * Bulge, Rr + (R - Rr) * 0.30f)); Half.Add(P2(-0.525f * Wd * Bulge, Rr + (R - Rr) * 0.55f));
	Half.Add(P2(-0.510f * Wd, Rr + (R - Rr) * 0.78f)); Half.Add(P2(-0.465f * Wd, R * 0.955f)); Half.Add(P2(-0.40f * Wd, R * 0.992f)); Half.Add(P2(-0.32f * Wd, R));
	if (Type == 0) { Half.Add(P2(-0.24f * Wd, R)); Half.Add(P2(-0.225f * Wd, R * 0.978f)); Half.Add(P2(-0.19f * Wd, R * 0.978f)); Half.Add(P2(-0.175f * Wd, R)); Half.Add(P2(-0.06f * Wd, R)); Half.Add(P2(-0.045f * Wd, R * 0.978f)); }
	else if (Type == 4) { Half.Add(P2(-0.20f * Wd, R * 1.01f)); Half.Add(P2(-0.10f * Wd, R * 1.018f)); }
	TArray<FVector2D> Prof = Mir(Half);
	B.Lathe(SRubber, FVector::ZeroVector, Ax, Prof, Type == 4 ? 56 : 64, 3.f);
	// letras / nervios del flanco
	for (int32 s = -1; s <= 1; s += 2) { B.Torus(SRubber, FVector(0, s * 0.508f * Wd * Bulge, 0), Ax, Rr + (R - Rr) * 0.40f, 0.5f, 56, 4); B.Torus(SRubber, FVector(0, s * 0.51f * Wd * Bulge, 0), Ax, Rr + (R - Rr) * 0.66f, 0.45f, 56, 4); }
	// ---- taco / dibujo
	if (Type == 1 || Type == 2)
	{
		const int32 N = Type == 2 ? 22 : 34; const float Kh = (Type == 2 ? 0.080f : 0.042f) * R, Kw = (Type == 2 ? 0.30f : 0.26f) * Wd, Kl = R * (Type == 2 ? 0.20f : 0.12f);
		for (int32 row = -1; row <= 1; row += 2) for (int32 i = 0; i < N; ++i)
		{
			B.Xf = FTransform(FQuat(Ax, 2 * PI * (i + (row > 0 ? 0.5f : 0.f)) / N)) * Keep;
			B.SuperBox(SRubber, FVector(0, row * Wd * 0.17f, R + Kh * 0.2f), FVector(Kl * 0.5f, Kw * 0.5f, Kh), 0.35f, 5);
			// taco de hombro que sube por el flanco
			const float a = row * (Type == 2 ? 0.46f : 0.42f);
			B.SuperBox(SRubber, FVector(0, a * Wd, R * 0.955f + Kh * 0.1f), FVector(Kl * 0.5f, Kw * 0.28f, Kh * 1.25f), 0.35f, 5);
		}
		B.Xf = Keep;
	}
	if (Type == 3)   // clavos
	{
		for (int32 row = -2; row <= 2; ++row) for (int32 i = 0; i < 40; ++i)
		{
			B.Xf = FTransform(FQuat(Ax, 2 * PI * (i + (row & 1 ? 0.5f : 0.f)) / 40)) * Keep;
			B.Cylinder(SChrome, FVector(0, row * Wd * 0.14f, R - 0.8f), FVector(0, 0, 1), 0.6f, 2.8f, 6, true);
		}
		B.Xf = Keep;
	}

	// ---- llanta
	const float Face = Side * 0.30f * Wd;          // cara exterior (hundida respecto al labio)
	TArray<FVector2D> Barrel = { P2(-0.44f * Wd, Rr * 0.95f), P2(-0.43f * Wd, Rr * 1.01f), P2(-0.36f * Wd, Rr * 0.965f), P2(0.36f * Wd, Rr * 0.965f), P2(0.43f * Wd, Rr * 1.01f), P2(0.44f * Wd, Rr * 0.95f) };
	B.Lathe(SRim, FVector::ZeroVector, Ax, Barrel, 48);
	for (int32 s = -1; s <= 1; s += 2) B.Torus(SRim, FVector(0, s * 0.435f * Wd, 0), Ax, Rr * 1.0f, 0.9f, 48, 6);   // labios
	// fondo oscuro tras los radios
	TArray<FVector2D> Dish = { P2(Face - Side * 0.5f, Rr * 0.30f), P2(Face - Side * 0.5f, Rr * 0.94f) };
	B.Lathe(SDark, FVector::ZeroVector, Ax, Dish, 40);
	const int32 Spokes = Type == 2 ? 6 : (Type == 4 ? 8 : (Type == 1 ? 6 : 5));
	const float tw = Type == 4 ? 0.20f : 0.13f;
	if (Type == 2 || Type == 4)   // acero embutido con agujeros
	{
		TArray<FVector2D> Disc = { P2(Face, Rr * 0.22f), P2(Face - Side * 0.5f, Rr * 0.30f), P2(Face - Side * 0.5f, Rr * 0.94f) };
		B.Lathe(SMetal, FVector::ZeroVector, Ax, Disc, 40);
		for (int32 i = 0; i < 8; ++i) { B.Xf = FTransform(FQuat(Ax, 2 * PI * i / 8)) * Keep; B.Cylinder(SDark, FVector(0, Face + Side * 0.15f, Rr * 0.62f), Ax * Side, Rr * 0.12f, 0.4f, 10, true); }
		B.Xf = Keep;
		if (Type == 2) { for (int32 i = 0; i < 24; ++i) { B.Xf = FTransform(FQuat(Ax, 2 * PI * i / 24)) * Keep; B.Cylinder(SChrome, FVector(0, Side * 0.43f * Wd, Rr * 1.04f), Ax * Side, 0.9f, 1.4f, 6, true); } B.Xf = Keep; B.Torus(SChrome, FVector(0, Side * 0.43f * Wd, 0), Ax, Rr * 1.04f, 1.3f, 48, 6); }
	}
	else for (int32 i = 0; i < Spokes; ++i)
	{
		B.Xf = FTransform(FQuat(Ax, 2 * PI * i / Spokes)) * Keep;
		const float r0 = Rr * 0.22f, r1 = Rr * 0.95f;
		B.SuperBox(SRim, FVector(0, Face - Side * 0.4f, 0.5f * (r0 + r1)), FVector(Rr * tw * (Type == 0 ? 0.60f : 0.8f), 1.3f, 0.5f * (r1 - r0)), 0.30f, 6);
		if (Type == 0) { B.Xf = FTransform(FQuat(Ax, 2 * PI * i / Spokes + 0.12f)) * Keep; B.SuperBox(SRim, FVector(0, Face - Side * 0.4f, 0.5f * (r0 + r1)), FVector(Rr * 0.08f, 1.3f, 0.5f * (r1 - r0)), 0.30f, 6); }
	}
	B.Xf = Keep;
	// tapa central y tuercas
	B.Cylinder(SChrome, FVector(0, Face, 0), Ax * Side, Rr * 0.19f, 1.6f, 20, true);
	B.Cylinder(SDark, FVector(0, Face + Side * 1.5f, 0), Ax * Side, Rr * 0.12f, 0.5f, 16, true);
	for (int32 i = 0; i < 5; ++i)
	{
		B.Xf = FTransform(FQuat(Ax, 2 * PI * i / 5)) * Keep;
		B.Prism(SMetal, FVector(0, Face - Side * 0.2f, Rr * 0.27f), Ax * Side, 1.1f, 1.5f, 6);
	}
	B.Xf = Keep;
	// ---- freno: disco ventilado/perforado + pinza
	if (bBrake)
	{
		const float yb = -Side * 0.20f * Wd;
		B.Cylinder(SMetal, FVector(0, yb, 0), Ax, Rr * 0.86f, 1.6f, 40, true);
		B.Cylinder(SBlock, FVector(0, yb + Side * -0.5f, 0), Ax, Rr * 0.36f, 3.f, 24, true);
		for (int32 i = 0; i < 24; ++i) { B.Xf = FTransform(FQuat(Ax, 2 * PI * i / 24)) * Keep; B.Cylinder(SDark, FVector(0, yb - 0.1f, Rr * 0.74f), Ax, 0.55f, 1.8f, 6, true); }
		B.Xf = FTransform(FQuat(Ax, 0.9f)) * Keep;
		B.SuperBox(SAccent, FVector(0, yb, Rr * 0.72f), FVector(Rr * 0.19f, 3.6f, Rr * 0.14f), 0.3f, 8);
		B.Box(SChrome, FVector(0, yb + Side * 3.4f, Rr * 0.72f), FVector(Rr * 0.10f, 0.2f, Rr * 0.05f));
		B.Xf = Keep;
	}
}

// ---------------------------------------------------------------- suspensión, chasis y transmisión
void ADustlineCar::BuildSuspension()
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const int32 St = M.Style;
	const float R = Setup.TireRadiusM * 100.f, gc = GroundClearance(), Wd = Setup.TireWidthM * 100.f, W = M.W;
	const float xF = FrontX(), xR = RearX();
	const bool bSolidF = (St == 0 || St == 3 || St == 6), bSolidR = (St == 0 || St == 3 || St == 5 || St == 6 || St == 2);
	FDustlineMeshBuilder B;

	// ---- larguerueros y travesaños (chasis)
	if (St != 4)
	{
		const float yr = W * 0.22f, z0 = gc + 4.f;
		for (int32 s = -1; s <= 1; s += 2)
		{
			B.SuperBox(SDark, FVector(0.5f * (xF + xR), s * yr, z0), FVector(0.5f * (xF - xR) + 40.f, 3.6f, 5.5f), 0.25f, 6);
			B.SuperBox(SMetal, FVector(0.5f * (xF + xR), s * (yr + 3.7f), z0), FVector(0.5f * (xF - xR) + 38.f, 0.3f, 3.5f), 0.25f, 4);
		}
		for (float x : { xF + 20.f, 0.5f * (xF + xR), xR - 10.f }) B.SuperBox(SDark, FVector(x, 0, z0), FVector(3.5f, yr, 3.6f), 0.25f, 6);
		B.SuperBox(SDark, FVector(xR + 40.f, 0, gc + 12.f), FVector(32.f, yr * 0.78f, 11.f), 0.3f, 8);                   // depósito
		B.Cylinder(SMetal, FVector(xR + 18.f, 0, gc + 24.f), FVector(1, 0, 0), 3.f, 40.f, 10, true);
		// escape: colector -> catalizador -> silencioso -> colas
		const float ye = yr * 0.45f;
		B.Tube(SMetal, FDustlineMeshBuilder::Smooth({ FVector(xF - 30.f, ye, gc + 26.f), FVector(xF - 60.f, ye, gc + 14.f), FVector(xF - 110.f, ye, gc + 9.f), FVector(xR + 70.f, ye, gc + 8.f) }, 6), 3.4f, 12);
		B.Cylinder(SAlu, FVector(xR + 120.f, ye, gc + 9.f), FVector(1, 0, 0), 7.5f, 55.f, 18, true);                                   // catalizador/silencioso
		B.Lathe(SAlu, FVector(xR + 20.f, ye, gc + 10.f), FVector(1, 0, 0), { P2(0, 5.f), P2(10.f, 11.f), P2(60.f, 11.f), P2(70.f, 4.f) }, 20);
		B.Tube(SChrome, { FVector(xR + 2.f, ye, gc + 10.f), FVector(xR - 12.f, ye, gc + 10.f) }, 3.f, 12);
	}
	// ---- ejes y ruedas
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f; const bool front = i < 2;
		const bool solid = front ? bSolidF : bSolidR;
		const float yHub = WP.Y - side * (Wd * 0.5f - 2.f), yIn = WP.Y - side * (Wd * 0.5f + 14.f);
		const float zLow = R + 9.f, zTop = FMath::Max(gc + 8.f, zLow + 22.f);
		const float wire = 0.55f + 0.75f * Setup.Stiffness, cr = 6.5f;
		// manguetas y buje
		B.SuperBox(SDark, FVector(WP.X, yHub - side * 2.f, R), FVector(7.f, 3.f, 12.f), 0.3f, 6);
		B.Cylinder(SMetal, FVector(WP.X, yHub, R), FVector(0, -side, 0), 5.5f, 8.f, 16, true);
		// muelle helicoidal con asientos, amortiguador con vástago y depósito
		const float sx = WP.X + (solid ? 0.f : -3.f), sy = yIn - side * 1.f;
		TArray<FVector> Coil; const int32 Turns = FMath::Clamp((int32)(10 - Setup.Stiffness * 2.5f), 4, 9), Pts = Turns * 14;
		for (int32 k = 0; k <= Pts; ++k) { const float a = 2 * PI * k / 14.f; Coil.Add(FVector(sx + FMath::Cos(a) * cr, sy + FMath::Sin(a) * cr, zLow + 3.f + (zTop - zLow - 3.f) * k / Pts)); }
		B.Tube(SAccent, Coil, wire, 6);
		B.Cylinder(SMetal, FVector(sx, sy, zLow - 4.f), FVector(0, 0, 1), 2.7f, (zTop - zLow) * 0.62f + 4.f, 14, true);
		B.Cylinder(SChrome, FVector(sx, sy, zLow + (zTop - zLow) * 0.55f), FVector(0, 0, 1), 1.2f, (zTop - zLow) * 0.55f + 8.f, 10, true);
		B.Cylinder(SDark, FVector(sx, sy, zLow), FVector(0, 0, 1), cr + 2.4f, 1.6f, 16, true);
		B.Cylinder(SDark, FVector(sx, sy, zTop), FVector(0, 0, 1), cr + 2.4f, 1.6f, 16, true);
		B.Cylinder(SAccent, FVector(sx + 7.f, sy, zLow + (zTop - zLow) * 0.3f), FVector(0, 0, 1), 2.2f, (zTop - zLow) * 0.45f, 10, true);   // depósito piggyback
		B.Tube(SChrome, { FVector(sx + 3.f, sy, zLow + (zTop - zLow) * 0.45f), FVector(sx + 6.f, sy, zLow + (zTop - zLow) * 0.55f) }, 0.5f, 4);
		B.SuperBox(SMetal, FVector(sx, sy, zLow - 6.f), FVector(4.f, 4.f, 2.5f), 0.4f, 6);
		if (!solid)
		{
			const float yC = side * W * 0.10f;
			// trapecios doble horquilla con silentblocks
			B.Tube(SDark, { FVector(WP.X + 14.f, yC, R - 6.f), FVector(WP.X, yHub - side * 2.f, R - 8.f), FVector(WP.X - 14.f, yC, R - 6.f) }, 2.2f, 8);
			B.Tube(SDark, { FVector(WP.X + 10.f, yC * 0.9f, zTop - 6.f), FVector(WP.X, yHub - side * 2.f, R + 12.f), FVector(WP.X - 10.f, yC * 0.9f, zTop - 6.f) }, 1.9f, 8);
			for (float dx : { 14.f, -14.f }) B.Sphere(SBlock, FVector(WP.X + dx, yC, R - 6.f), 2.6f, 8);
			B.Tube(SMetal, { FVector(WP.X - 18.f, yC, R + 3.f), FVector(WP.X - 14.f, yHub - side * 2.f, R + 4.f) }, 0.9f, 6);   // bieleta de dirección
			// palier con fuelles
			B.Tube(SMetal, { FVector(WP.X, side * 16.f, R + 2.f), FVector(WP.X, yHub, R) }, 1.6f, 8);
			B.Lathe(SDark, FVector(WP.X, yHub - side * 8.f, R), FVector(0, -side, 0), { P2(0, 3.6f), P2(2.f, 5.f), P2(4.f, 3.6f), P2(6.f, 5.f), P2(8.f, 3.6f), P2(10.f, 5.f), P2(12.f, 3.2f) }, 14);
		}
	}
	// eje rígido (carcasa + puente + brazos de control)
	for (int32 a = 0; a < 2; ++a)
	{
		const bool front = a == 0, solid = front ? bSolidF : bSolidR; if (!solid) continue;
		const float x = front ? xF : xR, zc = R;
		B.Tube(SDark, { WheelPos(front ? 0 : 2) + FVector(0, 8.f, 0), FVector(x, 0, zc), WheelPos(front ? 1 : 3) - FVector(0, 8.f, 0) }, 4.2f, 12);
		for (int32 s = -1; s <= 1; s += 2) { B.Tube(SDark, { FVector(x, s * 38.f, zc + 4.f), FVector(x + (front ? -55.f : 55.f), s * 28.f, zc + 10.f) }, 2.0f, 8); B.Sphere(SBlock, FVector(x + (front ? -55.f : 55.f), s * 28.f, zc + 10.f), 2.8f, 8); }
		B.Tube(SMetal, { FVector(x - 6.f, -36.f, zc + 22.f), FVector(x - 6.f, 36.f, zc + 22.f) }, 1.0f, 6);   // barra panhard
	}
	// diferenciales (carcasa fundida con tapa atornillada) y árboles de transmisión
	const bool bF = Setup.Drive != 1, bR = Setup.Drive != 2;
	auto Diff = [&](float x, bool front)
	{
		const float dir = front ? 1.f : -1.f;
		B.Sphere(SBlock, FVector(x, 0, R + 2.f), 11.f, 14);
		B.Cylinder(SBlock, FVector(x + dir * 6.f, 0, R + 2.f), FVector(dir, 0, 0), 6.f, 10.f, 14, true);
		B.Cylinder(SAlu, FVector(x - dir * 9.f, 0, R + 2.f), FVector(-dir, 0, 0), 9.f, 2.2f, 24, true);                 // tapa
		for (int32 b = 0; b < 8; ++b) { const float a = 2 * PI * b / 8; B.Prism(SMetal, FVector(x - dir * 11.f, FMath::Cos(a) * 7.3f, R + 2.f + FMath::Sin(a) * 7.3f), FVector(-dir, 0, 0), 0.9f, 1.0f, 6); }
	};
	if (bF) Diff(xF, true);
	if (bR) Diff(xR, false);
	auto Shaft = [&](const FVector& A, const FVector& Bp)
	{
		B.Tube(SMetal, { A, Bp }, 3.6f, 12);
		for (const FVector& J : { A, Bp }) { B.Sphere(SDark, J, 4.4f, 8); B.Cylinder(SDark, J - FVector(0, 0, 4.6f), FVector(0, 0, 1), 1.2f, 9.2f, 6, true); }
	};
	const float zS = R + 6.f;
	if (bF && bR) { B.SuperBox(SBlock, FVector(0.5f * (xF + xR) * 0.0f, 0, zS + 4.f), FVector(16.f, 12.f, 11.f), 0.3f, 8); Shaft(FVector(xF - 10.f, 0, zS), FVector(8.f, 0, zS + 2.f)); Shaft(FVector(-8.f, 0, zS + 2.f), FVector(xR + 10.f, 0, zS)); }
	else if (bF) Shaft(FVector(xF - 10.f, 0, zS), FVector(xF * 0.2f, 0, zS + 2.f));
	else if (bR) Shaft(FVector(xR + 10.f, 0, zS), FVector(xR * 0.2f, 0, zS + 2.f));
	B.Build(Suspension, Mats, false);
}

// ---------------------------------------------------------------- motor, accesorios y turbo
void ADustlineCar::BuildEngine()
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const float gc = GroundClearance(), cc = Setup.EngineCC;
	int32 n = FMath::Clamp(FMath::RoundToInt(cc / 650.f), 2, 12);
	const bool bV = n > 6 || (cc > 3000.f && n >= 6);
	if (bV && (n % 2)) n += 1;
	const int32 banks = bV ? 2 : 1, perBank = bV ? (n + 1) / 2 : n;
	const float dcc = cc / n, bore = 8.f * FMath::Pow(dcc / 500.f, 1.f / 3.f), pitch = bore * 1.25f;
	const float len = perBank * pitch + 10.f, hl = len * 0.5f;
	const float xFront = FrontX();
	const FVector Org(xFront - 4.f, 0, FMath::Max(gc + 28.f, Setup.TireRadiusM * 100.f + 6.f) + bore * 0.9f);
	const float zTopHood = gc + 0.50f * (BodyH() - gc);
	FDustlineMeshBuilder B; const FTransform Keep = B.Xf;
	const float bw = bore * (bV ? 0.95f : 0.80f);
	// familia de motor (cada opción del taller tiene su propia silueta)
	const int32 es = cc < 2000.f ? 0 : (cc < 2800.f ? 1 : (cc < 3600.f ? 2 : (cc < 4800.f ? 3 : (cc < 6300.f ? 4 : (cc < 8000.f ? 5 : 6)))));
	const int32 coverMat = es == 2 ? SAlu : (es == 3 ? SBlock : (es == 4 ? SAccent : (es == 5 ? SCarbon : (es == 6 ? SChrome : SDark))));

	// bloque, cárter y tapa de distribución
	B.SuperBox(SBlock, Org + FVector(0, 0, -bore * 0.45f), FVector(hl, bw, bore * 0.95f), 0.22f, 8);
	B.SuperBox(SDark, Org + FVector(0, 0, -bore * 1.60f), FVector(hl - 2.f, bore * 0.72f, bore * 0.50f), 0.30f, 8);
	for (int32 i = 0; i < 6; ++i) B.Box(SMetal, Org + FVector(-hl + 4.f + i * (len - 8.f) / 5.f, 0, -bore * 1.90f), FVector(0.4f, bore * 0.7f, 0.7f));   // aletas del cárter
	for (int32 b = 0; b < banks; ++b)
	{
		const float ang = bV ? (b == 0 ? -34.f : 34.f) : 0.f;
		B.Xf = FTransform(FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(ang)), Org + FVector(0, 0, bore * 0.15f)) * Keep;
		B.SuperBox(SAlu, FVector(0, 0, bore * 0.70f), FVector(hl, bore * 0.60f, bore * 0.55f), 0.22f, 8);                                // culata
		B.SuperBox(coverMat, FVector(0, 0, bore * 1.38f), FVector(hl - 1.5f, bore * 0.52f, bore * 0.20f), 0.30f, 8);                      // tapa de balancines
		for (int32 r = 0; r < perBank + 1; ++r) B.Box(SDark, FVector(-hl + 2.f + r * (len - 4.f) / perBank, 0, bore * 1.58f), FVector(0.35f, bore * 0.5f, 0.25f));
		B.Cylinder(SChrome, FVector(hl * 0.55f, bore * 0.25f, bore * 1.55f), FVector(0, 0, 1), 1.3f, 1.4f, 12, true);                      // tapón de aceite
		B.Box(SChrome, FVector(-hl * 0.3f, 0, bore * 1.60f), FVector(hl * 0.22f, bore * 0.20f, 0.25f));                                  // placa
		for (int32 c = 0; c < perBank; ++c)
		{
			const float cx = -hl + 6.f + c * pitch + pitch * 0.15f;
			B.SuperBox(SDark, FVector(cx, 0, bore * 1.62f), FVector(1.5f, 1.2f, 3.2f), 0.4f, 6);                                          // bobina
			B.Cylinder(SMetal, FVector(cx, 0, bore * 1.50f), FVector(0, 0, 1), 0.7f, 0.9f, 6, true);
			B.Tube(SDark, { FVector(cx, 0, bore * 1.62f + 2.4f), FVector(cx + 1.5f, 1.f, bore * 1.62f + 6.f) }, 0.4f, 4);                      // cable
		}
	}
	B.Xf = Keep;
	// admisión: colector con runners, cuerpo de mariposa, tubo y caja de filtro
	const float zP = Org.Z + bore * (bV ? 1.2f : 2.05f), sgnIn = bV ? 0.f : -1.f;
	const float xT = Org.X - hl * 0.82f - 2.f;
	const FVector AirBox(Org.X - hl - 14.f, Org.Y + 20.f, Org.Z + bore * 1.5f);
	if (es < 5)
	{
	B.SuperBox(SAlu, FVector(Org.X, Org.Y + sgnIn * bore * 0.5f, zP), FVector(hl * 0.82f, bore * 0.38f, bore * 0.28f), 0.45f, 10);
	for (int32 b = 0; b < banks; ++b) for (int32 c = 0; c < perBank; ++c)
	{
		const float cx = Org.X - hl + 6.f + c * pitch + pitch * 0.15f; const float sg = bV ? (b == 0 ? -1.f : 1.f) : -1.f;
		B.TubeVar(SAlu, FDustlineMeshBuilder::Smooth({ FVector(cx, Org.Y + sgnIn * bore * 0.5f, zP), FVector(cx, Org.Y + sg * bore * 0.95f, zP - bore * 0.15f), FVector(cx, Org.Y + sg * bore * 1.05f, Org.Z + bore * 0.60f) }, 5), bore * 0.20f, bore * 0.27f, 10, false);
	}
	B.Cylinder(SAlu, FVector(xT, Org.Y + sgnIn * bore * 0.5f, zP), FVector(-1, 0, 0), bore * 0.34f, bore * 0.5f, 16, true);
	B.Torus(SChrome, FVector(xT - bore * 0.5f, Org.Y + sgnIn * bore * 0.5f, zP), FVector(1, 0, 0), bore * 0.34f, 0.5f, 20, 5);
	B.SuperBox(SDark, AirBox, FVector(12.f, 8.f, 7.f), 0.3f, 8);
	B.Box(SMetal, AirBox + FVector(0, 0, 7.4f), FVector(11.f, 7.f, 0.3f));
	B.TubeVar(SDark, FDustlineMeshBuilder::Smooth({ FVector(xT - bore * 0.5f, Org.Y + sgnIn * bore * 0.5f, zP), FVector(xT - 10.f, Org.Y + 8.f, zP + 3.f), AirBox + FVector(10.f, -6.f, 0) }, 6), bore * 0.30f, bore * 0.30f, 14, true);
	}
	// rasgos propios de cada familia de motor
	{
		const float zT = Org.Z + bore * 1.95f;
		switch (es)
		{
		case 0:   // compacto: tapa de plástico lisa
			B.SuperBox(SDark, FVector(Org.X, Org.Y, zT), FVector(hl * 0.9f, bore * 0.92f, 1.5f), 0.3f, 8);
			B.Box(SMetal, FVector(Org.X, Org.Y, zT + 1.6f), FVector(hl * 0.3f, bore * 0.3f, 0.25f));
			break;
		case 1:   // turismo: cubierta decorativa con nervios y emblema
			B.SuperBox(SDark, FVector(Org.X, Org.Y, zT + 0.5f), FVector(hl * 0.96f, bore * 1.08f, 2.6f), 0.26f, 8);
			for (int32 q = 0; q < 5; ++q) B.Box(SMetal, FVector(Org.X - hl * 0.62f + q * hl * 0.31f, Org.Y, zT + 3.1f), FVector(0.4f, bore * 0.92f, 0.2f));
			B.SuperBox(SChrome, FVector(Org.X, Org.Y, zT + 3.3f), FVector(4.f, 2.4f, 0.3f), 0.4f, 6);
			break;
		case 2:   // V6 deportivo: pleno de admisión pulido con trompas cromadas
			B.SuperBox(SAlu, FVector(Org.X, Org.Y, zT + 1.5f), FVector(hl * 0.82f, bore * 0.88f, bore * 0.42f), 0.35f, 10);
			for (int32 sgn2 = -1; sgn2 <= 1; sgn2 += 2) B.Cylinder(SChrome, FVector(Org.X - hl * 0.8f, Org.Y + sgn2 * bore * 0.38f, zT + 1.5f), FVector(-1, 0, 0), 2.2f, 7.f, 14, true);
			break;
		case 3:   // V8 todoterreno: filtro de aire redondo clásico y poleas robustas
			B.Cylinder(SChrome, FVector(Org.X, Org.Y, zT + 1.f), FVector(0, 0, 1), bore * 2.1f, 5.5f, 28, true);
			B.Cylinder(SDark, FVector(Org.X, Org.Y, zT + 6.4f), FVector(0, 0, 1), bore * 1.9f, 1.2f, 28, true);
			B.Torus(SMetal, FVector(Org.X, Org.Y, zT + 6.f), FVector(0, 0, 1), bore * 1.95f, 0.6f, 28, 6);
			break;
		case 4:   // V8 alto rendimiento: filtro cónico rojo de admisión directa
			B.Lathe(SAccent, FVector(Org.X - hl - 8.f, Org.Y - 24.f, Org.Z + bore * 1.3f), FVector(-1, 0, 0), { P2(0, bore * 1.0f), P2(10.f, bore * 1.7f) }, 24);
			B.Torus(SChrome, FVector(Org.X - hl - 8.f, Org.Y - 24.f, Org.Z + bore * 1.3f), FVector(1, 0, 0), bore * 1.0f, 0.8f, 20, 5);
			B.TubeVar(SChrome, FDustlineMeshBuilder::Smooth({ FVector(Org.X - hl - 8.f, Org.Y - 24.f, Org.Z + bore * 1.3f), FVector(Org.X - hl * 0.7f, Org.Y - 14.f, zP), FVector(xT, Org.Y + sgnIn * bore * 0.5f, zP) }, 6), bore * 0.62f, bore * 0.62f, 14, true);
			break;
		case 5:   // V8 de competición: trompetas de admisión individuales
			for (int32 b2 = 0; b2 < banks; ++b2) for (int32 c2 = 0; c2 < perBank; ++c2)
			{
				const float cx2 = Org.X - hl + 6.f + c2 * pitch + pitch * 0.15f, sg2 = banks == 2 ? (b2 == 0 ? -1.f : 1.f) : 0.f;
				B.Lathe(SChrome, FVector(cx2, Org.Y + sg2 * bore * 0.75f, Org.Z + bore * 1.75f), FVector(0, 0, 1), { P2(0, bore * 0.42f), P2(bore * 0.30f, bore * 0.52f), P2(bore * 0.75f, bore * 0.86f) }, 18);
			}
			break;
		default:  // V10 extremo: supercargador Roots sobre el motor con polea y toma frontal
			B.SuperBox(SChrome, FVector(Org.X, Org.Y, zT + 1.f), FVector(hl * 0.86f, bore * 1.25f, bore * 0.85f), 0.3f, 10);
			for (int32 q = 0; q < 7; ++q) B.Box(SMetal, FVector(Org.X - hl * 0.7f + q * hl * 0.23f, Org.Y, zT + 1.f + bore * 0.88f), FVector(0.45f, bore * 1.1f, 0.3f));
			B.Cylinder(SAlu, FVector(Org.X + hl * 0.86f, Org.Y, zT + 1.f), FVector(1, 0, 0), bore * 0.55f, 5.f, 20, true);
			B.Lathe(SBlock, FVector(Org.X + hl * 0.86f + 5.f, Org.Y, zT + 1.f), FVector(1, 0, 0), { P2(0, bore * 0.7f), P2(1.2f, bore * 0.7f), P2(1.2f, bore * 0.4f) }, 24);
			B.SuperBox(SDark, FVector(Org.X - hl * 0.2f, Org.Y, zT + 1.f + bore * 1.25f), FVector(hl * 0.42f, bore * 0.75f, bore * 0.38f), 0.3f, 8);   // toma de aire
			break;
		}
	}
	// escape: colectores individuales hacia un colector común
	TArray<FVector> Collectors;
	for (int32 b = 0; b < banks; ++b)
	{
		const float sg = bV ? (b == 0 ? -1.f : 1.f) : 1.f;
		for (int32 c = 0; c < perBank; ++c)
		{
			const float cx = Org.X - hl + 6.f + c * pitch + pitch * 0.15f, zc = Org.Z + bore * 0.55f;
			B.TubeVar(SMetal, FDustlineMeshBuilder::Smooth({ FVector(cx, Org.Y + sg * bore * 1.05f, zc), FVector(cx - 1.f, Org.Y + sg * bore * 1.65f, zc - bore * 0.35f), FVector(Org.X - hl * 0.6f + c * 1.2f, Org.Y + sg * bore * 1.55f, Org.Z - bore * 1.1f) }, 5), bore * 0.26f, bore * 0.24f, 8, false);
			B.SuperBox(SMetal, FVector(cx, Org.Y + sg * bore * 1.08f, zc), FVector(1.8f, 0.5f, 2.4f), 0.3f, 4);   // brida
		}
		Collectors.Add(FVector(Org.X - hl * 0.6f + perBank * 0.6f, Org.Y + sg * bore * 1.55f, Org.Z - bore * 1.1f));
		B.Lathe(SMetal, FVector(Org.X - hl * 0.6f, Org.Y + sg * bore * 1.55f, Org.Z - bore * 1.1f), FVector(-1, 0, 0), { P2(0, bore * 0.5f), P2(bore * 0.9f, bore * 0.32f) }, 12);
	}
	// accesorios en la parte delantera del motor
	const float xA = Org.X + hl + 1.f;
	B.Lathe(SBlock, FVector(xA, 0, Org.Z - bore * 0.1f), FVector(1, 0, 0), { P2(0, bore * 0.9f), P2(2.f, bore * 0.9f), P2(2.f, bore * 0.5f), P2(5.f, bore * 0.5f), P2(5.f, bore * 0.7f), P2(7.f, bore * 0.7f) }, 28);   // polea/damper
	struct FAcc { float y, z, r, l; int32 id; };
	const FAcc Acc[] = { { bore * 1.3f, Org.Z + bore * 1.1f, bore * 0.55f, 9.f, SDark }, { -bore * 1.1f, Org.Z + bore * 0.5f, bore * 0.42f, 7.f, SAlu }, { bore * 0.95f, Org.Z - bore * 0.9f, bore * 0.38f, 7.f, SMetal }, { 0.f, Org.Z + bore * 1.5f, bore * 0.30f, 6.f, SChrome } };
	for (const FAcc& A : Acc) { B.Cylinder(A.id, FVector(xA - 3.f, A.y, A.z), FVector(1, 0, 0), A.r, A.l, 18, true); B.Cylinder(SBlock, FVector(xA + A.l - 3.f, A.y, A.z), FVector(1, 0, 0), A.r * 0.8f, 3.f, 18, true); }
	TArray<FVector> Belt;   // correa serpentín
	const float xb = xA + 6.4f;
	Belt.Add(FVector(xb, 0, Org.Z - bore * 0.1f - bore * 0.7f)); Belt.Add(FVector(xb, bore * 0.95f, Org.Z - bore * 0.9f - bore * 0.38f)); Belt.Add(FVector(xb, bore * 1.3f + bore * 0.55f, Org.Z + bore * 1.1f)); Belt.Add(FVector(xb, 0, Org.Z + bore * 1.5f + bore * 0.30f));
	Belt.Add(FVector(xb, -bore * 1.1f - bore * 0.42f, Org.Z + bore * 0.5f)); Belt.Add(FVector(xb, -bore * 0.7f, Org.Z - bore * 0.5f)); { const FVector B0 = Belt[0]; Belt.Add(B0); }
	B.Tube(SDark, FDustlineMeshBuilder::Smooth(Belt, 5), 0.9f, 6);
	// radiador, ventilador, mangueras, batería
	const float xr = xFront + (GetModel(Setup.Model).L * 0.5f - xFront) - 30.f;
	const float rw = M.W * 0.27f, rz = gc + 0.24f * (zTopHood - gc) + 12.f;
	B.Box(SDark, FVector(xr, 0, rz), FVector(3.2f, rw, 15.f));
	for (int32 i = -9; i <= 9; ++i) B.Box(SAlu, FVector(xr + 3.4f, i * rw / 9.5f, rz), FVector(0.25f, 0.3f, 14.5f));
	B.SuperBox(SAlu, FVector(xr, 0, rz + 16.f), FVector(3.4f, rw + 1.f, 2.6f), 0.35f, 6); B.SuperBox(SAlu, FVector(xr, 0, rz - 16.f), FVector(3.4f, rw + 1.f, 2.6f), 0.35f, 6);
	B.Torus(SDark, FVector(xr - 4.f, 0, rz), FVector(1, 0, 0), 15.f, 1.2f, 28, 6);
	for (int32 k = 0; k < 7; ++k) { B.Xf = FTransform(FQuat(FVector(1, 0, 0), 2 * PI * k / 7)) * FTransform(FVector(xr - 6.f, 0, rz)) * Keep; B.SuperBox(SDark, FVector(0, 0, 8.f), FVector(0.5f, 3.4f, 7.f), 0.4f, 5); }
	B.Xf = Keep; B.Cylinder(SMetal, FVector(xr - 7.f, 0, rz), FVector(1, 0, 0), 2.6f, 3.f, 12, true);
	B.TubeVar(SDark, FDustlineMeshBuilder::Smooth({ FVector(xr - 3.f, rw * 0.8f, rz + 16.f), FVector(xr - 20.f, 10.f, rz + 12.f), FVector(Org.X + hl * 0.4f, bore * 1.0f, Org.Z + bore * 0.2f) }, 5), 2.1f, 2.1f, 10, true);
	B.TubeVar(SDark, FDustlineMeshBuilder::Smooth({ FVector(xr - 3.f, -rw * 0.8f, rz - 16.f), FVector(xr - 20.f, -10.f, rz - 10.f), FVector(Org.X + hl * 0.8f, -bore * 1.0f, Org.Z - bore * 1.2f) }, 5), 2.1f, 2.1f, 10, true);
	B.Box(SDark, FVector(Org.X - hl - 20.f, -24.f, Org.Z + bore * 0.9f), FVector(10.f, 7.f, 8.f));                               // batería
	B.Cylinder(SAccent, FVector(Org.X - hl - 26.f, -28.f, Org.Z + bore * 0.9f + 8.f), FVector(0, 0, 1), 1.1f, 1.4f, 8, true); B.Cylinder(SChrome, FVector(Org.X - hl - 14.f, -28.f, Org.Z + bore * 0.9f + 8.f), FVector(0, 0, 1), 1.1f, 1.4f, 8, true);
	B.Cylinder(SHose, FVector(Org.X + hl * 0.3f, -bw - 3.f, Org.Z - bore * 0.6f), FVector(0, 1, 0), 3.6f, 7.f, 14, true);         // filtro de aceite
	for (int32 s = -1; s <= 1; s += 2) B.SuperBox(SDark, FVector(Org.X - hl * 0.2f, s * (bw + 3.f), Org.Z - bore * 1.4f), FVector(5.f, 4.f, 4.f), 0.4f, 6);   // soportes motor

	// ---- turbocompresor(es) + intercooler (aparecen con el slider)
	const float tb = Setup.Turbo;
	if (tb > 0.05f)
	{
		const int32 count = tb >= 2.7f ? 3 : (tb >= 1.7f ? 2 : 1); const int32 cMat = tb >= 2.2f ? SChrome : SAlu; const float r = 5.f + tb * 2.8f;
		const float xi = xr + 18.f, zi = rz - 3.f;   // intercooler delante del radiador
		const float icw = M.W * (0.17f + 0.06f * tb), ich = 11.f + tb * 1.5f;
		B.Box(SAlu, FVector(xi, 0, zi), FVector(3.6f + tb * 0.6f, icw, ich));
		for (int32 i = -12; i <= 12; ++i) B.Box(SDark, FVector(xi + 4.4f + tb * 0.6f, i * icw / 12.5f, zi), FVector(0.3f, 0.35f, ich - 0.6f));
		for (int32 s = -1; s <= 1; s += 2) { B.SuperBox(SAlu, FVector(xi, s * (icw + 3.2f), zi), FVector(4.f + tb * 0.5f, 3.6f, ich + 2.f), 0.4f, 8); B.Cylinder(SAlu, FVector(xi, s * (icw + 6.f), zi + (s > 0 ? 5.f : -5.f)), FVector(0, s, 0), 3.0f, 6.f, 14, true); }
		for (int32 k = 0; k < count; ++k)
		{
			const float sgn = (count == 1) ? 1.f : (k == 0 ? -1.f : 1.f);   // el tercero va centrado y alto
			FVector TP(Org.X - hl * 0.12f, Org.Y + sgn * (bore * 2.35f + r * 1.15f), Org.Z + bore * 0.35f);
			if (count == 3 && k == 2) { TP.Y = Org.Y; TP.Z += bore * 1.4f + r * 1.3f; }
			// turbina (lado caliente): voluta en espiral
			TArray<FVector> VoluteH; for (int32 a = 0; a <= 24; ++a) { const float t = a / 24.f * 5.4f; VoluteH.Add(TP + FVector(-r * 0.9f, FMath::Cos(t) * r * 0.80f, FMath::Sin(t) * r * 0.80f)); }
			B.TubeVar(STurbo, VoluteH, r * 0.20f, r * 0.50f, 12, true);
			B.Lathe(STurbo, TP + FVector(-r * 0.95f, 0, 0), FVector(-1, 0, 0), { P2(0, r * 0.9f), P2(r * 0.5f, r * 0.55f), P2(r * 1.2f, r * 0.34f), P2(r * 1.9f, r * 0.30f) }, 20);   // salida
			B.Torus(SChrome, TP + FVector(-r * 2.6f, 0, 0), FVector(1, 0, 0), r * 0.36f, 0.8f, 20, 5);                                                                               // abrazadera V-band
			B.Disc(SMetal, TP + FVector(-r * 0.30f, 0, 0), FVector(-1, 0, 0), r * 0.38f, r * 1.1f, 24);                                                                               // escudo térmico
			// cartucho central + voluta del compresor
			B.Cylinder(SAlu, TP + FVector(-r * 0.30f, 0, 0), FVector(1, 0, 0), r * 0.50f, r * 1.2f, 20, true);
			for (int32 f = 0; f < 3; ++f) B.Torus(SBlock, TP + FVector(-r * 0.05f + f * r * 0.3f, 0, 0), FVector(1, 0, 0), r * 0.50f, 0.5f, 20, 5);
			TArray<FVector> VoluteC; for (int32 a = 0; a <= 24; ++a) { const float t = a / 24.f * 5.4f; VoluteC.Add(TP + FVector(r * 1.05f, FMath::Cos(t) * r * 0.78f, FMath::Sin(t) * r * 0.78f)); }
			B.TubeVar(cMat, VoluteC, r * 0.20f, r * 0.48f, 12, true);
			B.Lathe(cMat, TP + FVector(r * 0.95f, 0, 0), FVector(1, 0, 0), { P2(0, r * 0.95f), P2(r * 0.4f, r * 0.70f), P2(r * 0.9f, r * 0.52f), P2(r * 1.4f, r * 0.48f) }, 24);       // entrada
			B.Torus(SChrome, TP + FVector(r * 2.4f, 0, 0), FVector(1, 0, 0), r * 0.50f, 0.7f, 20, 5);
			B.TubeVar(SHose, { TP + FVector(r * 2.4f, 0, 0), FVector(AirBox.X + 6.f, AirBox.Y - 6.f, AirBox.Z), }, r * 0.46f, r * 0.46f, 12, true);
			// wastegate + válvula de descarga
			B.Lathe(SAlu, TP + FVector(-r * 0.8f, sgn * r * 1.6f, r * 0.9f), FVector(0, sgn, 0), { P2(0, r * 0.35f), P2(r * 0.3f, r * 0.55f), P2(r * 0.9f, r * 0.55f), P2(r * 1.2f, r * 0.30f) }, 16);
			B.Tube(SChrome, { TP + FVector(-r * 0.8f, sgn * r * 1.6f, r * 0.9f), TP + FVector(-r * 0.9f, sgn * r * 0.9f, r * 0.55f) }, 0.4f, 5);
			B.Cylinder(SAccent, TP + FVector(r * 1.4f, 0, r * 1.15f), FVector(0, 0, 1), r * 0.32f, tb >= 2.2f ? r * 1.7f : r * 0.8f, 14, true);
			if (tb >= 2.2f) for (int32 hw = 0; hw < 3; ++hw) B.Torus(SCarbon, TP + FVector(-r * (1.0f + hw * 0.55f), 0, 0), FVector(1, 0, 0), r * 0.62f, r * 0.20f, 18, 6);   // fundas térmicas
			// líneas de aceite
			B.Tube(SChrome, FDustlineMeshBuilder::Smooth({ TP + FVector(-r * 0.1f, 0, r * 0.52f), TP + FVector(-r * 0.3f, -sgn * r * 0.6f, r * 1.6f), FVector(Org.X, Org.Y + sgn * bore * 1.3f, Org.Z + bore * 0.2f) }, 5), 0.45f, 5);
			// tubos: colector -> turbina; compresor -> intercooler -> admisión
			const FVector Coll = Collectors.Num() ? Collectors[FMath::Min(k, Collectors.Num() - 1)] : FVector(Org.X, Org.Y, Org.Z);
			B.TubeVar(STurbo, FDustlineMeshBuilder::Smooth({ Coll, FVector(Coll.X - r * 0.2f, TP.Y, TP.Z - r * 0.4f), TP + FVector(-r * 0.9f, 0, -r * 0.6f) }, 6), r * 0.30f, r * 0.30f, 12, true);
			B.TubeVar(SHose, FDustlineMeshBuilder::Smooth({ TP + FVector(r * 1.05f, 0, r * 0.82f), FVector(xr + 6.f, sgn * (icw * 0.9f), zi + ich + 8.f), FVector(xi, sgn * (icw + 6.f), zi + (sgn > 0 ? 5.f : -5.f)) }, 8), r * 0.28f, r * 0.28f, 12, true);
			for (float tt : { 0.12f, 0.85f }) B.Torus(SChrome, FVector(xr + 6.f, sgn * icw * 0.9f, zi + ich + 8.f) * 0.0f + FMath::Lerp(TP + FVector(r * 1.05f, 0, r * 0.82f), FVector(xi, sgn * (icw + 6.f), zi + 5.f), tt), FVector(1, 0, 0), r * 0.30f, 0.45f, 16, 5);
		}
		if (tb >= 2.7f)   // botella de óxido nitroso
		{
			const FVector NP(Org.X - hl - 8.f, Org.Y + 36.f, Org.Z + bore * 0.4f);
			B.Cylinder(STurbo, NP, FVector(1, 0, 0), 5.2f, 30.f, 18, true); B.Sphere(STurbo, NP + FVector(30.f, 0, 0), 5.2f, 12);
			B.Cylinder(SChrome, NP + FVector(-3.f, 0, 0), FVector(1, 0, 0), 1.6f, 4.f, 10, true);
			B.Tube(SChrome, FDustlineMeshBuilder::Smooth({ NP + FVector(-3.f, 0, 0), FVector(Org.X - hl - 4.f, Org.Y + 20.f, Org.Z + bore * 1.2f), FVector(xT, Org.Y + sgnIn * bore * 0.5f, zP) }, 5), 0.7f, 6);
		}
		B.TubeVar(SHose, FDustlineMeshBuilder::Smooth({ FVector(xi, -(icw + 6.f), zi - 5.f), FVector(xr + 10.f, -icw * 0.6f, zi + ich + 14.f), FVector(xT, Org.Y + sgnIn * bore * 0.5f, zP + 2.f) }, 8), r * 0.27f, r * 0.27f, 12, true);
		for (int32 q = 0; q < 2; ++q) B.Torus(SChrome, FVector(xi, -(icw + 6.f), zi - 5.f) + FVector(-0.f, -q * 2.f, 0), FVector(0, 1, 0), 3.0f + 0.6f, 0.5f, 16, 5);
	}
	B.Build(Engine, Mats, false);
}
