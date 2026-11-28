#include "DustlineCar.h"
#include "DustlineAudio.h"
#include "DustlineSettings.h"
#include "DustlineWorld.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogDustlineFx, Log, All);
using namespace DustlineWorld;

/*
 * Lo que el coche hace al mundo y suena:
 *  - Partículas (salpicaduras de agua, terrones de barro, nieve en polvo, polvo en seco): CPU, billboards en una malla procedural.
 *  - Rodadas: tiras con la huella de los tacos que oscurecen el suelo (material Modulate) en barro, nieve, arena, hierba
 *    mojada y tierra húmeda; se desvanecen a los pocos minutos.
 *  - Sonido: parámetros al sintetizador procedural (DustlineAudio).
 */
namespace
{
	constexpr int32 MaxParts = 700, MaxRut = 350;
	FORCEINLINE float Rand01(FRandomStream& R) { return R.FRand(); }
	FRandomStream GFxRng(4242);
}

void ADustlineCar::InitFx()
{
	if (Synth) return;
	Synth = NewObject<UDustlineSynth>(this, TEXT("EngineSynth"));
	Synth->SetupAttachment(GetRootComponent()); Synth->RegisterComponent(); Synth->Start();
	auto MkMesh = [&](const TCHAR* Name) {
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this, Name);
		M->SetUsingAbsoluteLocation(true); M->SetUsingAbsoluteRotation(true); M->SetUsingAbsoluteScale(true);
		M->SetupAttachment(GetRootComponent()); M->RegisterComponent();
		M->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetCastShadow(false); M->bUseAsyncCooking = false;
		return M; };
	FxMesh = MkMesh(TEXT("FxMesh")); RutMesh = MkMesh(TEXT("RutMesh"));
	if (UMaterialInterface* B = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Weather/M_Fx.M_Fx"))) FxMat = UMaterialInstanceDynamic::Create(B, this);
	if (FParse::Param(FCommandLine::Get(), TEXT("audiodump") ) || FCString::Strifind(FCommandLine::Get(), TEXT("-audiodump=")))
	{ DumpGen = MakeShared<FDustlineSoundGen>(); DumpGen->Init(48000); }
}

void ADustlineCar::UpdateFx(float Dt)
{
	if (!FxMesh) return;
	if (!bGarage && !bShowcase && !bPaused) { EmitFx(Dt); UpdateRuts(Dt); }
	// ---- simular y dibujar partículas ----
	for (int32 i = Parts.Num() - 1; i >= 0; --i)
	{
		FFxPart& p = Parts[i];
		p.Age += bPaused ? 0.f : Dt;
		if (p.Age >= p.Life) { Parts.RemoveAtSwap(i, EAllowShrinking::No); continue; }
		if (bPaused) continue;
		p.V.Z -= p.Grav * 100.f * Dt;
		p.V *= FMath::Exp(-p.Drag * Dt);
		p.P += p.V * Dt;
	}
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C; TArray<FProcMeshTangent> Tg;
	V.Reserve(Parts.Num() * 4); T.Reserve(Parts.Num() * 6);
	const FRotator CR = Cam->GetComponentRotation(); const FVector R = FRotationMatrix(CR).GetUnitAxis(EAxis::Y), U = FRotationMatrix(CR).GetUnitAxis(EAxis::Z), Fw = CR.Vector();
	for (const FFxPart& p : Parts)
	{
		const float a = p.Age / p.Life, S = FMath::Lerp(p.S0, p.S1, FMath::Sqrt(a));
		const float Alpha = p.C.A * FMath::Min(1.f, p.Age / 0.06f) * (1.f - a * a);
		const int32 b = V.Num();
		V.Add(p.P - R * S - U * S); V.Add(p.P + R * S - U * S); V.Add(p.P + R * S + U * S); V.Add(p.P - R * S + U * S);
		UV.Add(FVector2D(0, 1)); UV.Add(FVector2D(1, 1)); UV.Add(FVector2D(1, 0)); UV.Add(FVector2D(0, 0));
		for (int32 k = 0; k < 4; ++k) { N.Add(-Fw); C.Add(FLinearColor(p.C.R, p.C.G, p.C.B, Alpha)); Tg.Add(FProcMeshTangent(R, false)); }
		T.Append({ b, b + 2, b + 1, b, b + 3, b + 2 });
	}
	FxMesh->ClearAllMeshSections();
	if (V.Num()) { FxMesh->CreateMeshSection_LinearColor(0, V, T, N, UV, C, Tg, false); if (FxMat) FxMesh->SetMaterial(0, FxMat); }
	if (FxMat) { const FVector L = GetActorLocation(); FxMat->SetScalarParameterValue(TEXT("Light"), 1.f - 0.5f * WeatherAt(L.X, L.Y).Cloud); }
	UpdateSound(Dt);
}

