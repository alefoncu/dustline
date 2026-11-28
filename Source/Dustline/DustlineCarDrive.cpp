#include "DustlineCar.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "DustlineMenu.h"
#include "DustlineSettings.h"
#include "DustlineWorld.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogDustlineDrive, Log, All);
using namespace DustlineWorld;

/*
 * Física de conducción (SI, 240 Hz):
 *  - Suspensión por rayo a lo largo del eje vertical de la carrocería, muelle + amortiguador + barra estabilizadora.
 *  - Neumático con curva de deslizamiento combinada (longitudinal + deriva) cuyo pico, caída y agarre dependen de la mezcla de
 *    superficies del punto (DustlineWorld::GroundAt), del tipo de neumático, de la humedad, la nieve y el hielo del clima.
 *  - Cada rueda tiene velocidad angular propia; las cuatro se resuelven a la vez de forma implícita con los acoplamientos del
 *    diferencial (abierto con algo de viscosidad, o bloqueado), así que hay patinaje, bloqueo de frenada, rueda al aire que se
 *    embala, etc. Motor con curva de par, turbo con retraso, convertidor de par, caja automática o manual y reductora.
 *  - Suelo blando: el neumático se hunde según presión y anchura, el patinaje excava, la resistencia a la rodadura sube con
 *    el hundimiento (se puede quedar atascado). Carrocería con puntos de contacto (vuelcos, panza contra el suelo).
 *  - Agua: flotación por columnas de la carrocería, arrastre del agua con la corriente del río, inundación del habitáculo y
 *    motor ahogado si la toma de aire queda bajo el agua.
 */
namespace
{
	FORCEINLINE float Smooth(float a, float b, float x) { const float t = FMath::Clamp((x - a) / (b - a), 0.f, 1.f); return t * t * (3 - 2 * t); }
	FORCEINLINE float Hash(int32 X, int32 Y) { uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u; H = (H ^ (H >> 13)) * 1274126177u; return (float)((H ^ (H >> 16)) & 0xFFFF) / 65535.f; }
	float VNoise(float X, float Y)
	{
		const int32 Ix = FMath::FloorToInt(X), Iy = FMath::FloorToInt(Y); const float Fx = X - Ix, Fy = Y - Iy;
		const float U = Fx * Fx * (3 - 2 * Fx), V = Fy * Fy * (3 - 2 * Fy);
		const float A = Hash(Ix, Iy), B = Hash(Ix + 1, Iy), C = Hash(Ix, Iy + 1), D = Hash(Ix + 1, Iy + 1);
		return A + (B - A) * U + (C - A) * V + (A - B - C + D) * U * V;
	}
	// curva normalizada del neumático: sube hasta 1 en el pico (s=1) y luego cae hacia Fall
	FORCEINLINE float Curve(float S, float Fall) { return S < 1.f ? S * (2.f - S) : Fall + (1.f - Fall) * FMath::Exp(-(S - 1.f) * 1.3f); }

	struct FTire { float Mu, Peak, Alpha, Fall; };
	void TireForce(const FTire& T, float N, float Omega, float R, float Vl, float Vs, float& Fx, float& Fy, float& Kappa)
	{
		const float Vref = FMath::Max(FMath::Abs(Vl), 1.0f);
		Kappa = (Omega * R - Vl) / Vref;
		const float TanA = Vs / Vref;
		const float Sx = Kappa / T.Peak, Sy = TanA / T.Alpha, S = FMath::Sqrt(Sx * Sx + Sy * Sy);
		if (S < 1e-6f || N <= 0.f) { Fx = Fy = 0.f; return; }
		const float F = T.Mu * N * Curve(S, T.Fall);
		Fx = F * Sx / S; Fy = -F * Sy / S;
	}
	bool Solve4(float A[4][4], float B[4], float X[4])
	{
		for (int32 c = 0; c < 4; ++c)
		{
			int32 P = c; for (int32 r = c + 1; r < 4; ++r) if (FMath::Abs(A[r][c]) > FMath::Abs(A[P][c])) P = r;
			if (FMath::Abs(A[P][c]) < 1e-9f) return false;
			if (P != c) { for (int32 k = 0; k < 4; ++k) Swap(A[c][k], A[P][k]); Swap(B[c], B[P]); }
			for (int32 r = c + 1; r < 4; ++r) { const float f = A[r][c] / A[c][c]; for (int32 k = c; k < 4; ++k) A[r][k] -= f * A[c][k]; B[r] -= f * B[c]; }
		}
		for (int32 r = 3; r >= 0; --r) { float s = B[r]; for (int32 k = r + 1; k < 4; ++k) s -= A[r][k] * X[k]; X[r] = s / A[r][r]; }
		return true;
	}
	constexpr float Rho = 1000.f, Gr = 9.81f;
}

float ADustlineCar::EngineTorque(float R) const
{
	const float x = R / Sp.Redline;
	if (Sp.bDiesel) return Sp.TPeak * (0.5f + 0.5f * Smooth(0.12f, 0.35f, x)) * (1.f - 0.45f * Smooth(0.6f, 1.0f, x));
	return Sp.TPeak * (0.55f + 0.45f * Smooth(0.12f, 0.5f, x)) * (1.f - 0.3f * Smooth(0.78f, 1.05f, x));
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
	const FHandling& Hh = HandlingFor(M.Style);
	Sp.SpringC = Hh.Damp * 2.f * FMath::Sqrt(Sp.SpringK * Sp.Mass / 4.f);
	Sp.Arb = Sp.SpringK * Hh.Arb;
	Sp.RestLen = Setup.TravelM;
	const float L = M.L / 100.f, Wd = M.W / 100.f, H = M.H / 100.f;
	Sp.Inertia[0] = Sp.Mass / 12.f * (H * H + L * L) * 1.1f; Sp.Inertia[1] = Sp.Mass / 12.f * (Wd * Wd + L * L) * 1.1f * Hh.YawI; Sp.Inertia[2] = Sp.Mass / 12.f * (Wd * Wd + H * H) * 1.3f;
	Sp.Cd = M.Style == 4 ? 0.7f : 1.1f;
	// motor
	const int32 St = M.Style;
	Sp.bDiesel = (St == 3 || St == 5);
	Sp.Redline = St == 4 ? 8200.f : (Sp.bDiesel ? 4600.f : (St == 2 ? 6500.f : 6200.f));
	Sp.Idle = Sp.bDiesel ? 780.f : 850.f;
	const float WPk = (Sp.bDiesel ? 0.75f : 0.85f) * Sp.Redline * 2.f * PI / 60.f;
	Sp.TPeak = Sp.HP * 745.7f / WPk * (Sp.bDiesel ? 1.35f : 1.15f);
	// caja: relaciones totales (caja x grupo) entre una 1ª que da ~0,75 g y una última que llega a la punta al corte
	Sp.NGears = (St == 2 || St == 4) ? 5 : 6;
	const float VTop = Sp.VMaxGear * 1.12f;
	const float RTop = Sp.Redline * 2.f * PI / 60.f * R / VTop;
	const float R1 = FMath::Max(RTop * 2.2f, 0.62f * Sp.Mass * Gr * R / (Sp.TPeak * 0.9f));
	for (int32 g = 0; g < Sp.NGears; ++g) Sp.Gears[g] = R1 * FMath::Pow(RTop / R1, (float)g / (Sp.NGears - 1));
	Sp.Final = 1.f; Sp.Reverse = R1 * 0.95f;
	Sp.bHasLow = St != 2 && St != 4; Sp.Low = St == 6 ? 3.8f : 2.7f;
	// ruedas y frenos
	const float Mw = 22.f * (R / 0.4f) * (R / 0.4f) * (W / 0.28f) + 8.f;
	Sp.WheelI = 0.6f * Mw * R * R;
	Sp.EngI = 0.15f + Setup.EngineCC * 0.00003f;
	Sp.BrakeT = 1.15f * Sp.Mass * Gr * R * Hh.Brake;          // par total de freno (reparto 64/36)
	Sp.HandT = 0.35f * Sp.Mass * Gr * R;           // por rueda trasera
	const float CdA[7] = { 1.65f, 1.6f, 0.9f, 1.6f, 1.0f, 1.5f, 1.8f };
	Sp.CdA = CdA[FMath::Clamp(St, 0, 6)];
	// caja de la carrocería en coordenadas locales (origen = centro de masas)
	const float gc = GroundClearance();
	Sp.BodyBot = (gc - ZCom) / 100.f; Sp.BodyTop = (BodyH() - ZCom) / 100.f; Sp.BodyL = L; Sp.BodyW = Wd;
	const float IntakeFrac = St == 6 ? 1.05f : (St == 2 || St == 4 ? 0.42f : (Sp.bDiesel ? 0.55f : 0.5f));   // el crawler lleva snorkel
	Sp.IntakeZ = Sp.BodyBot + IntakeFrac * (Sp.BodyTop - Sp.BodyBot);
}

