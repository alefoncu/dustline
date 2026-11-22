#include "DustlineTerrainLibrary.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDustlineTerrain, Log, All);

namespace
{
	TArray<float> GHeightCm; int32 GSize = 0; float GXYScale = 100.f; FVector GOrigin = FVector::ZeroVector;
	FORCEINLINE float Hash2(int32 X, int32 Y, uint32 Seed)
	{
		uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u + Seed * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H = H ^ (H >> 16);
		return (float)(H & 0xFFFFFF) / (float)0x1000000;
	}
	float VNoise(float X, float Y, uint32 Seed)
	{
		const float Xi = FMath::FloorToFloat(X), Yi = FMath::FloorToFloat(Y);
		const float Fx = X - Xi, Fy = Y - Yi;
		const int32 Ix = (int32)Xi, Iy = (int32)Yi;
		const float U = Fx * Fx * Fx * (Fx * (Fx * 6 - 15) + 10);
		const float V = Fy * Fy * Fy * (Fy * (Fy * 6 - 15) + 10);
		const float A = Hash2(Ix, Iy, Seed), B = Hash2(Ix + 1, Iy, Seed), C = Hash2(Ix, Iy + 1, Seed), D = Hash2(Ix + 1, Iy + 1, Seed);
		return A + (B - A) * U + (C - A) * V + (A - B - C + D) * U * V;
	}
	float Fbm(float X, float Y, int32 Oct, uint32 Seed)
	{
		float Amp = 0.5f, Freq = 1.f, Sum = 0.f, Norm = 0.f;
		for (int32 i = 0; i < Oct; ++i)
		{
			Sum += Amp * VNoise(X * Freq, Y * Freq, Seed + i * 7919u);
			Norm += Amp; Amp *= 0.5f; Freq *= 2.03f;
		}
		return Sum / Norm;
	}
	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3 - 2 * T);
	}

	// Particle-based hydraulic erosion on a heightfield normalised to roughly 0..1.
	void Erode(TArray<float>& H, int32 N, int32 Droplets, uint32 Seed)
	{
		FRandomStream Rng(Seed);
		const float Inertia = 0.05f, Capacity = 4.f, MinSlope = 0.01f, Deposit = 0.3f, Erosion = 0.3f, Evap = 0.01f, Grav = 4.f;
		const int32 MaxLife = 40, R = 3;
		TArray<float> Brush; TArray<int32> BrushOff; TArray<FIntPoint> BrushPts;
		float WSum = 0;
		for (int32 y = -R; y <= R; ++y) for (int32 x = -R; x <= R; ++x)
		{
			const float D = FMath::Sqrt((float)(x * x + y * y));
			if (D < R) { const float W = R - D; BrushPts.Add({x, y}); Brush.Add(W); WSum += W; }
		}
		for (float& W : Brush) W /= WSum;

		auto HeightGrad = [&](float PX, float PY, float& OutH, float& GX, float& GY)
		{
			const int32 X = (int32)PX, Y = (int32)PY;
			const float U = PX - X, V = PY - Y;
			const int32 I = Y * N + X;
			const float A = H[I], B = H[I + 1], C = H[I + N], D = H[I + N + 1];
			GX = (B - A) * (1 - V) + (D - C) * V;
			GY = (C - A) * (1 - U) + (D - B) * U;
			OutH = A * (1 - U) * (1 - V) + B * U * (1 - V) + C * (1 - U) * V + D * U * V;
		};

		for (int32 Drop = 0; Drop < Droplets; ++Drop)
		{
			float PX = Rng.FRandRange(2, N - 3), PY = Rng.FRandRange(2, N - 3);
			float DX = 0, DY = 0, Speed = 1, Water = 1, Sed = 0;
			for (int32 Life = 0; Life < MaxLife; ++Life)
			{
				const int32 NX = (int32)PX, NY = (int32)PY;
				if (NX < R + 1 || NX >= N - R - 2 || NY < R + 1 || NY >= N - R - 2) break;
				float Hh, GX, GY; HeightGrad(PX, PY, Hh, GX, GY);
				DX = DX * Inertia - GX * (1 - Inertia);
				DY = DY * Inertia - GY * (1 - Inertia);
				const float Len = FMath::Sqrt(DX * DX + DY * DY);
				if (Len < 1e-6f) break;
				DX /= Len; DY /= Len;
				PX += DX; PY += DY;
				if ((int32)PX < 1 || (int32)PX >= N - 2 || (int32)PY < 1 || (int32)PY >= N - 2) break;
				float NewH, G2X, G2Y; HeightGrad(PX, PY, NewH, G2X, G2Y);
				const float DH = NewH - Hh;
				const float Cap = FMath::Max(-DH * Speed * Water * Capacity, MinSlope);
				if (Sed > Cap || DH > 0)
				{
					const float Amt = (DH > 0) ? FMath::Min(DH, Sed) : (Sed - Cap) * Deposit;
					Sed -= Amt;
					const int32 I = NY * N + NX; const float U = PX - NX - 0 , V = PY - NY;
					(void)U; (void)V;
					H[I] += Amt * 0.25f; H[I + 1] += Amt * 0.25f; H[I + N] += Amt * 0.25f; H[I + N + 1] += Amt * 0.25f;
				}
				else
				{
					const float Amt = FMath::Min((Cap - Sed) * Erosion, -DH);
					for (int32 k = 0; k < BrushPts.Num(); ++k)
					{
						const int32 X = NX + BrushPts[k].X, Y = NY + BrushPts[k].Y;
						const float W = Amt * Brush[k];
						H[Y * N + X] -= FMath::Min(H[Y * N + X], W);
						Sed += FMath::Min(H[Y * N + X] + W, W) * 0.0f; // sediment accounted below
					}
					Sed += Amt;
				}
				Speed = FMath::Sqrt(FMath::Max(0.f, Speed * Speed - DH * Grav));
				Water *= (1 - Evap);
			}
		}
	}
}