void ADustlineCar::EmitFx(float Dt)
{
	const FVector Fwd = GetActorForwardVector(), Right = GetActorRightVector();
	const float Speed = Vel.Size();
	const FVector VelCm = Vel * 100.f;
	auto Spawn = [&](const FVector& P, const FVector& Vv, float Life, float S0, float S1, float Grav, float Drag, const FLinearColor& Col)
	{
		if (Parts.Num() >= MaxParts) return;
		Parts.Add({ P, Vv, 0.f, Life * (0.75f + 0.5f * GFxRng.FRand()), S0, S1, Grav, Drag, Col });
	};
	auto Jit = [&](float A) { return FVector(GFxRng.FRandRange(-A, A), GFxRng.FRandRange(-A, A), GFxRng.FRandRange(-A, A)); };
	// golpe al entrar en el agua
	if (bInWater && WaterDepthM > 0.1f && !bWasInWater && Speed > 2.f)
	{
		const int32 Nb = FMath::Clamp((int32)(Speed * 9.f), 20, 140);
		const FVector C0 = GetActorLocation();
		for (int32 k = 0; k < Nb; ++k)
		{
			const FVector Dir = (Fwd * GFxRng.FRandRange(0.2f, 1.f) + Right * GFxRng.FRandRange(-1.f, 1.f)).GetSafeNormal();
			Spawn(FVector(C0.X, C0.Y, WaterZ * 100.f) + Fwd * 150.f + Dir * 80.f, Dir * Speed * 60.f + FVector(0, 0, GFxRng.FRandRange(300.f, 300.f + Speed * 60.f)), 1.1f, 25.f, 110.f, 9.8f, 0.6f, FLinearColor(0.78f, 0.83f, 0.88f, 0.55f));
		}
	}
	bWasInWater = bInWater && WaterDepthM > 0.1f;
	// ola de proa: el agua se levanta delante del parachoques y salpica a los lados
	if (bInWater && BowWave > 0.05f && Speed > 1.f)
	{
		const float n = BowWave * Speed * 40.f * Dt; int32 Nb = (int32)n; if (GFxRng.FRand() < n - Nb) ++Nb;
		const FVector C0 = GetActorLocation() + Fwd * (Sp.BodyL * 52.f);
		for (int32 k = 0; k < Nb; ++k)
		{
			const float sd = GFxRng.FRandRange(-1.f, 1.f);
			const FVector Pp = FVector(C0.X, C0.Y, WaterZ * 100.f + 5.f) + Right * (sd * Sp.BodyW * 50.f);
			const FVector Vv = Fwd * Speed * 70.f + Right * (sd * Speed * 60.f) + FVector(0, 0, GFxRng.FRandRange(150.f, 250.f + BowWave * 600.f));
			Spawn(Pp, Vv + VelCm * 0.3f, 0.9f, 18.f, 80.f, 9.8f, 0.8f, FLinearColor(0.80f, 0.85f, 0.90f, 0.5f));
		}
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const FWheelPhys& W = Wh[i];
		if (!W.Contact) continue;
		const float SlipV = FMath::Abs(W.Omega * Sp.R - FVector::DotProduct(Vel, Fwd));
		const FVector Side = Right * ((i % 2) ? 1.f : -1.f);
		const FVector Base = W.CP + W.CN * 10.f;
		auto Count = [&](float Rate) { const float n = Rate * Dt; int32 c = (int32)n; if (GFxRng.FRand() < n - c) ++c; return c; };
		if (W.bWater && Speed > 0.8f)
		{
			const int32 n = Count(18.f * FMath::Min(Speed, 10.f) + 10.f * SlipV);
			for (int32 k = 0; k < n; ++k)
				Spawn(Base + Jit(15.f), VelCm * 0.5f + Side * (80.f + Speed * 25.f) + FVector(0, 0, 150.f + Speed * 30.f) + Jit(80.f), 0.8f, 12.f, 60.f, 9.8f, 1.f, FLinearColor(0.72f, 0.78f, 0.84f, 0.45f));
			continue;
		}
		if (W.WMud > 0.3f && (Speed > 2.f || SlipV > 1.5f))
		{
			const int32 n = Count(14.f * (FMath::Min(Speed, 10.f) + 2.f * SlipV) * W.WMud);
			for (int32 k = 0; k < n; ++k)
				Spawn(Base + Jit(12.f), VelCm * 0.6f - Fwd * (SlipV * 60.f) * FMath::Sign(W.Omega) + FVector(0, 0, 120.f + SlipV * 40.f) + Side * 40.f + Jit(70.f), 0.9f, 4.f, 8.f, 9.8f, 0.3f, FLinearColor(0.045f, 0.032f, 0.02f, 0.95f));
		}
		if (W.WSnow > 0.3f && (Speed > 2.f || SlipV > 1.5f))
		{
			const int32 n = Count(10.f * (FMath::Min(Speed, 12.f) + 2.f * SlipV) * W.WSnow);
			for (int32 k = 0; k < n; ++k)
				Spawn(Base + Jit(15.f), VelCm * 0.4f - Fwd * SlipV * 40.f * FMath::Sign(W.Omega) + FVector(0, 0, 80.f + SlipV * 30.f) + Jit(60.f), 1.3f, 18.f, 110.f, 1.5f, 2.f, FLinearColor(0.86f, 0.89f, 0.93f, 0.5f));
		}
		const float Dry = W.WDirt + W.WSand + W.WGrav;
		if (W.Wet < 0.3f && Dry > 0.4f && Speed > 4.f && i >= 2)
		{
			const int32 n = Count(4.f * Speed * Dry * (1.f - W.Wet * 3.f));
			for (int32 k = 0; k < n; ++k)
				Spawn(Base - Fwd * 40.f + Jit(20.f), VelCm * 0.25f + FVector(0, 0, 40.f) + Jit(60.f), 2.4f, 40.f, 280.f, -0.15f, 1.6f, FLinearColor(0.40f, 0.35f, 0.27f, 0.22f));
		}
	}
}

