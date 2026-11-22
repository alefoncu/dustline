#include "DustlineCar.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "DustlineMenu.h"
#include "DustlineSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogDustlineDrive, Log, All);

float ADustlineCar::GWet = 0.f;
float ADustlineCar::GSnow = 0.f;

namespace
{
	enum EKind { KGrass, KDirt, KSand, KMud, KSnow, KWater, KRock };
	float SmoothStep(float a, float b, float x) { const float t = FMath::Clamp((x - a) / (b - a), 0.f, 1.f); return t * t * (3 - 2 * t); }
	const TCHAR* KindName(int32 k) { static const TCHAR* N[] = { TEXT("Hierba"), TEXT("Tierra"), TEXT("Arena"), TEXT("Barro"), TEXT("Nieve"), TEXT("Agua"), TEXT("Roca") }; return N[k]; }

	void SurfaceAt(float ZCm, float Nz, float Wet, float Snow, float& Mu, float& Drag, int32& Kind)
	{
		if (ZCm < -5.f) { Mu = 0.5f; Drag = 380.f; Kind = KWater; return; }
		const float snow = FMath::Max(SmoothStep(26000.f, 30000.f, ZCm), Snow * SmoothStep(0.6f, 0.85f, Nz));
		const float mud = Wet * FMath::Clamp((3000.f - ZCm) / 1800.f, 0.f, 1.f) * 0.85f;
		if (snow > 0.5f) { Mu = FMath::Lerp(0.8f, 0.4f, snow); Drag = 70.f + 60.f * snow; Kind = KSnow; return; }
		if (mud > 0.45f) { Mu = FMath::Lerp(0.9f, 0.5f, mud); Drag = 90.f + 260.f * mud; Kind = KMud; return; }
		if (ZCm < 300.f) { Mu = 0.75f; Drag = 120.f; Kind = KSand; return; }
		if (Nz < 0.72f) { Mu = 1.1f; Drag = 35.f; Kind = KRock; return; }
		Mu = 1.0f * (1.f - 0.3f * Wet); Drag = 55.f; Kind = KGrass;
	}

	// tyre type (0 road,1 AT,2 mud,3 studded,4 balloon) response: (grip mul, drag mul)
	void TireResp(int32 T, int32 K, float& Mu, float& Dr)
	{
		Mu = 1.f; Dr = 1.f;
		switch (T)
		{
		case 0: { const float m[7] = { .85f, .8f, .6f, .4f, .5f, .6f, 1.f }, d[7] = { 1, 1, 1.3f, 1.5f, 1.1f, 1, 1 }; Mu = m[K]; Dr = d[K]; break; }
		case 1: { const float m[7] = { 1.f, 1.f, .9f, .8f, .8f, 1.f, 1.f }; Mu = m[K]; break; }
		case 2: { const float m[7] = { .95f, 1.1f, 1.f, 1.5f, .9f, 1.1f, .9f }, d[7] = { 1.05f, 1.05f, 1, .6f, 1, 1, 1 }; Mu = m[K]; Dr = d[K]; break; }
		case 3: { const float m[7] = { 1.f, 1.05f, .85f, 1.f, 1.9f, 1.f, 1.05f }, d[7] = { 1, 1, 1, 1, .9f, 1, 1 }; Mu = m[K]; Dr = d[K]; break; }
		default: { const float m[7] = { .9f, .9f, 1.5f, 1.1f, 1.2f, 1.f, .8f }, d[7] = { .95f, .95f, .6f, .8f, .8f, 1, 1 }; Mu = m[K]; Dr = d[K]; break; }
		}
	}
}

