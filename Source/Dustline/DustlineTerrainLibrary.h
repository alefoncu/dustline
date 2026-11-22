#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DustlineTerrainLibrary.generated.h"

class ALandscape;
class UStaticMesh;
class UMaterialInterface;

/** Procedural realistic terrain (fBm + ridges + domain warp + hydraulic erosion) imported as a native Landscape. */
UCLASS()
class DUSTLINE_API UDustlineTerrainLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Size must be a valid landscape size (505, 1009, 2017, 4033). Scale.X/Y in cm per vertex. */
	UFUNCTION(BlueprintCallable, Category = "Dustline|Terrain", meta = (WorldContext = "WorldContextObject"))
	static ALandscape* CreateProceduralLandscape(UObject* WorldContextObject, int32 Size, int32 Seed, float XYScale, float ZScale,
		float MaxHeightMeters, int32 ErosionDroplets, bool bCenterOnOrigin, float WaterPercentile);

	/** Scatter instances of a mesh over the terrain (height band, slope, optional forest noise). Returns the actor holding the HISM. */
	UFUNCTION(BlueprintCallable, Category = "Dustline|Terrain", meta = (WorldContext = "WorldContextObject"))
	static AActor* ScatterInstances(UObject* WorldContextObject, UStaticMesh* Mesh, int32 Count, int32 Seed, float MinScale, float MaxScale,
		float MinZCm, float MaxZCm, float MaxSlope, float NoiseScale, float NoiseThreshold, bool bAlignToNormal, float ZOffsetCm,
		float CullEndCm, bool bCollision, FRotator PreRotation, const FString& Label);

	/** Builds a low-poly conifer StaticMesh asset (trunk + whorls of alpha-card needle sprays) with a trunk capsule collision. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "Dustline|Terrain")
	static UStaticMesh* CreateProceduralConifer(const FString& PackagePath, const FString& AssetName, int32 Seed, float HeightCm, float TrunkRadiusCm,
		int32 Whorls, int32 CardsPerWhorl, float CrownWidthCm, UMaterialInterface* BarkMaterial, UMaterialInterface* FoliageMaterial);

	/** Height (cm, world space) at X,Y or -1e9 if outside. */
	UFUNCTION(BlueprintCallable, Category = "Dustline|Terrain", meta = (WorldContext = "WorldContextObject"))
	static float GetTerrainHeight(UObject* WorldContextObject, float X, float Y);

	/** Finds a gentle (low slope) spot, above water, closest to the centre. Returns Z=-1e9 if none found. */
	UFUNCTION(BlueprintCallable, Category = "Dustline|Terrain", meta = (WorldContext = "WorldContextObject"))
	static FVector FindSpawnLocation(UObject* WorldContextObject);
};