void ADustlineCar::UpdateRuts(float Dt)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Fwd = GetActorForwardVector();
	for (int32 i = 0; i < 4; ++i)
	{
		const FWheelPhys& W = Wh[i];
		TArray<FRutPt>& R = Ruts[i];
		if (!W.Contact || W.bWater) continue;
		const float Soft = W.WMud + W.WSnow + 0.8f * W.WSand + 0.35f * W.WTall + 0.2f * W.WGrass * W.Wet + 0.5f * W.WDirt * W.Wet + 0.15f * W.WGrav;
		const float A = FMath::Clamp(Soft * (0.4f + W.Sink * 8.f), 0.f, 0.95f);
		if (A < 0.08f) { if (R.Num()) R.Last().T = FMath::Min(R.Last().T, Now); continue; }
		const FVector P = W.CP;
		const bool bStart = R.Num() == 0 || Now - R.Last().T > 0.6 || FVector::DistSquared(R.Last().P, P) > 150.f * 150.f;
		if (!bStart && FVector::DistSquared(R.Last().P, P) < 30.f * 30.f) continue;
		const float Ws = W.WMud + W.WSnow + W.WSand + W.WGrass + W.WTall + W.WDirt + W.WGrav + 1e-3f;
		FLinearColor C = (FLinearColor(0.42f, 0.36f, 0.30f) * W.WMud + FLinearColor(0.60f, 0.66f, 0.78f) * W.WSnow + FLinearColor(0.78f, 0.72f, 0.62f) * W.WSand
			+ FLinearColor(0.66f, 0.72f, 0.58f) * (W.WGrass + W.WTall) + FLinearColor(0.58f, 0.53f, 0.48f) * W.WDirt + FLinearColor(0.80f, 0.80f, 0.80f) * W.WGrav) * (1.f / Ws);
		FVector Dir = bStart ? Fwd : (P - R.Last().P).GetSafeNormal();
		R.Add({ P, W.CN, Dir, Sp.Width * 100.f, A, C, Now, bStart });
		if (R.Num() > MaxRut) { R.RemoveAt(0); R[0].bStart = true; }
		bRutDirty = true;
	}
	RutT += Dt;
	if (!bRutDirty || RutT < 0.12f || !RutMesh) return;
	RutT = 0.f; bRutDirty = false;
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C; TArray<FProcMeshTangent> Tg;
	for (int32 w = 0; w < 4; ++w)
	{
		const TArray<FRutPt>& R = Ruts[w];
		float Dist = 0.f;
		for (int32 k = 0; k < R.Num(); ++k)
		{
			const FRutPt& p = R[k];
			if (!p.bStart && k > 0) Dist += FVector::Dist(R[k - 1].P, p.P);
			const float Age = (float)(Now - p.T), Fade = 1.f - FMath::Clamp((Age - 180.f) / 120.f, 0.f, 1.f);
			const FVector Lat = FVector::CrossProduct(p.N, p.Dir).GetSafeNormal() * p.W * 0.5f, Up = p.N * 3.f;
			const int32 b = V.Num();
			V.Add(p.P + Up - Lat); V.Add(p.P + Up + Lat);
			UV.Add(FVector2D(0, Dist / 60.f)); UV.Add(FVector2D(1, Dist / 60.f));
			for (int32 e = 0; e < 2; ++e) { N.Add(p.N); C.Add(FLinearColor(p.C.R, p.C.G, p.C.B, p.A * Fade)); Tg.Add(FProcMeshTangent(p.Dir, false)); }
			if (k > 0 && !p.bStart) T.Append({ b - 2, b + 1, b, b - 2, b - 1, b + 1 });   // cara visible hacia arriba
		}
	}
	RutMesh->ClearAllMeshSections();
	if (T.Num())
	{
		RutMesh->CreateMeshSection_LinearColor(0, V, T, N, UV, C, Tg, false);
		static UMaterialInterface* RutMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Weather/M_Rut.M_Rut"));
		if (RutMat) RutMesh->SetMaterial(0, RutMat);
	}
}