void ADustlineCar::ComputeSpec()
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const float R = Setup.TireRadiusM, W = Setup.TireWidthM;
	const float wheelMass = R * R * W * 520.f, stock = M.Radius * M.Radius * M.Width * 520.f;
	Sp.Mass = FMath::Max(450.f, M.Mass + (Setup.EngineCC - M.CC) * 0.065f + (Setup.Turbo - M.Turbo) * 30.f + (wheelMass - stock) + Setup.Ballast + (Setup.TravelM - M.Travel) * 60.f);
	Sp.HP = Setup.EngineCC * 0.065f;
	const float peak = Sp.HP * (1.f + 0.5f * Setup.Turbo);
	Sp.R = R; Sp.Width = W;
	Sp.FMax = peak * 38.f * (0.46f / R);
	Sp.VMaxGear = 34.f * (R / 0.46f) * (1.f + 0.12f * Setup.Turbo) * (M.Style == 2 ? 0.8f : 1.f);
	Sp.SpringK = 30000.f * (Sp.Mass / 1500.f) * Setup.Stiffness;
	Sp.SpringC = 0.42f * 2.f * FMath::Sqrt(Sp.SpringK * Sp.Mass / 4.f);
	Sp.RestLen = Setup.TravelM;
	const float L = M.L / 100.f, Wd = M.W / 100.f, H = M.H / 100.f;
	Sp.Inertia[0] = Sp.Mass / 12.f * (H * H + L * L) * 1.1f; Sp.Inertia[1] = Sp.Mass / 12.f * (Wd * Wd + L * L) * 1.1f; Sp.Inertia[2] = Sp.Mass / 12.f * (Wd * Wd + H * H) * 1.3f;
	Sp.Cd = M.Style == 1 ? 0.95f : (M.Style == 4 ? 0.7f : 1.1f);
}

void ADustlineCar::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* Cmd = FCommandLine::Get();
	int32 Model = 0;
	if (FParse::Value(Cmd, TEXT("dmodel="), Model)) { Model = FMath::Clamp(Model, 0, NumModels() - 1); Setup = StockSetup(Model); if (!FParse::Param(Cmd, TEXT("stock"))) LoadSetupFromDisk(Model, Setup); Setup.Model = Model; }
	else { FString Last; if (FFileHelper::LoadFileToString(Last, *(FPaths::ProjectSavedDir() / TEXT("Garage") / TEXT("last_model.txt")))) { Model = FMath::Clamp(FCString::Atoi(*Last), 0, NumModels() - 1); Setup = StockSetup(Model); LoadSetupFromDisk(Model, Setup); Setup.Model = Model; } }
	if (FParse::Param(Cmd, TEXT("autodrive"))) bAutoDrive = true;
	FString W; if (FParse::Value(Cmd, TEXT("dweather="), W)) { if (W == TEXT("rain")) { GWet = 1.f; } else if (W == TEXT("snow")) { GSnow = 1.f; } }
	Rebuild();
	// drop onto the ground at the spawn point
	FHitResult Hit; const FVector L0 = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, L0 + FVector(0, 0, 3000), L0 - FVector(0, 0, 6000), ECC_Visibility))
		SetActorLocation(FVector(L0.X, L0.Y, Hit.ImpactPoint.Z + ZCom + 25.f), false, nullptr, ETeleportType::TeleportPhysics);
	Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector;
	UE_LOG(LogDustlineDrive, Display, TEXT("DRIVE_START model=%d mass=%.0f hp=%.0f fmax=%.0f vgear=%.1f autodrive=%d loc=%s"), Setup.Model, Sp.Mass, Sp.HP, Sp.FMax, Sp.VMaxGear, bAutoDrive ? 1 : 0, *GetActorLocation().ToString());
}

void ADustlineCar::ResetUpright()
{
	FRotator R = GetActorRotation(); R.Pitch = 0; R.Roll = 0;
	FHitResult Hit; const FVector L0 = GetActorLocation();
	float Z = L0.Z; if (GetWorld()->LineTraceSingleByChannel(Hit, L0 + FVector(0, 0, 3000), L0 - FVector(0, 0, 6000), ECC_Visibility)) Z = Hit.ImpactPoint.Z + ZCom + 80.f;
	SetActorLocationAndRotation(FVector(L0.X, L0.Y, Z), R, false, nullptr, ETeleportType::TeleportPhysics);
	Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector;
}