ALandscape* UDustlineTerrainLibrary::CreateProceduralLandscape(UObject* WorldContextObject, int32 Size, int32 Seed, float XYScale, float ZScale,
	float MaxHeightMeters, int32 ErosionDroplets, bool bCenterOnOrigin, float WaterPercentile)
{
#if !WITH_EDITOR
	return nullptr;
#else
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World) { UE_LOG(LogDustlineTerrain, Error, TEXT("No world")); return nullptr; }
	const int32 N = Size;
	const double T0 = FPlatformTime::Seconds();

	TArray<float> H; H.SetNumUninitialized(N * N);
	const uint32 S = (uint32)Seed;
	const float Inv = 1.f / (float)N;
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const float U = X * Inv * 5.f, V = Y * Inv * 5.f;
			// Domain warp
			const float WX = (Fbm(U * 0.9f + 11.f, V * 0.9f - 3.f, 4, S + 101) - 0.5f) * 1.6f;
			const float WY = (Fbm(U * 0.9f - 7.f, V * 0.9f + 19.f, 4, S + 202) - 0.5f) * 1.6f;
			const float Base = Fbm(U + WX, V + WY, 6, S);
			const float Rid = 1.f - FMath::Abs(2.f * Fbm((U + WX) * 1.7f, (V + WY) * 1.7f, 5, S + 303) - 1.f);
			const float Mask = SmoothStep(0.42f, 0.68f, Fbm(U * 0.45f + 5.f, V * 0.45f - 9.f, 3, S + 404));
			float Hh = 0.62f * Base + 0.38f * Rid * Rid * (0.15f + 0.85f * Mask);
			// Edge falloff so the map is bounded by mountains rather than cliffs
			const float Ex = FMath::Abs(X * Inv - 0.5f) * 2.f, Ey = FMath::Abs(Y * Inv - 0.5f) * 2.f;
			const float Edge = SmoothStep(0.80f, 1.0f, FMath::Max(Ex, Ey));
			Hh = FMath::Lerp(Hh, FMath::Max(Hh, 0.85f), Edge);
			H[Y * N + X] = FMath::Pow(Hh, 1.25f);
		}
	}
	UE_LOG(LogDustlineTerrain, Display, TEXT("Base noise done in %.1fs"), FPlatformTime::Seconds() - T0);

	{ // two 3x3 blur passes to remove needle-like spikes before erosion
		TArray<float> Tmp; Tmp.SetNumUninitialized(N * N);
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 Y = 0; Y < N; ++Y) for (int32 X = 0; X < N; ++X)
			{
				float Sum = 0; int32 C = 0;
				for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx)
				{
					const int32 XX = FMath::Clamp(X + dx, 0, N - 1), YY = FMath::Clamp(Y + dy, 0, N - 1);
					Sum += H[YY * N + XX]; ++C;
				}
				Tmp[Y * N + X] = Sum / C;
			}
			H = Tmp;
		}
	}
	if (ErosionDroplets > 0) { Erode(H, N, ErosionDroplets, S + 9); }
	UE_LOG(LogDustlineTerrain, Display, TEXT("Erosion done in %.1fs"), FPlatformTime::Seconds() - T0);

	// Normalise and convert to metres, with the lowest basins slightly under water level (z=0)
	float MinH = 1e9f, MaxH = -1e9f;
	for (float V : H) { MinH = FMath::Min(MinH, V); MaxH = FMath::Max(MaxH, V); }
	const float Range = FMath::Max(MaxH - MinH, 1e-4f);
	float WaterOffsetM = 0.04f * MaxHeightMeters;
	if (WaterPercentile > 0.f)
	{
		TArray<float> Sorted = H; Sorted.Sort();
		const float P = Sorted[FMath::Clamp((int32)(WaterPercentile * (Sorted.Num() - 1)), 0, Sorted.Num() - 1)];
		WaterOffsetM = ((P - MinH) / Range) * MaxHeightMeters;
	}
	// 16-bit height: world cm = (value - 32768) * ZScale / 128
	const float CmPerUnit = ZScale / 128.f;
	TArray<uint16> Data; Data.SetNumUninitialized(N * N);
	GHeightCm.SetNumUninitialized(N * N); GSize = N; GXYScale = XYScale;
	for (int32 i = 0; i < N * N; ++i)
	{
		const float Meters = ((H[i] - MinH) / Range) * MaxHeightMeters - WaterOffsetM;
		const uint16 V16 = (uint16)FMath::Clamp(FMath::RoundToInt(32768.f + Meters * 100.f / CmPerUnit), 0, 65535);
		Data[i] = V16;
		GHeightCm[i] = ((float)V16 - 32768.f) * CmPerUnit;
	}

	// Landscape actor
	const float Half = (N - 1) * XYScale * 0.5f;
	const FVector Loc = bCenterOnOrigin ? FVector(-Half, -Half, 0) : FVector::ZeroVector;
	GOrigin = Loc;
	ALandscape* L = World->SpawnActor<ALandscape>(ALandscape::StaticClass(), FTransform(FRotator::ZeroRotator, Loc, FVector(XYScale, XYScale, ZScale)));
	if (!L) { UE_LOG(LogDustlineTerrain, Error, TEXT("Spawn failed")); return nullptr; }
	const FGuid Guid = FGuid::NewGuid();
	L->SetLandscapeGuid(Guid);

	TMap<FGuid, TArray<uint16>> HeightData; HeightData.Add(FGuid(), MoveTemp(Data));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> Layers; Layers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
	L->Import(Guid, 0, 0, N - 1, N - 1, 2, 63, HeightData, nullptr, Layers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	L->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((N * N) / (2048 * 2048) + 1), (uint32)2);
	L->SetActorLocation(Loc);
	L->SetActorScale3D(FVector(XYScale, XYScale, ZScale));
	L->CreateLandscapeInfo();
	UE_LOG(LogDustlineTerrain, Display, TEXT("Landscape %dx%d created in %.1fs, %.0f m wide"), N, N, FPlatformTime::Seconds() - T0, Half * 2 / 100.f);
	return L;
