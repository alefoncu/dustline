#include "DustlineWorld.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Math/RandomStream.h"

DEFINE_LOG_CATEGORY_STATIC(LogDustlineWorld, Log, All);

namespace DustlineWorld
{
namespace
{
	FORCEINLINE float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	FORCEINLINE float Sat(float X) { return FMath::Clamp(X, 0.f, 1.f); }

	// ---------------- tabla de superficies ----------------
	struct FProps { float Mu, MuWet, Peak, Alpha, Fall, Crr, CrrWet, Sink, SinkWet, Dig, Bump, Drag; };
	//                                Mu    MuWet  Peak  Alpha Fall  Crr    CrrWet Sink  SinkW  Dig   Bump   Drag
	const FProps GProps[SNum] = {
		/* Hierba      */ {          0.78f, 0.48f, 0.12f, 0.13f, 0.78f, 0.035f, 0.040f, 0.01f, 0.03f, 0.02f, 0.010f, 0.000f },
		/* Hierba alta */ {          0.70f, 0.44f, 0.14f, 0.15f, 0.80f, 0.050f, 0.055f, 0.02f, 0.04f, 0.03f, 0.020f, 0.012f },
		/* Tierra      */ {          0.72f, 0.45f, 0.14f, 0.14f, 0.85f, 0.030f, 0.050f, 0.01f, 0.05f, 0.04f, 0.012f, 0.000f },
		/* Grava       */ {          0.62f, 0.56f, 0.22f, 0.20f, 0.95f, 0.045f, 0.045f, 0.03f, 0.03f, 0.05f, 0.025f, 0.000f },
		/* Arena       */ {          0.55f, 0.66f, 0.30f, 0.24f, 1.00f, 0.130f, 0.050f, 0.10f, 0.03f, 0.12f, 0.008f, 0.004f },
		/* Barro       */ {          0.38f, 0.29f, 0.28f, 0.24f, 0.92f, 0.060f, 0.080f, 0.07f, 0.11f, 0.25f, 0.015f, 0.006f },
		/* Roca        */ {          0.88f, 0.58f, 0.10f, 0.11f, 0.72f, 0.018f, 0.018f, 0.00f, 0.00f, 0.00f, 0.045f, 0.000f },
		/* Nieve       */ {          0.32f, 0.30f, 0.24f, 0.22f, 0.95f, 0.030f, 0.035f, 0.00f, 0.00f, 0.10f, 0.006f, 0.000f },
		/* Hielo       */ {          0.10f, 0.07f, 0.07f, 0.08f, 0.65f, 0.012f, 0.012f, 0.00f, 0.00f, 0.00f, 0.003f, 0.000f },
	};
	// agarre por tipo de neumático y superficie
	const float GTireMu[5][SNum] = {
		/* carretera */ { 0.92f, 0.85f, 0.95f, 0.80f, 0.65f, 0.50f, 1.08f, 0.62f, 0.90f },
		/* AT        */ { 1.00f, 1.00f, 1.00f, 1.00f, 0.90f, 0.85f, 1.00f, 0.90f, 0.95f },
		/* barro MT  */ { 0.95f, 1.05f, 1.05f, 1.05f, 0.95f, 1.55f, 0.95f, 1.05f, 0.85f },
		/* clavos    */ { 0.95f, 0.95f, 1.00f, 1.02f, 0.85f, 0.90f, 0.90f, 1.35f, 2.60f },
		/* balón     */ { 0.90f, 0.95f, 0.95f, 0.95f, 1.45f, 1.20f, 0.85f, 1.25f, 0.90f },
	};
	const float GTireSink[5] = { 1.15f, 1.f, 0.9f, 1.f, 0.45f };
	const float GTireDig[5] = { 1.2f, 1.f, 0.65f, 1.f, 0.6f };

	// ---------------- datos horneados ----------------
	bool bTried = false, bHave = false;
	int32 MaskN = 0, FlowN = 0; float DataHalf = HalfCm;
	TArray<uint8> Mask, Flow;

