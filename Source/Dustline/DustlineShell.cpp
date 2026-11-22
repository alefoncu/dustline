#include "DustlineCar.h"
#include "DustlineCarParts.h"

// Carrocerías v2 (Clase G, Urus, Panda 4x4, Defender): cada modelo se describe con sus cotas reales
// (perfil del techo/capó, línea de cintura, bajos, anchura en planta con esquinas redondeadas, voladizos),
// y se genera por secciones transversales: bajos, pasos de rueda con la forma propia del modelo, costado con
// pliegue, hombro, cintura, ventanas hundidas con juntas, pilares, techo y lunas. Luego, frontal y trasera propios.

namespace
{
	float Sm(float x) { x = FMath::Clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }

	float PL(const TArray<FVector2D>& P, float x)
	{
		if (x <= P[0].X) return P[0].Y;
		for (int32 i = 0; i < P.Num() - 1; ++i) if (x <= P[i + 1].X) { const float d = P[i + 1].X - P[i].X; return d < 1e-5f ? P[i + 1].Y : FMath::Lerp((float)P[i].Y, (float)P[i + 1].Y, (x - P[i].X) / d); }
		return P.Last().Y;
	}

	enum class EArch : uint8 { Round, RRect, Hex };

	struct FShellSpec
	{
		float XOff = 0.f;                              // centro de la carrocería respecto al centro de la batalla (voladizos distintos)
		TArray<FVector2D> Top, Sh, Bot;                // (x desde la trasera, z de serie) en cm
		bool bSmoothTop = false;
		float Half = 90.f, RcF = 10.f, RcR = 10.f;      // semiancho y radio de las esquinas en planta
		TArray<FVector> Haunch;                        // (x, ensanche, semilongitud)
		float RoofIn = 0.9f, BeltStep = 2.f, Rb = 3.f, Rs = 4.f, Rr = 5.f, CrownH = 1.5f, CrownR = 2.f, Bulge = 0.5f, Tuck = 2.f;
		float CreaseZ = 0.f, CreaseZR = -1.f, CreaseD = 0.f, CreaseW = 4.f;   // pliegue lateral (delante / detrás)
		float CladZ = 0.f;                             // revestimiento negro inferior
		bool bNoseDark = false;                        // morro negro hasta el pliegue, delante de la rueda delantera
		float SmoothDeg = 30.f;
		EArch Arch = EArch::Round; float ArchGap = 5.f, ArchW = 1.f, ArchH = 1.f, ArchRc = 0.3f;
		int32 FlareMat = -1; float FlareOut = 5.f, FlareT = 6.f;
		TArray<FVector2D> Win, DarkPil, TopGlass;      // rangos en x
		TArray<FVector2D> Gaps;                        // (x, 1 = sube hasta el techo)
		int32 BeltMat = SPaint, RoofMat = SPaint, FrameMat = SPaint;
		float Frame = 3.f;                             // chapa entre el cristal y el borde del techo
		float FrameBot = 0.6f;                         // chapa entre la cintura y el cristal
	};

	TArray<FVector2D> V2(std::initializer_list<float> L) { TArray<FVector2D> R; const float* p = L.begin(); for (size_t i = 0; i + 1 < L.size(); i += 2) R.Add(FVector2D(p[i], p[i + 1])); return R; }

	FShellSpec MakeSpec(int32 St)
	{
		FShellSpec D;
		switch (St)
		{
		case 0: // Mercedes Clase G (W463A): caja de 4,66 m, capó plano a 1,27 m, parabrisas casi vertical, techo a 1,97 m
			// cotas medidas sobre foto lateral (G 63, Ginebra 2018): chapa de 4,15 m, voladizos 64/62, techo 198, ventanas 129-180
			D.XOff = -1.f;
			D.Top = V2({ 0,187, 1,193, 3,196.5f, 6,198, 248,198, 251,196.5f, 254,192, 256,188, 288,132, 292,131, 330,129, 380,126, 405,123.5f, 411,122, 415,118 });
			D.Sh = V2({ 0,127, 286,127, 288,132, 415,132 });
			D.Bot = V2({ 0,54, 8,49, 30,48, 390,48, 405,50, 415,56 });
			D.Half = 94.f; D.RcF = 7.f; D.RcR = 5.f;
			D.RoofIn = 0.95f; D.BeltStep = 1.5f; D.Rb = 3.f; D.Rs = 3.5f; D.Rr = 4.5f; D.CrownH = 1.5f; D.CrownR = 2.f; D.Bulge = 0.6f; D.Tuck = 2.5f;
			D.CreaseZ = 104.f; D.CreaseD = 1.0f; D.CreaseW = 3.f; D.SmoothDeg = 22.f;
			D.Arch = EArch::Round; D.ArchGap = 7.f; D.FlareMat = SPaint; D.FlareOut = 6.f; D.FlareT = 6.f;
			D.Win = V2({ 9,90, 109,174, 195,272 }); D.TopGlass = V2({ 256,288 }); D.Frame = 13.5f; D.FrameBot = 2.5f;
			D.Gaps = V2({ 101,1, 184.5f,1, 286,0 });
			break;
		case 1: // Lamborghini Urus: 5,11 m, morro bajo, parabrisas muy tumbado, techo de coupé, cola alta, pasos hexagonales
			D.XOff = -3.5f; D.bSmoothTop = true;
			// según fotos: cola alta con luna corta y empinada que acaba en alerón (~1,28 m) y portón vertical; hombro muy ancho
			// sobre la rueda trasera con habitáculo estrecho encima; mitad inferior de frontal/trasera, faldones y pasos en negro
			D.Top = V2({ 0,128, 3,134, 10,137, 22,138.5f, 30,139.5f, 40,141, 70,148, 110,154, 150,158, 190,162, 220,164, 265,164, 295,162.5f, 310,160, 368,120, 380,118, 420,112, 460,104, 490,98, 503,95, 509,92, 511,88 });
			D.Sh = V2({ 0,137, 60,138, 128,137, 167,129, 261,121, 331,118, 368,117, 511,100 });
			D.Bot = V2({ 0,40, 15,33, 40,30, 470,30, 500,34, 511,40 });
			D.Half = 100.f; D.RcF = 32.f; D.RcR = 24.f;
			D.Haunch = { FVector(112.f, 7.f, 95.f), FVector(409.f, 3.f, 65.f) };
			D.RoofIn = 0.64f; D.BeltStep = 9.f; D.Rb = 5.f; D.Rs = 6.f; D.Rr = 9.f; D.CrownH = -2.5f; D.CrownR = 5.f; D.Bulge = 2.f; D.Tuck = 6.f;
			D.CreaseZ = 82.f; D.CreaseZR = 95.f; D.CreaseD = 2.0f; D.CreaseW = 3.f; D.CladZ = 47.f; D.bNoseDark = true; D.SmoothDeg = 34.f;
			D.Arch = EArch::Hex; D.ArchGap = 3.5f; D.ArchW = 1.04f; D.ArchH = 1.0f; D.FlareMat = SDark; D.FlareOut = 3.5f; D.FlareT = 5.f;
			D.Win = V2({ 128,160, 168,250, 260,345 }); D.DarkPil = V2({ 250,260, 160,168 }); D.TopGlass = V2({ 40,150, 310,368 });
			D.Gaps = V2({ 164,1, 255,1, 372,0 });
			D.RoofMat = SCarbon; D.FrameMat = SDark; D.Frame = 2.5f;
			break;
		case 2: // Fiat Panda 4x4 (2012+): 3,68 m, alto y corto, morro corto, formas "squircle", revestimientos negros
			D.XOff = 2.5f; D.bSmoothTop = true;
			D.Top = V2({ 0,128, 4,140, 9,148, 15,153, 24,154, 205,154, 222,153, 235,151, 312,98, 335,95, 352,91, 362,86, 366,81, 368,75 });
			D.Sh = V2({ 0,103, 60,102, 200,99, 312,97, 368,97 });
			D.Bot = V2({ 0,38, 12,31, 35,27, 340,27, 358,32, 368,40 });
			D.Half = 82.f; D.RcF = 26.f; D.RcR = 16.f;
			D.RoofIn = 0.84f; D.BeltStep = 2.f; D.Rb = 6.f; D.Rs = 8.f; D.Rr = 10.f; D.CrownH = 3.f; D.CrownR = 4.f; D.Bulge = 1.5f; D.Tuck = 3.f;
			D.CladZ = 52.f; D.SmoothDeg = 50.f;
			D.Arch = EArch::Round; D.ArchGap = 5.f; D.FlareMat = SDark; D.FlareOut = 6.f; D.FlareT = 7.f;
			D.Win = V2({ 48,116, 126,196, 205,305 }); D.DarkPil = V2({ 116,126, 196,205 }); D.TopGlass = V2({ 3,24, 232,314 }); D.Frame = 4.f; D.FrameMat = SDark;
			D.Gaps = V2({ 121,1, 200.5f,1, 312,0 });
			break;
		case 5: // Pick-up doble cabina: caja de carga abierta, capó alto y largo
			D.XOff = -15.f;
			D.Top = V2({ 0,85, 190,85, 190.5f,170, 192,180, 196,184, 204,185.5f, 328,185.5f, 336,183, 396,128, 402,125.5f, 480,123, 515,120, 525,115, 530,106 });
			D.Sh = V2({ 0,85, 190,85, 190.6f,126, 530,126 });
			D.Bot = V2({ 0,50, 12,45, 40,42, 500,42, 520,46, 530,54 });
			D.Half = 93.f; D.RcF = 10.f; D.RcR = 4.f;
			D.RoofIn = 0.88f; D.BeltStep = 2.5f; D.Rb = 3.f; D.Rs = 5.f; D.Rr = 7.f; D.CrownH = 2.f; D.CrownR = 3.f; D.Bulge = 1.f; D.Tuck = 3.f;
			D.CreaseZ = 100.f; D.CreaseD = 1.f; D.CreaseW = 3.f; D.SmoothDeg = 30.f;
			D.Arch = EArch::Round; D.ArchGap = 6.f; D.FlareMat = SDark; D.FlareOut = 6.f; D.FlareT = 7.f;
			D.Win = V2({ 216,290, 302,392 }); D.TopGlass = V2({ 336,397 }); D.Frame = 4.f;
			D.Gaps = V2({ 211,1, 296,1, 399,0 });
			break;
		case 6: // Rock crawler: bañera abierta sobre neumáticos de 49", aletas tubulares altas
			D.Top = V2({ 0,100, 3,104, 8,105, 215,105, 240,110, 370,112, 382,111, 388,107, 390,100 });
			D.Sh = V2({ 0,140, 390,140 });
			D.Bot = V2({ 0,62, 10,58, 380,58, 390,62 });
			D.Half = 82.f; D.RcF = 8.f; D.RcR = 8.f;
			D.Rb = 3.f; D.Rs = 6.f; D.CrownH = 2.f; D.Bulge = 0.5f; D.Tuck = 2.f; D.SmoothDeg = 25.f;
			D.Arch = EArch::Round; D.ArchGap = 7.f; D.FlareMat = SDark; D.FlareOut = 6.f; D.FlareT = 3.f;
			break;
		default: // 3 Land Rover Defender 110 clásico: paneles planos, parabrisas vertical, techo blanco, pasos cuadrados
			// cotas medidas sobre foto lateral (Defender 110 Station Wagon): techo 189, cintura 124, bajos a 58-64, parabrisas con 18 cm de caída
			D.XOff = -17.5f;
			D.Top = V2({ 0,182, 1,187, 4,189, 8,189.5f, 271,189.5f, 274,188, 276,184, 277,176, 295,127.5f, 300,126, 330,125, 395,122, 410,120, 416,118, 420,114 });
			D.Sh = V2({ 0,124, 420,124 });
			D.Bot = V2({ 0,62, 6,58, 30,58, 400,58, 414,60, 420,64 });
			D.Half = 89.5f; D.RcF = 4.f; D.RcR = 4.f;
			D.RoofIn = 0.92f; D.BeltStep = 3.5f; D.Rb = 2.f; D.Rs = 2.5f; D.Rr = 6.f; D.CrownH = 0.5f; D.CrownR = 3.f; D.Bulge = 0.f; D.Tuck = 0.f;
			D.SmoothDeg = 18.f;
			D.Arch = EArch::RRect; D.ArchGap = 6.f; D.ArchW = 1.0f; D.ArchH = 1.05f; D.ArchRc = 0.32f; D.FlareMat = SDark; D.FlareOut = 6.f; D.FlareT = 9.f;
			D.Win = V2({ 15,115, 133,196, 214,280 }); D.TopGlass = V2({ 277,295 }); D.Frame = 17.f; D.FrameBot = 4.f;
			D.Gaps = V2({ 126.5f,1, 206,1, 295,0 });
			D.BeltMat = SDark; D.RoofMat = SRoof;
			break;
		}
		return D;
	}
}