#endif
}

float UDustlineTerrainLibrary::GetTerrainHeight(UObject* WorldContextObject, float X, float Y)
{
	if (GSize > 0)
	{
		const float Fx = (X - GOrigin.X) / GXYScale, Fy = (Y - GOrigin.Y) / GXYScale;
		if (Fx < 0 || Fy < 0 || Fx >= GSize - 1 || Fy >= GSize - 1) return -1e9f;
		const int32 Ix = (int32)Fx, Iy = (int32)Fy; const float U = Fx - Ix, V = Fy - Iy;
		const float A = GHeightCm[Iy * GSize + Ix], B = GHeightCm[Iy * GSize + Ix + 1], C = GHeightCm[(Iy + 1) * GSize + Ix], D = GHeightCm[(Iy + 1) * GSize + Ix + 1];
		return A * (1 - U) * (1 - V) + B * U * (1 - V) + C * (1 - U) * V + D * U * V;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return -1e9f;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		const TOptional<float> Z = It->GetHeightAtLocation(FVector(X, Y, 0));
		if (Z.IsSet()) return Z.GetValue();
	}
	return -1e9f;
}

FVector UDustlineTerrainLibrary::FindSpawnLocation(UObject* WorldContextObject)
{
	float Best = 1e18f; bool bFound = false; FVector OutLocation(0, 0, -1e9f);
	for (float R = 0; R < 150000.f && !bFound; R += 5000.f)
	{
		for (int32 A = 0; A < 24; ++A)
		{
			const float Ang = A * 2 * PI / 24.f;
			const float X = FMath::Cos(Ang) * R, Y = FMath::Sin(Ang) * R;
			const float Z = GetTerrainHeight(WorldContextObject, X, Y);
			if (Z < 500.f || Z > 6000.f) continue; // above water, below high alpine
			const float Zx = GetTerrainHeight(WorldContextObject, X + 600, Y), Zy = GetTerrainHeight(WorldContextObject, X, Y + 600);
			const float Slope = FMath::Max(FMath::Abs(Zx - Z), FMath::Abs(Zy - Z)) / 600.f;
			if (Slope < Best && Slope < 0.12f) { Best = Slope; OutLocation = FVector(X, Y, Z + 200.f); bFound = true; }
		}
	}
	return OutLocation;
}

