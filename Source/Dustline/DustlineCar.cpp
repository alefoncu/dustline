#include "DustlineCar.h"
#include "DustlineMeshBuilder.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "DustlineCarParts.h"

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
// model index -> Kenney body: G=suv, Panda=hatchback, Defender=van, Buggy=kart, Pickup=truck, Crawler=tractor
static const int32 GKenIdx[] = { 0, 2, 6, 5, 3, 4 };


static const FDustlineModel GModels[] = {
	{ TEXT("Clase G"),          0, 415, 188, 198, 289, 2450, 4000, 1.0f, 0.40f, 0.285f, 0.55f, 1.1f, 1, 0, { FLinearColor(0.50f,0.44f,0.31f), FLinearColor(0.28f,0.29f,0.31f), FLinearColor(0.03f,0.17f,0.07f) } },
	{ TEXT("Panda 4x4"),        2, 368, 164, 158, 230,  980,  900, 0.0f, 0.32f, 0.185f, 0.42f, 0.8f, 1, 0, { FLinearColor(0.60f,0.60f,0.58f), FLinearColor(0.50f,0.025f,0.025f), FLinearColor(0.03f,0.15f,0.45f) } },
	{ TEXT("Defender cl\u00E1sico"), 3, 420, 179, 190, 279, 1850, 2500, 0.6f, 0.42f, 0.235f, 0.62f, 0.9f, 1, 0, { FLinearColor(0.15f,0.25f,0.09f), FLinearColor(0.60f,0.55f,0.40f), FLinearColor(0.32f,0.03f,0.02f) } },
	{ TEXT("Buggy"),            4, 335, 185, 172, 255,  780, 1600, 1.0f, 0.40f, 0.270f, 0.80f, 0.85f, 1, 0, { FLinearColor(0.95f,0.68f,0.01f), FLinearColor(0.55f,0.45f,0.30f), FLinearColor(0.75f,0.03f,0.02f) } },
	{ TEXT("Pick-up 4x4"),      5, 530, 186, 185, 310, 2050, 2800, 1.0f, 0.44f, 0.265f, 0.60f, 1.0f, 1, 0, { FLinearColor(0.05f,0.17f,0.52f), FLinearColor(0.60f,0.60f,0.60f), FLinearColor(0.30f,0.02f,0.02f) } },
	{ TEXT("Rock Crawler"),     6, 390, 190, 190, 250, 1500, 3500, 0.0f, 0.62f, 0.400f, 0.90f, 0.7f, 2, 0, { FLinearColor(0.70f,0.05f,0.01f), FLinearColor(0.70f,0.60f,0.02f), FLinearColor(0.05f,0.05f,0.06f) } },
};

// Carácter de conducción por estilo (0 Clase G, 2 Panda, 3 Defender, 4 Buggy, 5 Pick-up, 6 Crawler)
//                                    FrontW SteerMax Rate  Sens    Damp  Arb   Rc     Split CenterK AxleK  Lock   Alpha Brake YawI  Esc
static const ADustlineCar::FHandling GHand[7] = {
	/* 0 Clase G: pesado, estable, 4Matic 40/60, frenos fuertes */ { 0.52f, 0.60f, 6.0f, 0.045f, 0.42f, 0.55f, 0.35f, 0.40f, 60.f,  30.f,  false, 1.00f, 1.05f, 1.10f, true },
	/* 1 (sin uso) */                                               { 0.50f, 0.60f, 6.0f, 0.045f, 0.40f, 0.50f, 0.35f, 0.50f, 40.f,  20.f,  false, 1.00f, 1.00f, 1.00f, true },
	/* 2 Panda 4x4: ligero, ágil, morro pesado, viscoacoplador */  { 0.60f, 0.66f, 8.0f, 0.040f, 0.36f, 0.45f, 0.20f, 0.70f, 25.f,  12.f,  false, 0.92f, 0.90f, 0.90f, false },
	/* 3 Defender clásico: blando, lento de dirección, balancea */ { 0.50f, 0.55f, 4.5f, 0.050f, 0.30f, 0.32f, 0.42f, 0.50f, 20.f,  8.f,   false, 1.12f, 0.82f, 1.20f, false },
	/* 4 Buggy: motor detrás, nervioso, autoblocante trasero */    { 0.40f, 0.68f, 9.5f, 0.035f, 0.50f, 0.70f, 0.25f, 0.30f, 40.f,  90.f,  false, 0.88f, 1.10f, 0.85f, false },
	/* 5 Pick-up: caja vacía (detrás ligero), 4x4 conectable */    { 0.58f, 0.56f, 5.0f, 0.050f, 0.38f, 0.45f, 0.40f, 0.50f, -1.f,  15.f,  false, 1.05f, 0.92f, 1.35f, true },
	/* 6 Rock Crawler: bloqueos de serie, lento y muy articulado */ { 0.50f, 0.72f, 4.0f, 0.060f, 0.55f, 0.35f, 0.40f, 0.50f, -1.f,  40.f,  false, 1.10f, 1.00f, 1.00f, false },
};
const ADustlineCar::FHandling& ADustlineCar::HandlingFor(int32 Style) { return GHand[FMath::Clamp(Style, 0, 6)]; }

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

