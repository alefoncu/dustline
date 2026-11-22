#include "DustlineCar.h"
#include "DustlineMeshBuilder.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// Kenney (CC0) bodies: mesh path, half-extents (x width, y length), top z, wheel-centre z, arch x, front/back arch y, body centre y
struct FKen { const TCHAR* Mesh; float Ex, Ey, Top, WheelZ, ArchX, FrontY, BackY, OY; };
static const FKen GKen[] = {
	{ TEXT("/Game/Cars/Kenney/suv/body.body"),                         75, 127, 130, 30, 43, 76, -56, 8 },
	{ TEXT("/Game/Cars/Kenney/suv_luxury/body.body"),                  75, 142, 130, 30, 43, 81, -71, 2 },
	{ TEXT("/Game/Cars/Kenney/hatchback_sports/body.body"),            65, 142, 110, 30, 43, 81, -81, -2 },
	{ TEXT("/Game/Cars/Kenney/truck/body.body"),                       75, 147, 130, 30, 43, 86, -76, -2 },
	{ TEXT("/Game/Cars/Kenney/tractor/body.body"),                     67,  99, 161, 33, 44, 73, -58, 11 },
	{ TEXT("/Game/Cars/Kenney/kart_oobi/kart-oobi.kart-oobi"),         47,  71,  68, 21, 38, 32, -36, 11 },
	{ TEXT("/Game/Cars/Kenney/van/body.body"),                         75, 138, 135, 30, 43, 76, -76, -2 },
};
// model index -> Kenney body: G=suv, Urus=suv_luxury, Panda=hatchback, Defender=van, Buggy=kart, Pickup=truck, Crawler=tractor
static const int32 GKenIdx[] = { 0, 1, 2, 6, 5, 3, 4 };

// Sections / material slots
enum ESec { SPaint = 0, SGlass, SDark, SMetal, SRubber, SRim, SBlock, STurbo, SAccent, SLight, STail, SNum };

static const FDustlineModel GModels[] = {
	{ TEXT("Clase G"),          0, 480, 195, 195, 295, 2450, 4000, 1.0f, 0.40f, 0.285f, 0.55f, 1.1f, 1, 0, { FLinearColor(0.50f,0.52f,0.55f), FLinearColor(0.02f,0.02f,0.025f), FLinearColor(0.03f,0.17f,0.07f) } },
	{ TEXT("Urus"),             1, 510, 202, 164, 300, 2200, 4000, 1.6f, 0.41f, 0.325f, 0.42f, 1.5f, 1, 0, { FLinearColor(0.95f,0.62f,0.02f), FLinearColor(0.70f,0.03f,0.02f), FLinearColor(0.02f,0.02f,0.03f) } },
	{ TEXT("Panda 4x4"),        2, 370, 165, 156, 230,  980,  900, 0.0f, 0.32f, 0.185f, 0.42f, 0.8f, 1, 0, { FLinearColor(0.85f,0.85f,0.82f), FLinearColor(0.60f,0.03f,0.03f), FLinearColor(0.03f,0.15f,0.45f) } },
	{ TEXT("Defender clasico"), 3, 440, 180, 200, 279, 1850, 2500, 0.6f, 0.42f, 0.235f, 0.62f, 0.9f, 1, 0, { FLinearColor(0.07f,0.12f,0.04f), FLinearColor(0.60f,0.55f,0.40f), FLinearColor(0.32f,0.03f,0.02f) } },
	{ TEXT("Buggy"),            4, 330, 190, 135, 220,  720, 1600, 1.0f, 0.48f, 0.320f, 0.82f, 0.85f, 4, 1, { FLinearColor(0.95f,0.20f,0.01f), FLinearColor(0.02f,0.30f,0.70f), FLinearColor(0.70f,0.55f,0.02f) } },
	{ TEXT("Pick-up 4x4"),      5, 530, 186, 185, 310, 2050, 2800, 1.0f, 0.44f, 0.265f, 0.60f, 1.0f, 1, 0, { FLinearColor(0.02f,0.07f,0.30f), FLinearColor(0.60f,0.60f,0.60f), FLinearColor(0.30f,0.02f,0.02f) } },
	{ TEXT("Rock Crawler"),     6, 390, 190, 190, 250, 1500, 3500, 0.0f, 0.62f, 0.400f, 0.90f, 0.7f, 2, 0, { FLinearColor(0.70f,0.05f,0.01f), FLinearColor(0.70f,0.60f,0.02f), FLinearColor(0.05f,0.05f,0.06f) } },
};

const FDustlineModel& ADustlineCar::GetModel(int32 I) { return GModels[FMath::Clamp(I, 0, UE_ARRAY_COUNT(GModels) - 1)]; }
int32 ADustlineCar::NumModels() { return UE_ARRAY_COUNT(GModels); }
FString ADustlineCar::ModelName(int32 I) { return GetModel(I).Name; }
FDustlineSetup ADustlineCar::StockSetup(int32 I)
{
	const FDustlineModel& M = GetModel(I); FDustlineSetup S;
	S.Model = I; S.Paint = M.Colors[0]; S.EngineCC = M.CC; S.Turbo = M.Turbo; S.Stiffness = M.Stiff; S.TravelM = M.Travel;
	S.TireRadiusM = M.Radius; S.TireWidthM = M.Width; S.TireType = M.Tire; S.Drive = M.Drive; S.Ballast = 0; return S;
}

