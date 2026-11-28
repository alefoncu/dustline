#pragma once
#include "CoreMinimal.h"

/**
 * Datos compartidos del mundo: superficies, agua y clima.
 *  - La máscara de superficies (tierra, barro, hierba alta, grava) y el agua (corrientes, profundidad) se hornean en el editor
 *    junto con el terreno (Content/Data/world.bin) y la MISMA máscara la usa el material del terreno (T_SurfaceMask),
 *    así lo que se ve y lo que se siente al conducir coincide.
 *  - El clima es una simulación por zonas: frentes de lluvia/nieve que cruzan el mapa con el viento, temperatura por altitud
 *    y latitud, y el suelo que se moja, se seca, se cubre de nieve o se hiela celda a celda (rejilla de 64x64, ~63 m).
 */
namespace DustlineWorld
{
	enum ESurf : uint8 { SGrass, STallGrass, SDirt, SGravel, SSand, SMud, SRock, SSnow, SIce, SNum };
	const TCHAR* SurfName(int32 K);

	// Mapa: Landscape de 2017x2017 vértices a 200 cm, centrado en el origen
	constexpr float HalfCm = 201600.f;
	constexpr float SizeCm = 2.f * HalfCm;

	/** Propiedades físicas mezcladas en un punto del suelo (para un neumático concreto). */
	struct FGround
	{
		float Mu = 0.8f;         // coeficiente de rozamiento de pico
		float PeakSlip = 0.12f;  // deslizamiento longitudinal del pico
		float PeakAlpha = 0.13f; // ángulo de deriva del pico (rad)
		float Falloff = 0.8f;    // fracción de agarre que queda muy pasado el pico (1 = suelo suelto que "sigue empujando")
		float Crr = 0.02f;       // resistencia a la rodadura
		float Sink = 0.f;        // hundimiento objetivo (m) con carga nominal
		float Dig = 0.f;         // cuánto excava el patinaje (m por m patinado)
		float Bump = 0.f;        // amplitud de irregularidades (m)
		float Drag = 0.f;        // arrastre extra (hierba alta, nieve profunda), N por (N de carga · m/s)
		float W[SNum] = {};      // pesos de cada superficie
		int32 Dominant = SGrass;
		float Wet = 0.f, SnowM = 0.f, TempC = 10.f;
	};

	/** Mezcla de superficies en un punto (pesos) + estado del clima. Tire = tipo de neumático (0 carretera,1 AT,2 barro,3 clavos,4 balón). */
	FGround GroundAt(float Xcm, float Ycm, float Zcm, float Nz, int32 Tire);
	/** Suelo bajo el agua: cantos con verdín si hay corriente, limo blando si está quieta; siempre empapado. */
	void Submerge(FGround& G, float DepthM, float FlowMs, int32 Tire);

	// ---- datos horneados ----
	bool LoadData();
	bool HasData();
	struct FMask { float Dirt = 0, Mud = 0, TallGrass = 0, Gravel = 0; };
	FMask SampleMask(float Xcm, float Ycm);
	/** Agua horneada (ríos, lagos y mar): nivel de la superficie (cm) y corriente (m/s). false = no hay agua aquí. */
	bool WaterAt(float Xcm, float Ycm, float& OutLevelCm, FVector2D& OutFlow);
	// ---- puntos de interés y mapa (los genera el horneado del mundo en Content/Data) ----
	struct FPoi { FString Name; int32 Kind = 0; FVector P = FVector::ZeroVector; bool bFound = false; };
	TArray<FPoi>& Pois();                 // se cargan la primera vez (pois.txt) con lo ya descubierto (Saved)
	void SaveDiscovered();
	const TCHAR* PoiKindName(int32 Kind);
	class UTexture2D* MapTexture();       // map.png (relieve, agua y pistas), cargado una vez
	float MapHalfCm();                    // mitad del lado del mapa en cm (el mapa cubre [-Half, Half])

	/** Solo pruebas: una charca circular de nivel y corriente fijos. */
	void SetTestPool(float Xcm, float Ycm, float RadiusCm, float DepthCm, FVector2D Flow, TFunctionRef<float(float, float)> Ground);

	// ---- clima ----
	struct FLocal
	{
		float Wet = 0.f;       // humedad del suelo 0..1
		float SnowM = 0.f;     // nieve en el suelo (m)
		float Ice = 0.f;       // hielo 0..1
		float TempC = 12.f;
		float Rain = 0.f;      // precipitación líquida 0..1
		float Snowfall = 0.f;  // nevada 0..1
		float Cloud = 0.f;     // nubosidad 0..1
		FVector2D Wind = FVector2D(4, 1); // m/s
	};
	FLocal WeatherAt(float Xcm, float Ycm);
	const TCHAR* WeatherName(const FLocal& L);

	/** Simulación del clima (la mueve ADustlineWeather). Si no hay simulación activa, WeatherAt devuelve un día seco. */
	struct FSim
	{
		static constexpr int32 G = 64;
		float Alt[G * G] = {}, Wet[G * G] = {}, Snow[G * G] = {}, Ice[G * G] = {}, Temp[G * G] = {}, Precip[G * G] = {}, Cloud[G * G] = {};
		struct FStorm { FVector2D P; float R, Peak, Age, Life; };
		TArray<FStorm> Storms;
		FVector2D Wind = FVector2D(5.f, 1.2f);
		double Time = 0.0; float Accum = 0.f; uint32 Seed = 1;
		bool bActive = false; bool bForced = false; float ForceRain = 0.f, ForceSnow = 0.f;
		FVector2D Focus = FVector2D::ZeroVector;   // posición del jugador (los frentes tienden a pasar cerca)
		void Init(TFunctionRef<float(float, float)> HeightCm, uint32 InSeed);
		void Step(float Dt);
		float BaseTemp() const;
		void Fill(uint8* BGRA) const;  // textura 64x64 para el material: R humedad, G nieve, B hielo, A precipitación
	private:
		void SpawnStorm(bool bAnywhere);
		void Cells(float Dt);
	};
	FSim& Sim();
}