	FSim GSimInst;
}

const TCHAR* SurfName(int32 K)
{
	static const TCHAR* N[SNum] = { TEXT("Hierba"), TEXT("Hierba alta"), TEXT("Tierra"), TEXT("Grava"), TEXT("Arena"), TEXT("Barro"), TEXT("Roca"), TEXT("Nieve"), TEXT("Hielo") };
	return N[FMath::Clamp(K, 0, SNum - 1)];
}

bool LoadData()
{
	if (bTried) return bHave;
	bTried = true;
	TArray<uint8> Raw;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("world.bin");
	if (!FFileHelper::LoadFileToArray(Raw, *Path) || Raw.Num() < 16 || FMemory::Memcmp(Raw.GetData(), "DLW3", 4) != 0)
	{
		UE_LOG(LogDustlineWorld, Warning, TEXT("Sin datos del mundo en %s: superficies solo por altura/pendiente"), *Path);
		return false;
	}
	int32 Off = 4;
	auto I32 = [&]() { int32 V; FMemory::Memcpy(&V, Raw.GetData() + Off, 4); Off += 4; return V; };
	auto F32 = [&]() { float V; FMemory::Memcpy(&V, Raw.GetData() + Off, 4); Off += 4; return V; };
	MaskN = I32(); FlowN = I32(); DataHalf = F32();
	const int64 Need = 16 + (int64)MaskN * MaskN * 4 + (int64)FlowN * FlowN * 4;
	if (MaskN <= 0 || FlowN <= 0 || Raw.Num() < Need) { UE_LOG(LogDustlineWorld, Warning, TEXT("world.bin truncado")); return false; }
	Mask.SetNumUninitialized(MaskN * MaskN * 4); FMemory::Memcpy(Mask.GetData(), Raw.GetData() + Off, Mask.Num()); Off += Mask.Num();
	Flow.SetNumUninitialized(FlowN * FlowN * 4); FMemory::Memcpy(Flow.GetData(), Raw.GetData() + Off, Flow.Num());
	bHave = true;
	UE_LOG(LogDustlineWorld, Display, TEXT("Datos del mundo: máscara %d², agua %d²"), MaskN, FlowN);
	return true;
}
bool HasData() { return LoadData(); }

FMask SampleMask(float X, float Y)
{
	FMask M;
	if (!LoadData()) return M;
	const float Fx = (X + DataHalf) / (2.f * DataHalf) * MaskN - 0.5f, Fy = (Y + DataHalf) / (2.f * DataHalf) * MaskN - 0.5f;
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(Fx), 0, MaskN - 1), Y0 = FMath::Clamp(FMath::FloorToInt(Fy), 0, MaskN - 1);
	const int32 X1 = FMath::Min(X0 + 1, MaskN - 1), Y1 = FMath::Min(Y0 + 1, MaskN - 1);
	const float U = FMath::Clamp(Fx - X0, 0.f, 1.f), V = FMath::Clamp(Fy - Y0, 0.f, 1.f);
	float C[4] = {};
	for (int32 c = 0; c < 4; ++c)
	{
		const float A = Mask[(Y0 * MaskN + X0) * 4 + c], B = Mask[(Y0 * MaskN + X1) * 4 + c], Cc = Mask[(Y1 * MaskN + X0) * 4 + c], D = Mask[(Y1 * MaskN + X1) * 4 + c];
		C[c] = (A * (1 - U) * (1 - V) + B * U * (1 - V) + Cc * (1 - U) * V + D * U * V) / 255.f;
	}
	M.Dirt = C[0]; M.Mud = C[1]; M.TallGrass = C[2]; M.Gravel = C[3];
	return M;
}