ADustlineCar::ADustlineCar()
{
	PrimaryActorTick.bCanEverTick = true;
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision")); SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(200, 90, 70)); Collision->SetCollisionProfileName(TEXT("Pawn")); Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block); Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(false);
	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual")); Visual->SetupAttachment(Collision);
	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam")); Cam->SetupAttachment(Collision); Cam->FieldOfView = 75.f;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
	auto Mk = [&](const TCHAR* N) { UProceduralMeshComponent* P = CreateDefaultSubobject<UProceduralMeshComponent>(N); P->SetupAttachment(Visual); P->bUseComplexAsSimpleCollision = false; P->SetCollisionEnabled(ECollisionEnabled::NoCollision); P->CastShadow = true; return P; };
	KBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KBody")); KBody->SetupAttachment(Visual); KBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body = Mk(TEXT("Body")); Engine = Mk(TEXT("Engine")); Suspension = Mk(TEXT("Suspension"));
	for (int32 i = 0; i < 4; ++i) Wheels.Add(Mk(*FString::Printf(TEXT("Wheel%d"), i)));
}

void ADustlineCar::OnConstruction(const FTransform& T) { Super::OnConstruction(T); Rebuild(); }
void ADustlineCar::ApplySetup(const FDustlineSetup& S) { Setup = S; Rebuild(); }
void ADustlineCar::SetXRay(bool bOn) { bXRay = bOn; RefreshMaterials(); }

float ADustlineCar::GroundClearance() const { if (bKen) return KBodyZ + 22.f * KSz; return 0.40f * Setup.TireRadiusM * 100.f + 0.28f * Setup.TravelM * 100.f; }
float ADustlineCar::FrontX() const { return bKen ? GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, 6)]].FrontY * KSy : GetModel(Setup.Model).Wheelbase * 0.5f; }
float ADustlineCar::RearX() const { return bKen ? GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, 6)]].BackY * KSy : -GetModel(Setup.Model).Wheelbase * 0.5f; }
float ADustlineCar::BodyFront() const { if (!bKen) return GetModel(Setup.Model).L * 0.5f; const FKen& K = GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, 6)]]; return (K.OY + K.Ey) * KSy; }
void ADustlineCar::ComputeKenney()
{
	bKen = false;
	if (!bUseKenney) return;
	const FKen& K = GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, 6)]];
	if (!LoadObject<UStaticMesh>(nullptr, K.Mesh)) return;
	const FDustlineModel& M = GetModel(Setup.Model);
	const float R = Setup.TireRadiusM * 100.f, Tcm = Setup.TravelM * 100.f;
	const float lift = FMath::Max(0.f, Tcm * 0.30f - 10.f), extra = FMath::Max(0.f, R - 42.f);
	KSx = M.W * 0.5f / K.Ex; KSy = M.L * 0.5f / K.Ey;
	KSz = FMath::Max(0.5f, (M.H - lift - R - extra) / FMath::Max(10.f, K.Top - K.WheelZ));
	KBodyZ = lift + R - K.WheelZ * KSz + extra;
	bKen = true;
}
FVector ADustlineCar::WheelPos(int32 I) const
{
	const FDustlineModel& M = GetModel(Setup.Model);
	if (bKen) { const FKen& K = GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, 6)]]; return FVector((I < 2 ? K.FrontY : K.BackY) * KSy, ((I % 2) == 0 ? -1.f : 1.f) * K.ArchX * KSx, Setup.TireRadiusM * 100.f); }
	const float x = (I < 2 ? 1.f : -1.f) * M.Wheelbase * 0.5f;
	const float y = ((I % 2) == 0 ? -1.f : 1.f) * (M.W * 0.5f - Setup.TireWidthM * 50.f + 6.f);
	return FVector(x, y, Setup.TireRadiusM * 100.f);
}

UMaterialInstanceDynamic* ADustlineCar::MakeMat(const TCHAR* Path, const FLinearColor& C, float Metallic, float Rough)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, Path);
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetVectorParameterValue(TEXT("Color"), C); M->SetScalarParameterValue(TEXT("Metallic"), Metallic); M->SetScalarParameterValue(TEXT("Roughness"), Rough);
	return M;
}

