#pragma once
#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "DustlineAudio.generated.h"

/** Estado del coche y del entorno que alimenta el sonido (lo escribe el hilo de juego, lo lee el de audio). */
struct FDustlineSoundParams
{
	float Rpm = 850.f, Load = 0.f, Spool = 0.f, Speed = 0.f;   // m/s
	int32 Cyl = 6; bool bDiesel = false, bEngineOn = true;
	float Screech = 0.f;      // derrape en suelo duro 0..1
	float Gravel = 0.f, Mud = 0.f, Snow = 0.f, Grass = 0.f, Hard = 0.f;   // peso de cada suelo bajo las ruedas
	float Wade = 0.f;         // profundidad del agua relativa 0..1
	float Splash = 0.f;       // golpe de agua (se consume en el audio)
	float Rain = 0.f, Wind = 0.f;
	float Volume = 0.8f;
	float BlowOff = 0.f;      // soltar gas con el turbo cargado
	float Impact = 0.f;       // golpe (se consume en el audio)
};

/** Generador de sonido puro (sin UObjects): se usa en tiempo real y para renderizar pruebas a WAV. */
class FDustlineSoundGen
{
public:
	void Init(int32 InSampleRate) { SR = (float)InSampleRate; }
	void Render(const FDustlineSoundParams& P, float* Out, int32 N);
private:
	float SR = 48000.f;
	double Phase = 0.0, CrankPhase = 0.0, TurboPhase = 0.0, SqPhase = 0.0, Time = 0.0;
	float LpEngine = 0.f, LpRoll = 0.f, LpMud = 0.f, BpSnowA = 0.f, BpSnowB = 0.f, LpRain = 0.f, LpWind = 0.f, LpWade = 0.f, LpSq = 0.f, HpClat = 0.f, HpPrev = 0.f;
	float Thud = 0.f, Ring = 0.f; double RingPhase = 0.0; float LpThud = 0.f;
	float Splash = 0.f, Blow = 0.f, Crackle = 0.f, Clatter = 0.f, Patter = 0.f;
	float Smooth[16] = {};   // parámetros suavizados (sin chasquidos)
	uint32 Rng = 22222u;
	float Noise() { Rng = Rng * 1664525u + 1013904223u; return ((Rng >> 9) & 0x7FFFFF) / (float)0x400000 - 1.f; }
};

UCLASS()
class DUSTLINE_API UDustlineSynth : public USynthComponent
{
	GENERATED_BODY()
public:
	UDustlineSynth(const FObjectInitializer& OI);
	void SetParams(const FDustlineSoundParams& P);
protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
private:
	FCriticalSection Lock;
	FDustlineSoundParams Params;
	FDustlineSoundGen Gen;
};