void ADustlineCar::UpdateSound(float Dt)
{
	FDustlineSoundParams P;
	const FDustlineSettings& S = FDustlineSettings::Get();
	P.Rpm = Rpm; P.Load = LastThr * (bStalled ? 0.f : 1.f); P.Spool = TurboSpool * FMath::Clamp(Setup.Turbo, 0.f, 1.5f);
	P.Speed = (bGarage || bShowcase) ? 0.f : Vel.Size();
	int32 Cyl = FMath::Clamp(FMath::RoundToInt(Setup.EngineCC / 650.f), 2, 12); if (Cyl > 6 && (Cyl % 2)) Cyl += 1;
	P.Cyl = Cyl; P.bDiesel = Sp.bDiesel; P.bEngineOn = !bStalled;
	float Screech = 0.f, Nw = 0.f;
	for (int32 i = 0; i < 4; ++i)
	{
		const FWheelPhys& W = Wh[i]; if (!W.Contact) continue;
		Nw += 1.f;
		P.Gravel += W.WGrav; P.Mud += W.WMud; P.Snow += W.WSnow; P.Grass += W.WGrass + W.WTall; P.Hard += W.WRock + W.WDirt * (1.f - W.Wet) + W.WSand * 0.3f;
		const float Comb = FMath::Sqrt(FMath::Square(W.Slip / FMath::Max(W.Peak, 0.05f)) + FMath::Square(W.SlipA));
		const float Hard = W.WRock + W.WDirt * (1.f - W.Wet) * 0.8f + W.WGrav * 0.3f;
		Screech = FMath::Max(Screech, FMath::Clamp((Comb - 1.3f) / 2.f, 0.f, 1.f) * Hard);
	}
	if (Nw > 0.f) { P.Gravel /= Nw; P.Mud /= Nw; P.Snow /= Nw; P.Grass /= Nw; P.Hard /= Nw; }
	P.Screech = Screech * FMath::Clamp(Vel.Size() / 3.f, 0.f, 1.f);
	P.Wade = bInWater ? FMath::Clamp(WaterDepthM / 0.8f + 0.25f, 0.f, 1.f) : 0.f;
	static bool bPrevWet = false;
	if (bInWater && WaterDepthM > 0.1f && !bPrevWet) P.Splash = FMath::Clamp(Vel.Size() / 8.f, 0.3f, 1.f);
	bPrevWet = bInWater && WaterDepthM > 0.1f;
	const FVector L = GetActorLocation(); const FLocal Wx = WeatherAt(L.X, L.Y);
	P.Rain = Wx.Rain + 0.5f * Wx.Snowfall * 0.f; P.Wind = Wx.Wind.Size();
	if (PrevThr > 0.6f && LastThr < 0.1f && TurboSpool > 0.5f && Setup.Turbo > 0.2f) P.BlowOff = 1.f;
	P.Impact = ImpactSnd; ImpactSnd = 0.f;
	P.Volume = S.Volume / 100.f * (bPaused ? 0.25f : 1.f) * ((bGarage || bShowcase) ? 0.6f : 1.f);
	if (Synth) Synth->SetParams(P);
	if (DumpGen.IsValid())
	{
		const int32 Ns = FMath::RoundToInt(Dt * 48000.f);
		TArray<float> Buf; Buf.SetNumUninitialized(Ns);
		DumpGen->Render(P, Buf.GetData(), Ns);
		for (float x : Buf) AudioDump.Add((int16)FMath::Clamp(FMath::RoundToInt(x * 32000.f), -32767, 32767));
	}
}

void ADustlineCar::WriteAudioDump()
{
	FString Path;
	if (AudioDump.Num() == 0 || !FParse::Value(FCommandLine::Get(), TEXT("audiodump="), Path)) return;
	TArray<uint8> W;
	auto U32 = [&](uint32 v) { W.Append((uint8*)&v, 4); }; auto U16 = [&](uint16 v) { W.Append((uint8*)&v, 2); };
	const uint32 Bytes = AudioDump.Num() * 2;
	W.Append((const uint8*)"RIFF", 4); U32(36 + Bytes); W.Append((const uint8*)"WAVEfmt ", 8); U32(16); U16(1); U16(1); U32(48000); U32(96000); U16(2); U16(16);
	W.Append((const uint8*)"data", 4); U32(Bytes); W.Append((uint8*)AudioDump.GetData(), Bytes);
	FFileHelper::SaveArrayToFile(W, *Path);
	UE_LOG(LogDustlineFx, Display, TEXT("AUDIO_DUMP %s %.1fs"), *Path, AudioDump.Num() / 48000.f);
}