float ADustlineCar::BodyH() const
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const float stock = 0.40f * M.Radius * 100.f + 0.28f * M.Travel * 100.f;
	return M.H + FMath::Max(0.f, GroundClearance() - stock);
}
float ADustlineCar::GroundClearance() const { if (bKen) return KBodyZ + 22.f * KSz; return 0.40f * Setup.TireRadiusM * 100.f + 0.28f * Setup.TravelM * 100.f; }
float ADustlineCar::FrontX() const { return bKen ? GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, (int32)UE_ARRAY_COUNT(GKenIdx) - 1)]].FrontY * KSy : GetModel(Setup.Model).Wheelbase * 0.5f; }
float ADustlineCar::RearX() const { return bKen ? GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, (int32)UE_ARRAY_COUNT(GKenIdx) - 1)]].BackY * KSy : -GetModel(Setup.Model).Wheelbase * 0.5f; }
float ADustlineCar::BodyFront() const { if (!bKen) return GetModel(Setup.Model).L * 0.5f + DustlineBodyXOff(GetModel(Setup.Model).Style); const FKen& K = GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, (int32)UE_ARRAY_COUNT(GKenIdx) - 1)]]; return (K.OY + K.Ey) * KSy; }
void ADustlineCar::ComputeKenney()
{
	bKen = false;
	if (!bUseKenney) return;
	const FKen& K = GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, (int32)UE_ARRAY_COUNT(GKenIdx) - 1)]];
	if (!LoadObject<UStaticMesh>(nullptr, K.Mesh)) return;
	const FDustlineModel& M = GetModel(Setup.Model);
	const float R = Setup.TireRadiusM * 100.f, Tcm = Setup.TravelM * 100.f;
	const float lift = FMath::Max(0.f, Tcm * 0.30f - 10.f), extra = FMath::Max(0.f, R - 42.f);
	KSx = M.W * 0.5f / K.Ex; KSy = M.L * 0.5f / K.Ey;
	const float R0 = M.Radius * 100.f, lift0 = FMath::Max(0.f, M.Travel * 100.f * 0.30f - 10.f), extra0 = FMath::Max(0.f, R0 - 42.f);
	KSz = FMath::Max(0.5f, (M.H - lift0 - R0 - extra0) / FMath::Max(10.f, K.Top - K.WheelZ));
	KBodyZ = lift + R - K.WheelZ * KSz + extra;
	bKen = true;
}
FVector ADustlineCar::WheelPos(int32 I) const
{
	const FDustlineModel& M = GetModel(Setup.Model);
	if (bKen) { const FKen& K = GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, (int32)UE_ARRAY_COUNT(GKenIdx) - 1)]]; return FVector((I < 2 ? K.FrontY : K.BackY) * KSy, ((I % 2) == 0 ? -1.f : 1.f) * K.ArchX * KSx, Setup.TireRadiusM * 100.f); }
	const float x = (I < 2 ? 1.f : -1.f) * M.Wheelbase * 0.5f;
	// neumáticos grandes: se ensancha la vía (ruedas hacia fuera) en vez de estrechar la carrocería
	const float Wd = Setup.TireWidthM * 100.f, Rc = Setup.TireRadiusM * 100.f, Tw = FMath::SmoothStep(40.f, 50.f, Rc);
	const float yStock = M.W * 0.5f - Wd * 0.5f + 6.f, yWide = M.W * 0.5f + 2.f + Wd * 0.5f;
	const float y = ((I % 2) == 0 ? -1.f : 1.f) * FMath::Lerp(yStock, FMath::Max(yStock, yWide), Tw);
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
	DirtShown = DirtHShown = -1.f;   // materiales nuevos: hay que volver a poner el barro
	const TCHAR* G = TEXT("/Game/Cars/M_CarGeneric.M_CarGeneric");
	const float Fade = bXRay ? 0.12f : 1.f;
	auto Set = [&](int32 id, UMaterialInstanceDynamic* M) { if (M) Mats.Add(id, M); };
	UMaterialInstanceDynamic* Paint = MakeMat(G, Setup.Paint, 0.10f, 0.30f); if (Paint) Paint->SetScalarParameterValue(TEXT("Fade"), Fade);
	UMaterialInstanceDynamic* Glass = MakeMat(G, FLinearColor(0.006f, 0.008f, 0.011f), 0.0f, 0.07f); if (Glass) Glass->SetScalarParameterValue(TEXT("Fade"), bXRay ? 0.08f : 1.f);
	UMaterialInstanceDynamic* Dark = MakeMat(G, FLinearColor(0.014f, 0.014f, 0.016f), 0.15f, 0.62f); if (Dark && bXRay) Dark->SetScalarParameterValue(TEXT("Fade"), 0.3f);
	Set(SPaint, Paint); Set(SGlass, Glass); Set(SDark, Dark);
	if (UMaterialInstanceDynamic* Mt = MakeMat(G, FLinearColor(0.30f, 0.30f, 0.32f), 1.0f, 0.34f)) { Mt->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.07f : 0.f); Set(SMetal, Mt); }
	Set(SRubber, MakeMat(G, FLinearColor(0.010f, 0.010f, 0.011f), 0.0f, 0.90f));
	Set(SRim, MakeMat(G, FLinearColor(0.62f, 0.63f, 0.66f), 1.0f, 0.22f));
	if (UMaterialInstanceDynamic* Bk = MakeMat(G, FLinearColor(0.23f, 0.24f, 0.26f), 0.85f, 0.42f)) { Bk->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.10f : 0.f); Set(SBlock, Bk); }
	if (UMaterialInstanceDynamic* Tb = MakeMat(G, FLinearColor(0.75f, 0.30f, 0.05f), 1.0f, 0.30f)) { Tb->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.45f : 0.06f); Set(STurbo, Tb); }
	if (UMaterialInstanceDynamic* Ac = MakeMat(G, FLinearColor(0.62f, 0.02f, 0.02f), 0.35f, 0.32f)) { Ac->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.12f : 0.f); Set(SAccent, Ac); }
	if (UMaterialInstanceDynamic* L = MakeMat(G, FLinearColor(1.f, 0.95f, 0.8f), 0.f, 0.08f)) { L->SetScalarParameterValue(TEXT("Emissive"), 5.f); Set(SLight, L); }
	if (UMaterialInstanceDynamic* T = MakeMat(G, FLinearColor(0.9f, 0.02f, 0.02f), 0.f, 0.1f)) { T->SetScalarParameterValue(TEXT("Emissive"), 3.f); Set(STail, T); }
	Set(SChrome, MakeMat(G, FLinearColor(0.85f, 0.86f, 0.9f), 1.0f, 0.10f));
	if (UMaterialInstanceDynamic* Al = MakeMat(G, FLinearColor(0.58f, 0.60f, 0.64f), 1.0f, 0.38f)) { Al->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.08f : 0.f); Set(SAlu, Al); }
	if (UMaterialInstanceDynamic* Hs = MakeMat(G, FLinearColor(0.02f, 0.10f, 0.55f), 0.0f, 0.35f)) { Hs->SetScalarParameterValue(TEXT("Emissive"), bXRay ? 0.15f : 0.f); Set(SHose, Hs); }
	if (UMaterialInstanceDynamic* In = MakeMat(G, FLinearColor(0.10f, 0.09f, 0.085f), 0.0f, 0.85f)) { if (bXRay) In->SetScalarParameterValue(TEXT("Fade"), 0.3f); Set(SInterior, In); }
	Set(SCarbon, MakeMat(G, FLinearColor(0.025f, 0.025f, 0.03f), 0.4f, 0.30f));
	if (UMaterialInstanceDynamic* Rf = MakeMat(G, FLinearColor(0.80f, 0.80f, 0.78f), 0.1f, 0.36f)) { Rf->SetScalarParameterValue(TEXT("Fade"), Fade); Set(SRoof, Rf); }
	if (UMaterialInstanceDynamic* Am = MakeMat(G, FLinearColor(1.f, 0.42f, 0.02f), 0.f, 0.12f)) { Am->SetScalarParameterValue(TEXT("Emissive"), 1.2f); Set(SAmber, Am); }
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
		// centro de masas realista (bajo: chasis, motor y ejes) en vez del centro geométrico de la carrocería
		ZCom = gc + 0.31f * (BodyH() - gc) + (Setup.bRack ? 3.f : 0.f);   // la baca sube un poco el centro de masas
		ComX = (HandlingFor(M.Style).FrontW - 0.5f) * (FrontX() - RearX()) / 100.f;   // centro de masas desplazado hacia el lado pesado
		Visual->SetRelativeLocation(FVector(-ComX * 100.f, 0, -ZCom));
		// la caja de barrido (árboles/rocas) cubre la parte media; el suelo lo tocan los puntos de contacto de la carrocería
		Collision->SetBoxExtent(FVector(M.L * 0.46f, M.W * 0.47f, FMath::Clamp((ZCom - gc) * 0.75f, 18.f, (BodyH() - gc) * 0.45f)));
		ComputeSpec();
	}
}


void ADustlineCar::BuildBodyKen()
{
	const FKen& K = GKen[GKenIdx[FMath::Clamp(Setup.Model, 0, (int32)UE_ARRAY_COUNT(GKenIdx) - 1)]];
	Body->ClearAllMeshSections();
	KBody->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, K.Mesh));
	KBody->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	KBody->SetRelativeScale3D(FVector(KSx, KSy, KSz));
	KBody->SetRelativeLocation(FVector(0, 0, KBodyZ));
}