AActor* UDustlineTerrainLibrary::ScatterInstances(UObject* WorldContextObject, UStaticMesh* Mesh, int32 Count, int32 Seed, float MinScale, float MaxScale,
	float MinZCm, float MaxZCm, float MaxSlope, float NoiseScale, float NoiseThreshold, bool bAlignToNormal, float ZOffsetCm,
	float CullEndCm, bool bCollision, FRotator PreRotation, const FString& Label)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || !Mesh || GSize <= 0) { UE_LOG(LogDustlineTerrain, Error, TEXT("Scatter: missing world/mesh/terrain")); return nullptr; }
	FRandomStream Rng(Seed);
	const float Half = (GSize - 1) * GXYScale * 0.5f;
	const float Margin = 3000.f;
	TArray<FTransform> Xf; Xf.Reserve(Count);
	int32 Tries = 0;
	while (Xf.Num() < Count && Tries < Count * 40)
	{
		++Tries;
		const float X = GOrigin.X + Rng.FRandRange(Margin, 2 * Half - Margin);
		const float Y = GOrigin.Y + Rng.FRandRange(Margin, 2 * Half - Margin);
		if (X * X + Y * Y < 2500.f * 2500.f) continue; // clear area around the origin
		const float Z = GetTerrainHeight(WorldContextObject, X, Y);
		if (Z < MinZCm || Z > MaxZCm) continue;
		const float Zx = GetTerrainHeight(WorldContextObject, X + 200.f, Y), Zy = GetTerrainHeight(WorldContextObject, X, Y + 200.f);
		FVector N = FVector(-(Zx - Z) / 200.f, -(Zy - Z) / 200.f, 1.f).GetSafeNormal();
		if (N.Z < 1.f - MaxSlope) continue;
		if (NoiseScale > 0.f)
		{
			const float F = Fbm(X * NoiseScale, Y * NoiseScale, 3, (uint32)Seed + 77u);
			if (F < NoiseThreshold || Rng.FRand() > (F - NoiseThreshold) * 3.f + 0.15f) continue;
		}
		const float Sc = Rng.FRandRange(MinScale, MaxScale);
		FQuat Q = FQuat(FVector::UpVector, Rng.FRandRange(0.f, 2 * PI)) * PreRotation.Quaternion();
		if (bAlignToNormal) Q = FQuat::FindBetweenNormals(FVector::UpVector, N) * Q;
		Xf.Add(FTransform(Q, FVector(X, Y, Z + ZOffsetCm), FVector(Sc)));
	}
	AActor* A = World->SpawnActor<AActor>();
	if (!A) return nullptr;