void ADustlineCar::RefreshMaterials()
{
	const TCHAR* G = TEXT("/Game/Cars/M_CarGeneric.M_CarGeneric");
	const float Fade = bXRay ? 0.12f : 1.f;
	auto Set = [&](int32 id, UMaterialInstanceDynamic* M) { if (M) Mats.Add(id, M); };
	UMaterialInstanceDynamic* Paint = MakeMat(G, Setup.Paint, 0.7f, 0.28f); if (Paint) Paint->SetScalarParameterValue(TEXT("Fade"), Fade);
	UMaterialInstanceDynamic* Glass = MakeMat(G, FLinearColor(0.01f, 0.015f, 0.02f), 0.0f, 0.05f); if (Glass) Glass->SetScalarParameterValue(TEXT("Fade"), bXRay ? 0.08f : 0.78f);
	UMaterialInstanceDynamic* Dark = MakeMat(G, FLinearColor(0.012f, 0.012f, 0.014f), 0.2f, 0.6f); if (Dark && bXRay) Dark->SetScalarParameterValue(TEXT("Fade"), 0.3f);
	Set(SPaint, Paint); Set(SGlass, Glass); Set(SDark, Dark);
	if (UMaterialInstanceDynamic* Mt = MakeMat(G, FLinearColor(0.35f, 0.35f, 0.37f), 1.0f, 0.38f)) { Mt->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.07f : 0.02f); Set(SMetal, Mt); }
	Set(SRubber, MakeMat(G, FLinearColor(0.008f, 0.008f, 0.009f), 0.0f, 0.88f));
	Set(SRim, MakeMat(G, FLinearColor(0.55f, 0.56f, 0.6f), 1.0f, 0.25f));
	if (UMaterialInstanceDynamic* Bk = MakeMat(G, FLinearColor(0.20f, 0.21f, 0.23f), 0.6f, 0.5f)) { Bk->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.10f : 0.f); Set(SBlock, Bk); }
	if (UMaterialInstanceDynamic* Tb = MakeMat(G, FLinearColor(0.85f, 0.32f, 0.04f), 1.0f, 0.32f)) { Tb->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.45f : 0.05f); Set(STurbo, Tb); }
	if (UMaterialInstanceDynamic* Ac = MakeMat(G, FLinearColor(0.55f, 0.02f, 0.02f), 0.4f, 0.35f)) { Ac->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.12f : 0.f); Set(SAccent, Ac); }
	if (UMaterialInstanceDynamic* L = MakeMat(G, FLinearColor(1.f, 0.95f, 0.8f), 0.f, 0.1f)) { L->SetScalarParameterValue(TEXT("Emissive"), 6.f); Set(SLight, L); }
	if (UMaterialInstanceDynamic* T = MakeMat(G, FLinearColor(0.9f, 0.02f, 0.02f), 0.f, 0.1f)) { T->SetScalarParameterValue(TEXT("Emissive"), 3.f); Set(STail, T); }
	KBody->SetCastShadow(!bXRay); Body->SetCastShadow(!bXRay);
	if (UMaterialInterface* KB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cars/M_KenneyBody.M_KenneyBody")))
	{
		UMaterialInstanceDynamic* KM = UMaterialInstanceDynamic::Create(KB, this);
		KM->SetVectorParameterValue(TEXT("Paint"), Setup.Paint); KM->SetScalarParameterValue(TEXT("Fade"), Fade);
		KBody->SetMaterial(0, KM);
	}
	auto Apply = [&](UProceduralMeshComponent* P) { for (int32 i = 0; i < P->GetNumSections(); ++i) if (UMaterialInterface** M = Mats.Find(i)) P->SetMaterial(i, *M); };
	Apply(Body); Apply(Engine); Apply(Suspension); for (UProceduralMeshComponent* W : Wheels) Apply(W);
}

void ADustlineCar::Rebuild()
{
	Mats.Empty();
	ComputeKenney();
	RefreshMaterials();
	BuildBody(); BuildWheels(); BuildSuspension(); BuildEngine();
	RefreshMaterials();
	{
		const FDustlineModel& M = GetModel(Setup.Model);
		const float gc = GroundClearance();
		ZCom = 0.5f * (gc + M.H);
		Visual->SetRelativeLocation(FVector(0, 0, -ZCom));
		Collision->SetBoxExtent(FVector(M.L * 0.46f, M.W * 0.47f, FMath::Max(25.f, (M.H - gc) * 0.45f)));
		ComputeSpec();
	}
}