void ADustlineCar::StepPhysics(float dt, float Throttle, float Steer, bool bHandbrake, bool bBoost)
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const double mass = Sp.Mass;
	FQuat Q = GetActorQuat();
	FVector P = GetActorLocation() / 100.0;
	const FVector Up(0, 0, 1);
	const FVector Fwd = Q.RotateVector(FVector(1, 0, 0));
	const double vLong = FVector::DotProduct(Vel, Fwd);
	const double maxSteer = 0.58 / (1.0 + FMath::Abs(vLong) * 0.05);
	SteerAngle += (Steer * maxSteer - SteerAngle) * FMath::Min(1.0, dt * 7.0);

	float thr = Throttle; bool braking = false;
	if (thr * vLong < -1.5) { braking = true; thr = 0.f; }
	if (Throttle == 0.f && bHandbrake == false && false) {}
	const float want = (thr > 0 || bBoost) ? 1.f : 0.f;
	TurboSpool = FMath::Clamp(TurboSpool + (want > TurboSpool ? 0.9f : -1.6f) * dt, 0.f, 1.f);
	const double hpNow = Sp.HP * (1.0 + 0.5 * Setup.Turbo * TurboSpool) * (bBoost ? 1.25 : 1.0);
	const double powerW = hpNow * 745.7;
	const double widthFactor = FMath::Clamp(0.85 + (Sp.Width - 0.2) * 2.0, 0.8, 1.3), dragWidth = FMath::Clamp(0.25 / Sp.Width, 0.7, 1.3);

	FVector F = FVector::ZeroVector, T = FVector::ZeroVector;
	int32 grounded = 0; int32 bestKind = KGrass; float bestDrag = -1.f;
	const double zA = Sp.R + 0.55 * Sp.RestLen;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DustlineWheel), false, this);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i) / 100.0;
		const bool front = i < 2;
		const FVector rLocal(WP.X, WP.Y, zA - ZCom / 100.0);
		const FVector r = Q.RotateVector(rLocal);
		const FVector a = P + r;
		const FVector Start = a * 100.0 + FVector(0, 0, 60), End = Start - FVector(0, 0, (Sp.RestLen + Sp.R) * 100.0 + 160.0);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) { Wh[i].Contact = false; Wh[i].Length = FMath::FInterpTo(Wh[i].Length, Sp.RestLen, dt, 8.f); continue; }
		const FVector n = Hit.ImpactNormal.GetSafeNormal();
		const double dist = ((Hit.Distance - 60.0) / 100.0) * n.Z;
		const double L = dist - Sp.R;
		Wh[i].Length = FMath::Clamp(L, 0.1, (double)Sp.RestLen);
		if (L >= Sp.RestLen) { Wh[i].Contact = false; continue; }
		Wh[i].Contact = true; ++grounded;

		float mu, drag; int32 kind; SurfaceAt(Hit.ImpactPoint.Z, n.Z, GWet, GSnow, mu, drag, kind);
		float tm, td; TireResp(Setup.TireType, kind, tm, td);
		const bool soft = kind == KMud || kind == KSand || kind == KSnow || kind == KWater;
		mu *= tm * (soft ? widthFactor : 1.0); drag *= td * (soft ? dragWidth : 1.0) * FMath::Sqrt(0.46f / Sp.R);
		if (drag > bestDrag) { bestDrag = drag; bestKind = kind; }

		const double comp = Sp.RestLen - L;
		const FVector vp = Vel + FVector::CrossProduct(AngVel, r);
		const double vn = FVector::DotProduct(vp, n);
		double fs = Sp.SpringK * comp - Sp.SpringC * vn;
		if (L < 0.1) fs += 6.0 * Sp.SpringK * (0.1 - L);
		fs = FMath::Max(fs, 0.0);
		const FVector Fs = n * fs; F += Fs; T += FVector::CrossProduct(r, Fs);

		FVector wf = Fwd;
		if (front) wf = Q.RotateVector(FVector(FMath::Cos(SteerAngle), -FMath::Sin(SteerAngle), 0));
		wf = (wf - n * FVector::DotProduct(wf, n)).GetSafeNormal();
		const FVector lat = FVector::CrossProduct(n, wf);
		const double vl = FVector::DotProduct(vp, wf), vs = FVector::DotProduct(vp, lat);
		const double N = fs, muC = FMath::Max(mu, 0.05f);
		const bool rearHB = bHandbrake && !front;
		const double fyMu = rearHB ? muC * 0.4 : muC;
		const double eff = mass / 4.0;
		double share = 0.0;
		switch (Setup.Drive) { case 0: share = 0.25; break; case 1: share = front ? 0.0 : 0.5; break; default: share = front ? 0.5 : 0.0; }
		double fx = 0.0;
		if (thr != 0.f && share > 0.0)
		{
			const double maxV = Sp.VMaxGear * (thr > 0 ? 1.0 : 0.32) * (bBoost ? 1.15 : 1.0);
			const double along = vl * (thr > 0 ? 1.0 : -1.0);
			const double ratio = along > 0 ? FMath::Max(0.0, 1.0 - FMath::Pow(along / maxV, 4.0)) : 1.0;
			const double fPower = powerW / FMath::Max(FMath::Abs(vl), 2.5);
			const double fBase = FMath::Min((double)Sp.FMax * (1.0 + 0.5 * Setup.Turbo * TurboSpool), fPower);
			fx = thr * fBase * share * ratio;
		}
		else if (thr == 0.f && !braking) fx = -vl * (FMath::Abs(vl) < 2.0 ? eff * 14.0 : eff * 0.25);
		if (braking || rearHB) { const double maxB = ((rearHB && !braking) ? 1.0 : 0.9) * muC * N; fx += -FMath::Clamp(vl * eff * 12.0, -maxB, maxB); }
		fx += -FMath::Tanh(vl * 2.0) * (drag * 0.0006) * N;
		if (kind == KWater) fx += -vl * 450.0;
		if (kind == KMud) fx += -vl * 80.0;
		double fy = FMath::Clamp(-vs * eff * 14.0, -fyMu * N, fyMu * N);
		const double maxF = muC * N * 1.2;
		fx = FMath::Clamp(fx, -maxF, maxF);
		const double mag = FMath::Sqrt(fx * fx + fy * fy); if (mag > maxF) { const double s = maxF / mag; fx *= s; fy *= s; }
		const FVector Ft = wf * fx + lat * fy;
		F += Ft; T += FVector::CrossProduct(FVector(r.X, r.Y, 0), Ft);
		Wh[i].Spin += (float)(vl / Sp.R) * dt;
	}
	// aero + water
	F += -Vel * Vel.Size() * 0.6 * Sp.Cd;
	const double depth = FMath::Clamp(0.0 - (P.Z - 0.6) + (double)GetActorLocation().Z * 0.0, 0.0, 1.6);
	FHitResult G; double groundZ = -1e9;
	if (GetWorld()->LineTraceSingleByChannel(G, GetActorLocation() + FVector(0, 0, 100), GetActorLocation() - FVector(0, 0, 5000), ECC_Visibility, Params)) groundZ = G.ImpactPoint.Z / 100.0;
	const double wdepth = FMath::Clamp(0.0 - ((P.Z - ZCom / 100.0) - 0.0) + 0.0, 0.0, 1.6) * (groundZ < 0.0 ? 1.0 : 0.0);
	(void)depth;
	if (wdepth > 0) { F += -Vel * 700.0 * (mass / 1500.0) * wdepth; F += FVector(0, 0, 3500.0 * (mass / 1500.0) * wdepth); }
	F += FVector(0, 0, -9.81 * mass);
	Vel += F / mass * dt;
	T += -AngVel * 1800.0 * (mass / 1500.0);
	const FQuat Qi = Q.Inverse();
	const FVector Tl = Qi.RotateVector(T); FVector wl = Qi.RotateVector(AngVel);
	wl += FVector(Tl.X / Sp.Inertia[2], Tl.Y / Sp.Inertia[1], Tl.Z / Sp.Inertia[1]) * dt;   // roll about X, pitch about Y, yaw about Z
	AngVel = Q.RotateVector(wl);
	P += Vel * dt;
	const double wlen = AngVel.Size();
	if (wlen > 1e-5) Q = (FQuat(AngVel / wlen, wlen * dt) * Q).GetNormalized();
	SurfaceName = KindName(bestKind);
	// move with sweep so trees / rocks block the body
	FHitResult Blocked;
	SetActorLocationAndRotation(P * 100.0, Q, true, &Blocked, ETeleportType::None);
	if (Blocked.bBlockingHit)
	{
		const FVector n = Blocked.ImpactNormal; const double vn = FVector::DotProduct(Vel, n);
		if (vn < 0) Vel -= n * vn * 1.25;
	}
}