static struct { bool bOn = false; float X = 0, Y = 0, R = 0, Depth = 0; FVector2D Flow; TArray<float> G; int32 N = 0; } GPool;
void SetTestPool(float X, float Y, float R, float DepthCm, FVector2D InFlow, TFunctionRef<float(float, float)> Ground)
{
	GPool.bOn = true; GPool.X = X; GPool.Y = Y; GPool.R = R; GPool.Depth = DepthCm; GPool.Flow = InFlow;
	GPool.N = FMath::CeilToInt(2.f * R / 100.f) + 1; GPool.G.SetNumUninitialized(GPool.N * GPool.N);
	for (int32 j = 0; j < GPool.N; ++j) for (int32 i = 0; i < GPool.N; ++i) GPool.G[j * GPool.N + i] = Ground(X - R + i * 100.f, Y - R + j * 100.f);
}

bool WaterAt(float X, float Y, float& OutLevel, FVector2D& OutFlow)
{
	OutLevel = -1e9f; OutFlow = FVector2D::ZeroVector;
	if (GPool.bOn && FVector2D::DistSquared(FVector2D(X, Y), FVector2D(GPool.X, GPool.Y)) < GPool.R * GPool.R)
	{   // charca de prueba: profundidad fija sobre el terreno (superficie que sigue al suelo, solo para medir)
		const float Fx = (X - GPool.X + GPool.R) / 100.f, Fy = (Y - GPool.Y + GPool.R) / 100.f;
		const int32 I = FMath::Clamp(FMath::FloorToInt(Fx), 0, GPool.N - 2), J = FMath::Clamp(FMath::FloorToInt(Fy), 0, GPool.N - 2);
		const float U = FMath::Clamp(Fx - I, 0.f, 1.f), V = FMath::Clamp(Fy - J, 0.f, 1.f);
		const float* G = GPool.G.GetData(); const int32 N = GPool.N;
		OutLevel = FMath::Lerp(FMath::Lerp(G[J * N + I], G[J * N + I + 1], U), FMath::Lerp(G[(J + 1) * N + I], G[(J + 1) * N + I + 1], U), V) + GPool.Depth;
		OutFlow = GPool.Flow; return true;
	}
	if (!LoadData()) return false;
	const float Fx = (X + DataHalf) / (2.f * DataHalf) * FlowN - 0.5f, Fy = (Y + DataHalf) / (2.f * DataHalf) * FlowN - 0.5f;
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(Fx), 0, FlowN - 1), Y0 = FMath::Clamp(FMath::FloorToInt(Fy), 0, FlowN - 1);
	const int32 X1 = FMath::Min(X0 + 1, FlowN - 1), Y1 = FMath::Min(Y0 + 1, FlowN - 1);
	const float U = FMath::Clamp(Fx - X0, 0.f, 1.f), V = FMath::Clamp(Fy - Y0, 0.f, 1.f);
	const int32 Xs[4] = { X0, X1, X0, X1 }, Ys[4] = { Y0, Y0, Y1, Y1 };
	const float Ws[4] = { (1 - U) * (1 - V), U * (1 - V), (1 - U) * V, U * V };
	float WSum = 0.f, L = 0.f, Vx = 0.f, Vy = 0.f;
	for (int32 k = 0; k < 4; ++k)
	{
		const uint8* C = &Flow[(Ys[k] * FlowN + Xs[k]) * 4];
		const uint16 Lv = (uint16)C[2] | ((uint16)C[3] << 8);
		if (Lv == 0) continue;
		WSum += Ws[k]; L += Ws[k] * ((float)Lv - 1000.f); Vx += Ws[k] * (float)(int8)C[0] / 20.f; Vy += Ws[k] * (float)(int8)C[1] / 20.f;
	}
	if (WSum < 0.15f) return false;   // en la orilla: solo cuenta si hay agua alrededor de verdad
	OutLevel = L / WSum; OutFlow = FVector2D(Vx, Vy) / WSum;
	return true;
}