// ---------------------------------------------------------------- wheel geometry
static void AddWheelGeometry(FDustlineMeshBuilder& B, float R, float Wd, int32 Type, float SideSign)
{
	const float Rr = R * (Type == 4 ? 0.50f : 0.60f);     // rim radius
	const FVector Ax(0, 1, 0);
	TArray<FVector2D> Prof;
	const float Bulge = (Type == 4) ? 1.12f : 1.0f;
	Prof = { FVector2D(-0.44f * Wd, Rr * 1.00f), FVector2D(-0.50f * Wd, Rr * 1.10f), FVector2D(-0.52f * Wd * Bulge, R * 0.78f), FVector2D(-0.47f * Wd, R * 0.93f),
		FVector2D(-0.36f * Wd, R * 0.992f), FVector2D(-0.22f * Wd, R), FVector2D(0.22f * Wd, R), FVector2D(0.36f * Wd, R * 0.992f), FVector2D(0.47f * Wd, R * 0.93f),
		FVector2D(0.52f * Wd * Bulge, R * 0.78f), FVector2D(0.50f * Wd, Rr * 1.10f), FVector2D(0.44f * Wd, Rr * 1.00f) };
	B.Lathe(SRubber, FVector::ZeroVector, Ax, Prof, 40, 2.f);
	// tread
	const FTransform Keep = B.Xf;
	if (Type == 1 || Type == 2)
	{
		const int32 N = Type == 2 ? 20 : 30; const float Kh = (Type == 2 ? 0.075f : 0.04f) * R, Kw = (Type == 2 ? 0.40f : 0.30f) * Wd, Kl = R * (Type == 2 ? 0.20f : 0.13f);
		for (int32 row = -1; row <= 1; row += 2) for (int32 i = 0; i < N; ++i)
		{
			B.Xf = FTransform(FRotator(0, 0, 0)); B.Xf = FTransform(FQuat(Ax, 2 * PI * (i + (row > 0 ? 0.5f : 0.f)) / N)) * Keep;
			B.Box(SRubber, FVector(0, row * Wd * 0.25f, R + Kh * 0.35f), FVector(Kw * 0.5f, Kw * 0.5f, Kh).ComponentMax(FVector(0.3f)));
			B.Box(SRubber, FVector(Kl * 0.0f, row * Wd * 0.25f, R + Kh * 0.35f), FVector(Kl * 0.5f, Kw * 0.5f, Kh));
		}
		B.Xf = Keep;
	}
	if (Type == 3)   // studs
	{
		for (int32 row = -2; row <= 2; ++row) for (int32 i = 0; i < 36; ++i)
		{
			B.Xf = FTransform(FQuat(Ax, 2 * PI * (i + (row & 1 ? 0.5f : 0.f)) / 36)) * Keep;
			B.Cylinder(SMetal, FVector(0, row * Wd * 0.18f, R - 0.6f), FVector(0, 0, 1), 0.55f, 2.4f, 6, true);
		}
		B.Xf = Keep;
	}
	// rim: barrel + face + spokes + hub + bolts (face towards outside = +Y * SideSign)
	TArray<FVector2D> Barrel = { FVector2D(-0.42f * Wd, Rr * 0.96f), FVector2D(-0.40f * Wd, Rr * 1.02f), FVector2D(0.40f * Wd, Rr * 1.02f), FVector2D(0.42f * Wd, Rr * 0.96f) };
	B.Lathe(SRim, FVector::ZeroVector, Ax, Barrel, 32);
	const float Face = SideSign * 0.34f * Wd;
	TArray<FVector2D> Disc = { FVector2D(Face, Rr * 0.18f), FVector2D(Face, Rr * 0.96f) };
	B.Lathe(SDark, FVector::ZeroVector, Ax, Disc, 32);
	const int32 Spokes = Type == 2 ? 8 : 6;
	for (int32 i = 0; i < Spokes; ++i)
	{
		B.Xf = FTransform(FQuat(Ax, 2 * PI * i / Spokes)) * Keep;
		B.Box(SRim, FVector(0, Face + SideSign * 0.5f, Rr * 0.58f), FVector(Rr * 0.07f, 0.9f, Rr * 0.42f));
	}
	B.Xf = Keep;
	B.Cylinder(SRim, FVector(0, Face, 0), FVector(0, SideSign, 0), Rr * 0.20f, 1.6f, 16, true);
	for (int32 i = 0; i < 5; ++i)
	{
		B.Xf = FTransform(FQuat(Ax, 2 * PI * i / 5)) * Keep;
		B.Cylinder(SMetal, FVector(0, Face + SideSign * 1.2f, Rr * 0.30f), FVector(0, SideSign, 0), 0.8f, 1.2f, 6, true);
	}
	B.Xf = Keep;
	// brake disc + caliper (inboard)
	B.Cylinder(SMetal, FVector(0, -SideSign * 0.22f * Wd, 0), FVector(0, SideSign, 0), Rr * 0.86f, 1.8f, 28, true);
	B.Box(SAccent, FVector(0, -SideSign * 0.22f * Wd, Rr * 0.80f), FVector(Rr * 0.22f, 3.2f, Rr * 0.12f));
}

void ADustlineCar::BuildWheels()
{
	const float R = Setup.TireRadiusM * 100.f, Wd = Setup.TireWidthM * 100.f;
	for (int32 i = 0; i < 4; ++i)
	{
		FDustlineMeshBuilder B;
		AddWheelGeometry(B, R, Wd, Setup.TireType, (i % 2) == 0 ? -1.f : 1.f);
		B.Build(Wheels[i], Mats, false);
		Wheels[i]->SetRelativeLocation(WheelPos(i));
	}
}