void ADustlineCar::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* Cmd = FCommandLine::Get();
	{ float Eb = -1.0f; FParse::Value(Cmd, TEXT("expbias="), Eb); Cam->PostProcessSettings.bOverride_AutoExposureBias = true; Cam->PostProcessSettings.AutoExposureBias = Eb; Cam->PostProcessBlendWeight = 1.f; }
	int32 Model = 0;
	if (FParse::Value(Cmd, TEXT("dmodel="), Model)) { Model = FMath::Clamp(Model, 0, NumModels() - 1); Setup = StockSetup(Model); if (!FParse::Param(Cmd, TEXT("stock"))) LoadSetupFromDisk(Model, Setup); Setup.Model = Model; }
	else { FString Last; if (FFileHelper::LoadFileToString(Last, *(FPaths::ProjectSavedDir() / TEXT("Garage") / TEXT("last_model.txt")))) { { const int32 St = FCString::Atoi(*Last); Model = 0; for (int32 i = 0; i < NumModels(); ++i) if (GetModel(i).Style == St) Model = i; } Setup = StockSetup(Model); LoadSetupFromDisk(Model, Setup); Setup.Model = Model; } }
	if (FParse::Param(Cmd, TEXT("autodrive"))) bAutoDrive = true;
	{ int32 T = -1; if (FParse::Value(Cmd, TEXT("dtire="), T)) Setup.TireType = FMath::Clamp(T, 0, 4); }
	Rebuild(); LastSetup = Setup; PaintShown = Setup.Paint; bHaveLast = true;
	// -dpos=X,Y para probar en un sitio concreto (en cm)
	FString PosS; if (FParse::Value(Cmd, TEXT("dpos="), PosS)) { FString A, B; if (PosS.Split(TEXT(","), &A, &B)) SetActorLocation(FVector(FCString::Atof(*A), FCString::Atof(*B), 60000.f)); }
	FHitResult Hit; const FVector L0 = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, L0 + FVector(0, 0, 3000), L0 - FVector(0, 0, 80000), ECC_Visibility))
		SetActorLocation(FVector(L0.X, L0.Y, Hit.ImpactPoint.Z + ZCom + 25.f), false, nullptr, ETeleportType::TeleportPhysics);
	Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector;
	for (FWheelPhys& W : Wh) { W.Omega = 0.f; W.Sink = 0.f; }
	Gear = 1; Rpm = Sp.Idle;
	bDiffLock = Hd().bLockers;
	InitFx();
	UE_LOG(LogDustlineDrive, Display, TEXT("DRIVE_START model=%d mass=%.0f hp=%.0f tpeak=%.0f r1=%.1f rtop=%.1f autodrive=%d loc=%s"), Setup.Model, Sp.Mass, Sp.HP, Sp.TPeak, Sp.Gears[0], Sp.Gears[Sp.NGears - 1], bAutoDrive ? 1 : 0, *GetActorLocation().ToString());
}

void ADustlineCar::ResetUpright()
{
	FRotator R = GetActorRotation(); R.Pitch = 0; R.Roll = 0;
	FHitResult Hit; const FVector L0 = GetActorLocation();
	float Z = L0.Z; if (GetWorld()->LineTraceSingleByChannel(Hit, L0 + FVector(0, 0, 3000), L0 - FVector(0, 0, 6000), ECC_Visibility)) Z = Hit.ImpactPoint.Z + ZCom + 80.f;
	SetActorLocationAndRotation(FVector(L0.X, L0.Y, Z), R, false, nullptr, ETeleportType::TeleportPhysics);
	Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector;
	for (FWheelPhys& W : Wh) { W.Omega = 0.f; W.Sink = 0.f; }
	bStalled = false; Flood = 0.f; Gear = 1;
}

void ADustlineCar::UpdateWater()
{
	WaterZ = -1e9f; bInWater = false; WaterFlow = FVector2D::ZeroVector;
	const FVector L = GetActorLocation();
	float Lv; FVector2D Fl;
	if (WaterAt(L.X, L.Y, Lv, Fl))
	{
		WaterZ = Lv / 100.f;
		bInWater = WaterZ > L.Z / 100.f + Sp.BodyBot - 0.9f;   // agua cerca de los bajos (también las ruedas)
		if (bInWater) WaterFlow = Fl;
	}
}