namespace
{
	/** Propiedades físicas a partir de los pesos G.W (para el neumático T y la humedad Wet). */
	void Mix(FGround& G, int32 T, float Wet)
	{
		float Best = -1.f; G.Mu = G.PeakSlip = G.PeakAlpha = G.Falloff = G.Crr = G.Sink = G.Dig = G.Bump = G.Drag = 0.f;
		for (int32 k = 0; k < SNum; ++k)
		{
			const float w = G.W[k]; if (w <= 1e-4f) continue;
			if (w > Best) { Best = w; G.Dominant = k; }
			const FProps& P = GProps[k];
			G.Mu += w * FMath::Lerp(P.Mu, P.MuWet, Wet) * GTireMu[T][k];
			G.PeakSlip += w * P.Peak; G.PeakAlpha += w * P.Alpha; G.Falloff += w * P.Fall;
			G.Crr += w * FMath::Lerp(P.Crr, P.CrrWet, Wet) * ((T == 4 && k == SSand) ? 0.5f : 1.f);
			G.Sink += w * FMath::Lerp(P.Sink, P.SinkWet, Wet) * GTireSink[T];
			G.Dig += w * P.Dig * GTireDig[T];
			G.Bump += w * P.Bump; G.Drag += w * P.Drag;
		}
		G.Wet = Wet;
	}
}

// ---------------- mezcla de superficies (idéntica a la del material M_Terrain) ----------------
FGround GroundAt(float X, float Y, float Z, float Nz, int32 Tire)
{
	FGround G;
	const FLocal Wx = WeatherAt(X, Y);
	const FMask M = SampleMask(X, Y);
	const float Slope = 1.f - Nz;
	const float wRock = Smooth(0.22f, 0.42f, Slope);
	const float Flat = Smooth(0.80f, 0.95f, Nz);
	const float aDirt = M.Dirt * (1.f - wRock);
	const float aMud = Sat(M.Mud + Wx.Wet * (0.15f + 0.45f * M.Dirt) * Flat) * (1.f - wRock);
	const float aSand = Smooth(0.f, 280.f, 300.f - Z);
	const float aGravel = M.Gravel * (1.f - 0.5f * wRock);
	const float aSnow = Smooth(0.015f, 0.07f, Wx.SnowM) * (0.35f + 0.65f * Smooth(0.55f, 0.8f, Nz));
	const float aIce = Wx.Ice * (1.f - aSnow) * Smooth(0.7f, 0.9f, Nz);
	float* W = G.W;
	W[SGrass] = 1.f;
	auto Layer = [W](int32 K, float A) { for (int32 i = 0; i < SNum; ++i) W[i] *= (1.f - A); W[K] += A; };
	Layer(SDirt, aDirt); Layer(SMud, aMud); Layer(SSand, aSand); Layer(SGravel, aGravel); Layer(SRock, wRock); Layer(SSnow, aSnow); Layer(SIce, aIce);
	const float g = W[SGrass]; W[STallGrass] = g * M.TallGrass; W[SGrass] = g - W[STallGrass];

	const int32 T = FMath::Clamp(Tire, 0, 4);
	Mix(G, T, Wx.Wet);
	// nieve: cuanto más profunda, más se hunde y más frena (y la nieve húmeda/pesada frena más)
	if (W[SSnow] > 0.f)
	{
		const float D = FMath::Min(Wx.SnowM, 0.8f);
		G.Sink += W[SSnow] * D * 0.45f * GTireSink[T];   // el neumático compacta la nieve: se hunde ~la mitad del espesor
		G.Crr += W[SSnow] * D * 0.12f;
		G.Drag += W[SSnow] * D * 0.02f;
		// nieve muy fría y polvo: aún menos agarre; cerca de 0 °C, nieve húmeda algo mejor
		G.Mu *= FMath::Lerp(1.f, 0.9f, Smooth(-2.f, -12.f, Wx.TempC) * W[SSnow]);
	}
	G.SnowM = Wx.SnowM; G.TempC = Wx.TempC;
	return G;
}