// ---------------------------------------------------------------- body
void ADustlineCar::BuildBody()
{
	if (bKen)
	{
		const FKen& K = GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, 6)]];
		Body->ClearAllMeshSections();
		KBody->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, K.Mesh));
		KBody->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		KBody->SetRelativeScale3D(FVector(KSx, KSy, KSz));
		KBody->SetRelativeLocation(FVector(0, 0, KBodyZ));
		return;
	}
	KBody->SetStaticMesh(nullptr);
	const FDustlineModel& M = GetModel(Setup.Model);
	const float L = M.L, W = M.W, H = M.H, gc = GroundClearance();
	const int32 St = M.Style;
	const bool bBuggy = St == 4;
	const float belt = gc + (bBuggy ? 0.40f : 0.52f) * (H - gc);
	struct FCab { float t0, t1, rakeR, rakeF, tumble; } Cab;
	switch (St)
	{
	case 0: Cab = { 0.03f, 0.64f, 0.035f, 0.07f, 0.05f }; break;
	case 1: Cab = { 0.10f, 0.60f, 0.16f, 0.14f, 0.20f }; break;
	case 2: Cab = { 0.05f, 0.70f, 0.07f, 0.12f, 0.09f }; break;
	case 3: Cab = { 0.04f, 0.66f, 0.03f, 0.06f, 0.06f }; break;
	case 5: Cab = { 0.42f, 0.72f, 0.04f, 0.09f, 0.08f }; break;
	case 6: Cab = { 0.10f, 0.56f, 0.04f, 0.07f, 0.08f }; break;
	default: Cab = { 2.f, 3.f, 0.1f, 0.1f, 0.f }; break;   // no cabin
	}
	auto Smooth = [](float x) { x = FMath::Clamp(x, 0.f, 1.f); return x * x * (3 - 2 * x); };
	auto ZBot = [&](float t) { return gc + 12.f * FMath::Pow(FMath::Abs(2 * t - 1), 6.f); };
	auto ZBelt = [&](float t) { float z = belt - 11.f * Smooth((t - 0.55f) / 0.45f); if (St == 5 && t < Cab.t0 - 0.02f) z -= 14.f; return z; };
	auto HalfW = [&](float t) { return W * 0.5f * (1.f - 0.07f * Smooth((t - 0.90f) / 0.10f) - 0.04f * Smooth((0.10f - t) / 0.10f)); };
	auto ZRoof = [&](float t) {
		if (t <= Cab.t0 || t >= Cab.t1) return ZBelt(t);
		const float r = Smooth((t - Cab.t0) / Cab.rakeR), f = Smooth((Cab.t1 - t) / Cab.rakeF);
		return ZBelt(t) + (H - ZBelt(t)) * FMath::Min(r, f);
	};
	const int32 NS = 64, NR = 18;
	TArray<TArray<FVector>> P; P.SetNum(NS + 1);
	for (int32 i = 0; i <= NS; ++i)
	{
		const float t = (float)i / NS, x = (t - 0.5f) * L;
		const float zb = ZBot(t), zbelt = ZBelt(t), zroof = ZRoof(t), w = HalfW(t);
		const float hl = zbelt - zb, hc = zroof - zbelt;
		const float cb = 0.88f * w, ct = cb - Cab.tumble * w;
		TArray<FVector2D> R;   // (y>=0, z)
		R.Add(FVector2D(0, zb)); R.Add(FVector2D(0.80f * w, zb)); R.Add(FVector2D(0.96f * w, zb + 0.10f * hl)); R.Add(FVector2D(w, zb + 0.30f * hl)); R.Add(FVector2D(w, zb + 0.82f * hl));
		R.Add(FVector2D(0.97f * w, zbelt)); R.Add(FVector2D(cb, zbelt)); R.Add(FVector2D(ct, zroof - 0.08f * hc)); R.Add(FVector2D(ct * 0.80f, zroof)); R.Add(FVector2D(0, zroof));
		TArray<FVector>& Ring = P[i];
		for (int32 k = 0; k < 10; ++k) Ring.Add(FVector(x, R[k].X, R[k].Y));
		for (int32 k = 8; k >= 1; --k) Ring.Add(FVector(x, -R[k].X, R[k].Y));
	}
	// smooth normals
	TArray<TArray<FVector>> Nm; Nm.SetNum(NS + 1);
	for (int32 i = 0; i <= NS; ++i) { Nm[i].SetNum(NR); for (int32 j = 0; j < NR; ++j)
	{
		const FVector Pa = P[FMath::Min(i + 1, NS)][j] - P[FMath::Max(i - 1, 0)][j];
		const FVector Pb = P[i][(j + 1) % NR] - P[i][(j + NR - 1) % NR];
		FVector N = FVector::CrossProduct(Pa, Pb).GetSafeNormal();
		const FVector Out = (j <= 9) ? FVector(0, 1, 0.3f) : FVector(0, -1, 0.3f);
		if (FVector::DotProduct(N, P[i][j] - FVector(P[i][j].X, 0, (P[i][j].Z > belt ? belt + 20 : belt - 30))) < 0) N = -N;
		Nm[i][j] = N;
	} }
	FDustlineMeshBuilder B;
	const float Pillar[3] = { Cab.t0 + Cab.rakeR * 0.9f, 0.5f * (Cab.t0 + Cab.t1), Cab.t1 - Cab.rakeF * 0.9f };
	for (int32 i = 0; i < NS; ++i)
	{
		const float tm = (i + 0.5f) / NS;
		const bool bCab = ZRoof(tm) > ZBelt(tm) + 2.f;
		bool bPillar = false; for (float pc : Pillar) if (FMath::Abs(tm - pc) < 0.013f) bPillar = true;
		for (int32 j = 0; j < NR; ++j)
		{
			const int32 j2 = (j + 1) % NR;
			int32 Id = SPaint;
			// map ring segment to material
			const int32 s = (j <= 9) ? j : (j == 17 ? 0 : 18 - j - 1 + 0);   // mirrored index for left side
			int32 seg = (j <= 8) ? j : (j == 9 ? 8 : (j < 17 ? 17 - j : 0));
			if (j == 17) Id = SDark;   // bottom closing segment (left to centre)
			if (seg == 0 && j != 9) Id = SDark;
			if (bCab && !bPillar && seg == 6) Id = SGlass;
			const float dz = FMath::Abs(P[i + 1][j].Z - P[i][j].Z), dx = FMath::Abs(P[i + 1][j].X - P[i][j].X);
			if (bCab && (seg == 7 || seg == 8) && dz > 0.40f * dx && dz > 1.2f) Id = SGlass;
			B.QuadN(Id, P[i][j], P[i][j2], P[i + 1][j2], P[i + 1][j], Nm[i][j], Nm[i][j2], Nm[i + 1][j2], Nm[i + 1][j]);
		}
	}
	// end caps
	for (int32 e = 0; e < 2; ++e)
	{
		const int32 i = e == 0 ? 0 : NS; const FVector N(e == 0 ? -1.f : 1.f, 0, 0);
		FVector C(0); for (const FVector& v : P[i]) C += v; C /= NR;
		for (int32 j = 0; j < NR; ++j) B.Tri(e == 0 ? SPaint : SPaint, C, P[i][j], P[i][(j + 1) % NR], N, N, N, FVector2D(.5, .5), FVector2D(0, 0), FVector2D(1, 0));
	}
	// bumpers, grille, lights, trim
	const float zbot = gc + 6.f;
	B.Box(SDark, FVector(L * 0.5f - 4, 0, zbot + 16), FVector(9, W * 0.46f, 15));
	B.Box(SDark, FVector(-L * 0.5f + 4, 0, zbot + 16), FVector(9, W * 0.46f, 15));
	B.Box(SDark, FVector(L * 0.5f - 1, 0, belt - 18), FVector(3, W * 0.22f, 12));   // grille
	for (int32 s = -1; s <= 1; s += 2)
	{
		B.Box(SLight, FVector(L * 0.5f - 2, s * W * 0.34f, belt - 12), FVector(2.5f, 13.f, 6.f));
		B.Box(STail, FVector(-L * 0.5f + 2, s * W * 0.36f, belt - 10), FVector(2.5f, 11.f, 8.f));
	}
	// wheel arch trims (dark rings on the body side)
	const float R = Setup.TireRadiusM * 100.f;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f;
		TArray<FVector2D> Ring = { FVector2D(0, R + 4.f), FVector2D(0, R + 11.f) };
		B.Lathe(SDark, FVector(WP.X, side * (W * 0.5f + 0.8f), WP.Z), FVector(0, 1, 0), Ring, 36);
	}
	// style extras
	if (St == 5)   // pickup bed walls
	{
		const float bx0 = -L * 0.5f, bx1 = (Cab.t0 - 0.5f) * L - 4.f, zb0 = ZBelt(0.1f), hgt = 38.f;
		B.Box(SPaint, FVector((bx0 + bx1) * 0.5f, W * 0.5f - 3.f, zb0 + hgt * 0.5f), FVector((bx1 - bx0) * 0.5f, 2.f, hgt * 0.5f));
		B.Box(SPaint, FVector((bx0 + bx1) * 0.5f, -W * 0.5f + 3.f, zb0 + hgt * 0.5f), FVector((bx1 - bx0) * 0.5f, 2.f, hgt * 0.5f));
		B.Box(SPaint, FVector(bx0 + 2.f, 0, zb0 + hgt * 0.5f), FVector(2.f, W * 0.5f - 3.f, hgt * 0.5f));
		B.Box(SPaint, FVector(bx1, 0, zb0 + hgt * 0.5f), FVector(2.f, W * 0.5f - 3.f, hgt * 0.5f));
	}
	if (St == 4)   // buggy: roll cage + seats
	{
		const float zt = H, hw = W * 0.5f - 34.f;
		auto Hoop = [&](float x) { B.Tube(SPaint, { FVector(x, -hw, belt), FVector(x, -hw * 0.85f, zt), FVector(x, hw * 0.85f, zt), FVector(x, hw, belt) }, 2.4f, 8); };
		Hoop(-10.f); Hoop(55.f);
		for (int32 s = -1; s <= 1; s += 2) B.Tube(SPaint, { FVector(-10, s * hw * 0.85f, zt), FVector(55, s * hw * 0.85f, zt) }, 2.4f, 8);
		B.Tube(SPaint, { FVector(-10, -hw * 0.85f, zt), FVector(-70, -hw * 0.5f, belt + 6) }, 2.2f, 8);
		B.Tube(SPaint, { FVector(-10, hw * 0.85f, zt), FVector(-70, hw * 0.5f, belt + 6) }, 2.2f, 8);
		for (int32 s = -1; s <= 1; s += 2) { B.Box(SDark, FVector(15, s * 26.f, belt + 2), FVector(16, 20, 4)); B.Box(SDark, FVector(0, s * 26.f, belt + 24), FVector(4, 20, 22)); }
	}
	if (St == 3 || St == 0 || St == 6)   // spare wheel on the back
	{
		FDustlineMeshBuilder::FSec& dummy = B.S(SRubber); (void)dummy;
		const FTransform Keep = B.Xf; B.Xf = FTransform(FRotator(0, 90, 90), FVector(-L * 0.5f - 18.f, 0, belt - 18.f)) * Keep;
		AddWheelGeometry(B, FMath::Min(R, 42.f), FMath::Min(Setup.TireWidthM * 100.f, 28.f), 1, 1.f);
		B.Xf = Keep;
	}
	B.Build(Body, Mats, false);
}