void ADustlineCar::UpdateVisuals(float Throttle)
{
	const double zA = (Sp.R + 0.55 * Sp.RestLen) * 100.0;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i);
		const float yaw = (i < 2) ? -SteerAngle : 0.f;
		const FQuat Rq = FQuat(FVector(0, 0, 1), yaw) * FQuat(FVector(0, 1, 0), Wh[i].Spin);
		Wheels[i]->SetRelativeLocationAndRotation(FVector(WP.X, WP.Y, zA - Wh[i].Length * 100.0), Rq);
	}
	(void)Throttle;
}

void ADustlineCar::UpdateCamera(float dt)
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const FVector f = GetActorForwardVector();
	const float yaw = FMath::Atan2(f.Y, f.X);
	float d = yaw - CamYaw; while (d > PI) d -= 2 * PI; while (d < -PI) d += 2 * PI;
	CamYaw += d * (1.f - FMath::Exp(-3.5f * dt));
	const float dist = M.L * 0.9f + 360.f + CamMode * 300.f, h = M.H + 130.f + CamMode * 80.f;
	const FVector Loc = GetActorLocation() - FVector(0, 0, ZCom) + FVector(0, 0, M.H * 0.5f);
	FVector CamPos = Loc - FVector(FMath::Cos(CamYaw), FMath::Sin(CamYaw), 0) * dist + FVector(0, 0, h);
	FHitResult H; if (GetWorld()->LineTraceSingleByChannel(H, Loc + FVector(0, 0, 150), CamPos, ECC_Visibility)) CamPos = H.ImpactPoint + (Loc - CamPos).GetSafeNormal() * 30.f;
	Cam->SetWorldLocationAndRotation(CamPos, (Loc + FVector(0, 0, 40) - CamPos).Rotation());
}

