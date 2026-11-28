#pragma once
#include "CoreMinimal.h"
#include "DustlineSettings.generated.h"

/** Ajustes propios del jugador (se guardan en Saved/Config/dustline_settings.json). Los gráficos usan además UGameUserSettings. */
USTRUCT()
struct FDustlineSettings
{
	GENERATED_BODY()
	UPROPERTY() int32 Units = 0;            // 0 km/h, 1 mph
	UPROPERTY() float Fov = 75.f;           // 60..110
	UPROPERTY() bool bHUD = true;
	UPROPERTY() bool bHints = true;
	UPROPERTY() float RenderScale = 100.f;  // % (50..100)
	UPROPERTY() bool bManual = false;      // cambio manual (Q/E)
	UPROPERTY() bool bAbs = true;
	UPROPERTY() bool bTc = true;           // control de tracción
	UPROPERTY() float Volume = 80.f;       // % volumen general

	static FDustlineSettings& Get();
	void Load();
	void Save() const;
	/** Aplica lo que no depende del coche (escala de render). */
	void ApplyGlobal() const;
};

namespace DustlineGfx
{
	void ReapplyCritical(UWorld* World);
	void SetQuality(UWorld* World, int32 Level);   // 0..3
	int32 GetQuality();
	void SetFullscreen(UWorld* World, bool bFull);
	bool IsFullscreen();
	void SetVSync(UWorld* World, bool bOn);
	bool GetVSync();
	void SetFpsLimit(UWorld* World, int32 Limit);  // 0 = sin límite
	int32 GetFpsLimit();
}