// ---------------------------------------------------------------- suspension & drivetrain
void ADustlineCar::BuildSuspension()
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const float R = Setup.TireRadiusM * 100.f, gc = GroundClearance(), Wd = Setup.TireWidthM * 100.f;
	FDustlineMeshBuilder B;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i); const float side = (i % 2) == 0 ? -1.f : 1.f;
		const float yIn = WP.Y - side * (Wd * 0.5f + 11.f);
		const float zLow = R + 9.f, zTop = FMath::Max(gc + 8.f, zLow + 20.f);
		const float wire = 0.55f + 0.75f * Setup.Stiffness, cr = 6.5f;
		TArray<FVector> Coil; const int32 Turns = FMath::Clamp((int32)(10 - Setup.Stiffness * 2.5f), 4, 9), Pts = Turns * 14;
		for (int32 k = 0; k <= Pts; ++k) { const float a = 2 * PI * k / 14.f; Coil.Add(FVector(WP.X + FMath::Cos(a) * cr, yIn + FMath::Sin(a) * cr, zLow + (zTop - zLow) * k / Pts)); }
		B.Tube(SAccent, Coil, wire, 6);
		B.Cylinder(SDark, FVector(WP.X, yIn, zLow - 4.f), FVector(0, 0, 1), 2.6f, (zTop - zLow) * 0.62f, 12, true);
		B.Cylinder(SMetal, FVector(WP.X, yIn, zLow + (zTop - zLow) * 0.55f), FVector(0, 0, 1), 1.3f, (zTop - zLow) * 0.55f + 6.f, 10, true);
		// control arms (upper & lower) to the chassis centre line
		const float yC = side * M.W * 0.12f;
		B.Tube(SDark, { FVector(WP.X + 12, yC, R - 4.f), FVector(WP.X, WP.Y - side * Wd * 0.3f, R - 2.f) }, 2.4f, 8);
		B.Tube(SDark, { FVector(WP.X - 12, yC, R - 4.f), FVector(WP.X, WP.Y - side * Wd * 0.3f, R - 2.f) }, 2.4f, 8);
		B.Tube(SDark, { FVector(WP.X, yC * 0.8f, zTop - 4.f), FVector(WP.X, WP.Y - side * Wd * 0.3f, R + 6.f) }, 2.2f, 8);
	}
	// differentials / drive shafts according to drivetrain
	const float xF = FrontX(), xR = RearX();
	const bool bF = Setup.Drive != 1, bR = Setup.Drive != 2;
	if (bF) { B.Sphere(SBlock, FVector(xF, 0, R + 2.f), 11.f, 10); B.Tube(SMetal, { WheelPos(0) + FVector(0, 14, 0), FVector(xF, 0, R + 2.f), WheelPos(1) - FVector(0, 14, 0) }, 2.6f, 8); }
	if (bR) { B.Sphere(SBlock, FVector(xR, 0, R + 2.f), 11.f, 10); B.Tube(SMetal, { WheelPos(2) + FVector(0, 14, 0), FVector(xR, 0, R + 2.f), WheelPos(3) - FVector(0, 14, 0) }, 2.6f, 8); }
	if (bF && bR) B.Tube(SMetal, { FVector(xF - 10, 0, R + 4.f), FVector(0, 0, R + 6.f), FVector(xR + 10, 0, R + 4.f) }, 3.8f, 10);
	else if (bF) B.Tube(SMetal, { FVector(xF - 10, 0, R + 4.f), FVector(xF * 0.3f, 0, R + 6.f) }, 3.8f, 10);
	else B.Tube(SMetal, { FVector(xR + 10, 0, R + 4.f), FVector(xR * 0.3f, 0, R + 6.f) }, 3.8f, 10);
	B.Build(Suspension, Mats, false);
}