float DustlineBodyXOff(int32 Style) { return Style != 4 ? MakeSpec(Style).XOff : 0.f; }

void ADustlineCar::BuildShellV2(FDustlineMeshBuilder& B)
{
	const FDustlineModel& M = GetModel(Setup.Model);
	FShellSpec D = MakeSpec(M.Style);
	const int32 St = M.Style;
	const float L = M.L, R = Setup.TireRadiusM * 100.f, Wd = Setup.TireWidthM * 100.f, gc = GroundClearance();
	const float gc0 = 0.40f * M.Radius * 100.f + 0.28f * M.Travel * 100.f, dz = gc - gc0;
	const FTransform Keep0(FVector(D.XOff, 0.f, 0.f));
	B.Xf = Keep0;
	const float xWF = WheelPos(0).X - D.XOff, xWR = WheelPos(2).X - D.XOff, wyAbs = FMath::Abs(WheelPos(0).Y);
	const float Ra = R + D.ArchGap;
	if (D.bSmoothTop)
	{
		TArray<FVector> C; for (const FVector2D& p : D.Top) C.Add(FVector(p.X, p.Y, 0));
		const TArray<FVector> S = FDustlineMeshBuilder::Smooth(C, 8);
		D.Top.Reset(); for (const FVector& v : S) if (D.Top.Num() == 0 || v.X > D.Top.Last().X + 1e-3f) D.Top.Add(FVector2D(v.X, v.Y));
	}

	// ------------------------------------------------------------------ funciones de forma
	auto XB = [&](float xr) { return xr - 0.5f * L; };
	auto Z = [&](float zs) { return zs + dz; };
	auto TopAt = [&](float xr) { return PL(D.Top, xr) + dz; };
	auto ShAt = [&](float xr) { return FMath::Min(TopAt(xr), PL(D.Sh, xr) + dz); };
	auto BotAt = [&](float xr) { return PL(D.Bot, xr) + dz; };
	auto Corner = [&](float d, float rc) { if (rc <= 0.f || d >= rc) return 0.f; const float q = rc - FMath::Max(d, 0.f); return rc - FMath::Sqrt(FMath::Max(0.f, rc * rc - q * q)); };
	auto WAt = [&](float xr)
	{
		float w = D.Half;
		for (const FVector& h : D.Haunch) w += h.Y * Sm(1.f - FMath::Abs(xr - h.X) / h.Z);
		return w - Corner(L - xr, D.RcF) - Corner(xr, D.RcR);
	};
	auto CreaseAt = [&](float xr) { return Z(FMath::Lerp(D.CreaseZR < 0.f ? D.CreaseZ : D.CreaseZR, D.CreaseZ, xr / L)); };
	auto SideW = [&](float xr, float z)
	{
		const float zb = BotAt(xr), zsh = ShAt(xr);
		const float t = FMath::Clamp((z - zb - D.Rb) / FMath::Max(1.f, zsh - D.Rs - zb - D.Rb), 0.f, 1.f);
		float y = WAt(xr) - D.Tuck * FMath::Pow(1.f - t, 3.f) + D.Bulge * 4.f * t * (1.f - t);
		if (D.CreaseD > 0.f) y += D.CreaseD * FMath::Max(0.f, 1.f - FMath::Abs(z - CreaseAt(xr)) / D.CreaseW);
		return y;
	};
	auto ArchTop = [&](float dx) -> float
	{
		const float a = FMath::Abs(dx);
		if (D.Arch == EArch::Round) return a < Ra ? FMath::Sqrt(Ra * Ra - a * a) : -1e9f;
		const float hw = Ra * D.ArchW, ht = Ra * D.ArchH;
		if (a >= hw) return -1e9f;
		if (D.Arch == EArch::RRect)
		{
			const float rc = Ra * D.ArchRc; if (a <= hw - rc) return ht;
			const float q = a - (hw - rc); return ht - rc + FMath::Sqrt(FMath::Max(0.f, rc * rc - q * q));
		}
		const float th = Ra * 0.42f, hb = Ra * 0.20f;
		return a <= th ? ht : FMath::Lerp(ht, hb, (a - th) / (hw - th));
	};
	auto ArchZ = [&](float xb) { float za = -1e9f; for (float wx : { xWF, xWR }) { const float t = ArchTop(xb - wx); if (t > -1e8f) za = FMath::Max(za, R + t); } return za; };
	auto InR = [](const TArray<FVector2D>& A, float x) { for (const FVector2D& r : A) if (x >= r.X && x <= r.Y) return true; return false; };
	auto GapAt = [&](float x, bool& bFull) { for (const FVector2D& g : D.Gaps) if (FMath::Abs(x - g.X) < 0.36f) { bFull = g.Y > 0.5f; return true; } return false; };

	// ------------------------------------------------------------------ estaciones (cortes en x)
	TArray<float> Xs;
	for (float x = 0.f; x < L; x += 2.5f) Xs.Add(x);
	Xs.Add(L);
	for (const FVector2D& p : D.Top) Xs.Add(p.X);
	for (const FVector2D& p : D.Sh) Xs.Add(p.X);
	for (const FVector2D& p : D.Bot) Xs.Add(p.X);
	for (const TArray<FVector2D>* A : { &D.Win, &D.DarkPil, &D.TopGlass }) for (const FVector2D& r : *A) { Xs.Add(r.X - 0.2f); Xs.Add(r.X + 0.2f); Xs.Add(r.Y - 0.2f); Xs.Add(r.Y + 0.2f); }
	for (const FVector2D& g : D.Gaps) { Xs.Add(g.X - 0.35f); Xs.Add(g.X + 0.35f); }
	for (float wx : { xWF, xWR })
	{
		const float hw = D.Arch == EArch::Round ? Ra : Ra * D.ArchW, c = wx + 0.5f * L;
		for (float s = -hw - 2.f; s <= hw + 2.f; s += 1.25f) Xs.Add(c + s);
		Xs.Add(c - hw + 0.05f); Xs.Add(c + hw - 0.05f); Xs.Add(c - hw - 0.05f); Xs.Add(c + hw + 0.05f);
	}
	for (float& x : Xs) x = FMath::Clamp(x, 0.f, L);
	Xs.Sort();
	{ TArray<float> X2; for (float x : Xs) if (X2.Num() == 0 || x - X2.Last() > 0.12f) X2.Add(x); if (L - X2.Last() > 1e-3f) X2.Add(L); else X2.Last() = L; Xs = X2; }
	const int32 NS = Xs.Num() - 1;
	constexpr int32 NH = 19, NR = 2 * NH - 2;

	// ------------------------------------------------------------------ secciones
	TArray<TArray<FVector>> P; P.SetNum(NS + 1);
	TArray<float> Kc; Kc.SetNum(NS + 1);
	const float c45 = 0.70711f;
	static const float F[8] = { 0.05f, 0.10f, 0.16f, 0.24f, 0.34f, 0.46f, 0.70f, 1.f };
	for (int32 i = 0; i <= NS; ++i)
	{
		const float xr = Xs[i], xb = XB(xr);
		const float zt = TopAt(xr), zsh = ShAt(xr), zb = BotAt(xr), w = WAt(xr);
		const float k = FMath::Clamp((zt - zsh) / 14.f, 0.f, 1.f);
		Kc[i] = k;
		const bool bOutboard = wyAbs - Wd * 0.5f > w - 1.f;
		float zw = bOutboard ? zb : FMath::Max(zb, ArchZ(xb));
		zw = FMath::Max(zb, FMath::Min(zw, zsh - D.Rs - 4.f));
		const float zLo = zw + D.Rb, zHi = FMath::Max(zsh - D.Rs, zLo + 2.f);
		float zc = D.CreaseD > 0.f ? CreaseAt(xr) : 0.5f * (zLo + zHi);
		zc = (zHi - zLo > 2.f) ? FMath::Clamp(zc, zLo + 0.8f, zHi - 0.8f) : 0.5f * (zLo + zHi);
		const float yLo = SideW(xr, zLo), yHi = SideW(xr, zHi), zS = zHi + D.Rs;
		const float yi = FMath::Clamp(FMath::Min(w - 14.f, wyAbs - Wd * 0.5f - 2.f), 6.f, FMath::Max(6.f, yLo - D.Rb - 1.f));
		FVector2D h[NH];
		h[0] = FVector2D(0, zb); h[1] = FVector2D(yi, zb); h[2] = FVector2D(yi, zw); h[3] = FVector2D(yLo - D.Rb, zw); h[4] = FVector2D(yLo, zLo);
		const float z5 = D.CladZ > 0.f ? FMath::Clamp(Z(D.CladZ), zLo + 0.4f, zc - 0.4f) : 0.5f * (zLo + zc), z7 = 0.5f * (zc + zHi);
		h[5] = FVector2D(SideW(xr, z5), z5); h[6] = FVector2D(SideW(xr, zc), zc); h[7] = FVector2D(SideW(xr, z7), z7); h[8] = FVector2D(yHi, zHi);
		h[9] = FVector2D(yHi - D.Rs + D.Rs * c45, zHi + D.Rs * c45); h[10] = FVector2D(yHi - D.Rs, zS);
		const float yH = yHi - D.Rs;
		// capó (k = 0) o habitáculo (k = 1)
		const bool bWin = k > 0.5f && InR(D.Win, xr);
		const float ins = bWin ? 1.3f : 0.f;
		const float cb = FMath::Max(yH - D.BeltStep, 4.f), zr = FMath::Max(zt, zS + 4.f);
		const float ct = FMath::Min(cb, FMath::Max(D.RoofIn * w, D.Rr + 2.f));
		const float zre = FMath::Max(zr - D.Rr, zS + 1.6f), zg0 = zS + (bWin ? D.FrameBot : 0.6f), zg1 = FMath::Max(zre - (bWin ? D.Frame : 0.6f), zg0 + 0.2f);
		auto GL = [&](float z) { return FMath::Lerp(cb, ct, FMath::Clamp((z - zS) / FMath::Max(0.5f, zre - zS), 0.f, 1.f)); };
		const FVector2D cab[8] = { FVector2D(cb, zS), FVector2D(GL(zg0) - ins, zg0), FVector2D(GL(zg1) - ins, zg1), FVector2D(ct, zre),
			FVector2D(ct - D.Rr + D.Rr * c45, zre + D.Rr * c45), FVector2D(ct - D.Rr, zre + D.Rr), FVector2D((ct - D.Rr) * 0.5f, zre + D.Rr + 0.75f * D.CrownR), FVector2D(0, zre + D.Rr + D.CrownR) };
		for (int32 q = 0; q < 8; ++q)
		{
			const FVector2D hood(yH * (1.f - F[q]), zS + D.CrownH * (1.f - FMath::Square(1.f - F[q])));
			h[11 + q] = FMath::Lerp(hood, cab[q], (double)k);
		}
		P[i].SetNum(NR);
		for (int32 j = 0; j < NH; ++j) P[i][j] = FVector(xb, h[j].X, h[j].Y);
		for (int32 j = NH; j < NR; ++j) { const int32 src = 2 * NH - 2 - j; P[i][j] = FVector(xb, -h[src].X, h[src].Y); }
	}

	// ------------------------------------------------------------------ materiales por cara
	auto MatOf = [&](int32 i, int32 j) -> int32
	{
		const int32 sg = j <= NH - 2 ? j : NR - 1 - j, j2 = (j + 1) % NR;
		const float xm = 0.5f * (Xs[i] + Xs[i + 1]);
		const float kmin = FMath::Min(Kc[i], Kc[i + 1]), kmax = FMath::Max(Kc[i], Kc[i + 1]);
		bool bFull = false; const bool bGap = GapAt(xm, bFull);
		const bool bCab = kmin > 0.5f;
		if (sg <= 2) return SDark;
		if (sg <= 9)
		{
			if (bGap) return SDark;
			if ((D.CladZ > 0.f || D.bNoseDark) && sg <= 7)
			{
				const float zq = FMath::Max(FMath::Max(P[i][j].Z, P[i][j2].Z), FMath::Max(P[i + 1][j].Z, P[i + 1][j2].Z));
				if (D.CladZ > 0.f && zq < Z(D.CladZ) + 0.5f) return SDark;
				if (D.bNoseDark && sg <= 5 && XB(xm) > xWF + Ra * D.ArchW + 2.f) return SDark;
			}
			return SPaint;
		}
		if (sg == 10) return bCab ? ((bGap && bFull) ? SDark : D.BeltMat) : SPaint;
		if (sg <= 13)
		{
			if (!bCab) return SPaint;
			if (bGap && bFull) return SDark;
			if (InR(D.Win, xm)) return sg == 12 ? SGlass : (sg == 13 || (sg == 11 && D.FrameBot > 2.f) ? D.FrameMat : SDark);
			if (InR(D.DarkPil, xm)) return SDark;
			return SPaint;
		}
		if (sg <= 15) return bCab ? D.RoofMat : SPaint;
		if (kmax > 0.15f && InR(D.TopGlass, xm)) return SGlass;
		return bCab ? D.RoofMat : SPaint;
	};

	// ------------------------------------------------------------------ normales y caras
	TArray<FVector> FN; FN.SetNum(NS * NR);
	double Orient = 0.0;
	for (int32 i = 0; i < NS; ++i)
	{
		FVector C(0); for (const FVector& v : P[i]) C += v; C /= NR;
		for (int32 j = 0; j < NR; ++j)
		{
			const int32 j2 = (j + 1) % NR;
			FN[i * NR + j] = FVector::CrossProduct(P[i + 1][j2] - P[i][j], P[i + 1][j] - P[i][j2]).GetSafeNormal();
			Orient += FVector::DotProduct(FN[i * NR + j], 0.5f * (P[i][j] + P[i][j2]) - C);
		}
	}
	if (Orient < 0.0) for (FVector& n : FN) n = -n;
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
	for (int32 e = 0; e < 2; ++e)   // tapas delantera y trasera
	{
		const int32 i = e == 0 ? 0 : NS; const FVector N(e == 0 ? -1.f : 1.f, 0, 0);
		FVector C(0); for (const FVector& v : P[i]) C += v; C /= NR;
		for (int32 j = 0; j < NR; ++j) B.Tri(SPaint, C, P[i][j], P[i][(j + 1) % NR], N, N, N, FVector2D(.5, .5), FVector2D(0, 0), FVector2D(1, 0));
	}

	for (const FVector2D& g : D.TopGlass)
	{
		TArray<int32> Ids; for (int32 i = 0; i <= NS; ++i) if (Xs[i] >= g.X && Xs[i] <= g.Y && Kc[i] > 0.15f) Ids.Add(i);
		if (Ids.Num() < 2) continue;
		for (int32 s = 0; s < 2; ++s)
		{
			TArray<FVector> Edge; for (int32 i : Ids) Edge.Add(P[i][s == 0 ? 16 : NR - 16] + FVector(0, 0, 0.3f));
			B.Tube(SDark, Edge, 1.1f, 6);
		}
		for (int32 e = 0; e < 2; ++e)
		{
			const int32 i = e == 0 ? Ids[0] : Ids.Last(); TArray<FVector> Edge;
			for (int32 j = 16; j <= NR - 16; ++j) Edge.Add(P[i][j] + FVector(0, 0, 0.3f));
			B.Tube(SDark, Edge, 1.1f, 6);
		}
	}

	// ------------------------------------------------------------------ aletines de los pasos de rueda (siguen la forma del arco)
	auto Contour = [&](TArray<FVector2D>& C, float th0, float th1, int32 N)
	{
		for (int32 s = 0; s <= N; ++s)
		{
			const float th = th0 + (th1 - th0) * s / N; const FVector2D d(FMath::Cos(th), FMath::Sin(th));
			float lo = 0.f, hi = Ra * 2.5f;
			for (int32 it = 0; it < 22; ++it) { const float m = 0.5f * (lo + hi); const float t = ArchTop(d.X * m); if (t > -1e8f && d.Y * m < t) lo = m; else hi = m; }
			C.Add(d * lo);
		}
	};
	auto Flare = [&](int32 Mat, float wx, float side, float y0, float y1, float T)
	{
		TArray<FVector2D> C; const float tA = D.Arch == EArch::Hex ? 0.06f : (wyAbs - Wd * 0.5f > D.Half - 1.f ? 0.45f : -0.22f); Contour(C, tA, PI - tA, 30);
		const int32 N = C.Num(); TArray<FVector2D> Nn;
		for (int32 k = 0; k < N; ++k) { const FVector2D t = C[FMath::Min(k + 1, N - 1)] - C[FMath::Max(k - 1, 0)]; Nn.Add(FVector2D(t.Y, -t.X).GetSafeNormal()); }
		auto Pt = [&](const FVector2D& c, float y) { return FVector(wx + c.X, side * y, R + c.Y); };
		const FVector Ns(0, side, 0);
		for (int32 k = 0; k < N - 1; ++k)
		{
			const FVector2D a = C[k], b = C[k + 1], ao = a + Nn[k] * T, bo = b + Nn[k + 1] * T, am = a + Nn[k] * T * 0.55f, bm = b + Nn[k + 1] * T * 0.55f;
			const FVector NA(Nn[k].X, 0, Nn[k].Y), NB(Nn[k + 1].X, 0, Nn[k + 1].Y);
			B.QuadN(Mat, Pt(ao, y0), Pt(bo, y0), Pt(bo, y1 - 1.2f), Pt(ao, y1 - 1.2f), NA, NB, NB, NA);
			B.QuadN(Mat, Pt(ao, y1 - 1.2f), Pt(bo, y1 - 1.2f), Pt(bm, y1), Pt(am, y1), (NA + Ns).GetSafeNormal(), (NB + Ns).GetSafeNormal(), (NB + Ns).GetSafeNormal(), (NA + Ns).GetSafeNormal());
			B.QuadN(Mat, Pt(a, y0), Pt(a, y1), Pt(b, y1), Pt(b, y0), -NA, -NA, -NB, -NB);
			B.QuadN(Mat, Pt(a, y1), Pt(am, y1), Pt(bm, y1), Pt(b, y1), Ns, Ns, Ns, Ns);
		}
		for (int32 e = 0; e < 2; ++e)
		{
			const int32 k = e == 0 ? 0 : N - 1; const FVector2D t = (e == 0 ? C[0] - C[1] : C[N - 1] - C[N - 2]).GetSafeNormal(); const FVector Nt(t.X, 0, t.Y);
			const FVector2D a = C[k], ao = a + Nn[k] * T;
			B.QuadN(Mat, Pt(a, y0), Pt(ao, y0), Pt(ao, y1), Pt(a, y1), Nt, Nt, Nt, Nt);
		}
	};
	if (D.FlareMat >= 0) for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f, wx = WP.X - D.XOff, xr = wx + 0.5f * L;
		const bool bOut = wyAbs - Wd * 0.5f > D.Half - 1.f;
		const float y0 = bOut ? wyAbs - Wd * 0.5f - 3.f : SideW(xr, R + Ra * 0.8f) - 0.6f;
		const float y1 = FMath::Max(y0 + D.FlareOut, wyAbs + Wd * 0.5f + 1.5f);
		Flare(D.FlareMat, wx, side, y0, y1, D.FlareT);
	}

	// ------------------------------------------------------------------ utilidades de colocación
	const float xf = XB(L), xk = XB(0.f);
	auto WidthAt = [&](float xr, float z)
	{
		const float zsh = ShAt(xr), zHi = zsh - D.Rs;
		if (z <= zHi) return SideW(xr, z);
		const float yHi = SideW(xr, zHi);
		if (z <= zsh) return yHi - D.Rs + FMath::Sqrt(FMath::Max(0.f, D.Rs * D.Rs - FMath::Square(z - zHi)));
		const float cb = yHi - D.Rs - D.BeltStep, ct = FMath::Min(cb, D.RoofIn * WAt(xr));
		return FMath::Lerp(cb, ct, FMath::Clamp((z - zsh) / FMath::Max(1.f, TopAt(xr) - D.Rr - zsh), 0.f, 1.f));
	};
	auto Inside = [&](float xr, float y, float z) { return z <= TopAt(xr) + D.CrownH * 0.5f && z >= BotAt(xr) - 1.f && FMath::Abs(y) <= WidthAt(xr, z); };
	auto FrontX = [&](float y, float zs) { const float z = Z(zs); for (float xr = L; xr > L - 90.f; xr -= 0.25f) if (Inside(xr, y, z)) return XB(xr); return XB(L - 90.f); };
	auto RearX = [&](float y, float zs) { const float z = Z(zs); for (float xr = 0.f; xr < 90.f; xr += 0.25f) if (Inside(xr, y, z)) return XB(xr); return XB(90.f); };
	auto SY = [&](float xr, float zs) { return SideW(xr, Z(zs)); };
	auto HoodZ = [&](float xr, float y) { const float w = FMath::Max(10.f, WAt(xr) - D.Rs); return TopAt(xr) + D.CrownH * (1.f - FMath::Square(FMath::Clamp(FMath::Abs(y) / w, 0.f, 1.f))); };
	auto Box = [&](int32 m, const FVector& c, const FVector& h) { B.Box(m, c, h); };
	auto SB = [&](int32 m, const FVector& c, const FVector& h, float e = 0.3f, int32 sg = 6) { B.SuperBox(m, c, h, e, sg); };
	auto At = [&](const FVector& c, const FRotator& r) { B.Xf = FTransform(r, c) * Keep0; };
	auto Reset = [&]() { B.Xf = Keep0; };
	// pieza plana pegada al frontal/trasera siguiendo la curva en planta (gira con la superficie)
	auto OnEnd = [&](bool bFront, float yc, float zs, float out, float& xOut, float sp = 3.f) -> FRotator
	{
		const float x0 = bFront ? FrontX(yc - sp, zs) : RearX(yc - sp, zs), x1 = bFront ? FrontX(yc + sp, zs) : RearX(yc + sp, zs);
		const float xc = bFront ? FrontX(yc, zs) : RearX(yc, zs);
		xOut = 0.5f * (0.5f * (x0 + x1) + xc) + (bFront ? out : -out);
		return FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(x0 - x1, 2.f * sp)), 0.f);
	};
	auto Plate = [&](bool bFront, float zs)
	{
		const float x = bFront ? FrontX(0.f, zs) : RearX(0.f, zs), s = bFront ? 1.f : -1.f;
		Box(SDark, FVector(x + s * 0.5f, 0, Z(zs)), FVector(0.5f, 27.f, 7.f));
		Box(SAlu, FVector(x + s * 1.1f, 0, Z(zs)), FVector(0.3f, 26.f, 6.2f));
	};
	auto Mirror = [&](float xr, float zs, int32 HouseMat, float hx, float hy, float hz)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			const float y0 = SY(xr, zs - 6.f);
			SB(SDark, FVector(XB(xr) - 2.f, s * (y0 + 3.f), Z(zs - 4.f)), FVector(4.f, 4.5f, 2.f), 0.4f, 6);
			SB(HouseMat, FVector(XB(xr) - 2.f, s * (y0 + 6.f + hy), Z(zs)), FVector(hx, hy, hz), 0.45f, 8);
			SB(SGlass, FVector(XB(xr) - 2.f - hx + 0.2f, s * (y0 + 6.f + hy), Z(zs)), FVector(0.3f, hy - 1.2f, hz - 1.2f), 0.35f, 6);
		}
	};
	auto Handle = [&](int32 m, float xr, float zs, float hx)
	{
		for (int32 s = -1; s <= 1; s += 2) SB(m, FVector(XB(xr), s * (SY(xr, zs) + 0.6f), Z(zs)), FVector(hx, 1.0f, 1.5f), 0.35f, 6);
	};
	auto Wipers = [&](float xr, float zs, float span)
	{
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SDark, { FVector(XB(xr), s * 4.f, Z(zs)), FVector(XB(xr) - 3.f, s * (4.f + span), Z(zs) + 3.f) }, 0.55f, 5);
	};
	auto Spare = [&](float xBody, float yc, float zs, float Rs)
	{
		B.Xf = FTransform(FRotator(0, 90, 0), FVector(xBody, yc, Z(zs))) * Keep0;
		DustlineAddWheel(B, FMath::Min(R, Rs), FMath::Min(Wd, 26.f), 1, 1.f, false);
		Reset();
	};

	// ------------------------------------------------------------------ detalles de cada modelo
	switch (St)
	{
	case 0: // ============================ Clase G
	{
		// paragolpes delantero negro envolvente, matrícula, tomas y protector
		SB(SDark, FVector(xf + 1.5f, 0, Z(55.f)), FVector(7.f, D.Half + 2.f, 11.5f), 0.22f, 8);
		{ const float xp = xf + 8.6f; Box(SDark, FVector(xp, 0, Z(57.f)), FVector(0.5f, 27.f, 7.f)); Box(SAlu, FVector(xp + 0.6f, 0, Z(57.f)), FVector(0.3f, 26.f, 6.2f)); }
		for (int32 s = -1; s <= 1; s += 2)
		{
			Box(SCarbon, FVector(xf + 8.4f, s * 47.f, Z(52.f)), FVector(0.5f, 15.f, 5.f));
			for (int32 q = 0; q < 3; ++q) Box(SMetal, FVector(xf + 8.8f, s * 47.f, Z(48.5f) + q * 3.5f), FVector(0.3f, 14.f, 0.4f));
			B.Cylinder(SChrome, FVector(xf + 8.f, s * 80.f, Z(55.f)), FVector(1, 0, 0), 4.2f, 0.8f, 16, true);   // antiniebla
			B.Cylinder(SLight, FVector(xf + 8.6f, s * 80.f, Z(55.f)), FVector(1, 0, 0), 3.2f, 0.4f, 16, true);
		}
		SB(SMetal, FVector(xf - 8.f, 0, Z(41.f)), FVector(14.f, 44.f, 2.f), 0.3f, 6);
		// parrilla: marco, tres lamas y estrella
		const float xg = xf + 0.5f;
		Box(SDark, FVector(xg, 0, Z(96.f)), FVector(0.8f, 36.f, 20.f));
		Box(SChrome, FVector(xg + 0.6f, 0, Z(116.4f)), FVector(0.6f, 36.6f, 0.7f)); Box(SChrome, FVector(xg + 0.6f, 0, Z(75.6f)), FVector(0.6f, 36.6f, 0.7f));
		for (int32 s = -1; s <= 1; s += 2) Box(SChrome, FVector(xg + 0.6f, s * 36.2f, Z(96.f)), FVector(0.6f, 0.7f, 20.4f));
		for (float zz : { 84.f, 96.f, 108.f }) SB(SPaint, FVector(xg + 1.6f, 0, Z(zz)), FVector(1.4f, 35.f, 1.7f), 0.3f, 6);
		B.Torus(SChrome, FVector(xg + 3.2f, 0, Z(96.f)), FVector(1, 0, 0), 8.6f, 0.9f, 28, 6);
		B.Cylinder(SDark, FVector(xg + 2.4f, 0, Z(96.f)), FVector(1, 0, 0), 8.f, 0.8f, 24, true);
		for (int32 a = 0; a < 3; ++a)
		{
			At(FVector(xg + 3.4f, 0, Z(96.f)), FRotator(0.f, 0.f, 120.f * a));
			Box(SChrome, FVector(0, 0, 4.2f), FVector(0.5f, 0.9f, 4.2f)); Reset();
		}
		// faros redondos en el frontal de las aletas con aro LED
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector c(xf, s * 70.f, Z(104.f));
			B.Cylinder(SPaint, c + FVector(-2.f, 0, 0), FVector(1, 0, 0), 15.f, 4.2f, 28, true);
			B.Cylinder(SDark, c + FVector(2.1f, 0, 0), FVector(1, 0, 0), 13.2f, 0.6f, 28, true);
			B.Cylinder(SChrome, c + FVector(2.6f, 0, 0), FVector(1, 0, 0), 12.2f, 0.4f, 28, true);
			B.Torus(SLight, c + FVector(3.2f, 0, 0), FVector(1, 0, 0), 10.6f, 0.8f, 28, 6);
			for (int32 q = -1; q <= 1; q += 2) { B.Cylinder(SDark, c + FVector(3.0f, q * 4.f, 0), FVector(1, 0, 0), 3.2f, 0.6f, 16, true); B.Sphere(SChrome, c + FVector(3.4f, q * 4.f, 0), 2.3f, 10); }
			// intermitentes sobre las aletas (seña de identidad)
			const float xi = XB(402.f), yi = s * 80.f, zi = HoodZ(402.f, 80.f);
			SB(SDark, FVector(xi, yi, zi + 1.6f), FVector(5.f, 4.2f, 1.8f), 0.35f, 6);
			SB(SAmber, FVector(xi + 0.5f, yi, zi + 2.4f), FVector(4.f, 3.4f, 1.4f), 0.35f, 6);
			// juntas capó-aletas
			Box(SDark, FVector(XB(351.f), s * 77.f, HoodZ(351.f, 77.f) + 0.05f), FVector(60.f, 0.35f, 0.2f));
			// bisagras de puerta a la vista
			for (float xh : { 284.f, 182.f }) for (float zh : { 70.f, 112.f })
				SB(SDark, FVector(XB(xh), s * (SY(xh, zh) + 1.0f), Z(zh)), FVector(2.4f, 1.3f, 4.f), 0.35f, 6);
			// estribo
			const float xa = XB(64.f) + Ra + 6.f, xb2 = XB(353.f) - Ra - 6.f;
			SB(SMetal, FVector(0.5f * (xa + xb2), s * (SY(200.f, 52.f) + 8.f), Z(45.f)), FVector(0.5f * (xb2 - xa), 9.f, 1.6f), 0.3f, 6);
			for (int32 q = 0; q < 3; ++q) SB(SDark, FVector(xa + (xb2 - xa) * (0.15f + 0.35f * q), s * (SY(200.f, 52.f) + 2.f), Z(47.f)), FVector(2.f, 6.f, 2.f), 0.4f, 6);
			// canalones del techo
			B.Tube(SDark, { FVector(XB(6.f), s * (0.95f * D.Half + 0.4f), Z(194.f)), FVector(XB(250.f), s * (0.95f * D.Half + 0.4f), Z(194.f)) }, 0.8f, 6);
		}
		Wipers(289.f, 133.f, 32.f);
		Mirror(276.f, 141.f, SPaint, 5.f, 9.f, 7.f);
		Handle(SChrome, 200.f, 101.f, 6.f); Handle(SChrome, 112.f, 101.f, 6.f);
		// trasera: paragolpes, pilotos verticales, luneta en el portón, bisagras, rueda de repuesto
		SB(SDark, FVector(xk - 1.5f, 0, Z(54.f)), FVector(7.f, D.Half + 2.f, 11.f), 0.22f, 8);
		for (int32 s = -1; s <= 1; s += 2)
		{
			float xo; const FRotator r = OnEnd(false, s * 86.f, 90.f, 0.6f, xo);
			At(FVector(xo, s * 86.f, Z(90.f)), r); SB(SDark, FVector(0), FVector(1.4f, 7.f, 19.f), 0.25f, 6); SB(STail, FVector(-0.8f, 0, 3.f), FVector(1.f, 5.6f, 13.f), 0.3f, 6); SB(SAmber, FVector(-0.8f, 0, -13.f), FVector(1.f, 5.6f, 3.f), 0.3f, 6); Reset();
		}
		Box(SDark, FVector(xk - 0.3f, 0, Z(162.f)), FVector(0.4f, 65.f, 24.f));
		Box(SGlass, FVector(xk - 0.7f, 0, Z(162.f)), FVector(0.4f, 62.f, 21.f));
		for (float zh : { 96.f, 154.f }) SB(SDark, FVector(xk - 1.f, -80.f, Z(zh)), FVector(1.5f, 3.f, 5.f), 0.35f, 6);
		SB(SChrome, FVector(xk - 1.f, 74.f, Z(118.f)), FVector(1.2f, 2.f, 6.f), 0.35f, 6);
		Plate(false, 76.f);
		Spare(xk - 14.f, 0.f, 126.f, 38.f);
		B.Cylinder(SChrome, FVector(xk - 27.f, 0, Z(126.f)), FVector(-1, 0, 0), 9.f, 1.f, 24, true);
		break;
	}
	case 1: // ============================ Urus
	{
		// banda que sigue la curva en planta del frontal/trasera (láminas orientadas con la superficie)
		auto Skin = [&](bool bFront, float zs, float hz, float yMax, int32 Mat, float out, float th)
		{
			for (float y = -yMax; y <= yMax + 0.01f; y += 2.f)
			{
				float xo; const FRotator r = OnEnd(bFront, y, zs, out, xo, 1.5f);
				At(FVector(xo, y, Z(zs)), r); Box(Mat, FVector(0), FVector(th, 1.6f, hz)); Reset();
			}
		};
		// frontal: mitad inferior negra (calandra hexagonal y tomas con marco en "Y" del color de la carrocería), faros finos arriba
		Skin(true, 55.f, 25.f, D.Half - D.RcF + 2.f, SDark, 0.15f, 0.6f);
		const float xg = FrontX(0.f, 52.f);
		// tira que recorre el frontal siguiendo su curva: polilínea (y, z) de serie
		auto FrontStrip = [&](const TArray<FVector2D>& Pl, int32 Mat, float hz, float out, float th)
		{
			for (int32 k = 0; k + 1 < Pl.Num(); ++k)
			{
				const FVector2D A = Pl[k], Bp = Pl[k + 1]; const float len = (Bp - A).Size(); const int32 n = FMath::Max(1, FMath::CeilToInt(len / 2.5f));
				const float roll = FMath::RadiansToDegrees(FMath::Atan2(Bp.Y - A.Y, Bp.X - A.X));
				for (int32 q = 0; q < n; ++q)
				{
					const FVector2D P0 = FMath::Lerp(A, Bp, (q + 0.5f) / n); float xo; const FRotator r = OnEnd(true, P0.X, P0.Y, out, xo, 1.5f);
					At(FVector(xo, P0.X, Z(P0.Y)), r); B.Xf = FTransform(FRotator(0.f, 0.f, -roll)) * B.Xf; Box(Mat, FVector(0), FVector(th, 0.5f * len / n + 0.15f, hz)); Reset();
				}
			}
		};
		// calandra hexagonal en la franja alta, lámina en "V" del color de la carrocería, tomas con 3 lamas, aletas del splitter
		for (int32 r = 0; r < 3; ++r) for (int32 c = -12; c <= 12; ++c)
		{
			const float yy = c * 5.2f + ((r & 1) ? 2.6f : 0.f); if (FMath::Abs(yy) > 60.f) continue;
			B.Prism(SCarbon, FVector(FrontX(yy, 66.f) + 0.6f, yy, Z(62.5f) + r * 4.6f), FVector(1, 0, 0), 2.5f, 1.2f, 6, 0.f);
		}
		FrontStrip({ FVector2D(-95.f, 74.f), FVector2D(-72.f, 59.f), FVector2D(72.f, 59.f), FVector2D(95.f, 74.f) }, SPaint, 1.7f, 1.0f, 1.2f);
		for (int32 s = -1; s <= 1; s += 2) for (float zz : { 41.f, 47.5f, 54.f })
			FrontStrip({ FVector2D(s * 70.f, zz - 1.f), FVector2D(s * 92.f, zz + 3.f) }, SPaint, 0.9f, 0.9f, 1.f);
		for (int32 q = -3; q <= 3; ++q) if (q) { const float yy = q * 12.f; SB(SPaint, FVector(FrontX(yy, 33.f) + 1.5f, yy, Z(33.f)), FVector(3.f, 0.9f, 3.5f), 0.4f, 6); }
		for (int32 s = -1; s <= 1; s += 2)
		{
			float xo; FRotator r;
			// faro: carcasa fina inclinada con firma LED en "Y"
			r = OnEnd(true, s * 70.f, 91.f, 0.3f, xo, 14.f);
			At(FVector(xo, s * 70.f, Z(91.f)), r + FRotator(0.f, 0.f, s * -6.f));
			SB(SDark, FVector(0), FVector(2.0f, 21.f, 3.4f), 0.35f, 8);
			SB(SLight, FVector(0.9f, s * -2.f, 1.1f), FVector(1.3f, 17.f, 0.8f), 0.45f, 6);
			At(FVector(xo + 0.6f, s * 82.f, Z(90.f)), r + FRotator(0.f, 0.f, s * -40.f)); SB(SLight, FVector(0), FVector(1.2f, 5.5f, 0.6f), 0.45f, 6);
			Reset();
		}
		SB(SCarbon, FVector(FrontX(0.f, 32.f) + 2.5f, 0, Z(30.5f)), FVector(7.f, 84.f, 1.8f), 0.3f, 8);
		for (int32 s = -1; s <= 1; s += 2) SB(SPaint, FVector(XB(440.f), s * 30.f, HoodZ(440.f, 30.f) + 0.4f), FVector(55.f, 1.2f, 0.7f), 0.4f, 6);   // nervios del capó
		Wipers(367.f, 120.f, 36.f);
		Mirror(352.f, 124.f, SCarbon, 5.5f, 9.5f, 6.f);
		Handle(SDark, 242.f, 113.f, 7.f); Handle(SDark, 166.f, 118.f, 7.f);
		// trasera: banda negra con la luz roja de lado a lado y pilotos en "Y", alerones, difusor negro con doble salida a cada lado
		Skin(false, 119.f, 5.5f, D.Half - 11.f, SCarbon, 0.5f, 0.9f);
		Box(STail, FVector(RearX(0.f, 119.f) - 1.2f, 0, Z(119.f)), FVector(0.5f, 60.f, 0.6f));
		for (int32 s = -1; s <= 1; s += 2)
		{
			float xo; FRotator r = OnEnd(false, s * 80.f, 119.f, 1.2f, xo, 10.f);
			At(FVector(xo, s * 80.f, Z(119.f)), r); SB(STail, FVector(0, s * -8.f, 0.f), FVector(0.6f, 12.f, 0.9f), 0.4f, 6);
			At(FVector(xo, s * 88.f, Z(121.5f)), r + FRotator(0.f, 0.f, s * 30.f)); SB(STail, FVector(0), FVector(0.6f, 6.f, 0.9f), 0.4f, 6);
			At(FVector(xo, s * 88.f, Z(116.5f)), r + FRotator(0.f, 0.f, s * -30.f)); SB(STail, FVector(0), FVector(0.6f, 6.f, 0.9f), 0.4f, 6);
			Reset();
		}
		SB(SCarbon, FVector(XB(26.f), 0, Z(139.5f)), FVector(8.f, 60.f, 1.3f), 0.35f, 8);               // labio del portón
		Skin(false, 46.f, 16.f, D.Half - D.RcR + 2.f, SDark, 0.15f, 0.6f);
		for (int32 q = -3; q <= 3; ++q) Box(SCarbon, FVector(xk + 2.f, q * 9.f, Z(34.f)), FVector(6.f, 0.6f, 4.5f));
		for (int32 s = -1; s <= 1; s += 2) for (float yy : { 36.f, 47.f })
		{
			B.Cylinder(SChrome, FVector(xk + 4.f, s * yy, Z(41.f)), FVector(-1, 0, 0), 4.8f, 8.f, 18, true);
			B.Cylinder(SDark, FVector(xk + 4.f, s * yy, Z(41.f)), FVector(-1, 0, 0), 3.8f, 8.3f, 18, true);
		}
		Plate(false, 80.f);
		break;
	}
	case 2: // ============================ Panda 4x4
	{
		// faros grandes en las esquinas del morro, ranura de parrilla, paragolpes negro con antinieblas y protector plateado
		for (int32 s = -1; s <= 1; s += 2)
		{
			float xo; const FRotator r = OnEnd(true, s * 55.f, 82.f, -0.4f, xo, 13.f);
			const float xa = FrontX(s * 55.f, 76.f), xc = FrontX(s * 55.f, 88.f), pitch = FMath::RadiansToDegrees(FMath::Atan2(xa - xc, 12.f));
			At(FVector(xo, s * 55.f, Z(82.f)), r + FRotator(pitch, 0.f, 0.f));
			SB(SDark, FVector(0), FVector(2.4f, 19.f, 7.5f), 0.32f, 8);
			SB(SChrome, FVector(0.8f, 0, 0), FVector(1.8f, 17.5f, 6.2f), 0.32f, 8);
			B.Sphere(SLight, FVector(1.8f, s * 6.f, 0.f), 3.6f, 12); B.Sphere(SLight, FVector(1.8f, s * -7.f, 0.5f), 2.8f, 12);
			SB(SAmber, FVector(1.8f, s * 15.f, -2.f), FVector(1.f, 2.2f, 2.2f), 0.5f, 6);
			Reset();
			B.Cylinder(SChrome, FVector(FrontX(s * 58.f, 50.f) + 2.5f, s * 58.f, Z(50.f)), FVector(1, 0, 0), 5.2f, 1.f, 18, true);
			B.Cylinder(SLight, FVector(FrontX(s * 58.f, 50.f) + 3.4f, s * 58.f, Z(50.f)), FVector(1, 0, 0), 4.f, 0.4f, 18, true);
		}
		SB(SDark, FVector(FrontX(0.f, 80.f) + 0.6f, 0, Z(80.f)), FVector(1.f, 27.f, 2.4f), 0.35f, 8);
		Box(SChrome, FVector(FrontX(0.f, 80.f) + 1.4f, 0, Z(80.f)), FVector(0.3f, 22.f, 0.5f));
		SB(SDark, FVector(FrontX(0.f, 52.f) - 1.f, 0, Z(52.f)), FVector(6.f, D.Half + 1.f, 15.f), 0.28f, 8);
		SB(SCarbon, FVector(FrontX(0.f, 52.f) + 4.6f, 0, Z(54.f)), FVector(0.8f, 36.f, 7.f), 0.3f, 8);
		for (int32 q = -1; q <= 1; q += 2) Box(SMetal, FVector(FrontX(0.f, 52.f) + 5.4f, 0, Z(54.f) + q * 2.8f), FVector(0.3f, 34.f, 0.4f));
		SB(SAlu, FVector(FrontX(0.f, 40.f) - 1.f, 0, Z(37.5f)), FVector(6.f, 44.f, 3.2f), 0.35f, 8);
		Wipers(311.f, 99.f, 30.f);
		Mirror(306.f, 106.f, SPaint, 4.5f, 8.f, 6.f);
		Handle(SPaint, 190.f, 94.f, 5.5f); Handle(SPaint, 118.f, 96.f, 5.5f);
		// barras de techo
		for (int32 s = -1; s <= 1; s += 2)
		{
			const float yr = s * (0.84f * D.Half - 6.f);
			B.Tube(SDark, { FVector(XB(28.f), yr, Z(158.f)), FVector(XB(212.f), yr, Z(158.f)) }, 1.3f, 8);
			for (float xp : { 28.f, 212.f }) SB(SDark, FVector(XB(xp), yr, Z(156.f)), FVector(4.f, 1.6f, 2.4f), 0.4f, 6);
		}
		// trasera: pilotos verticales altos en los pilares, luneta, paragolpes negro, protector, matrícula, alerón
		for (int32 s = -1; s <= 1; s += 2)
		{
			float xo; const FRotator r = OnEnd(false, s * 61.f, 104.f, 0.3f, xo, 5.f);
			At(FVector(xo, s * 61.f, Z(104.f)), r); SB(SDark, FVector(0), FVector(1.8f, 7.f, 24.f), 0.4f, 8); SB(STail, FVector(-1.f, 0, 5.f), FVector(1.3f, 6.f, 16.f), 0.45f, 8); SB(SLight, FVector(-1.f, 0, -16.f), FVector(1.3f, 6.f, 3.2f), 0.45f, 6); Reset();
		}
		Box(SDark, FVector(xk - 0.3f, 0, Z(118.f)), FVector(0.4f, 50.f, 11.f));
		Box(SGlass, FVector(xk - 0.7f, 0, Z(118.5f)), FVector(0.4f, 47.f, 9.5f));
		SB(SDark, FVector(RearX(0.f, 52.f) + 1.f, 0, Z(52.f)), FVector(6.f, D.Half + 1.f, 14.f), 0.28f, 8);
		SB(SAlu, FVector(RearX(0.f, 40.f) + 1.f, 0, Z(38.f)), FVector(6.f, 40.f, 3.f), 0.35f, 8);
		Plate(false, 90.f);
		SB(SChrome, FVector(xk - 0.8f, 0, Z(102.f)), FVector(0.6f, 10.f, 1.0f), 0.35f, 6);
		SB(SPaint, FVector(XB(7.f), 0, Z(153.f)), FVector(7.f, 56.f, 1.6f), 0.35f, 8);
		break;
	}
	case 5: // ============================ Pick-up
	{
		// frontal: parrilla cromada grande, faros rectangulares, paragolpes cromado, protector
		const float xg = FrontX(0.f, 101.f);
		SB(SDark, FVector(xg + 0.4f, 0, Z(101.f)), FVector(1.2f, 52.f, 13.f), 0.25f, 8);
		for (int32 k = -2; k <= 2; ++k) SB(SChrome, FVector(xg + 1.5f, 0, Z(101.f) + k * 5.f), FVector(0.6f, 50.f, 0.9f), 0.35f, 6);
		for (int32 s = -1; s <= 1; s += 2) SB(SChrome, FVector(xg + 1.2f, s * 53.f, Z(101.f)), FVector(0.8f, 1.4f, 14.f), 0.35f, 6);
		SB(SChrome, FVector(xg + 1.2f, 0, Z(114.5f)), FVector(0.8f, 54.f, 1.3f), 0.35f, 6);
		for (int32 s = -1; s <= 1; s += 2)
		{
			float xo; const FRotator r = OnEnd(true, s * 74.f, 106.f, 0.2f, xo, 10.f);
			At(FVector(xo, s * 74.f, Z(106.f)), r);
			SB(SDark, FVector(0), FVector(2.f, 15.f, 6.f), 0.3f, 8); SB(SChrome, FVector(0.8f, 0, 0), FVector(1.6f, 13.5f, 4.8f), 0.3f, 8);
			B.Sphere(SLight, FVector(1.6f, s * -5.f, 0.5f), 3.4f, 12); B.Sphere(SLight, FVector(1.6f, s * 5.f, 0.5f), 2.6f, 12);
			SB(SAmber, FVector(1.6f, s * 11.5f, -2.f), FVector(1.f, 2.f, 1.8f), 0.5f, 6);
			Reset();
			B.Cylinder(SLight, FVector(FrontX(s * 70.f, 60.f) + 9.f, s * 70.f, Z(60.f)), FVector(1, 0, 0), 4.f, 0.6f, 16, true);
		}
		SB(SChrome, FVector(FrontX(0.f, 66.f) + 3.f, 0, Z(66.f)), FVector(7.f, D.Half + 3.f, 9.f), 0.3f, 8);
		SB(SDark, FVector(FrontX(0.f, 54.f) + 1.f, 0, Z(53.f)), FVector(7.f, 72.f, 4.f), 0.35f, 8);
		{ const float xp = FrontX(0.f, 66.f) + 10.2f; Box(SDark, FVector(xp, 0, Z(66.f)), FVector(0.5f, 27.f, 7.f)); Box(SAlu, FVector(xp + 0.6f, 0, Z(66.f)), FVector(0.3f, 26.f, 6.2f)); }
		SB(SMetal, FVector(xf - 12.f, 0, Z(46.f)), FVector(18.f, 46.f, 2.f), 0.3f, 6);
		SB(SDark, FVector(XB(468.f), 0, HoodZ(468.f, 0.f) + 1.8f), FVector(17.f, 13.f, 2.6f), 0.4f, 8);   // toma del capó
		for (int32 q = 0; q < 4; ++q) Box(SMetal, FVector(XB(482.f), -9.f + q * 6.f, HoodZ(468.f, 0.f) + 2.2f), FVector(0.4f, 2.2f, 1.6f));
		Wipers(398.f, 127.f, 38.f);
		Mirror(388.f, 136.f, SDark, 5.f, 10.f, 9.f);
		Handle(SChrome, 300.f, 118.f, 6.f); Handle(SChrome, 222.f, 118.f, 6.f);
		// estribos
		for (int32 s = -1; s <= 1; s += 2)
		{
			const float xa = XB(125.f) + Ra + 8.f, xb2 = XB(435.f) - Ra - 8.f;
			SB(SMetal, FVector(0.5f * (xa + xb2), s * (SY(300.f, 48.f) + 7.f), Z(44.f)), FVector(0.5f * (xb2 - xa), 8.f, 1.6f), 0.3f, 6);
		}
		// caja de carga: suelo forrado, paredes con remate, portón, pilotos verticales, arco antivuelco
		const float yw = D.Half - 3.f;
		Box(SInterior, FVector(XB(96.f), 0, Z(85.f) + 2.4f), FVector(92.f, yw - 3.f, 0.4f));
		for (int32 q = -5; q <= 5; ++q) Box(SDark, FVector(XB(96.f), q * 14.f, Z(85.f) + 2.9f), FVector(90.f, 1.2f, 0.3f));
		for (int32 s = -1; s <= 1; s += 2)
		{
			SB(SPaint, FVector(XB(96.f), s * (SY(96.f, 100.f) - 2.6f), Z(102.f)), FVector(94.f, 2.6f, 16.f), 0.2f, 6);
			SB(SDark, FVector(XB(96.f), s * (SY(96.f, 100.f) - 2.6f), Z(118.6f)), FVector(94.f, 3.4f, 1.f), 0.3f, 6);
			SB(STail, FVector(xk - 0.6f, s * (SY(4.f, 100.f) - 3.f), Z(104.f)), FVector(1.2f, 3.4f, 11.f), 0.3f, 6);
			SB(SLight, FVector(xk - 0.6f, s * (SY(4.f, 100.f) - 3.f), Z(91.f)), FVector(1.2f, 3.4f, 2.2f), 0.3f, 6);
		}
		SB(SPaint, FVector(xk + 2.6f, 0, Z(102.f)), FVector(2.6f, yw - 4.f, 16.f), 0.2f, 6);
		SB(SChrome, FVector(xk - 0.3f, 0, Z(111.f)), FVector(0.5f, 8.f, 1.6f), 0.35f, 6);
		Box(SDark, FVector(xk - 0.2f, 0, Z(86.f)), FVector(0.3f, yw - 4.f, 0.3f));
		SB(SChrome, FVector(xk - 1.f, 0, Z(64.f)), FVector(8.f, D.Half + 1.f, 7.f), 0.3f, 8);
		Plate(false, 76.f);
		B.Cylinder(SMetal, FVector(xk - 6.f, 0, Z(58.f)), FVector(-1, 0, 0), 2.5f, 10.f, 10, true);
		{ const float xb0 = XB(182.f), zb0 = Z(87.f), zt = Z(152.f), hw = yw - 8.f;
		  B.Tube(SDark, { FVector(xb0, -hw, zb0), FVector(xb0, -hw, zt - 6.f), FVector(xb0 - 2.f, -hw + 6.f, zt), FVector(xb0 - 2.f, hw - 6.f, zt), FVector(xb0, hw, zt - 6.f), FVector(xb0, hw, zb0) }, 2.f, 8);
		  for (int32 s = -1; s <= 1; s += 2) B.Tube(SDark, { FVector(xb0, s * hw, zt - 6.f), FVector(xb0 - 50.f, s * hw, zb0 + 4.f) }, 1.6f, 8); }
		// luna trasera de la cabina
		Box(SDark, FVector(XB(191.6f), 0, Z(155.f)), FVector(0.4f, 54.f, 15.f));
		Box(SGlass, FVector(XB(191.2f), 0, Z(155.f)), FVector(0.4f, 51.f, 12.f));
		break;
	}
	case 6: // ============================ Rock crawler
	{
		const float zt = Z(105.f), hw = D.Half - 6.f;
		SB(SMetal, FVector(xf + 2.f, 0, Z(68.f)), FVector(8.f, D.Half + 6.f, 8.f), 0.3f, 8);                      // paragolpes con cabrestante
		SB(SDark, FVector(xf + 4.f, 0, Z(82.f)), FVector(7.f, 20.f, 7.f), 0.4f, 8);
		B.Cylinder(SMetal, FVector(xf + 4.f, -14.f, Z(82.f)), FVector(0, 1, 0), 5.f, 28.f, 14, true);
		B.Tube(SMetal, { FVector(xf + 10.f, -2.f, Z(82.f)), FVector(xf + 12.f, 0.f, Z(76.f)) }, 0.6f, 5);
		SB(SMetal, FVector(xk - 2.f, 0, Z(68.f)), FVector(8.f, D.Half + 4.f, 8.f), 0.3f, 8);
		// calandra de siete ranuras y faros redondos
		SB(SDark, FVector(xf + 0.4f, 0, Z(94.f)), FVector(1.f, 30.f, 11.f), 0.3f, 6);
		for (int32 k = -3; k <= 3; ++k) SB(SPaint, FVector(xf + 1.f, k * 8.f, Z(94.f)), FVector(0.8f, 2.4f, 10.f), 0.4f, 6);
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector c(xf + 1.f, s * 50.f, Z(95.f));
			B.Cylinder(SDark, c, FVector(1, 0, 0), 10.f, 1.2f, 24, true);
			B.Torus(SChrome, c + FVector(1.2f, 0, 0), FVector(1, 0, 0), 8.8f, 0.9f, 24, 6);
			B.Lathe(SLight, c + FVector(1.f, 0, 0), FVector(1, 0, 0), { FVector2D(0.f, 8.f), FVector2D(1.f, 6.5f), FVector2D(1.6f, 3.5f), FVector2D(1.8f, 0.f) }, 24);
			SB(SAmber, FVector(xf + 1.f, s * 70.f, Z(95.f)), FVector(0.8f, 3.f, 3.f), 0.5f, 6);
		}
		// soportes de las aletas
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f;
			for (int32 q = -1; q <= 1; q += 2) B.Tube(SDark, { FVector(WP.X - D.XOff + q * 30.f, side * (D.Half - 1.f), zt - 10.f), FVector(WP.X - D.XOff + q * 36.f, side * (wyAbs - Wd * 0.5f - 2.f), R + Ra * 0.85f) }, 1.6f, 6);
		}
		// marco del parabrisas, jaula, asientos, salpicadero
		const float xw0 = XB(238.f), xw1 = XB(224.f), zw1 = zt + 56.f;
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SDark, { FVector(xw0, s * hw, zt + 2.f), FVector(xw1, s * hw, zw1) }, 2.4f, 8);
		B.Tube(SDark, { FVector(xw1, -hw, zw1), FVector(xw1, hw, zw1) }, 2.4f, 8);
		B.Quad(SGlass, FVector(xw0 - 0.5f, -hw + 2.f, zt + 3.f), FVector(xw0 - 0.5f, hw - 2.f, zt + 3.f), FVector(xw1 - 0.5f, hw - 2.f, zw1 - 1.f), FVector(xw1 - 0.5f, -hw + 2.f, zw1 - 1.f));
		B.Quad(SGlass, FVector(xw1 - 0.5f, -hw + 2.f, zw1 - 1.f), FVector(xw1 - 0.5f, hw - 2.f, zw1 - 1.f), FVector(xw0 - 0.5f, hw - 2.f, zt + 3.f), FVector(xw0 - 0.5f, -hw + 2.f, zt + 3.f));
		const float xr2 = XB(60.f);
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SDark, { FVector(xw1, s * hw, zw1), FVector(0.5f * (xw1 + xr2), s * hw, zw1 + 3.f), FVector(xr2, s * hw, zw1 - 4.f), FVector(xr2, s * hw, zt) }, 2.4f, 8);
		B.Tube(SDark, { FVector(xr2, -hw, zw1 - 4.f), FVector(xr2, hw, zw1 - 4.f) }, 2.4f, 8);
		B.Tube(SDark, { FVector(0.5f * (xw1 + xr2), -hw, zw1 + 3.f), FVector(0.5f * (xw1 + xr2), hw, zw1 + 3.f) }, 2.f, 8);
		for (int32 s = -1; s <= 1; s += 2) { SB(SInterior, FVector(XB(165.f), s * 24.f, zt + 12.f), FVector(16.f, 18.f, 10.f), 0.4f, 8); SB(SInterior, FVector(XB(146.f), s * 24.f, zt + 34.f), FVector(5.f, 18.f, 24.f), 0.4f, 8); }
		SB(SDark, FVector(xw0 - 14.f, 0, zt + 9.f), FVector(10.f, hw - 4.f, 6.f), 0.4f, 8);
		B.Torus(SDark, FVector(xw0 - 30.f, -24.f, zt + 24.f), FVector(0.8f, 0.f, 0.6f), 9.f, 1.6f, 20, 6);
		for (int32 s = -1; s <= 1; s += 2) { SB(STail, FVector(xk - 0.6f, s * (D.Half - 10.f), Z(92.f)), FVector(1.f, 4.f, 5.f), 0.4f, 6); }
		Spare(xk - 22.f, 0.f, 122.f, 46.f);
		B.Tube(SDark, { FVector(xk, -20.f, zt), FVector(xk - 10.f, -20.f, Z(122.f)) }, 2.f, 6); B.Tube(SDark, { FVector(xk, 20.f, zt), FVector(xk - 10.f, 20.f, Z(122.f)) }, 2.f, 6);
		break;
	}
	default: // ============================ Defender 110
	{
		// frontal: panel negro con calandra de malla, faros redondos en marcos cuadrados, pilotos bajo los faros
		Box(SDark, FVector(xf + 0.4f, 0, Z(96.f)), FVector(0.7f, 84.f, 16.f));
		for (int32 r = 0; r < 5; ++r) for (int32 c = -6; c <= 6; ++c) Box(SMetal, FVector(xf + 1.2f, c * 5.2f, Z(84.f) + r * 6.2f), FVector(0.3f, 2.f, 2.2f));
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector c(xf + 1.f, s * 63.f, Z(100.f));
			SB(SDark, c, FVector(1.2f, 13.f, 13.f), 0.18f, 6);
			B.Torus(SChrome, c + FVector(1.2f, 0, 0), FVector(1, 0, 0), 9.2f, 1.f, 28, 6);
			B.Cylinder(SChrome, c + FVector(0.6f, 0, 0), FVector(1, 0, 0), 9.f, 0.6f, 28, true);
			B.Lathe(SLight, c + FVector(1.f, 0, 0), FVector(1, 0, 0), { FVector2D(0.f, 8.6f), FVector2D(1.2f, 7.f), FVector2D(1.8f, 4.f), FVector2D(2.0f, 0.f) }, 24);
			SB(SAmber, FVector(xf + 1.2f, s * 72.f, Z(84.f)), FVector(0.8f, 5.f, 2.6f), 0.3f, 6);
			SB(SLight, FVector(xf + 1.2f, s * 57.f, Z(84.f)), FVector(0.8f, 5.f, 2.6f), 0.3f, 6);
			// chapa estriada sobre las aletas
			const float zp = HoodZ(370.f, 74.f) + 0.3f;
			Box(SAlu, FVector(XB(368.f), s * 74.f, zp), FVector(42.f, 13.f, 0.3f));
			for (int32 a = 0; a < 14; ++a) for (int32 b = 0; b < 4; ++b) { At(FVector(XB(330.f) + a * 6.f + (b & 1) * 3.f, s * (64.f + b * 6.f), zp + 0.4f), FRotator(0.f, 45.f, 0.f)); Box(SAlu, FVector(0), FVector(1.6f, 0.4f, 0.25f)); Reset(); }
			// junta del capó (más estrecho que las aletas)
			Box(SDark, FVector(XB(358.f), s * 58.f, HoodZ(358.f, 58.f) + 0.05f), FVector(57.f, 0.35f, 0.2f));
			// bisagras grandes a la vista y manillas de pulsador
			for (float xh : { 292.f, 203.f }) for (float zh : { 76.f, 110.f })
				SB(SDark, FVector(XB(xh), s * (SY(xh, zh) + 1.2f), Z(zh)), FVector(2.6f, 1.5f, 4.5f), 0.3f, 6);
			// canalones del techo
			B.Tube(SDark, { FVector(XB(5.f), s * (0.92f * D.Half + 0.3f), Z(184.f)), FVector(XB(272.f), s * (0.92f * D.Half + 0.3f), Z(184.f)) }, 0.8f, 6);
			// ventanas alpinas en el techo
			for (float xa : { 108.f, 160.f })
			{
				At(FVector(XB(xa), s * (0.92f * D.Half - 3.4f), Z(186.9f)), FRotator(0.f, 0.f, s * 45.f));
				SB(SDark, FVector(0), FVector(21.f, 3.8f, 0.5f), 0.3f, 6); SB(SGlass, FVector(0, 0, 0.3f), FVector(19.5f, 3.f, 0.4f), 0.3f, 6); Reset();
			}
		}
		Handle(SChrome, 224.f, 101.f, 3.f); Handle(SChrome, 144.f, 101.f, 3.f);
		// rejilla lateral en la aleta derecha
		Box(SDark, FVector(XB(318.f), SY(318.f, 104.f) + 0.25f, Z(104.f)), FVector(12.f, 0.3f, 7.f));
		for (int32 q = 0; q < 4; ++q) Box(SMetal, FVector(XB(318.f), SY(318.f, 104.f) + 0.6f, Z(99.f) + q * 3.3f), FVector(11.f, 0.3f, 0.5f));
		// parachoques de acero, protector, defensa
		Box(SDark, FVector(xf + 5.f, 0, Z(63.f)), FVector(5.f, 92.f, 6.5f));
		for (int32 s = -1; s <= 1; s += 2) { Box(SDark, FVector(xf + 2.f, s * 95.f, Z(63.f)), FVector(5.f, 4.f, 6.f)); B.Cylinder(SMetal, FVector(xf + 10.f, s * 40.f, Z(63.f)), FVector(1, 0, 0), 2.2f, 2.f, 10, true); }
		SB(SMetal, FVector(xf - 12.f, 0, Z(52.f)), FVector(16.f, 40.f, 1.8f), 0.3f, 6);
		Wipers(296.f, 128.f, 34.f);
		// estribos y largueros del chasis a la vista bajo la carrocería
		for (int32 s = -1; s <= 1; s += 2)
		{
			const float xa = XB(88.f) + Ra + 8.f, xb2 = XB(367.f) - Ra - 8.f;
			Box(SDark, FVector(0.5f * (xa + xb2), s * 42.f, Z(52.f)), FVector(0.5f * (xb2 - xa) + 30.f, 3.f, 6.f));
			SB(SDark, FVector(0.5f * (xa + xb2), s * (D.Half - 4.f), Z(48.f)), FVector(0.5f * (xb2 - xa), 7.f, 1.6f), 0.3f, 6);
		}
		// retrovisores sobre brazo
		for (int32 s = -1; s <= 1; s += 2)
		{
			const float y0 = SY(286.f, 118.f);
			B.Tube(SDark, { FVector(XB(286.f), s * y0, Z(120.f)), FVector(XB(286.f), s * (y0 + 12.f), Z(128.f)) }, 0.9f, 6);
			SB(SDark, FVector(XB(287.f), s * (y0 + 15.f), Z(134.f)), FVector(1.6f, 7.f, 9.f), 0.25f, 6);
			Box(SGlass, FVector(XB(285.3f), s * (y0 + 15.f), Z(134.f)), FVector(0.3f, 6.f, 8.f));
		}
		// snórquel en el pilar A derecho
		{
			const float ys = SY(312.f, 108.f) + 5.f;
			B.Tube(SDark, FDustlineMeshBuilder::Smooth({ FVector(XB(316.f), ys - 4.f, Z(102.f)), FVector(XB(312.f), ys, Z(108.f)), FVector(XB(296.f), ys, Z(132.f)), FVector(XB(280.f), ys - 1.f, Z(186.f)), FVector(XB(278.f), ys - 2.f, Z(196.f)) }, 5), 4.2f, 10);
			SB(SDark, FVector(XB(280.f), ys - 2.f, Z(199.f)), FVector(6.f, 5.f, 4.f), 0.4f, 8);
		}
		// baca expedicionaria con patas
		for (int32 s = -1; s <= 1; s += 2)
		{
			B.Tube(SDark, { FVector(XB(20.f), s * 76.f, Z(198.f)), FVector(XB(266.f), s * 76.f, Z(198.f)) }, 1.4f, 6);
			for (float xp : { 24.f, 104.f, 184.f, 262.f }) SB(SDark, FVector(XB(xp), s * 78.f, Z(193.f)), FVector(2.f, 2.f, 4.5f), 0.4f, 6);
		}
		for (int32 q = 0; q < 8; ++q) B.Tube(SDark, { FVector(XB(28.f + q * 33.f), -76.f, Z(198.f)), FVector(XB(28.f + q * 33.f), 76.f, Z(198.f)) }, 1.0f, 6);
		// trasera: puerta con rueda de repuesto, luneta, grupos de luces bajos, travesaño
		Box(SDark, FVector(xk - 0.3f, -6.f, Z(156.f)), FVector(0.4f, 47.f, 19.f));
		Box(SGlass, FVector(xk - 0.7f, -6.f, Z(156.f)), FVector(0.4f, 44.f, 16.f));
		Box(SDark, FVector(xk - 0.25f, 0, Z(124.f)), FVector(0.3f, 72.f, 0.3f));
		for (int32 s = -1; s <= 1; s += 2)
		{
			SB(SDark, FVector(xk - 0.8f, s * 79.f, Z(73.f)), FVector(1.f, 6.f, 15.f), 0.25f, 6);
			SB(STail, FVector(xk - 1.4f, s * 79.f, Z(82.f)), FVector(0.8f, 4.6f, 4.f), 0.4f, 6);
			SB(SAmber, FVector(xk - 1.4f, s * 79.f, Z(72.f)), FVector(0.8f, 4.6f, 3.5f), 0.4f, 6);
			SB(SLight, FVector(xk - 1.4f, s * 79.f, Z(63.f)), FVector(0.8f, 4.6f, 3.5f), 0.4f, 6);
		}
		for (float zh : { 84.f, 150.f }) SB(SDark, FVector(xk - 1.f, 64.f, Z(zh)), FVector(1.5f, 3.5f, 5.f), 0.3f, 6);
		SB(SChrome, FVector(xk - 1.f, -58.f, Z(122.f)), FVector(1.2f, 2.f, 4.f), 0.35f, 6);
		Box(SDark, FVector(xk + 3.f, 0, Z(56.f)), FVector(6.f, 88.f, 5.f));
		Plate(false, 70.f);
		Spare(xk - 14.f, -6.f, 110.f, 41.f);
		break;
	}
	}
	Reset();
}