void ADustlineCar::StepPhysics(float dt, float Throttle, float Steer, bool bHandbrake, bool bBoost)
{
	const double mass = Sp.Mass;
	FQuat Q = GetActorQuat();
	FVector P = GetActorLocation() / 100.0;
	const FVector Fwd = Q.RotateVector(FVector(1, 0, 0)), UpB = Q.RotateVector(FVector(0, 0, 1));
	const float vLong = FVector::DotProduct(Vel, Fwd);
	const FHandling& H = Hd();
	const float maxSteer = H.SteerMax / (1.f + FMath::Abs(vLong) * H.SteerSens);
	SteerAngle += (Steer * maxSteer - SteerAngle) * FMath::Min(1.f, dt * H.SteerRate);
	++SubStep;

	// ---------------- mandos -> acelerador, freno y marcha ----------------
	float thr = 0.f, brake = 0.f;
	if (bManual)
	{
		thr = FMath::Max(Throttle, 0.f); brake = FMath::Max(-Throttle, 0.f);
	}
	else
	{
		if (Gear >= 1)
		{
			if (Throttle > 0) thr = Throttle;
			else if (Throttle < 0) { if (vLong > 0.8f) brake = -Throttle; else { Gear = -1; thr = -Throttle; } }
		}
		else
		{
			if (Throttle < 0) thr = -Throttle;
			else if (Throttle > 0) { if (vLong < -0.8f) brake = Throttle; else { Gear = 1; thr = Throttle; } }
		}
	}
	const bool bAutoHold = thr < 0.02f && brake < 0.02f && !bHandbrake && Vel.Size() < 0.5f;
	if (bAutoHold) brake = 0.4f;
	// control de descenso (con la reductora y sin pisar nada): mantiene ~8 km/h cuesta abajo frenando rueda a rueda
	bHdcActive = bLowRange && thr < 0.05f && brake < 0.05f && !bHandbrake && Vel.Size() > 2.f && Vel.Z < -0.25f;
	if (bHdcActive) brake = FMath::Clamp((Vel.Size() - 2.2f) / 1.5f + 0.15f, 0.15f, 1.f);

	// ---------------- motor, turbo y convertidor ----------------
	const int32 Drive = Setup.Drive;
	bool Driven[4]; int32 NDriven = 0;
	for (int32 i = 0; i < 4; ++i) { Driven[i] = Drive == 0 || (Drive == 1 && i >= 2) || (Drive == 2 && i < 2); NDriven += Driven[i]; }
	const float Ratio = (Gear > 0 ? Sp.Gears[FMath::Clamp(Gear - 1, 0, Sp.NGears - 1)] : (Gear < 0 ? -Sp.Reverse : 0.f)) * (bLowRange ? Sp.Low : 1.f);
	float AvgW = 0.f; for (int32 i = 0; i < 4; ++i) if (Driven[i]) AvgW += Wh[i].Omega; AvgW /= FMath::Max(1, NDriven);
	const float ShaftRpm = AvgW * Ratio * 60.f / (2.f * PI);
	const float StallRpm = (Sp.bDiesel ? 0.45f : 0.40f) * Sp.Redline;
	const float FreeRpm = Sp.Idle + thr * (StallRpm - Sp.Idle);
	const float RpmTarget = Ratio == 0.f ? Sp.Idle + thr * (Sp.Redline - Sp.Idle) : FMath::Max(FMath::Abs(ShaftRpm), FreeRpm);
	Rpm += (RpmTarget - Rpm) * FMath::Min(1.f, dt * 18.f);
	// cambio automático
	ShiftT = FMath::Max(0.f, ShiftT - dt);
	if (!bManual && Gear >= 1 && ShiftT <= 0.f)
	{
		const float Up = FMath::Lerp(0.55f, 0.92f, thr) * Sp.Redline, Dn = FMath::Lerp(0.28f, 0.50f, thr) * Sp.Redline;
		// régimen de la caja según la velocidad del coche (no la de las ruedas: un bache o un patinazo no la hacen dudar)
		const float RoadRpm = FMath::Abs(vLong) / Sp.R * FMath::Abs(Ratio) * 60.f / (2.f * PI);
		GearHold += dt;
		if (RoadRpm > Up && Gear < Sp.NGears && GearHold > 0.8f) { ++Gear; ShiftT = 0.35f; GearHold = 0.f; }
		else if (RoadRpm < Dn && Gear > 1 && GearHold > 1.0f) { --Gear; ShiftT = 0.3f; GearHold = 0.f; }
	}
	const float SpoolTarget = thr * Smooth(0.22f, 0.55f, Rpm / Sp.Redline) + (bBoost ? 0.3f : 0.f);
	TurboSpool = FMath::Clamp(TurboSpool + (SpoolTarget > TurboSpool ? 1.1f : -1.8f) * dt, 0.f, 1.f);
	float Te = EngineTorque(Rpm) * thr * (1.f + 0.5f * Setup.Turbo * TurboSpool) * (bBoost ? 1.15f : 1.f);
	if (Rpm > Sp.Redline) Te *= FMath::Clamp(1.f - (Rpm - Sp.Redline) / 300.f, 0.f, 1.f);
	if (ShiftT > 0.f) Te *= 0.3f;
	Te *= 1.f - 0.45f * FMath::Clamp(Damage[0], 0.f, 1.f);   // radiador / admisión dañados
	// control de tracción: si alguna rueda motriz patina más allá del pico, se recorta el par del motor
	{
		float Worst = 0.f;
		for (int32 i = 0; i < 4; ++i) if (Driven[i] && Wh[i].Contact) Worst = FMath::Max(Worst, Wh[i].Slip * FMath::Sign(Ratio) / FMath::Max(Wh[i].Peak, 0.05f));
		if (bTc && thr > 0.f && Worst > 1.6f) TcCut = FMath::Max(0.05f, TcCut - dt * 5.f);
		else TcCut = FMath::Min(1.f, TcCut + dt * 2.f);
		if (bTc && thr > 0.f) Te *= TcCut;
	}
	if (bStalled) { Te = 0.f; Rpm = FMath::Max(0.f, Rpm - 3000.f * dt); }
	const float Sr = FMath::Abs(ShaftRpm) / FMath::Max(Rpm, 1.f);
	float TM = FMath::Lerp(1.6f, 1.f, FMath::Clamp(Sr / 0.88f, 0.f, 1.f));
	if (thr < 0.05f && Ratio != 0.f && FMath::Abs(ShaftRpm) > Sp.Idle * 1.1f)   // freno motor
	{ Te = -(0.06f * Sp.TPeak + 0.08f * Sp.TPeak * FMath::Abs(ShaftRpm) / Sp.Redline) * FMath::Sign(ShaftRpm) * (bStalled ? 0.3f : 1.f); TM = 1.f; }
	const float WheelTorque = Te * TM * Ratio * 0.9f;
	float Share[4];
	for (int32 i = 0; i < 4; ++i) Share[i] = !Driven[i] ? 0.f : (Drive == 0 ? (i < 2 ? H.Split * 0.5f : (1.f - H.Split) * 0.5f) : 0.5f);

	// ---------------- contacto de ruedas (fase 1: suspensión) ----------------
	const float zA = Sp.R + 0.55f * Sp.RestLen;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DustlineWheel), false, this);
	struct FC { bool C = false; FVector Hit, N, Rc, Wf, Lat; float Fs = 0, Comp = 0, Crr = 0; FGround G; FTire T; float Vl = 0, Vs = 0; bool bWater = false; } Cw[4];
	const float Nnom = mass * Gr / 4.f;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i) / 100.0;
		const FVector r = Q.RotateVector(FVector(WP.X - ComX, WP.Y, zA - ZCom / 100.f));
		const FVector a = P + r;
		const FVector Start = a * 100.0 + UpB * 60.0, End = Start - UpB * ((Sp.RestLen + Sp.R) * 100.0 + 160.0);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) { Wh[i].Contact = false; Wh[i].Length = FMath::FInterpTo(Wh[i].Length, Sp.RestLen, dt, 8.f); Wh[i].Sink *= 1.f - dt * 2.f; continue; }
		const FVector n = Hit.ImpactNormal.GetSafeNormal();
		FC& c = Cw[i];
		c.G = GroundAt(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z, n.Z, Setup.TireType);
		const float Bump = c.G.Bump * (VNoise(Hit.ImpactPoint.X / 70.f, Hit.ImpactPoint.Y / 70.f) * 2.f - 1.f) + c.G.Bump * 0.4f * (VNoise(Hit.ImpactPoint.X / 23.f + 7.f, Hit.ImpactPoint.Y / 23.f) * 2.f - 1.f);
		const float dist = ((Hit.Distance - 60.0) / 100.0) * FMath::Max(0.3f, (float)FVector::DotProduct(n, UpB));
		const float L = dist - Sp.R + Wh[i].Sink - Bump;
		Wh[i].Length = FMath::Clamp(L, 0.1f, Sp.RestLen);
		if (L >= Sp.RestLen) { Wh[i].Contact = false; continue; }
		Wh[i].Contact = true;
		c.C = true; c.N = n; c.Hit = Hit.ImpactPoint / 100.0; c.Rc = c.Hit - P;
		c.Comp = Sp.RestLen - L;
		const FVector vp = Vel + FVector::CrossProduct(AngVel, c.Rc);
		const float vn = FVector::DotProduct(vp, n);
		const float KMul = 2.f * (i < 2 ? H.FrontW : 1.f - H.FrontW);   // muelles tarados para la carga de cada eje
		float fs = Sp.SpringK * KMul * c.Comp - Sp.SpringC * FMath::Sqrt(KMul) * vn;
		if (L < 0.1f) fs += 6.f * Sp.SpringK * KMul * (0.1f - L);
		c.Fs = fs;
		c.bWater = bInWater && WaterZ > c.Hit.Z + 0.05f;
	}
	// barras estabilizadoras
	for (int32 ax = 0; ax < 2; ++ax)
	{
		FC& A = Cw[ax * 2]; FC& B = Cw[ax * 2 + 1];
		const float Arb = Sp.Arb * ((A.C ? A.Comp : 0.f) - (B.C ? B.Comp : 0.f));
		if (A.C) A.Fs += Arb; if (B.C) B.Fs -= Arb;
	}

	// ---------------- neumáticos (fase 2) + resolución implícita de las ruedas ----------------
	float Am[4][4] = {}, Bv[4] = {}, Xw[4] = {}, Ieff[4], Fx0[4] = {}, Trr[4] = {};
	const float widthFactor = FMath::Clamp(0.85f + (Sp.Width - 0.2f) * 2.f, 0.8f, 1.3f), sinkWidth = FMath::Clamp(FMath::Sqrt(0.28f / Sp.Width), 0.7f, 1.4f);
	for (int32 i = 0; i < 4; ++i)
	{
		FC& c = Cw[i];
		Ieff[i] = Sp.WheelI + (Driven[i] && Ratio != 0.f ? Sp.EngI * Ratio * Ratio * 0.35f / NDriven : 0.f);
		float J = 0.f;
		if (c.C)
		{
			const float N = FMath::Max(c.Fs, 0.f);
			FVector wf = Fwd;
			if (i < 2) wf = Q.RotateVector(FVector(FMath::Cos(SteerAngle + SteerPull), -FMath::Sin(SteerAngle + SteerPull), 0));
			wf = (wf - c.N * FVector::DotProduct(wf, c.N)).GetSafeNormal();
			c.Wf = wf; c.Lat = FVector::CrossProduct(c.N, wf);
			const FVector vp = Vel + FVector::CrossProduct(AngVel, c.Rc);
			c.Vl = FVector::DotProduct(vp, wf); c.Vs = FVector::DotProduct(vp, c.Lat);
			const float Soft = c.G.W[SMud] + c.G.W[SSand] + c.G.W[SSnow];
			float Mu = c.G.Mu * FMath::Lerp(1.f, widthFactor, Soft) * (1.f - 0.08f * FMath::Clamp(N / Nnom - 1.f, -0.5f, 2.f)) * (1.f - 0.12f * WetTyres);
			float Crr = c.G.Crr;
			if (c.bWater) { Mu *= 0.85f; Crr += 0.03f; if (FMath::Abs(c.Vl) > 8.f) Mu *= 0.6f; }
			// aquaplaning: lámina de agua en llano empapado a partir de ~50 km/h (el balón flota antes, el de barro evacua mejor)
			{
				static const float AquaTire[5] = { 1.f, 0.8f, 0.6f, 0.9f, 1.3f };
				const float Film = Smooth(0.6f, 0.95f, c.G.Wet) * Smooth(0.96f, 0.995f, (float)c.N.Z) * FMath::Clamp(0.3f + c.G.W[SMud] + c.G.W[SDirt] + c.G.W[SGrass], 0.f, 1.f);
				const float Aqua = Film * Smooth(13.f, 25.f, FMath::Abs(c.Vl)) * AquaTire[FMath::Clamp(Setup.TireType, 0, 4)];
				Mu *= 1.f - 0.6f * FMath::Clamp(Aqua, 0.f, 1.f);
			}
			c.T = { FMath::Max(Mu, 0.04f), c.G.PeakSlip, c.G.PeakAlpha * H.TyreAlpha, c.G.Falloff };
			// hundimiento: presión (carga / huella) y patinaje que excava; al rodar limpio el neumático sale hacia arriba
			const float Press = FMath::Sqrt(FMath::Max(N, 0.f) / Nnom) * FMath::Lerp(1.f, sinkWidth, Soft) * FMath::Sqrt(0.40f / Sp.R);
			const float Target = c.G.Sink * Press;
			const float Roll = FMath::Abs(c.Vl) * dt, SlipD = FMath::Abs(Wh[i].Omega * Sp.R - c.Vl) * dt;
			Wh[i].Sink += (Target - Wh[i].Sink) * FMath::Min(1.f, Roll * 1.5f + dt * 0.5f);
			Wh[i].Sink += c.G.Dig * SlipD * 0.12f;
			if (Wh[i].Sink > Target && FMath::Abs(Wh[i].Slip) < 0.15f) Wh[i].Sink -= Roll * 0.05f;
			Wh[i].Sink = FMath::Clamp(Wh[i].Sink, 0.f, FMath::Min(0.55f * Sp.R, 0.4f));
			Crr += 0.4f * Wh[i].Sink / Sp.R;   // empujar el suelo blando (efecto bulldozer)
			Trr[i] = 0.004f * N * Sp.R * FMath::Tanh(Wh[i].Omega * Sp.R / 0.4f);   // rodamientos
			c.Crr = Crr;   // la resistencia del suelo (rodadura + hundimiento) frena el avance en la huella
			float Fy, K; TireForce(c.T, N, Wh[i].Omega, Sp.R, c.Vl, c.Vs, Fx0[i], Fy, K);
			float Fx1, Fy1, K1; TireForce(c.T, N, Wh[i].Omega + 0.05f, Sp.R, c.Vl, c.Vs, Fx1, Fy1, K1);
			J = FMath::Max(0.f, (Fx1 - Fx0[i]) / 0.05f);
			Wh[i].Load = N; Wh[i].Surf = c.bWater ? -1 : c.G.Dominant;
			Wh[i].CP = c.Hit * 100.0; Wh[i].CN = c.N; Wh[i].bWater = c.bWater; Wh[i].Wet = c.G.Wet;
			Wh[i].WMud = c.G.W[SMud]; Wh[i].WSnow = c.G.W[SSnow]; Wh[i].WSand = c.G.W[SSand]; Wh[i].WGrav = c.G.W[SGravel];
			Wh[i].WGrass = c.G.W[SGrass]; Wh[i].WTall = c.G.W[STallGrass]; Wh[i].WDirt = c.G.W[SDirt]; Wh[i].WRock = c.G.W[SRock] + c.G.W[SIce];
			Wh[i].SlipA = c.Vs / FMath::Max(FMath::Abs(c.Vl), 1.f) / FMath::Max(c.G.PeakAlpha, 0.05f);
		}
		else { Wh[i].Load = 0.f; Trr[i] = 1.5f * Wh[i].Omega; }   // rueda al aire: rozamiento de rodamientos y frenos
		Am[i][i] = Ieff[i] / dt + Sp.R * J;
		Bv[i] = (Ieff[i] / dt + Sp.R * J) * Wh[i].Omega + WheelTorque * Share[i] - Sp.R * Fx0[i] - Trr[i];
	}
	// acoplamientos del diferencial: abierto con un poco de viscosidad, o bloqueado (casi rígido)
	auto Couple = [&](int32 a, int32 b, float k) { Am[a][a] += k; Am[b][b] += k; Am[a][b] -= k; Am[b][a] -= k; };
	const float KLock = 6000.f;
	if (Drive == 0 || Drive == 2) Couple(0, 1, bDiffLock ? KLock : FMath::Min(H.AxleK, 60.f));   // delante nunca autoblocante fuerte
	if (Drive == 0 || Drive == 1) Couple(2, 3, bDiffLock ? KLock : H.AxleK);
	if (Drive == 0) { const float Kc = (bDiffLock || H.CenterK < 0.f) ? KLock * 0.5f : H.CenterK; Couple(0, 2, Kc); Couple(1, 3, Kc); }   // 4x4 conectable = central rígido
	if (!Solve4(Am, Bv, Xw)) for (int32 i = 0; i < 4; ++i) Xw[i] = Wh[i].Omega;
	// control de estabilidad: compara la guiñada real con la que pide el volante y frena una rueda para corregir
	float EscB[4] = {}; bEscActive = false;
	if (bTc && H.bEsc && FMath::Abs(vLong) > 5.f && !bHandbrake)
	{
		const float WB = (FrontX() - RearX()) / 100.f;
		const float RMax = 0.85f * Gr / FMath::Abs(vLong);
		const float RRef = FMath::Clamp(-vLong * FMath::Tan(SteerAngle) / WB, -RMax, RMax);
		const float RAct = FVector::DotProduct(AngVel, UpB);
		const float Err = RAct - RRef;
		const bool bOver = FMath::Abs(RAct) > FMath::Abs(RRef) + 0.12f && RAct * RRef >= 0.f, bUnder = FMath::Abs(RAct) < FMath::Abs(RRef) - 0.18f;
		const float TurnSign = FMath::Abs(RRef) > 0.02f ? FMath::Sign(RRef) : FMath::Sign(RAct);   // >0: gira a la derecha
		if (bOver) { const int32 Outer = TurnSign > 0.f ? 0 : 1; EscB[Outer] = FMath::Clamp(FMath::Abs(Err) * 0.8f, 0.f, 0.5f); bEscActive = true; }
		else if (bUnder) { const int32 Inner = TurnSign > 0.f ? 3 : 2; EscB[Inner] = FMath::Clamp(FMath::Abs(Err) * 0.6f, 0.f, 0.35f); bEscActive = true; }
		if (bEscActive) TcCut = FMath::Max(0.4f, TcCut - dt * 2.f);
	}
	// frenos (con ABS opcional) y freno de mano: par de rozamiento que puede llegar a bloquear la rueda
	for (int32 i = 0; i < 4; ++i)
	{
		float Tb = brake * Sp.BrakeT * (i < 2 ? 0.32f : 0.18f) + EscB[i] * Sp.BrakeT * 0.3f;
		// ABS: el par de freno no pasa de lo que el neumático puede transmitir (la rueda sigue girando cerca del pico)
		if (bAbs && brake > 0.f && Cw[i].C && FMath::Abs(Cw[i].Vl) > 1.5f)
		{
			Tb = FMath::Min(Tb, Cw[i].T.Mu * FMath::Max(Cw[i].Fs, 0.f) * Sp.R * 0.9f);
			if (Wh[i].Slip * FMath::Sign(Cw[i].Vl) < -1.4f * Cw[i].T.Peak) Tb = 0.f;   // rueda pasada del pico: suelta presión
		}
		if (bHandbrake && i >= 2) Tb += Sp.HandT;
		const float Dw = Tb * dt / Ieff[i];
		Xw[i] = FMath::Abs(Xw[i]) <= Dw ? 0.f : Xw[i] - FMath::Sign(Xw[i]) * Dw;
		Xw[i] = FMath::Clamp(Xw[i], -400.f, 400.f);
		Wh[i].Omega = Xw[i];
	}

	// ---------------- fuerzas sobre la carrocería ----------------
	FVector F = FVector::ZeroVector, T = FVector::ZeroVector;
	int32 bestW = -1; float bestLoad = -1.f;
	for (int32 i = 0; i < 4; ++i)
	{
		FC& c = Cw[i];
		if (!c.C) continue;
		const float N = FMath::Max(c.Fs, 0.f);
		float Fx, Fy, K; TireForce(c.T, N, Wh[i].Omega, Sp.R, c.Vl, c.Vs, Fx, Fy, K);
		Wh[i].Slip = K; Wh[i].Peak = c.T.Peak;
		// sujeción en parado (rueda frenada o bloqueada en pendiente): rozamiento estático
		if (Wh[i].Omega == 0.f && FMath::Abs(c.Vl) < 0.3f) { const float Lim = FMath::Max(0.f, c.T.Mu * N - FMath::Abs(Fx)); Fx += FMath::Clamp(-c.Vl * (float)mass * 8.f, -Lim, Lim); }
		Fx -= c.G.Drag * N * c.Vl;   // hierba alta / nieve profunda frenan el avance
		Fx -= c.Crr * N * FMath::Tanh(c.Vl / 0.15f);
		// la fuerza vertical y la longitudinal actúan en la huella; la lateral entra por los brazos del eje a la altura del
		// centro de balanceo (eje rígido: ~35 % de la altura del centro de masas), que es lo que limita el balanceo real
		const FVector Fv = c.N * N + c.Wf * Fx, Fl = c.Lat * Fy;
		F += Fv + Fl; T += FVector::CrossProduct(c.Rc, Fv) + FVector::CrossProduct(c.Rc + UpB * (ZCom / 100.f) * H.Rc, Fl);
		if (N > bestLoad) { bestLoad = N; bestW = i; }
	}
	for (int32 i = 0; i < 4; ++i) Wh[i].Spin += Wh[i].Omega * dt;

	// carrocería contra el suelo (vuelcos, panza, roces): puntos con muelle-amortiguador y rozamiento
	{
		static const float PX[11] = { 0.47f, 0.47f, -0.47f, -0.47f, 0.f, 0.f, 0.44f, 0.44f, -0.44f, -0.44f, 0.f };
		static const float PY[11] = { 0.47f, -0.47f, 0.47f, -0.47f, 0.48f, -0.48f, 0.42f, -0.42f, 0.42f, -0.42f, 0.f };
		static const bool Top[11] = { false, false, false, false, false, false, true, true, true, true, true };
		const float Kb = mass * Gr * 10.f, Cb = 2.f * FMath::Sqrt(Kb * (float)mass / 4.f) * 0.4f;
		FCollisionQueryParams BP(SCENE_QUERY_STAT(DustlineBody), false, this);
		for (int32 k = 0; k < 11; ++k)
		{
			const FVector Lp(PX[k] * Sp.BodyL - ComX, PY[k] * Sp.BodyW, Top[k] ? Sp.BodyTop : Sp.BodyBot);
			const FVector Wp = P + Q.RotateVector(Lp);
			if ((SubStep & 1) == 0)
			{
				FHitResult BH; BodyPlane[k] = false;
				const FVector Dir = (Wp - P).GetSafeNormal();
				if (GetWorld()->LineTraceSingleByChannel(BH, P * 100.0, Wp * 100.0 + Dir * 30.0, ECC_Visibility, BP)) { BodyPlane[k] = true; BodyPlaneP[k] = BH.ImpactPoint / 100.0; BodyPlaneN[k] = BH.ImpactNormal; }
			}
			if (!BodyPlane[k]) continue;
			const float d = FVector::DotProduct(BodyPlaneP[k] - Wp, BodyPlaneN[k]);
			if (d <= 0.f) continue;
			const FVector rp = Wp - P, vp = Vel + FVector::CrossProduct(AngVel, rp);
			const float vn = FVector::DotProduct(vp, BodyPlaneN[k]);
			const float Fn = FMath::Max(0.f, Kb * FMath::Min(d, 0.4f) - Cb * vn);
			const FVector vt = vp - BodyPlaneN[k] * vn;
			const FVector Ft = vt.SizeSquared() > 1e-4 ? -vt.GetSafeNormal() * FMath::Min(0.55f * Fn, (float)vt.Size() * (float)mass * 3.f) : FVector::ZeroVector;
			const FVector Fb = BodyPlaneN[k] * Fn + Ft;
			F += Fb; T += FVector::CrossProduct(rp, Fb);
		}
	}

	// agua: flotación por columnas, arrastre con la corriente, inundación y toma de aire
	WaterDepthM = 0.f;
	if (bInWater)
	{
		const FVector Flow(WaterFlow.X, WaterFlow.Y, 0.f);
		const float CL = Sp.BodyL / 3.f, CW = Sp.BodyW / 2.f, HBody = Sp.BodyTop - Sp.BodyBot;
		for (int32 ix = 0; ix < 3; ++ix) for (int32 iy = 0; iy < 2; ++iy)
		{
			const FVector Lp((ix - 1) * CL - ComX, (iy - 0.5f) * CW, Sp.BodyBot);
			const FVector Wb = P + Q.RotateVector(Lp);
			const float h = FMath::Clamp(WaterZ - (float)Wb.Z, 0.f, HBody);
			if (h <= 0.f) continue;
			WaterDepthM = FMath::Max(WaterDepthM, h);
			const FVector Pt = Wb + FVector(0, 0, h * 0.5f), rp = Pt - P;
			const float Vol = CL * CW * h * 0.55f * (1.f - 0.85f * Flood);
			const FVector Fb(0, 0, Rho * Gr * Vol);
			F += Fb; T += FVector::CrossProduct(rp, Fb);
			// arrastre: de frente solo la fila de columnas que empuja el agua, de lado solo la columna del lado que avanza
			const FVector vrel = Vel + FVector::CrossProduct(AngVel, rp) - Flow;
			const FVector vb = Q.UnrotateVector(vrel);
			const bool bFront = (vb.X > 0.f && ix == 2) || (vb.X < 0.f && ix == 0), bSide = (vb.Y > 0.f && iy == 1) || (vb.Y < 0.f && iy == 0);
			const FVector Db(bFront ? -0.5f * Rho * 0.9f * (CW * h) * vb.X * FMath::Abs(vb.X) : -0.5f * Rho * 0.05f * (CW * h) * vb.X * FMath::Abs(vb.X),
				bSide ? -0.5f * Rho * 1.1f * (CL * h) * vb.Y * FMath::Abs(vb.Y) : 0.f, -0.5f * Rho * 1.3f * (CL * CW) * vb.Z * FMath::Abs(vb.Z));
			const FVector Fd = Q.RotateVector(Db);
			// el agua empuja la carrocería por delante a media altura de lo sumergido, no por los bajos (no lo "zancadillea")
			const FVector rd(rp.X, rp.Y, rp.Z * 0.35f);
			F += Fd; T += FVector::CrossProduct(rd, Fd);
		}
		// ruedas dentro del agua: arrastre de las ruedas y de los bajos
		for (int32 i = 0; i < 4; ++i) if (Cw[i].C && Cw[i].bWater)
		{
			const float dW = FMath::Clamp(WaterZ - (float)Cw[i].Hit.Z, 0.f, 2.f * Sp.R);
			const FVector vrel = Vel - Flow;
			const FVector Fd = -vrel * (float)vrel.Size() * 0.5f * Rho * 0.8f * (Sp.Width * dW);
			F += Fd; T += FVector::CrossProduct(Cw[i].Rc, Fd);
		}
		const float Sill = Sp.BodyBot + 0.35f;
		const float OverSill = WaterZ - (float)(P + Q.RotateVector(FVector(0, 0, Sill))).Z;
		if (OverSill > 0.f) Flood = FMath::Min(1.f, Flood + dt * 0.012f * FMath::Clamp(OverSill / 0.5f, 0.f, 1.f));
		const float IntakeWZ = (float)(P + Q.RotateVector(FVector(0.3f * Sp.BodyL - ComX, 0.f, Sp.IntakeZ))).Z;
		if (WaterZ > IntakeWZ && !bStalled) { bStalled = true; UE_LOG(LogDustlineDrive, Display, TEXT("DRIVE motor ahogado (agua %.2f > toma %.2f)"), WaterZ, IntakeWZ); }
		if (WaterZ < IntakeWZ) StallT += dt; else StallT = 0.f;
		WetTyres = 1.f;
	}
	else { StallT += dt; Flood = FMath::Max(0.f, Flood - dt * 0.01f); WetTyres = FMath::Max(0.f, WetTyres - dt * 0.08f); }
	if (bStalled && StallT > 2.5f) bStalled = false;

	// aire, gravedad e integración
	F += -Vel * Vel.Size() * 0.5f * 1.2f * Sp.CdA;
	F += FVector(0, 0, -Gr * mass);
	Vel += F / mass * dt;
	T += -AngVel * 120.f * (mass / 1500.f);
	const FQuat Qi = Q.Inverse();
	const FVector Tl = Qi.RotateVector(T); FVector wl = Qi.RotateVector(AngVel);
	wl += FVector(Tl.X / Sp.Inertia[2], Tl.Y / Sp.Inertia[0], Tl.Z / Sp.Inertia[1]) * dt;   // alabeo X, cabeceo Y, guiñada Z
	AngVel = Q.RotateVector(wl);
	AngVel = AngVel.GetClampedToMaxSize(12.f);
	P += Vel * dt;
	const double wlen = AngVel.Size();
	if (wlen > 1e-5) Q = (FQuat(AngVel / wlen, wlen * dt) * Q).GetNormalized();
	if (WaterDepthM > 0.15f) SurfaceName = TEXT("Agua");
	else SurfaceName = bestW >= 0 ? (Wh[bestW].Surf < 0 ? TEXT("Vado") : SurfName(Wh[bestW].Surf)) : TEXT("En el aire");
	FHitResult Blocked;
	SetActorLocationAndRotation(P * 100.0, Q, true, &Blocked, ETeleportType::None);
	// el suelo lo llevan las ruedas y los puntos de la carrocería: si el barrido toca el terreno, se completa el movimiento sin choque
	if (Blocked.bBlockingHit && ((Blocked.GetComponent() && Blocked.GetComponent()->IsA<ULandscapeHeightfieldCollisionComponent>()) || Blocked.ImpactNormal.Z > 0.65f))
	{
		SetActorLocationAndRotation(P * 100.0, Q, false, nullptr, ETeleportType::None);
		Blocked = FHitResult();
	}
	if (Blocked.bBlockingHit && FParse::Param(FCommandLine::Get(), TEXT("phystest")))
	{
		static double LastB = 0.0; const double Nw = GetWorld()->GetTimeSeconds();
		if (Nw - LastB > 1.0) { LastB = Nw; UE_LOG(LogDustlineDrive, Display, TEXT("PTBLOCK %s / %s n=%s"), Blocked.GetActor() ? *Blocked.GetActor()->GetName() : TEXT("-"), Blocked.GetComponent() ? *Blocked.GetComponent()->GetName() : TEXT("-"), *Blocked.ImpactNormal.ToString()); }
	}
	if (Blocked.bBlockingHit)
	{
		// choque como impulso en el punto de contacto: el coche rebota poco, gira si golpea con una esquina y se raspa
		const FVector n = Blocked.ImpactNormal;
		const FVector Pc = GetActorLocation() / 100.0, rc = Blocked.ImpactPoint / 100.0 - Pc;
		const FQuat Qc = GetActorQuat();
		auto InvI = [&](const FVector& W) { FVector l = Qc.UnrotateVector(W); l = FVector(l.X / Sp.Inertia[2], l.Y / Sp.Inertia[0], l.Z / Sp.Inertia[1]); return Qc.RotateVector(l); };
		const FVector vp = Vel + FVector::CrossProduct(AngVel, rc);
		const float vn = FVector::DotProduct(vp, n);
		if (vn < 0.f)
		{
			const float e = vn < -4.f ? 0.12f : 0.25f;
			const float Kn = 1.f / (float)mass + FVector::DotProduct(n, FVector::CrossProduct(InvI(FVector::CrossProduct(rc, n)), rc));
			const float Jn = -(1.f + e) * vn / Kn;
			Vel += n * (Jn / (float)mass); AngVel += InvI(FVector::CrossProduct(rc, n * Jn)).GetClampedToMaxSize(2.5f);
			FVector vt = vp - n * vn;
			if (vt.SizeSquared() > 1e-4f)
			{
				const FVector t = vt.GetSafeNormal();
				const float Kt = 1.f / (float)mass + FVector::DotProduct(t, FVector::CrossProduct(InvI(FVector::CrossProduct(rc, t)), rc));
				const float Jt = FMath::Min((float)vt.Size() / Kt, 0.5f * Jn);
				Vel -= t * (Jt / (float)mass); AngVel -= InvI(FVector::CrossProduct(rc, t * Jt));
			}
			const float Dv = Jn / (float)mass;   // cambio de velocidad del golpe (m/s)
			if (Dv > 2.5f)
			{
				const FVector Nl = Qc.UnrotateVector(-n);
				const int32 Zone = FMath::Abs(Nl.X) > FMath::Abs(Nl.Y) ? (Nl.X > 0.f ? 0 : 1) : (Nl.Y < 0.f ? 2 : 3);
				const float Hit = FMath::Clamp((Dv - 2.5f) / 22.f, 0.f, 1.f);
				Damage[Zone] = FMath::Min(1.f, Damage[Zone] + Hit);
				if (Zone == 0 || Zone >= 2) SteerPull = FMath::Clamp(SteerPull + Hit * 0.05f * (Zone == 2 ? -1.f : (Zone == 3 ? 1.f : (FMath::RandBool() ? 1.f : -1.f))), -0.08f, 0.08f);
				if (Dv > 12.f && Zone == 0) { bStalled = true; StallT = 0.f; }
				UE_LOG(LogDustlineDrive, Display, TEXT("DRIVE choque dv=%.1f m/s zona=%d daños=%.2f/%.2f/%.2f/%.2f"), Dv, Zone, Damage[0], Damage[1], Damage[2], Damage[3]);
			}
			ImpactSnd = FMath::Max(ImpactSnd, FMath::Clamp(Dv / 10.f, 0.f, 1.f));
			CamShake = FMath::Max(CamShake, FMath::Clamp(Dv / 8.f, 0.f, 1.f));
		}
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
	ApplyAnims();
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
	// no meter la cámara bajo el agua
	{ float Lv; FVector2D Fl; if (WaterAt(CamPos.X, CamPos.Y, Lv, Fl) && Lv > CamPos.Z - 40.f) CamPos.Z = Lv + 40.f; }
	if (CamShake > 0.01f) { CamPos += FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * CamShake * 18.f; CamShake *= FMath::Exp(-dt * 6.f); }
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
	const FDustlineSettings& Set = FDustlineSettings::Get();
	bManual = Set.bManual; bAbs = Set.bAbs; bTc = Set.bTc;
	static const bool bPhysTest = FParse::Param(FCommandLine::Get(), TEXT("phystest"));
	if (bPhysTest)
	{
		// prueba controlada: 0-8 s acelera recto, 8-13 s giro a tope manteniendo ~50 km/h, 13-17 s frenada a fondo, 17-22 acelera en recta
		const float T = Clock - 2.f;
		if (T < 0.f) {}
		else if (T < 4.f) Throttle = SpeedKmh > 45.f ? 0.f : 1.f;
		else if (T < 10.f) { Throttle = SpeedKmh > 45.f ? 0.f : 1.f; Steer = 1.f; }
		else if (T < 14.f) Throttle = -1.f;
		else if (T < 18.f) { Throttle = 1.f; Steer = -0.4f; }
		else Throttle = -1.f;
		static float PT = 0.f; PT += dt;
		if (PT > 0.5f) { PT = 0.f; UE_LOG(LogDustlineDrive, Display, TEXT("PT t=%.1f v=%.0f ay=%.2f esc=%d gear=%d rpm=%.0f up=%.2f roll=%.1f surf=%s slipF=%.2f slipR=%.2f grounded=%d%d%d%d"), T, SpeedKmh, FMath::Abs(AngVel.Z) * Vel.Size() / 9.81f, bEscActive ? 1 : 0, Gear, Rpm, GetActorUpVector().Z,
			GetActorRotation().Roll, *SurfaceName, Wh[0].Slip, Wh[2].Slip, Wh[0].Contact, Wh[1].Contact, Wh[2].Contact, Wh[3].Contact); }
		if (T > 22.f) FPlatformMisc::RequestExit(false);
	}
	else if (bAutoDrive) { Throttle = 1.f; Steer = 0.55f * FMath::Sin(Clock * 0.45f); Boost = Clock > 3.f; }
	else if (PC && !bPaused && !bShowcase && !bGarage && !(Menu.IsValid() && Menu->IsOpen()))
	{
		Throttle = (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) ? 1.f : 0.f);
		Steer = (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right) ? 1.f : 0.f);
		Hand = PC->IsInputKeyDown(EKeys::SpaceBar); Boost = PC->IsInputKeyDown(EKeys::LeftShift);
		if (PC->WasInputKeyJustPressed(EKeys::R)) ResetUpright();
		if (PC->WasInputKeyJustPressed(EKeys::C)) CamMode = (CamMode + 1) % 3;
		if (PC->WasInputKeyJustPressed(EKeys::B)) bDiffLock = !bDiffLock;
		if (PC->WasInputKeyJustPressed(EKeys::L) && Sp.bHasLow && SpeedKmh < 8.f) { bLowRange = !bLowRange; Gear = Gear < 0 ? -1 : 1; }
		if (bManual)
		{
			if (PC->WasInputKeyJustPressed(EKeys::E)) { Gear = Gear < 0 ? 0 : FMath::Min(Gear + 1, Sp.NGears); ShiftT = 0.25f; }
			if (PC->WasInputKeyJustPressed(EKeys::Q)) { Gear = Gear <= 0 ? -1 : Gear - 1; ShiftT = 0.25f; if (Gear == 0 && SpeedKmh > 5.f) Gear = 1; }
		}
	}
	{ FString Dl; if (FParse::Value(FCommandLine::Get(), TEXT("dlock="), Dl)) bDiffLock = Dl == TEXT("1"); if (FParse::Param(FCommandLine::Get(), TEXT("dlow"))) bLowRange = Sp.bHasLow; }
	if (FParse::Param(FCommandLine::Get(), TEXT("garagedemo")))
	{
		static int32 Step = 0;
		auto Shot = [&](int32 n) { FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("/Volumes/SSD/UE-Cache/gd_%d.png"), n), true, false); };
		if (Step == 0 && Clock > 4.f) { ToggleGarage(); Step = 1; }
		else if (Step == 1 && Clock > 11.f) { Shot(1); Step = 2; }
		else if (Step == 2 && Clock > 12.f) { SelectModel(1); Setup.EngineCC = 7000.f; Setup.Turbo = 3.f; GarageFocus = 1; SetXRay(true); bSetupDirty = true; Step = 3; }
		else if (Step == 3 && Clock > 20.f) { Shot(2); Step = 4; }
		else if (Step == 4 && Clock > 21.f) { Setup.TireType = 3; Setup.TireRadiusM = 0.52f; Setup.TireWidthM = 0.35f; GarageFocus = 2; SetXRay(false); bSetupDirty = true; Step = 5; }
		else if (Step == 5 && Clock > 28.f) { Shot(3); Step = 6; }
		else if (Step == 6 && Clock > 31.f) { FPlatformMisc::RequestExit(false); }
	}
	Cam->FieldOfView = Set.Fov;
	const bool bDemoRun = bAutoDrive || bPhysTest || FParse::Param(FCommandLine::Get(), TEXT("garagedemo")) || FParse::Param(FCommandLine::Get(), TEXT("nomenu")) || FParse::Value(FCommandLine::Get(), TEXT("dshot="), Shot0);
	if (!bMenuStarted && Clock > 0.3f && PC) { bMenuStarted = true; Menu = MakeShared<FDustlineMenu>(this); Menu->Start(!bDemoRun); }
	if (PC && Menu.IsValid())
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape)) Menu->OnEscape();
		else if (PC->WasInputKeyJustPressed(EKeys::G) && !Menu->IsOpen()) ToggleGarage();
	}
	else if (PC && PC->WasInputKeyJustPressed(EKeys::G)) ToggleGarage();
	if (FParse::Param(FCommandLine::Get(), TEXT("uidemo"))) UiDemo();
	if (FParse::Param(FCommandLine::Get(), TEXT("wheeltest"))) WheelTest();
	if (FParse::Param(FCommandLine::Get(), TEXT("enginetest"))) EngineTest();
	if (FParse::Param(FCommandLine::Get(), TEXT("bodytest"))) BodyTest();
	if (bSetupDirty && Clock - LastRebuild > 0.04f) { bSetupDirty = false; LastRebuild = Clock; if (!NoteSetupChange()) Rebuild(); }
	StepAnims(dt);
	{
		const FVector L = GetActorLocation();
		const FLocal Wx = WeatherAt(L.X, L.Y);
		WeatherLabel = WeatherName(Wx); TempC = Wx.TempC;
	}
	if (bGarage || bShowcase)
	{
		Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector;
		for (int32 i = 0; i < 4; ++i) { Wh[i].Length = FMath::FInterpTo(Wh[i].Length, Sp.RestLen * 0.55f, dt, 6.f); Wh[i].Omega = 0.f; }
		UpdateVisuals(0.f); UpdateGarageCamera(dt);
	}
	else if (bPaused)
	{
		UpdateVisuals(0.f); UpdateCamera(dt);
	}
	else
	{
		UpdateWater();
		Accum += dt;
		const float h = 1.f / 240.f;
		while (Accum >= h) { StepPhysics(h, Throttle, Steer, Hand, Boost); Accum -= h; }
		UpdateVisuals(Throttle);
		UpdateCamera(dt);
	}
	SpeedKmh = Vel.Size() * 3.6f;
	PrevThr = LastThr; LastThr = Gear < 0 ? FMath::Max(-Throttle, 0.f) : FMath::Max(Throttle, 0.f);
	UpdateFx(dt);
	LogT += dt; FpsAcc += DeltaTime; ++FpsN;
	if (LogT > 2.f)
	{
		LogT = 0.f;
		const FVector L = GetActorLocation();
		const FLocal Wx = WeatherAt(L.X, L.Y);
		UE_LOG(LogDustlineDrive, Display, TEXT("DRIVE t=%.1f speed=%.0f km/h gear=%d rpm=%.0f loc=%s surf=%s sink=%.2f slip=%.2f/%.2f water=%.2f flood=%.2f stall=%d lock=%d low=%d grounded=%d%d%d%d up.z=%.2f wx=%s wet=%.2f snow=%.2f T=%.1f fps=%.0f"),
			Clock, SpeedKmh, Gear, Rpm, *L.ToString(), *SurfaceName, MaxSinkM(), Wh[0].Slip, Wh[2].Slip, WaterDepthM, Flood, bStalled ? 1 : 0, bDiffLock ? 1 : 0, bLowRange ? 1 : 0,
			Wh[0].Contact, Wh[1].Contact, Wh[2].Contact, Wh[3].Contact, GetActorUpVector().Z, *WeatherLabel, Wx.Wet, Wx.SnowM, Wx.TempC, FpsN / FMath::Max(FpsAcc, 0.001f));
		FpsAcc = 0.f; FpsN = 0;
	}
	FString Shot; if (FParse::Value(FCommandLine::Get(), TEXT("dshot="), Shot) && GEngine)
	{
		float ShotAt = 22.f; FParse::Value(FCommandLine::Get(), TEXT("dshotat="), ShotAt);
		static bool bDone = false; static float T0 = 0.f;
		if (!bDone && Clock > ShotAt) { FScreenshotRequest::RequestScreenshot(Shot, true, false); bDone = true; T0 = Clock; }
		else if (bDone && Clock - T0 > 6.f) FPlatformMisc::RequestExit(false);
	}
}