// ---------------------------------------------------------------- engine bay
void ADustlineCar::BuildEngine()
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const float gc = GroundClearance(), cc = Setup.EngineCC;
	int32 n = FMath::Clamp(FMath::RoundToInt(cc / 650.f), 2, 12);
	const bool bV = n > 6 || (cc > 3000.f && n >= 6);
	if (bV && (n % 2)) n += 1;
	const int32 banks = bV ? 2 : 1, perBank = bV ? (n + 1) / 2 : n;
	const float dcc = cc / n, bore = 8.f * FMath::Pow(dcc / 500.f, 1.f / 3.f), pitch = bore * 1.2f;
	const float len = perBank * pitch + 8.f;
	const FVector Org(FrontX() - 4.f, 0, FMath::Max(gc + 28.f, Setup.TireRadiusM * 100.f + 6.f) + bore * 0.9f);
	FDustlineMeshBuilder B; const FTransform Keep = B.Xf;
	B.Box(SBlock, Org + FVector(0, 0, -bore * 0.5f), FVector(len * 0.5f, bore * (bV ? 0.9f : 0.75f), bore * 0.9f));            // block
	B.Box(SDark, Org + FVector(0, 0, -bore * 1.55f), FVector(len * 0.5f - 2, bore * 0.7f, bore * 0.45f));                     // oil pan
	for (int32 b = 0; b < banks; ++b)
	{
		const float ang = bV ? (b == 0 ? -32.f : 32.f) : 0.f;
		B.Xf = FTransform(FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(ang)), Org + FVector(0, 0, bore * 0.2f)) * Keep;
		B.Box(SBlock, FVector(0, 0, bore * 0.65f), FVector(len * 0.5f, bore * 0.58f, bore * 0.55f));                          // head
		B.Box(SAccent, FVector(0, 0, bore * 1.25f), FVector(len * 0.5f - 1.5f, bore * 0.5f, bore * 0.16f));                    // valve cover
		for (int32 c = 0; c < perBank; ++c) B.Cylinder(SMetal, FVector(-len * 0.5f + 6.f + c * pitch + pitch * 0.2f, 0, bore * 1.38f), FVector(0, 0, 1), 1.3f, bore * 0.28f, 8, true);  // plugs/coils
	}
	B.Xf = Keep;
	B.Box(SMetal, Org + FVector(0, 0, bore * (bV ? 1.1f : 1.9f)), FVector(len * 0.40f, bore * 0.35f, bore * 0.2f));            // intake plenum
	for (int32 c = 0; c < perBank; ++c) for (int32 b = 0; b < banks; ++b)                                                      // exhaust runners
	{
		const float sgn = bV ? (b == 0 ? -1.f : 1.f) : 1.f; const float x = Org.X - len * 0.5f + 6.f + c * pitch;
		B.Tube(SMetal, { FVector(x, Org.Y + sgn * bore * 1.05f, Org.Z + bore * 0.4f), FVector(x, Org.Y + sgn * bore * 1.5f, Org.Z - bore * 0.2f), FVector(x - 4.f, Org.Y + sgn * bore * 1.5f, Org.Z - bore * 1.0f) }, 1.6f, 8);
	}
	B.Cylinder(SDark, Org + FVector(len * 0.5f, 0, 0), FVector(1, 0, 0), bore * 0.9f, 5.f, 18, true);                           // belt cover / damper
	B.Cylinder(SMetal, Org + FVector(len * 0.5f + 5.f, 0, bore * 0.3f), FVector(1, 0, 0), bore * 0.35f, 4.f, 14, true);
	B.Cylinder(SDark, Org + FVector(len * 0.5f - 3.f, bore * 1.1f, -bore * 0.4f), FVector(1, 0, 0), bore * 0.4f, 9.f, 12, true); // alternator
	// radiator + fan
	const float xr = BodyFront() - 26.f;
	B.Box(SDark, FVector(xr, 0, gc + 30.f), FVector(3.f, M.W * 0.22f, 14.f));
	B.Cylinder(SMetal, FVector(xr - 6.f, 0, gc + 36.f), FVector(1, 0, 0), 17.f, 2.f, 16, true);
	// turbo(s) + intercooler (physically appear with the Turbo slider)
	const float tb = Setup.Turbo;
	if (tb > 0.05f)
	{
		const int32 count = tb > 1.8f ? 2 : 1; const float r = 5.f + tb * 3.2f;
		for (int32 k = 0; k < count; ++k)
		{
			const float sgn = (count == 1) ? 1.f : (k == 0 ? -1.f : 1.f);
			const FVector TP(Org.X - len * 0.10f, Org.Y + sgn * (bore * 1.9f + r * 1.2f), Org.Z + bore * 0.7f);
			TArray<FVector2D> Comp = { FVector2D(0, r * 0.35f), FVector2D(r * 0.25f, r * 0.9f), FVector2D(r * 0.8f, r), FVector2D(r * 1.2f, r * 0.8f), FVector2D(r * 1.4f, r * 0.45f) };
			B.Lathe(SMetal, TP, FVector(1, 0, 0), Comp, 24);                                                                // compressor housing
			TArray<FVector2D> Turb = { FVector2D(0, r * 0.4f), FVector2D(-r * 0.3f, r * 0.9f), FVector2D(-r * 0.9f, r * 0.85f), FVector2D(-r * 1.3f, r * 0.5f) };
			B.Lathe(STurbo, TP, FVector(1, 0, 0), Turb, 24);                                                               // hot side
			B.Cylinder(SMetal, TP + FVector(-r * 1.0f, 0, 0), FVector(1, 0, 0), r * 0.16f, r * 2.0f, 10, true);
			B.Tube(STurbo, { FVector(Org.X - len * 0.15f, Org.Y + sgn * bore * 1.5f, Org.Z - bore * 0.5f), TP + FVector(-r * 0.6f, 0, 0) }, r * 0.28f, 10);     // exhaust feed
			B.Tube(SMetal, { TP + FVector(r * 1.3f, 0, 0), FVector(Org.X + len * 0.6f, Org.Y + sgn * bore * 2.2f, Org.Z - bore * 1.1f), FVector(BodyFront() - 42.f, sgn * M.W * 0.26f, gc + 30.f) }, r * 0.30f, 10);   // charge pipe
		}
		const float icw = M.W * (0.16f + 0.08f * tb);
		B.Box(SMetal, FVector(BodyFront() - 38.f, 0, gc + 28.f), FVector(5.f + tb * 1.5f, icw, 14.f));
	}
	B.Build(Engine, Mats, false);
}
