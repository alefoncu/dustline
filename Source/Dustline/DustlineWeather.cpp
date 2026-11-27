#include "DustlineWeather.h"
#include "DustlineSettings.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "LandscapeProxy.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Rendering/Texture2DResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogDustlineWeather, Log, All);
using namespace DustlineWorld;

namespace
{
	bool CameraPos(UWorld* W, FVector& Out)
	{
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->PlayerCameraManager) return false;
		Out = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	struct FLayerDef { float R; float TileW, TileH; float Fall, SpeedU; float Op; };
	const FLayerDef GLayers[6] = {
		{ 350.f, 140.f, 300.f, 900.f, 0.f, 0.38f }, { 800.f, 150.f, 320.f, 900.f, 0.f, 0.32f }, { 1600.f, 170.f, 360.f, 900.f, 0.f, 0.26f },
		{ 350.f, 220.f, 220.f, 120.f, 0.05f, 0.85f }, { 800.f, 240.f, 240.f, 110.f, -0.04f, 0.75f }, { 1600.f, 260.f, 260.f, 100.f, 0.03f, 0.60f },
	};
}

ADustlineWeather::ADustlineWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADustlineWeather::BeginPlay()
{
	Super::BeginPlay();
	UWorld* W = GetWorld();
	{ TActorIterator<ADirectionalLight> It(W); if (It) Sun = It->GetComponent(); }
	{ TActorIterator<ASkyLight> It(W); if (It) Sky = It->GetLightComponent(); }
	{ TActorIterator<AExponentialHeightFog> It(W); if (It) Fog = It->GetComponent(); }
	if (Sun.IsValid()) SunBase = Sun->Intensity;
	if (Sky.IsValid()) SkyBase = Sky->Intensity;
	if (Fog.IsValid()) { FogBase = Fog->FogDensity; FogColBase = Fog->FogInscatteringLuminance; }
	for (TActorIterator<ALandscapeProxy> It(W); It; ++It) Lands.Add(*It);

	FSim& S = Sim();
	const uint32 Seed = (uint32)FDateTime::Now().GetTicks();
	S.Init([W](float X, float Y)
	{
		FHitResult H;
		if (W->LineTraceSingleByChannel(H, FVector(X, Y, 200000.f), FVector(X, Y, -200000.f), ECC_Visibility)) return (float)H.ImpactPoint.Z;
		return -1e9f;
	}, Seed);
	FString Force;
	if (FParse::Value(FCommandLine::Get(), TEXT("dweather="), Force))
	{
		S.bForced = true; S.ForceRain = Force == TEXT("rain") ? 1.f : 0.f; S.ForceSnow = Force == TEXT("snow") ? 1.f : 0.f;
		for (int32 i = 0; i < (S.ForceSnow > 0.f ? 60 : 120); ++i) S.Step(5.f);   // el suelo ya empapado / nevado para las pruebas
	}
	StateTex = UTexture2D::CreateTransient(FSim::G, FSim::G, PF_B8G8R8A8, TEXT("WeatherState"));
	if (StateTex)
	{
		StateTex->SRGB = false; StateTex->Filter = TF_Bilinear; StateTex->AddressX = TA_Clamp; StateTex->AddressY = TA_Clamp;
		StateTex->UpdateResource();
		Pixels.SetNumZeroed(FSim::G * FSim::G * 4);
		PushTexture();
		for (auto& L : Lands) if (L.IsValid()) L->SetLandscapeMaterialTextureParameterValue(TEXT("WeatherTex"), StateTex);
	}
	MakeLayers();
	UE_LOG(LogDustlineWeather, Display, TEXT("WEATHER start sun=%.2f sky=%.2f fog=%.4f lands=%d forced=%s"), SunBase, SkyBase, FogBase, Lands.Num(), *Force);
}

void ADustlineWeather::PushTexture()
{
	if (!StateTex) return;
	Sim().Fill(Pixels.GetData());
	uint8* Copy = (uint8*)FMemory::Malloc(Pixels.Num()); FMemory::Memcpy(Copy, Pixels.GetData(), Pixels.Num());
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, FSim::G, FSim::G);
	StateTex->UpdateTextureRegions(0, 1, Region, FSim::G * 4, 4, Copy, [](uint8* Data, const FUpdateTextureRegion2D* R) { FMemory::Free(Data); delete R; });
}

