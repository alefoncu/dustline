#include "DustlineGameMode.h"
#include "DustlineCar.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

ADustlineGameMode::ADustlineGameMode()
{
	DefaultPawnClass = ADustlineCar::StaticClass();
}

void ADustlineGameMode::BeginPlay()
{
	Super::BeginPlay();
	UWorld* W = GetWorld(); if (!W) return;
	// Árboles/rocas: asegura que el árbol de instancias está construido (si no, la colisión existe pero el render tarda en aparecer)
	for (TObjectIterator<UHierarchicalInstancedStaticMeshComponent> It; It; ++It)
		if (It->GetWorld() == W) It->BuildTreeIfOutdated(true, true);
	// Corrección de color / acabado de imagen suave (sin tocar la exposición automática)
	APostProcessVolume* V = W->SpawnActor<APostProcessVolume>();
	if (V)
	{
		V->bUnbound = true; V->BlendWeight = 1.f;
		FPostProcessSettings& S = V->Settings;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.35f;
		S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.15f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.35f;
		S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = 0.6f;
		S.bOverride_AmbientOcclusionRadius = true; S.AmbientOcclusionRadius = 120.f;
		S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(0.93, 0.93, 0.95, 1.0);
		S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.06, 1.06, 1.06, 1.0);
		S.bOverride_FilmToe = true; S.FilmToe = 0.55f;
		S.bOverride_SceneColorTint = true; S.SceneColorTint = FLinearColor(1.0f, 0.985f, 0.96f);
	}
}