// ---------------- lecho de ríos y lagos ----------------
void Submerge(FGround& G, float DepthM, float FlowMs, int32 Tire)
{
	if (DepthM <= 0.f) return;
	const int32 T = FMath::Clamp(Tire, 0, 4);
	const float A = Smooth(0.f, 0.12f, DepthM);   // en la lámina de la orilla todavía manda el suelo de fuera
	float* W = G.W;
	auto Layer = [W](int32 K, float X) { for (int32 i = 0; i < SNum; ++i) W[i] *= (1.f - X); W[K] += X; };
	// bajo el agua no hay nieve ni hierba seca: lo que hubiera pasa a ser fondo
	const float Gone = (W[SSnow] + W[SIce] + W[STallGrass] + W[SGrass] * 0.7f) * A;
	W[SSnow] *= 1.f - A; W[SIce] *= 1.f - A; W[STallGrass] *= 1.f - A; W[SGrass] *= 1.f - 0.7f * A; W[SDirt] += Gone;
	// corriente viva: cantos rodados y grava lavada; agua quieta: limo blando que se traga la rueda
	const float Fast = Smooth(0.35f, 1.4f, FlowMs), Still = 1.f - Smooth(0.15f, 0.7f, FlowMs);
	const float Silt = Still * Smooth(0.25f, 0.9f, DepthM);   // el limo se deposita en el agua quieta y honda; la orilla es tierra mojada
	Layer(SMud, A * Silt * 0.5f);
	Layer(SGravel, A * Fast * 0.7f);
	Layer(SRock, A * Fast * Smooth(1.2f, 2.5f, FlowMs) * 0.4f);
	Mix(G, T, 1.f);
	// las piedras del cauce tienen verdín (resbalan) y el limo hunde más
	const float Stones = W[SGravel] + W[SRock];
	G.Mu *= 1.f - A * 0.22f * Stones * (T == 3 ? 0.5f : 1.f);
	G.Sink += A * Silt * 0.03f * GTireSink[T];
	G.Bump += A * Fast * 0.012f;
	G.SnowM = 0.f;
}

// ---------------- clima ----------------
FSim& Sim() { return GSimInst; }

FLocal WeatherAt(float X, float Y)
{
	FLocal L;
	FSim& S = GSimInst;
	if (!S.bActive) return L;
	constexpr int32 G = FSim::G;
	const float Fx = (X + HalfCm) / SizeCm * G - 0.5f, Fy = (Y + HalfCm) / SizeCm * G - 0.5f;
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(Fx), 0, G - 1), Y0 = FMath::Clamp(FMath::FloorToInt(Fy), 0, G - 1);
	const int32 X1 = FMath::Min(X0 + 1, G - 1), Y1 = FMath::Min(Y0 + 1, G - 1);
	const float U = FMath::Clamp(Fx - X0, 0.f, 1.f), V = FMath::Clamp(Fy - Y0, 0.f, 1.f);
	auto B = [&](const float* A) { return A[Y0 * G + X0] * (1 - U) * (1 - V) + A[Y0 * G + X1] * U * (1 - V) + A[Y1 * G + X0] * (1 - U) * V + A[Y1 * G + X1] * U * V; };
	L.Wet = B(S.Wet); L.SnowM = B(S.Snow); L.Ice = B(S.Ice); L.TempC = B(S.Temp); L.Cloud = B(S.Cloud);
	const float P = B(S.Precip);
	const float SnowFrac = Smooth(1.5f, -0.5f, L.TempC);
	L.Snowfall = P * SnowFrac; L.Rain = P - L.Snowfall;
	L.Wind = S.Wind;
	return L;
}

