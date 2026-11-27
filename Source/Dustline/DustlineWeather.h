#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DustlineWorld.h"
#include "DustlineWeather.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class UHierarchicalInstancedStaticMeshComponent;

/** Clima del mapa: mueve la simulación por zonas, la pinta en el terreno (humedad, charcos, nieve, hielo), ajusta sol/cielo/niebla
 *  según el tiempo que hace donde está la cámara y dibuja la lluvia o la nieve que cae alrededor. No es una opción: cambia solo. */
UCLASS()
class DUSTLINE_API ADustlineWeather : public AActor
{
	GENERATED_BODY()
public:
	ADustlineWeather();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	DustlineWorld::FLocal Here;   // clima suavizado en la cámara (para el HUD)
private:
	UPROPERTY() UTexture2D* StateTex = nullptr;
	UPROPERTY() TArray<UProceduralMeshComponent*> PrecipLayers;   // 0..2 lluvia, 3..5 nieve
	UPROPERTY() TArray<UMaterialInstanceDynamic*> LayerMats;
	TWeakObjectPtr<class UDirectionalLightComponent> Sun; TWeakObjectPtr<class USkyLightComponent> Sky; TWeakObjectPtr<class UExponentialHeightFogComponent> Fog;
	TArray<TWeakObjectPtr<class ALandscapeProxy>> Lands;
	float SunBase = 4.f, SkyBase = 1.f, FogBase = 0.004f; FLinearColor FogColBase = FLinearColor(0.45f, 0.55f, 0.7f);
	float TexT = 0.f, SimT = 0.f, RainVis = 0.f, SnowVis = 0.f, CloudVis = 0.f;
	TArray<uint8> Pixels;
	void MakeLayers();
	void PushTexture();
};

/** Hierba alta alrededor de la cámara: baldosas de 24 m que se rellenan según la máscara horneada (misma que usa la física). */
UCLASS()
class DUSTLINE_API ADustlineGrass : public AActor
{
	GENERATED_BODY()
public:
	ADustlineGrass();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
private:
	UPROPERTY() TArray<class UStaticMesh*> Meshes;
	UPROPERTY() TMap<FIntPoint, UHierarchicalInstancedStaticMeshComponent*> Tiles;
	TSet<FIntPoint> Empty;
	void BuildTile(const FIntPoint& T);
};