void ADustlineCar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Sp.Mass <= 0.f) return;
	FString Shot0;
	const float dt = FMath::Min(DeltaTime, 0.05f);
	Clock += dt;
	float Throttle = 0.f, Steer = 0.f; bool Hand = false, Boost = false;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (bAutoDrive) { Throttle = 1.f; Steer = 0.55f * FMath::Sin(Clock * 0.45f); Boost = Clock > 3.f; }
	else if (PC && !bPaused && !bShowcase && !bGarage && !(Menu.IsValid() && Menu->IsOpen()))
	{
		Throttle = (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) ? 1.f : 0.f);
		Steer = (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right) ? 1.f : 0.f);
		Hand = PC->IsInputKeyDown(EKeys::SpaceBar); Boost = PC->IsInputKeyDown(EKeys::LeftShift);
		if (PC->WasInputKeyJustPressed(EKeys::R)) ResetUpright();
		if (PC->WasInputKeyJustPressed(EKeys::C)) CamMode = (CamMode + 1) % 3;
		if (PC->WasInputKeyJustPressed(EKeys::One)) { GWet = 0; GSnow = 0; }
		if (PC->WasInputKeyJustPressed(EKeys::Two)) { GWet = 1; GSnow = 0; }
		if (PC->WasInputKeyJustPressed(EKeys::Three)) { GWet = 0; GSnow = 1; }
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("garagedemo")))
	{
		static int32 Step = 0;
		auto Shot = [&](int32 n) { FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("/Volumes/SSD/UE-Cache/gd_%d.png"), n), true, false); };
		if (Step == 0 && Clock > 4.f) { ToggleGarage(); Step = 1; }
		else if (Step == 1 && Clock > 11.f) { Shot(1); Step = 2; }
		else if (Step == 2 && Clock > 12.f) { SelectModel(2); Setup.EngineCC = 7000.f; Setup.Turbo = 3.f; GarageFocus = 1; SetXRay(true); bSetupDirty = true; Step = 3; }
		else if (Step == 3 && Clock > 20.f) { Shot(2); Step = 4; }
		else if (Step == 4 && Clock > 21.f) { Setup.TireType = 3; Setup.TireRadiusM = 0.52f; Setup.TireWidthM = 0.35f; GarageFocus = 2; SetXRay(false); bSetupDirty = true; Step = 5; }
		else if (Step == 5 && Clock > 28.f) { Shot(3); Step = 6; }
		else if (Step == 6 && Clock > 31.f) { FPlatformMisc::RequestExit(false); }
	}
	Cam->FieldOfView = FDustlineSettings::Get().Fov;
	const bool bDemoRun = bAutoDrive || FParse::Param(FCommandLine::Get(), TEXT("garagedemo")) || FParse::Param(FCommandLine::Get(), TEXT("nomenu")) || FParse::Value(FCommandLine::Get(), TEXT("dshot="), Shot0);
	if (!bMenuStarted && Clock > 0.3f && PC) { bMenuStarted = true; Menu = MakeShared<FDustlineMenu>(this); Menu->Start(!bDemoRun); }
	if (PC && Menu.IsValid())
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape)) Menu->OnEscape();
		else if (PC->WasInputKeyJustPressed(EKeys::G) && !Menu->IsOpen()) ToggleGarage();
	}
	else if (PC && PC->WasInputKeyJustPressed(EKeys::G)) ToggleGarage();
	if (FParse::Param(FCommandLine::Get(), TEXT("uidemo"))) UiDemo();
	if (bSetupDirty && Clock - LastRebuild > 0.04f) { bSetupDirty = false; LastRebuild = Clock; Rebuild(); }
	if (bGarage || bShowcase)
	{
		Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector;
		for (int32 i = 0; i < 4; ++i) Wh[i].Length = FMath::FInterpTo(Wh[i].Length, Sp.RestLen * 0.55f, dt, 6.f);
		UpdateVisuals(0.f); UpdateGarageCamera(dt);
	}
	else if (bPaused)
	{
		UpdateVisuals(0.f); UpdateCamera(dt);
	}
	else
	{
		Accum += dt;
		const float h = 1.f / 120.f;
		while (Accum >= h) { StepPhysics(h, Throttle, Steer, Hand, Boost); Accum -= h; }
		UpdateVisuals(Throttle);
		UpdateCamera(dt);
	}
	SpeedKmh = Vel.Size() * 3.6f;
	LogT += dt; FpsAcc += DeltaTime; ++FpsN;
	if (LogT > 2.f)
	{
		LogT = 0.f;
		UE_LOG(LogDustlineDrive, Display, TEXT("DRIVE t=%.1f speed=%.0f km/h loc=%s surf=%s grounded=%d%d%d%d up.z=%.2f fps=%.0f"), Clock, SpeedKmh, *GetActorLocation().ToString(), *SurfaceName,
			Wh[0].Contact, Wh[1].Contact, Wh[2].Contact, Wh[3].Contact, GetActorUpVector().Z, FpsN / FMath::Max(FpsAcc, 0.001f));
		FpsAcc = 0.f; FpsN = 0;
	}
	FString Shot; if (FParse::Value(FCommandLine::Get(), TEXT("dshot="), Shot) && Clock > 22.f && GEngine)
	{
		static bool bDone = false; static float T0 = 0.f;
		if (!bDone) { FScreenshotRequest::RequestScreenshot(Shot, true, false); bDone = true; T0 = Clock; }
		else if (Clock - T0 > 6.f) FPlatformMisc::RequestExit(false);
	}
}