const TCHAR* WeatherName(const FLocal& L)
{
	if (L.Snowfall > 0.55f) return TEXT("Nevada");
	if (L.Snowfall > 0.08f) return TEXT("Nieve");
	if (L.Rain > 0.55f) return TEXT("Lluvia fuerte");
	if (L.Rain > 0.08f) return TEXT("Lluvia");
	if (L.Cloud > 0.55f) return TEXT("Nublado");
	if (L.Cloud > 0.25f) return TEXT("Nubes y claros");
	return TEXT("Despejado");
}

float FSim::BaseTemp() const { return 10.f + 4.f * FMath::Sin(2.f * PI * (float)(Time / 1800.0)); }

void FSim::Init(TFunctionRef<float(float, float)> HeightCm, uint32 InSeed)
{
	Seed = InSeed; Time = 0.0; Storms.Reset(); bActive = true;
	for (int32 y = 0; y < G; ++y) for (int32 x = 0; x < G; ++x)
	{
		const float Px = ((x + 0.5f) / G * 2.f - 1.f) * HalfCm, Py = ((y + 0.5f) / G * 2.f - 1.f) * HalfCm;
		const float H = HeightCm(Px, Py);
		Alt[y * G + x] = H > -1e8f ? FMath::Max(H, 0.f) / 100.f : 0.f;
	}
	Cells(0.f);
	for (int32 i = 0; i < G * G; ++i)
	{
		Snow[i] = Temp[i] < -1.f ? FMath::Clamp(-Temp[i] * 0.06f, 0.f, 0.6f) : 0.f;
		const float Px = ((i % G + 0.5f) / G * 2.f - 1.f);
		Wet[i] = 0.12f + 0.25f * Sat(-Px);   // el oeste es más húmedo
	}
	for (int32 i = 0; i < 3; ++i) SpawnStorm(true);
	// arranque: 15 minutos de clima simulados para que el suelo ya tenga historia (charcos donde llovió, etc.)
	for (int32 i = 0; i < 180; ++i) Step(5.f);
	UE_LOG(LogDustlineWorld, Display, TEXT("Clima iniciado: %d frentes, temp base %.1f"), Storms.Num(), BaseTemp());
}

void FSim::SpawnStorm(bool bAnywhere)
{
	FRandomStream R(Seed++ * 7919u + (uint32)(Time * 13.0));
	FStorm S;
	S.R = R.FRandRange(50000.f, 110000.f); S.Peak = R.FRandRange(0.45f, 1.f); S.Life = R.FRandRange(480.f, 1000.f); S.Age = 0.f;
	const FVector2D Wd = Wind.GetSafeNormal(), Perp(-Wd.Y, Wd.X);
	if (bAnywhere) { S.P = FVector2D(R.FRandRange(-HalfCm, HalfCm), R.FRandRange(-HalfCm, HalfCm)); S.Age = R.FRandRange(0.f, S.Life * 0.6f); }
	else if (R.FRand() < 0.55f)  // dirigido a pasar cerca del jugador
		S.P = Focus - Wd * R.FRandRange(180000.f, 300000.f) + Perp * R.FRandRange(-70000.f, 70000.f);
	else
		S.P = -Wd * HalfCm * 1.1f + Perp * R.FRandRange(-HalfCm, HalfCm);
	Storms.Add(S);
}