void ADustlineWeather::MakeLayers()
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Weather/M_Precip.M_Precip"));
	UTexture* Rain = LoadObject<UTexture>(nullptr, TEXT("/Game/Weather/T_Rain.T_Rain"));
	UTexture* Snow = LoadObject<UTexture>(nullptr, TEXT("/Game/Weather/T_Snow.T_Snow"));
	if (!Base || !Rain || !Snow) { UE_LOG(LogDustlineWeather, Warning, TEXT("Faltan M_Precip/T_Rain/T_Snow: sin lluvia visible")); return; }
	for (int32 l = 0; l < 6; ++l)
	{
		const FLayerDef& D = GLayers[l];
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
		P->SetupAttachment(GetRootComponent()); P->RegisterComponent();
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision); P->SetCastShadow(false); P->bUseAsyncCooking = false;
		const int32 Seg = 28; const float HH = 1300.f;
		TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C; TArray<FProcMeshTangent> Tg;
		const float TilesU = FMath::Max(1.f, FMath::RoundToFloat(2 * PI * D.R / D.TileW)), TilesV = 2 * HH / D.TileH;
		for (int32 s = 0; s <= Seg; ++s)
		{
			const float A = 2 * PI * s / Seg;
			for (int32 e = 0; e < 2; ++e)
			{
				V.Add(FVector(FMath::Cos(A) * D.R, FMath::Sin(A) * D.R, e == 0 ? -HH : HH));
				N.Add(FVector(-FMath::Cos(A), -FMath::Sin(A), 0)); UV.Add(FVector2D((float)s / Seg * TilesU, e == 0 ? 0.f : TilesV));
				C.Add(FLinearColor::White); Tg.Add(FProcMeshTangent(-FMath::Sin(A), FMath::Cos(A), 0));
			}
		}
		for (int32 s = 0; s < Seg; ++s) { const int32 a = s * 2; T.Append({ a, a + 1, a + 3, a, a + 3, a + 2 }); }
		P->CreateMeshSection_LinearColor(0, V, T, N, UV, C, Tg, false);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetTextureParameterValue(TEXT("Tex"), l < 3 ? Rain : Snow);
		M->SetScalarParameterValue(TEXT("SpeedV"), D.Fall / D.TileH * TilesV / TilesV);
		M->SetScalarParameterValue(TEXT("SpeedU"), D.SpeedU);
		M->SetScalarParameterValue(TEXT("Opacity"), 0.f);
		M->SetVectorParameterValue(TEXT("Tint"), l < 3 ? FLinearColor(0.30f, 0.33f, 0.37f) : FLinearColor(0.9f, 0.92f, 0.95f));
		P->SetMaterial(0, M);
		P->SetVisibility(false);
		PrecipLayers.Add(P); LayerMats.Add(M);
	}
}

