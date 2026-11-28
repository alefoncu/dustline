#include "DustlineAudio.h"

/*
 * Sonido procedural (sin muestras grabadas):
 *  - Motor: armónicos de la frecuencia de encendido (rpm/60 · cilindros/2) con más brillo bajo carga, retumbo del cigüeñal,
 *    rascado de la combustión sincronizado con cada explosión y, en diésel, el golpeteo de la inyección. Silbido del turbo
 *    según la carga del turbo y soplido de la válvula al soltar gas.
 *  - Ruedas: rodadura según velocidad y suelo, chasquidos de grava, chapoteo del barro, crujido de la nieve y chirrido al
 *    derrapar en suelo duro.
 *  - Agua (vadeo y golpe al entrar), lluvia (siseo + gotas) y viento según la velocidad.
 */
namespace
{
	FORCEINLINE float OnePole(float& State, float In, float Fc, float SR) { const float A = 1.f - FMath::Exp(-2.f * PI * Fc / SR); State += A * (In - State); return State; }
}

void FDustlineSoundGen::Render(const FDustlineSoundParams& P, float* Out, int32 N)
{
	const float Tgt[14] = { P.Rpm, P.Load, P.Spool, P.Speed, P.Screech, P.Gravel, P.Mud, P.Snow, P.Grass + P.Hard, P.Wade, P.Rain, P.Wind, P.Volume, P.bEngineOn ? 1.f : 0.f };
	const float K = 1.f - FMath::Exp(-1.f / (SR * 0.04f));
	if (P.Splash > Splash) Splash = P.Splash;
	if (P.Impact > Thud) { Thud = P.Impact; Ring = P.Impact; }
	if (P.BlowOff > 0.5f && Blow < 0.1f) Blow = FMath::Min(1.f, P.BlowOff);
	const float Cyl = (float)FMath::Clamp(P.Cyl, 2, 12);
	for (int32 n = 0; n < N; ++n)
	{
		for (int32 i = 0; i < 14; ++i) Smooth[i] += (Tgt[i] - Smooth[i]) * K;
		const float Rpm = FMath::Max(Smooth[0], 0.f), Load = FMath::Clamp(Smooth[1], 0.f, 1.f), Spool = Smooth[2], V = FMath::Max(Smooth[3], 0.f), On = Smooth[13];

		// ---- motor ----
		const float FFire = Rpm / 60.f * Cyl * 0.5f, FCrank = Rpm / 60.f;
		Phase += FFire / SR; bool bWrap = false; if (Phase >= 1.0) { Phase -= 1.0; bWrap = true; }
		CrankPhase += FCrank / SR; if (CrankPhase >= 1.0) CrankPhase -= 1.0;
		const float Ph = (float)Phase * 2.f * PI;
		float Harm = 0.f;
		for (int32 k = 1; k <= 7; ++k) Harm += FMath::Pow((float)k, -(1.3f - 0.6f * Load)) * FMath::Sin(Ph * k + k * 0.7f);
		const float Sub = 0.35f * FMath::Sin((float)CrankPhase * 2.f * PI);
		const float Pulse = FMath::Pow(0.5f + 0.5f * FMath::Sin(Ph), 6.f);
		const float Rasp = Noise() * Pulse * (0.3f + 0.9f * Load);
		float Eng = OnePole(LpEngine, Harm * 0.5f + Sub + Rasp * 0.6f, 300.f + 2500.f * Load + Rpm * 0.25f, SR);
		if (P.bDiesel)
		{
			if (bWrap) Clatter = 1.f;
			const float Nz = Noise(); const float Hp = Nz - HpPrev; HpPrev = Nz;   // paso alto sencillo
			Eng += Hp * Clatter * 0.22f; Clatter *= FMath::Exp(-1.f / (SR * 0.003f));
		}
		Eng *= 0.26f * (0.45f + 0.55f * Load) * (0.55f + 0.45f * FMath::Min(Rpm / 6000.f, 1.2f)) * On;
		// turbo
		TurboPhase += (1800.f + 5200.f * Spool) / SR; if (TurboPhase >= 1.0) TurboPhase -= 1.0;
		float Turbo = FMath::Sin((float)TurboPhase * 2.f * PI) * 0.022f * Spool * (0.3f + 0.7f * Load) * On;
		Turbo += Noise() * Blow * 0.12f; Blow *= FMath::Exp(-1.f / (SR * 0.25f));

		// ---- ruedas ----
		const float Gr = Smooth[5], Mud = Smooth[6], Snow = Smooth[7], Firm = Smooth[8];
		const float Roll = OnePole(LpRoll, Noise(), 150.f + 30.f * V, SR) * FMath::Min(0.012f * V, 0.22f) * (0.35f + Firm * 0.7f + Gr * 0.4f);
		if (Noise() * 0.5f + 0.5f < Gr * V * 0.004f) Crackle = 0.6f + 0.4f * (Noise() * 0.5f + 0.5f);
		const float Crack = Noise() * Crackle * 0.3f; Crackle *= 0.93f;
		const float Squelch = OnePole(LpMud, Noise(), 220.f, SR) * Mud * (FMath::Min(V, 8.f) * 0.05f + Load * 0.12f) * (0.6f + 0.4f * FMath::Sin(V * 1.3f * (float)Time * 2.f * PI));
		const float SnowN = OnePole(BpSnowA, Noise(), 4000.f, SR) - OnePole(BpSnowB, BpSnowA, 1200.f, SR);
		const float Crunch = SnowN * Snow * FMath::Min(V, 10.f) * 0.05f * (0.7f + 0.3f * Noise());
		Time += 1.0 / SR; if (Time > 1000.0) Time -= 1000.0;
		SqPhase += (760.f + 40.f * FMath::Sin((float)Time * 7.f)) / SR; if (SqPhase >= 1.0) SqPhase -= 1.0;
		const float Screech = (FMath::Sin((float)SqPhase * 2.f * PI) * 0.6f + OnePole(LpSq, Noise(), 1500.f, SR) * 0.8f) * Smooth[4] * 0.16f;

		// ---- agua, lluvia, viento ----
		const float Wade = OnePole(LpWade, Noise(), 500.f, SR) * Smooth[9] * (0.06f + 0.03f * FMath::Min(V, 6.f));
		const float Spl = Noise() * Splash * 0.45f; Splash *= FMath::Exp(-1.f / (SR * 0.6f));
		if (Noise() * 0.5f + 0.5f < Smooth[10] * 0.002f) Patter = 1.f;
		const float Rain = OnePole(LpRain, Noise(), 5000.f, SR) * Smooth[10] * 0.10f + Noise() * Patter * 0.12f; Patter *= 0.9f;
		const float WindAmt = FMath::Square(FMath::Min(V / 30.f, 1.f)) * 0.18f + Smooth[11] * 0.008f;
		const float Wind = OnePole(LpWind, Noise(), 120.f + V * 15.f, SR) * WindAmt * 3.f;

		// golpe: estampido grave + chapa que vibra
		const float ThudS = OnePole(LpThud, Noise(), 180.f, SR) * Thud * 2.2f; Thud *= FMath::Exp(-1.f / (SR * 0.18f));
		RingPhase += 210.f / SR; if (RingPhase >= 1.0) RingPhase -= 1.0;
		const float RingS = (FMath::Sin((float)RingPhase * 2.f * PI) + 0.5f * FMath::Sin((float)RingPhase * 2.f * PI * 2.73f)) * Ring * 0.15f; Ring *= FMath::Exp(-1.f / (SR * 0.35f));
		float X = (ThudS + RingS + Eng + Turbo + Roll + Crack + Squelch + Crunch + Screech + Wade + Spl + Rain + Wind) * Smooth[12];
		X = X / (1.f + FMath::Abs(X));   // saturación suave
		Out[n] = X;
	}
}

UDustlineSynth::UDustlineSynth(const FObjectInitializer& OI) : Super(OI)
{
	NumChannels = 1;
	bAllowSpatialization = false;
	bAutoActivate = false;
}

void UDustlineSynth::SetParams(const FDustlineSoundParams& P)
{
	FScopeLock L(&Lock);
	const float Spl = FMath::Max(Params.Splash, P.Splash), Imp = FMath::Max(Params.Impact, P.Impact);
	Params = P; Params.Splash = Spl; Params.Impact = Imp;
}

bool UDustlineSynth::Init(int32& SampleRate)
{
	NumChannels = 1;
	Gen.Init(SampleRate);
	return true;
}

int32 UDustlineSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FDustlineSoundParams P;
	{ FScopeLock L(&Lock); P = Params; Params.Splash = 0.f; Params.BlowOff = 0.f; Params.Impact = 0.f; }
	Gen.Render(P, OutAudio, NumSamples);
	return NumSamples;
}