void FSim::Cells(float Dt)
{
	const float Base = BaseTemp();
	for (int32 y = 0; y < G; ++y) for (int32 x = 0; x < G; ++x)
	{
		const int32 i = y * G + x;
		const FVector2D P(((x + 0.5f) / G * 2.f - 1.f) * HalfCm, ((y + 0.5f) / G * 2.f - 1.f) * HalfCm);
		float Pr = 0.f, Cl = 0.08f;
		for (const FStorm& S : Storms)
		{
			const float E = Smooth(0.f, 0.2f, S.Age / S.Life) * (1.f - Smooth(0.7f, 1.f, S.Age / S.Life));
			const float D2 = FVector2D::DistSquared(P, S.P);
			Pr += S.Peak * E * FMath::Exp(-D2 / (S.R * S.R));
			Cl += S.Peak * E * 1.3f * FMath::Exp(-D2 / (3.2f * S.R * S.R));
		}
		const float Ux = P.X / HalfCm;
		Pr *= (0.75f + 0.5f * FMath::Min(Alt[i] / 300.f, 1.2f)) * (1.f - 0.35f * (Ux + 1.f) * 0.5f);   // más en montaña, menos en el este seco
		if (bForced) { Pr = FMath::Max(Pr * 0.f, ForceRain + ForceSnow); Cl = FMath::Max(Cl, Pr > 0 ? 0.9f : 0.f); }
		Pr = Sat(Pr); Cl = Sat(FMath::Max(Cl, Pr));
		Precip[i] = Pr; Cloud[i] = Cl;
		float T = Base - Alt[i] * 0.045f + 3.5f * (P.Y / HalfCm) - 2.5f * Pr;
		if (bForced && ForceSnow > 0.f) T = FMath::Min(T, -3.f);
		if (bForced && ForceRain > 0.f) T = FMath::Max(T, 6.f);
		Temp[i] = T;
		if (Dt <= 0.f) continue;
		const float SnowFall = Pr * Smooth(1.5f, -0.5f, T), RainF = Pr - SnowFall;
		Snow[i] += SnowFall * 0.0007f * Dt;
		if (T > 0.f)
		{
			const float Melt = FMath::Min(Snow[i], (0.00003f * T + 0.0002f * RainF) * Dt);
			Snow[i] -= Melt; Wet[i] += Melt * 8.f;
		}
		Wet[i] += RainF * 0.006f * Dt;
		Wet[i] -= (0.0007f + 0.0001f * FMath::Max(T, 0.f)) * (1.f - 0.7f * Cl) * Dt * (Snow[i] > 0.02f ? 0.3f : 1.f);
		if (T < -0.5f) Ice[i] += (Wet[i] > 0.3f ? (Wet[i] - 0.3f) * 0.01f * Dt : 0.f);
		else Ice[i] -= 0.004f * Dt * (T + 0.5f);
		Wet[i] = Sat(Wet[i]); Ice[i] = Sat(Ice[i]); Snow[i] = FMath::Clamp(Snow[i], 0.f, 1.2f);
	}
}

void FSim::Step(float Dt)
{
	if (!bActive) return;
	Time += Dt;
	const float Ang = 0.2f + 0.6f * FMath::Sin((float)(Time / 900.0)), Spd = 6.5f + 2.5f * FMath::Sin((float)(Time / 410.0) + 1.f);
	Wind = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Spd;
	for (FStorm& S : Storms) { S.Age += Dt; S.P += Wind * 100.f * Dt; }
	Storms.RemoveAll([](const FStorm& S) { return S.Age > S.Life || FMath::Abs(S.P.X) > HalfCm * 1.6f || FMath::Abs(S.P.Y) > HalfCm * 1.6f; });
	// nuevos frentes con huecos de cielo despejado entre medias
	static float NextSpawn = 0.f;
	NextSpawn -= Dt;
	if (Storms.Num() < 3 && NextSpawn <= 0.f) { SpawnStorm(false); FRandomStream R(Seed * 31u); NextSpawn = R.FRandRange(60.f, 300.f); }
	Cells(Dt);
}

void FSim::Fill(uint8* BGRA) const
{
	for (int32 i = 0; i < G * G; ++i)
	{
		BGRA[i * 4 + 0] = (uint8)FMath::RoundToInt(Sat(Ice[i]) * 255.f);
		BGRA[i * 4 + 1] = (uint8)FMath::RoundToInt(Sat(Snow[i] / 0.5f) * 255.f);
		BGRA[i * 4 + 2] = (uint8)FMath::RoundToInt(Sat(Wet[i]) * 255.f);
		BGRA[i * 4 + 3] = (uint8)FMath::RoundToInt(Sat(Precip[i]) * 255.f);
	}
}
}