void ADustlineWeather::Tick(float Dt)
{
	Super::Tick(Dt);
	FSim& S = Sim();
	FVector Cam = FVector::ZeroVector; CameraPos(GetWorld(), Cam);
	S.Focus = FVector2D(Cam.X, Cam.Y);
	SimT += Dt;
	if (SimT >= 0.25f) { S.Step(SimT); SimT = 0.f; }
	TexT += Dt;
	if (TexT >= 0.5f) { TexT = 0.f; PushTexture(); }

	const FLocal L = WeatherAt(Cam.X, Cam.Y);
	const float k = 1.f - FMath::Exp(-Dt * 0.8f);
	RainVis += (L.Rain - RainVis) * k; SnowVis += (L.Snowfall - SnowVis) * k; CloudVis += (L.Cloud - CloudVis) * k;
	Here = L; Here.Rain = RainVis; Here.Snowfall = SnowVis; Here.Cloud = CloudVis;
	const float Precip = FMath::Clamp(RainVis + SnowVis, 0.f, 1.f);

	if (Sun.IsValid()) Sun->SetIntensity(SunBase * (1.f - 0.78f * CloudVis));
	if (Sky.IsValid()) Sky->SetIntensity(SkyBase * (1.f - 0.30f * CloudVis));
	if (Fog.IsValid())
	{
		Fog->SetFogDensity(FogBase * (1.f + 2.5f * CloudVis + 9.f * RainVis + 16.f * SnowVis));
		const FLinearColor Grey(0.42f, 0.45f, 0.5f), White(0.75f, 0.78f, 0.82f);
		Fog->SetFogInscatteringColor(FMath::Lerp(FMath::Lerp(FogColBase, Grey, FMath::Clamp(CloudVis * 0.8f + RainVis * 0.4f, 0.f, 1.f)), White, SnowVis * 0.7f));
	}

	// cortinas de lluvia / nieve centradas en la cámara e inclinadas por el viento relativo
	static FVector LastCam = Cam; static FVector CamVel = FVector::ZeroVector;
	if (Dt > 0.f) CamVel = FMath::Lerp(CamVel, (Cam - LastCam) / Dt, FMath::Min(1.f, Dt * 4.f));
	LastCam = Cam;
	for (int32 l = 0; l < PrecipLayers.Num(); ++l)
	{
		const bool bRain = l < 3;
		const float Amt = bRain ? RainVis : SnowVis;
		const float Op = FMath::Clamp((Amt - 0.03f) / 0.6f, 0.f, 1.f) * GLayers[l].Op;
		PrecipLayers[l]->SetVisibility(Op > 0.01f);
		if (Op <= 0.01f) continue;
		const FVector Wind(S.Wind.X * 100.f * (bRain ? 0.6f : 1.f), S.Wind.Y * 100.f * (bRain ? 0.6f : 1.f), 0.f);
		FVector Rel = Wind - FVector(CamVel.X, CamVel.Y, 0.f); Rel.Z = -GLayers[l].Fall;
		FVector Dir = Rel.GetSafeNormal();
		if (Dir.Z > -0.57f) { Dir.Z = 0; Dir = Dir.GetSafeNormal() * 0.82f; Dir.Z = -0.57f; Dir.Normalize(); }
		PrecipLayers[l]->SetWorldLocationAndRotation(Cam, FQuat::FindBetweenNormals(FVector(0, 0, -1), Dir));
		LayerMats[l]->SetScalarParameterValue(TEXT("Opacity"), Op);
	}
	(void)Precip;
}

// =================================================================== hierba alta
ADustlineGrass::ADustlineGrass()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADustlineGrass::BeginPlay()
{
	Super::BeginPlay();
	for (int32 i = 0; i < 3; ++i)
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Vegetation/Grass/SM_TallGrass_%d.SM_TallGrass_%d"), i, i))) Meshes.Add(M);
	if (Meshes.Num() == 0 || !LoadData()) { UE_LOG(LogDustlineWeather, Warning, TEXT("Sin hierba alta (faltan mallas o datos)")); SetActorTickEnabled(false); }
}

namespace { constexpr float GTile = 2400.f, GRadius = 6600.f; }