#if WITH_EDITOR
	A->SetActorLabel(Label);
#endif
	UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(A, TEXT("Instances"));
	C->SetStaticMesh(Mesh);
	C->CreationMethod = EComponentCreationMethod::Instance;
	A->SetRootComponent(C);
	C->RegisterComponent();
	A->AddInstanceComponent(C);
	C->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (CullEndCm > 0.f) { C->SetCullDistances(0, (int32)CullEndCm); }
	C->bAffectDistanceFieldLighting = false;
	C->AddInstances(Xf, false, true);
	C->BuildTreeIfOutdated(true, true);
	UE_LOG(LogDustlineTerrain, Display, TEXT("Scatter %s: %d/%d instances (%d tries)"), *Label, Xf.Num(), Count, Tries);
	return A;
}

UStaticMesh* UDustlineTerrainLibrary::CreateProceduralConifer(const FString& PackagePath, const FString& AssetName, int32 Seed, float HeightCm, float TrunkRadiusCm,
	int32 Whorls, int32 CardsPerWhorl, float CrownWidthCm, UMaterialInterface* BarkMaterial, UMaterialInterface* FoliageMaterial)
{
#if WITH_EDITOR
	auto BuildLOD = [&](FMeshDescription& MD, int32 LW, int32 LC, float WMul, float LMul)
	{
	FRandomStream Rng(Seed);
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tan = Attr.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> Sign = Attr.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> UV = Attr.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector4f> Col = Attr.GetVertexInstanceColors();
	TPolygonGroupAttributesRef<FName> Slots = Attr.GetPolygonGroupMaterialSlotNames();
	UV.SetNumChannels(1);
	const FPolygonGroupID GBark = MD.CreatePolygonGroup(); Slots[GBark] = FName("Bark");
	const FPolygonGroupID GFol = MD.CreatePolygonGroup(); Slots[GFol] = FName("Foliage");

	auto AddTri = [&](FPolygonGroupID G, const FVector3f P[3], const FVector3f N[3], const FVector2f T[3], const FVector3f& Tangent)
	{
		FVertexInstanceID VI[3];
		for (int32 i = 0; i < 3; ++i)
		{
			const FVertexID V = MD.CreateVertex(); Pos[V] = P[i];
			VI[i] = MD.CreateVertexInstance(V);
			Nrm[VI[i]] = N[i]; Tan[VI[i]] = Tangent; Sign[VI[i]] = 1.f; UV.Set(VI[i], 0, T[i]); Col[VI[i]] = FVector4f(1, 1, 1, 1);
		}
		MD.CreatePolygon(G, TArray<FVertexInstanceID>{ VI[0], VI[1], VI[2] });
	};
	auto AddQuad = [&](FPolygonGroupID G, const FVector3f& A, const FVector3f& B, const FVector3f& C, const FVector3f& D, const FVector3f& N, const FVector3f& Tg, bool bFlip)
	{
		// A-B-C-D around the quad; UVs: A(0,1) B(1,1) C(1,0) D(0,0)
		FVector2f ua(0, 1), ub(1, 1), uc(1, 0), ud(0, 0);
		if (bFlip) { ua.X = 1 - ua.X; ub.X = 1 - ub.X; uc.X = 1 - uc.X; ud.X = 1 - ud.X; }
		const FVector3f P1[3] = { A, B, C }, P2[3] = { A, C, D };
		const FVector3f N3[3] = { N, N, N };
		const FVector2f T1[3] = { ua, ub, uc }, T2[3] = { ua, uc, ud };
		AddTri(G, P1, N3, T1, Tg); AddTri(G, P2, N3, T2, Tg);
	};

	// ---- trunk: tapered 8-sided prism ----
	const int32 Seg = 8, Rings = 7;
	auto TrunkR = [&](float Z) { return TrunkRadiusCm * FMath::Pow(FMath::Clamp(1.f - Z / HeightCm, 0.f, 1.f), 0.85f) + 1.2f; };
	for (int32 r = 0; r < Rings; ++r)
	{
		const float Z0 = HeightCm * r / (float)Rings, Z1 = HeightCm * (r + 1) / (float)Rings;
		for (int32 s = 0; s < Seg; ++s)
		{
			const float A0 = 2 * PI * s / Seg, A1 = 2 * PI * (s + 1) / Seg;
			const FVector3f P00(FMath::Cos(A0) * TrunkR(Z0), FMath::Sin(A0) * TrunkR(Z0), Z0), P10(FMath::Cos(A1) * TrunkR(Z0), FMath::Sin(A1) * TrunkR(Z0), Z0);
			const FVector3f P01(FMath::Cos(A0) * TrunkR(Z1), FMath::Sin(A0) * TrunkR(Z1), Z1), P11(FMath::Cos(A1) * TrunkR(Z1), FMath::Sin(A1) * TrunkR(Z1), Z1);
			const float Am = 0.5f * (A0 + A1);
			const FVector3f N(FMath::Cos(Am), FMath::Sin(Am), 0.f);
			const FVector3f Tg(-FMath::Sin(Am), FMath::Cos(Am), 0.f);
			const float U0 = (float)s / Seg * 2.f, U1 = (float)(s + 1) / Seg * 2.f, V0 = Z0 / HeightCm * 4.f, V1 = Z1 / HeightCm * 4.f;
			const FVector3f Pa[3] = { P00, P01, P11 }, Pb[3] = { P00, P11, P10 };
			const FVector3f Nn[3] = { N, N, N };
			const FVector2f Ta[3] = { FVector2f(U0, V0), FVector2f(U0, V1), FVector2f(U1, V1) }, Tb[3] = { FVector2f(U0, V0), FVector2f(U1, V1), FVector2f(U1, V0) };
			AddTri(GBark, Pa, Nn, Ta, Tg); AddTri(GBark, Pb, Nn, Tb, Tg);
		}
	}

	// ---- foliage: whorls of drooping needle-spray cards ----
	for (int32 w = 0; w < LW; ++w)
	{
		const float T = (float)w / FMath::Max(1, LW - 1);
		const float Z = HeightCm * (0.14f + 0.84f * T);
		const float Len = LMul * CrownWidthCm * 0.5f * (FMath::Pow(1.f - T, 0.9f) * 0.92f + 0.10f) * Rng.FRandRange(0.88f, 1.12f);
		const float Phase = Rng.FRandRange(0.f, 2 * PI);
		const int32 Cards = FMath::Max(3, LC - (int32)(T * 2.f));
		for (int32 k = 0; k < Cards; ++k)
		{
			const float Th = Phase + 2 * PI * (k + Rng.FRandRange(-0.18f, 0.18f)) / Cards;
			const FVector3f D(FMath::Cos(Th), FMath::Sin(Th), 0.f);
			const FVector3f Side(-D.Y, D.X, 0.f);
			const float Droop = Len * FMath::Lerp(0.34f, 0.10f, T) + Rng.FRandRange(-0.04f, 0.04f) * Len;
			const float R0 = TrunkR(Z) * 0.6f;
			const float WBase = Len * 0.26f * WMul, WTip = Len * 0.62f * WMul;
			const FVector3f Nrm0 = FVector3f(D.X, D.Y, 0.62f).GetSafeNormal();
			// horizontal-ish card
			const FVector3f A = D * R0 - Side * WBase + FVector3f(0, 0, Z), B = D * R0 + Side * WBase + FVector3f(0, 0, Z);
			const FVector3f C = D * (R0 + Len) + Side * WTip + FVector3f(0, 0, Z - Droop), Dd = D * (R0 + Len) - Side * WTip + FVector3f(0, 0, Z - Droop);
			AddQuad(GFol, A, B, C, Dd, Nrm0, Side, Rng.FRand() < 0.5f);
			// vertical card for volume (follows the same axis, tilted from above)
			const float HV = Len * 0.34f;
			const FVector3f A2 = D * R0 + FVector3f(0, 0, Z + HV * 0.5f), B2 = D * R0 + FVector3f(0, 0, Z - HV * 0.5f);
			const FVector3f C2 = D * (R0 + Len * 0.92f) + FVector3f(0, 0, Z - Droop - HV * 0.2f), D2 = D * (R0 + Len * 0.92f) + FVector3f(0, 0, Z - Droop + HV * 0.9f);
			AddQuad(GFol, A2, B2, C2, D2, FVector3f(Side.X, Side.Y, 0.3f).GetSafeNormal(), D, Rng.FRand() < 0.5f);
		}
	}
	// leader tip
	for (int32 k = 0; k < 3; ++k)
	{
		const float Th = 2 * PI * k / 3.f + Rng.FRandRange(0.f, 1.f);
		const FVector3f D(FMath::Cos(Th), FMath::Sin(Th), 0.f), Side(-D.Y, D.X, 0.f);
		const float L = CrownWidthCm * 0.07f;
		const FVector3f A = FVector3f(0, 0, HeightCm) - Side * L * 0.25f, B = FVector3f(0, 0, HeightCm) + Side * L * 0.25f;
		const FVector3f C = FVector3f(0, 0, HeightCm + L * 2.2f) + Side * L * 0.05f, Dd = FVector3f(0, 0, HeightCm + L * 2.2f) - Side * L * 0.05f;
		AddQuad(GFol, A, B, C, Dd, D, Side, false);
	}

	};
	FMeshDescription MD0, MD1, MD2;
	BuildLOD(MD0, Whorls + 4, CardsPerWhorl + 3, 1.0f, 1.0f);
	BuildLOD(MD1, Whorls, CardsPerWhorl, 1.2f, 1.0f);
	BuildLOD(MD2, FMath::Max(5, Whorls / 2), 5, 1.9f, 1.08f);
	const FString PackageName = PackagePath + TEXT("/") + AssetName;
	UPackage* Pkg = CreatePackage(*PackageName);
	UStaticMesh* SM = NewObject<UStaticMesh>(Pkg, *AssetName, RF_Public | RF_Standalone);
	SM->GetStaticMaterials().Add(FStaticMaterial(BarkMaterial, FName("Bark")));
	SM->GetStaticMaterials().Add(FStaticMaterial(FoliageMaterial, FName("Foliage")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = true; Params.bBuildSimpleCollision = false; Params.bFastBuild = true;
	TArray<const FMeshDescription*> MDs; MDs.Add(&MD0); MDs.Add(&MD1); MDs.Add(&MD2);
	SM->SetAutoComputeLODScreenSize(false);
	SM->BuildFromMeshDescriptions(MDs, Params);
	{ const float Sz[3] = { 1.0f, 0.13f, 0.035f }; for (int32 l = 0; l < SM->GetNumSourceModels() && l < 3; ++l) SM->GetSourceModel(l).ScreenSize.Default = Sz[l]; }
	// trunk capsule collision (cheap; lets vehicles hit the trunk)
	if (!SM->GetBodySetup()) SM->CreateBodySetup();
	if (UBodySetup* BS = SM->GetBodySetup())
	{
		FKSphylElem E; E.Radius = FMath::Max(8.f, TrunkRadiusCm * 0.8f); E.Length = FMath::Max(10.f, HeightCm * 0.55f);
		E.Center = FVector(0, 0, HeightCm * 0.32f);
		BS->AggGeom.SphylElems.Add(E);
		BS->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		BS->InvalidatePhysicsData(); BS->CreatePhysicsMeshes();
	}
	FAssetRegistryModule::AssetCreated(SM);
	SM->MarkPackageDirty();
	return SM;
#else
	return nullptr;
#endif
}