void ADustlineGrass::BuildTile(const FIntPoint& T)
{
	UWorld* W = GetWorld();
	FVector Cam; if (!CameraPos(W, Cam)) return;
	const int32 Q = DustlineGfx::GetQuality();
	const float Mult = Q <= 0 ? 0.3f : (Q == 1 ? 0.55f : (Q == 2 ? 0.8f : 1.f));
	const float Step = 95.f / FMath::Sqrt(Mult);
	FRandomStream R(T.X * 73856093 ^ T.Y * 19349663);
	TArray<FTransform> X[3];
	const int32 NS = FMath::CeilToInt(GTile / Step);
	FCollisionQueryParams P(SCENE_QUERY_STAT(DustlineGrass), false);
	for (int32 j = 0; j < NS; ++j) for (int32 i = 0; i < NS; ++i)
	{
		const float Px = T.X * GTile + (i + R.FRand()) * Step, Py = T.Y * GTile + (j + R.FRand()) * Step;
		const FMask M = SampleMask(Px, Py);
		const float Want = M.TallGrass * (1.f - M.Dirt) * (1.f - M.Mud) * (1.f - M.Gravel);
		if (R.FRand() > Want * 1.6f) continue;
		if (WeatherAt(Px, Py).SnowM > 0.08f) continue;
		FHitResult H;
		if (!W->LineTraceSingleByChannel(H, FVector(Px, Py, Cam.Z + 30000.f), FVector(Px, Py, Cam.Z - 30000.f), ECC_Visibility, P)) continue;
		if (!H.GetComponent() || !H.GetComponent()->IsA<ULandscapeHeightfieldCollisionComponent>()) continue;
		if (H.ImpactNormal.Z < 0.86f || H.ImpactPoint.Z < 60.f) continue;
		{ float Lv; FVector2D Fl; if (WaterAt(Px, Py, Lv, Fl) && Lv > H.ImpactPoint.Z - 10.f) continue; }
		const FQuat Rot = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(FVector::UpVector, H.ImpactNormal), 0.6f) * FQuat(FVector::UpVector, R.FRandRange(0.f, 2 * PI));
		const float Sc = R.FRandRange(0.7f, 1.25f) * (0.6f + 0.4f * FMath::Clamp(Want * 1.5f, 0.f, 1.f));
		X[R.RandHelper(Meshes.Num())].Add(FTransform(Rot, H.ImpactPoint, FVector(Sc, Sc, Sc * R.FRandRange(0.85f, 1.15f))));
	}
	bool bAny = false;
	for (int32 m = 0; m < Meshes.Num(); ++m)
	{
		if (X[m].Num() == 0) continue;
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		C->SetStaticMesh(Meshes[m]); C->SetupAttachment(GetRootComponent());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); C->SetCullDistances(4800, (int32)GRadius);
		C->bAffectDistanceFieldLighting = false;
		C->RegisterComponent();
		C->AddInstances(X[m], false, true);
		Tiles.Add(FIntPoint(T.X * 4 + m, T.Y), C);   // clave por baldosa y variante
		bAny = true;
	}
	if (!bAny) Empty.Add(T);
	else Tiles.Add(FIntPoint(T.X * 4 + 3, T.Y), nullptr);   // marca "baldosa hecha"
}

void ADustlineGrass::Tick(float Dt)
{
	Super::Tick(Dt);
	FVector Cam; if (!CameraPos(GetWorld(), Cam)) return;
	const int32 Cx = FMath::FloorToInt(Cam.X / GTile), Cy = FMath::FloorToInt(Cam.Y / GTile);
	const int32 Rt = FMath::CeilToInt(GRadius / GTile);
	auto Done = [&](const FIntPoint& T) { return Empty.Contains(T) || Tiles.Contains(FIntPoint(T.X * 4 + 3, T.Y)); };
	// construir la baldosa pendiente más cercana (una por fotograma)
	float Best = 1e18f; FIntPoint BestT(0, 0); bool bFound = false;
	for (int32 dy = -Rt; dy <= Rt; ++dy) for (int32 dx = -Rt; dx <= Rt; ++dx)
	{
		const FIntPoint T(Cx + dx, Cy + dy);
		const FVector2D C((T.X + 0.5f) * GTile, (T.Y + 0.5f) * GTile);
		const float D = FVector2D::Distance(C, FVector2D(Cam.X, Cam.Y));
		if (D > GRadius + GTile * 0.75f || Done(T)) continue;
		if (D < Best) { Best = D; BestT = T; bFound = true; }
	}
	if (bFound) BuildTile(BestT);
	// quitar las lejanas
	TArray<FIntPoint> Kill;
	for (auto& It : Tiles)
	{
		const int32 TX = FMath::FloorToInt(It.Key.X / 4.f), TY = It.Key.Y;
		const FVector2D C((TX + 0.5f) * GTile, (TY + 0.5f) * GTile);
		if (FVector2D::Distance(C, FVector2D(Cam.X, Cam.Y)) > GRadius + GTile * 2.f) Kill.Add(It.Key);
	}
	for (const FIntPoint& K : Kill) { if (UHierarchicalInstancedStaticMeshComponent* C = Tiles[K]) C->DestroyComponent(); Tiles.Remove(K); }
	for (auto It = Empty.CreateIterator(); It; ++It)
	{
		const FVector2D C((It->X + 0.5f) * GTile, (It->Y + 0.5f) * GTile);
		if (FVector2D::Distance(C, FVector2D(Cam.X, Cam.Y)) > GRadius + GTile * 2.f) It.RemoveCurrent();
	}
}
