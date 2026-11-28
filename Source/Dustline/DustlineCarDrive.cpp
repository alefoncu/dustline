#include "DustlineCar.h"
#include "DustlineCareer.h"
#include "DustlineCarParts.h"
#include "Materials/MaterialInstanceDynamic.h"
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
#include "Math/RandomStream.h"
#include "Algo/Reverse.h"
#include "Components/InstancedStaticMeshComponent.h"

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
	Sp.Mass = FMath::Max(450.f, M.Mass + (Setup.EngineCC - M.CC) * 0.065f + (Setup.Turbo - M.Turbo) * 30.f + (wheelMass - stock) + Setup.Ballast + (Setup.TravelM - M.Travel) * 60.f + (Setup.bRack ? 45.f : 0.f) + (Setup.bSnorkel ? 6.f : 0.f));
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
	Sp.CdA = CdA[FMath::Clamp(St, 0, 6)] + (Setup.bRack ? 0.12f : 0.f);
	// caja de la carrocería en coordenadas locales (origen = centro de masas)
	const float gc = GroundClearance();
	Sp.BodyBot = (gc - ZCom) / 100.f; Sp.BodyTop = (BodyH() - ZCom) / 100.f; Sp.BodyL = L; Sp.BodyW = Wd;
	const float IntakeFrac = St == 6 ? 1.05f : (Setup.bSnorkel || St == 3) ? 0.97f : (St == 2 || St == 4 ? 0.42f : (Sp.bDiesel ? 0.55f : 0.5f));   // el crawler lleva snorkel
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
	FString PosS; if (FParse::Value(Cmd, TEXT("dpos="), PosS, false)) { FString A, B; if (PosS.Split(TEXT(","), &A, &B)) SetActorLocation(FVector(FCString::Atof(*A), FCString::Atof(*B), 60000.f)); }
	{ float Yaw = 0.f; if (FParse::Value(Cmd, TEXT("dyaw="), Yaw)) SetActorRotation(FRotator(0.f, Yaw, 0.f)); }
	// -droll=GRADOS: empieza tumbado (90 = de lado, 180 = boca abajo) para probar que volcado no se conduce
	float RollDeg = 0.f; const bool bRollTest = FParse::Value(Cmd, TEXT("droll="), RollDeg);
	// -dpool=PROFUNDIDAD,CORRIENTE: charca de prueba de 18 m de radio que empieza 14 m por delante del coche; la corriente va de izquierda a derecha
	if (FString PoolS; FParse::Value(Cmd, TEXT("dpool="), PoolS, false))
	{
		FString A, B; if (!PoolS.Split(TEXT(","), &A, &B)) A = PoolS;
		const float Dp = FCString::Atof(*A), Cur = B.IsEmpty() ? 0.f : FCString::Atof(*B);
		const FVector L = GetActorLocation(), F = GetActorForwardVector(), Rv = GetActorRightVector();
		const FVector C = L + F * 3200.f;
		UWorld* W = GetWorld();
		SetTestPool(C.X, C.Y, 1800.f, Dp * 100.f, FVector2D(Rv.X, Rv.Y) * Cur, [W](float X, float Y) { FHitResult H; return W->LineTraceSingleByChannel(H, FVector(X, Y, 80000), FVector(X, Y, -20000), ECC_Visibility) ? (float)H.ImpactPoint.Z : 0.f; });
		const float Gz = 0.f;
		UE_LOG(LogDustlineDrive, Display, TEXT("POOL profundidad=%.2f corriente=%.2f nivel=%.0f"), Dp, Cur, Gz + Dp * 100.f);
	}
	// -dford=PROFUNDIDAD (m): busca un vado de esa profundidad con orillas suaves y deja el coche en la orilla mirando al otro lado
	if (float FordD = 0.f; FParse::Value(Cmd, TEXT("dford="), FordD))
	{
		FRandomStream R(FordD * 1000.f + 7.f); int32 Seed = 0; FParse::Value(Cmd, TEXT("dfordseed="), Seed); R.Initialize(Seed * 7919 + (int32)(FordD * 1000.f));
		auto Ground = [&](float X, float Y) { FHitResult H; return GetWorld()->LineTraceSingleByChannel(H, FVector(X, Y, 80000), FVector(X, Y, -20000), ECC_Visibility) ? (float)H.ImpactPoint.Z : -1e9f; };
		float BestE = 1e9f; FVector BestP; float BestYaw = 0.f;
		int32 NDepth = 0;
		for (int32 k = 0; k < 200000 && BestE > 0.08f; ++k)
		{
			const float X = R.FRandRange(-180000.f, 180000.f), Y = R.FRandRange(-180000.f, 180000.f);
			float Lv; FVector2D Fl; if (!WaterAt(X, Y, Lv, Fl) || Lv < 300.f) continue;   // ríos y lagos, no el mar
			const float G0 = Ground(X, Y); const float D = (Lv - G0) / 100.f;
			if (FMath::Abs(D - FordD) > 0.2f) continue;
			if (FParse::Param(Cmd, TEXT("dfordlake")) && Fl.Size() > 0.1f) continue;
			{   // fondo llano alrededor (sin cauces en V): el suelo a 4 m en cruz no cambia más de 40 cm
				bool bFlat = true;
				for (int32 q = 0; q < 4 && bFlat; ++q) { const float Gq = Ground(X + (q == 0 ? 400.f : q == 1 ? -400.f : 0.f), Y + (q == 2 ? 400.f : q == 3 ? -400.f : 0.f)); bFlat = FMath::Abs(Gq - G0) < 40.f; }
				if (!bFlat) continue;
			}
			++NDepth;
			{   // tramo casi llano: el nivel apenas cambia 15 m río arriba y abajo (nada de torrentes)
				const FVector2D Fd = Fl.SizeSquared() > 0.01f ? Fl.GetSafeNormal() : FVector2D(1, 0);
				float L1, L2; FVector2D F1, F2;
				if ((WaterAt(X + Fd.X * 1500.f, Y + Fd.Y * 1500.f, L1, F1) && FMath::Abs(L1 - Lv) > 50.f) || (WaterAt(X - Fd.X * 1500.f, Y - Fd.Y * 1500.f, L2, F2) && FMath::Abs(L2 - Lv) > 50.f)) continue;
				float L3, L4; const FVector2D Pd(-Fd.Y, Fd.X);
				if (WaterAt(X + Pd.X * 600.f, Y + Pd.Y * 600.f, L3, F1) && FMath::Abs(L3 - Lv) > 25.f) continue;
				if (WaterAt(X - Pd.X * 600.f, Y - Pd.Y * 600.f, L4, F2) && FMath::Abs(L4 - Lv) > 25.f) continue;
			}
			if (FParse::Param(Cmd, TEXT("dfordin")))   // dentro del agua directamente (sin mirar las orillas)
			{
				const FVector2D Dn = Fl.SizeSquared() > 0.01f ? FVector2D(-Fl.Y, Fl.X).GetSafeNormal() : FVector2D(1, 0);
				const float E = FMath::Abs(D - FordD);
				if (E < BestE) { BestE = E; BestP = FVector(X, Y, 60000.f); BestYaw = FMath::RadiansToDegrees(FMath::Atan2(Dn.Y, Dn.X)); UE_LOG(LogDustlineDrive, Display, TEXT("FORD dentro depth=%.2f flow=%.2f en %.0f,%.0f"), D, Fl.Size(), X, Y); }
				continue;
			}
			FVector2D Dir = Fl.SizeSquared() > 0.01f ? FVector2D(-Fl.Y, Fl.X).GetSafeNormal() : FVector2D(FMath::Cos(R.FRand() * 6.28f), FMath::Sin(R.FRand() * 6.28f));
			// orillas: a cada lado el suelo sale del agua en menos de 75 m sin cortados (escalones < 1,1 m cada 3 m)
			bool bOk = true; float Rough = 0.f, DryBack = 0.f;
			for (int32 sd = -1; sd <= 1 && bOk; sd += 2)
			{
				float Prev = G0; int32 Dry = -1;
				for (int32 j = 1; j <= 25; ++j)
				{
					const float Gj = Ground(X + sd * Dir.X * j * 300.f, Y + sd * Dir.Y * j * 300.f);
					if (FMath::Abs(Gj - Prev) > 70.f) { bOk = false; break; }
					Rough += FMath::Abs(Gj - Prev); Prev = Gj;
					if (Gj > Lv + 40.f) { Dry = j; break; }
				}
				if (Dry < 0) bOk = false; else if (sd < 0) DryBack = Dry * 300.f;
			}
			if (!bOk) continue;
			const float E = FMath::Abs(D - FordD) + Rough / 20000.f;
			if (E < BestE) { BestE = E; BestP = FVector(X - Dir.X * (DryBack + 900.f), Y - Dir.Y * (DryBack + 900.f), 60000.f); BestYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)); UE_LOG(LogDustlineDrive, Display, TEXT("FORD cand depth=%.2f flow=%.2f centre=%.0f,%.0f orilla=%.0f m e=%.3f"), D, Fl.Size(), X, Y, DryBack / 100.f, E); }
		}
		UE_LOG(LogDustlineDrive, Display, TEXT("FORD puntos con esa profundidad: %d"), NDepth);
		if (BestE < 1e8f) SetActorLocationAndRotation(BestP, FRotator(0.f, BestYaw, 0.f));
	}
	FHitResult Hit; const FVector L0 = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, L0 + FVector(0, 0, 3000), L0 - FVector(0, 0, 80000), ECC_Visibility))
		SetActorLocation(FVector(L0.X, L0.Y, Hit.ImpactPoint.Z + (bRollTest ? 160.f : ZCom + 25.f)), false, nullptr, ETeleportType::TeleportPhysics);
	if (bRollTest) { FRotator R = GetActorRotation(); R.Roll = RollDeg; SetActorRotation(R); }
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
	bStalled = bHydrolock; Flood = 0.f; Gear = 1; InvertT = OverT = 0.f; bOverturned = false;
}

void ADustlineCar::UpdateWater()
{
	// ¿hay agua bajo alguna parte del coche? (centro y esquinas; el detalle por rueda y por columna va en cada subpaso)
	bNearWater = false;
	const FVector L = GetActorLocation(), Fw = GetActorForwardVector() * (Sp.BodyL * 60.f), Rt = GetActorRightVector() * (Sp.BodyW * 60.f);
	const FVector Pts[5] = { L, L + Fw + Rt, L + Fw - Rt, L - Fw + Rt, L - Fw - Rt };
	for (const FVector& Q : Pts)
	{
		float Lv; FVector2D Fl;
		if (WaterAt(Q.X, Q.Y, Lv, Fl) && Lv / 100.f > L.Z / 100.f + Sp.BodyBot - 1.2f) { bNearWater = true; break; }
	}
	if (!bNearWater) { WaterZ = -1e9f; bInWater = false; WaterFlow = FVector2D::ZeroVector; CurrentMs = 0.f; }
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
	if (!Sp.bDiesel && EngineBayWater > 0.f)   // gasolina con el encendido mojado: tirones y menos fuerza
		Te *= 1.f - 0.55f * Smooth(0.f, 0.25f, EngineBayWater) * (0.6f + 0.4f * FMath::Abs(FMath::Sin(Clock * 11.f + SubStep * 0.37f)));
	// control de tracción: la rueda motriz que patina sola se frena (el par pasa por el diferencial abierto a las demás);
	// solo si patinan todas se recorta el par del motor
	float TcB[4] = {};
	{
		float Rt[4] = {}, Sum = 0.f, Best2 = 0.f; int32 Nd = 0;
		for (int32 i = 0; i < 4; ++i) if (Driven[i] && Wh[i].Contact) { Rt[i] = Wh[i].Slip * FMath::Sign(Ratio) / FMath::Max(Wh[i].Peak, 0.05f); Sum += Rt[i]; ++Nd; }
		const float Avg = Nd > 0 ? Sum / Nd : 0.f;
		if (bTc && thr > 0.f)
			for (int32 i = 0; i < 4; ++i) if (Driven[i] && Rt[i] > 1.4f && Rt[i] > Avg + 0.4f) TcB[i] = FMath::Clamp((Rt[i] - 1.2f) * 0.3f, 0.f, 0.7f);
		for (int32 i = 0; i < 4; ++i) if (Driven[i] && TcB[i] <= 0.f) Best2 = FMath::Max(Best2, Rt[i]);
		if (bTc && thr > 0.f && FMath::Max(Avg, Best2) > 1.6f) TcCut = FMath::Max(0.05f, TcCut - dt * 5.f);
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
	struct FC { bool C = false; FVector Hit, N, Rc, Wf, Lat; float Fs = 0, Comp = 0, Crr = 0; FGround G; FTire T; float Vl = 0, Vs = 0; bool bWater = false; float Upr = 1.f; } Cw[4];
	const float Nnom = mass * Gr / 4.f;
	// agua en cada rueda (nivel y corriente en su sitio: a caballo de la orilla una rueda vadea y la otra no)
	float WLv[4]; FVector WFl[4], WAnchor[4];
	auto WaterHere = [this](const FVector& Pm, float& Lv, FVector& Fl) -> bool
	{
		Lv = -1e9f; Fl = FVector::ZeroVector; if (!bNearWater) return false;
		float L; FVector2D F; if (!WaterAt(Pm.X * 100.f, Pm.Y * 100.f, L, F)) return false;
		Lv = L / 100.f; Fl = FVector(F.X, F.Y, 0.f); return true;
	};
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector WP = WheelPos(i) / 100.0;
		const FVector r = Q.RotateVector(FVector(WP.X - ComX, WP.Y, zA - ZCom / 100.f));
		const FVector a = P + r;
		WAnchor[i] = a; WaterHere(a, WLv[i], WFl[i]);
		const FVector Start = a * 100.0 + UpB * 60.0, End = Start - UpB * ((Sp.RestLen + Sp.R) * 100.0 + 160.0);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) { Wh[i].Contact = false; Wh[i].Length = FMath::FInterpTo(Wh[i].Length, Sp.RestLen, dt, 8.f); Wh[i].Sink *= 1.f - dt * 2.f; continue; }
		const FVector n = Hit.ImpactNormal.GetSafeNormal();
		// el neumático solo agarra con la banda de rodadura: si el suelo no queda más o menos debajo de la rueda (coche de lado,
		// volcado o la rueda contra un talud) no hay contacto de rueda; lo que toque lo llevan los puntos de la carrocería
		const float CosA = (float)FVector::DotProduct(n, UpB);
		if (CosA < 0.35f) { Wh[i].Contact = false; Wh[i].Length = FMath::FInterpTo(Wh[i].Length, Sp.RestLen, dt, 8.f); continue; }
		FC& c = Cw[i];
		c.Upr = Smooth(0.35f, 0.65f, CosA);   // de 50° a 70° el agarre se va perdiendo (flanco del neumático)
		c.G = GroundAt(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z, n.Z, Setup.TireType);
		const float WdHit = WLv[i] - (float)Hit.ImpactPoint.Z / 100.f;   // agua sobre la huella
		if (WdHit > 0.f) Submerge(c.G, WdHit, WFl[i].Size(), Setup.TireType);   // fondo del río: cantos con verdín o limo
		const float Bump = c.G.Bump * (VNoise(Hit.ImpactPoint.X / 70.f, Hit.ImpactPoint.Y / 70.f) * 2.f - 1.f) + c.G.Bump * 0.4f * (VNoise(Hit.ImpactPoint.X / 23.f + 7.f, Hit.ImpactPoint.Y / 23.f) * 2.f - 1.f);
		const float dist = ((Hit.Distance - 60.0) / 100.0) * CosA;
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
		c.bWater = WdHit > 0.05f;
	}
	// profundidad del agua en cada rueda (también las que van al aire, p. ej. flotando)
	float WDep[4];
	for (int32 i = 0; i < 4; ++i)
	{
		const float Bot = (float)(WAnchor[i] - UpB * (Wh[i].Length + Sp.R)).Z;
		WDep[i] = FMath::Clamp(WLv[i] - Bot, 0.f, 2.f * Sp.R); Wh[i].WaterD = WDep[i];
	}
	// barras estabilizadoras
	for (int32 ax = 0; ax < 2; ++ax)
	{
		FC& A = Cw[ax * 2]; FC& B = Cw[ax * 2 + 1];
		const float Arb = Sp.Arb * ((A.C ? A.Comp : 0.f) - (B.C ? B.Comp : 0.f));
		if (A.C) A.Fs += Arb; if (B.C) B.Fs -= Arb;
	}

	// ---------------- neumáticos (fase 2) + resolución implícita de las ruedas ----------------
	float Am[4][4] = {}, Bv[4] = {}, Xw[4] = {}, Ieff[4], Fx0[4] = {}, Trr[4] = {}, PaddleF[4] = {}; FVector PaddleDir[4];
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
			// vadeo rápido: la ola bajo la rueda la levanta (hidroplaneo en agua profunda); despacio el agua apenas quita agarre
			if (c.bWater)
			{
				static const float PlaneTire[5] = { 1.f, 0.85f, 0.65f, 0.9f, 1.3f };
				const float Plane = Smooth(4.f, 11.f, FMath::Abs(c.Vl)) * Smooth(0.05f, 0.35f, WDep[i]) * PlaneTire[FMath::Clamp(Setup.TireType, 0, 4)];
				Mu *= 1.f - 0.55f * FMath::Clamp(Plane, 0.f, 1.f);
			}
			// neumático embotado de barro: el dibujo lleno de barro es casi un slick (el de barro se limpia solo al girar)
			{
				static const float ClogRate[5] = { 1.3f, 1.f, 0.45f, 1.f, 0.8f }, SelfClean[5] = { 0.4f, 0.7f, 1.6f, 0.7f, 0.8f };
				const int32 Tt = FMath::Clamp(Setup.TireType, 0, 4);
				const float Spin = FMath::Abs(Wh[i].Omega * Sp.R), SlipV = FMath::Abs(Wh[i].Omega * Sp.R - c.Vl);
				float& Cg = Wh[i].Clog;
				Cg += dt * c.G.W[SMud] * (SlipV * 0.025f + FMath::Abs(c.Vl) * 0.003f) * ClogRate[Tt] * (1.f - Cg);
				Cg -= dt * Cg * (Spin * 0.02f * SelfClean[Tt] + (1.f - c.G.W[SMud]) * 0.04f * FMath::Min(FMath::Abs(c.Vl), 10.f) * 0.1f);
				if (WDep[i] > 0.2f * Sp.R) Cg -= dt * Cg * (0.35f + 0.05f * Spin);   // el agua lava el dibujo
				Cg = FMath::Clamp(Cg, 0.f, 1.f);
				Mu *= 1.f - 0.32f * Cg * (1.f - 0.4f * c.G.W[SMud]);
			}
			// aquaplaning: lámina de agua en llano empapado a partir de ~50 km/h (el balón flota antes, el de barro evacua mejor)
			{
				static const float AquaTire[5] = { 1.f, 0.8f, 0.6f, 0.9f, 1.3f };
				const float Film = Smooth(0.6f, 0.95f, c.G.Wet) * Smooth(0.96f, 0.995f, (float)c.N.Z) * FMath::Clamp(0.3f + c.G.W[SMud] + c.G.W[SDirt] + c.G.W[SGrass], 0.f, 1.f);
				const float Aqua = Film * Smooth(13.f, 25.f, FMath::Abs(c.Vl)) * AquaTire[FMath::Clamp(Setup.TireType, 0, 4)];
				Mu *= 1.f - 0.6f * FMath::Clamp(Aqua, 0.f, 1.f);
			}
			Mu *= c.Upr;
			c.T = { FMath::Max(Mu, 0.04f * c.Upr + 0.001f), c.G.PeakSlip, c.G.PeakAlpha * H.TyreAlpha, c.G.Falloff };
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
		// rueda que gira dentro del agua: el dibujo bate el agua (frena la rueda y empuja algo el coche, sobre todo flotando)
		float Jw = 0.f;
		PaddleF[i] = 0.f;
		if (WDep[i] > 0.02f)
		{
			static const float PaddleCd[5] = { 0.25f, 0.45f, 1.f, 0.45f, 0.6f };
			FVector wd = Fwd; if (i < 2) wd = Q.RotateVector(FVector(FMath::Cos(SteerAngle), -FMath::Sin(SteerAngle), 0));
			const FVector Ctr = WAnchor[i] - UpB * Wh[i].Length;
			const float Vw = FVector::DotProduct(Vel + FVector::CrossProduct(AngVel, Ctr - P) - WFl[i], wd);
			const float kP = 0.5f * Rho * PaddleCd[FMath::Clamp(Setup.TireType, 0, 4)] * Sp.Width * WDep[i] * 0.5f;
			const float u = Wh[i].Omega * Sp.R - Vw;
			PaddleF[i] = kP * u * FMath::Abs(u); PaddleDir[i] = wd;
			Trr[i] += PaddleF[i] * Sp.R;
			Jw = 2.f * kP * FMath::Abs(u) * Sp.R * Sp.R;
		}
		Am[i][i] = Ieff[i] / dt + Sp.R * J + Jw;
		Bv[i] = (Ieff[i] / dt + Sp.R * J + Jw) * Wh[i].Omega + WheelTorque * Share[i] - Sp.R * Fx0[i] - Trr[i];
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
		float Tb = (brake * Sp.BrakeT * (i < 2 ? 0.32f : 0.18f) + (EscB[i] + TcB[i]) * Sp.BrakeT * 0.3f) * (1.f - 0.5f * WetBrakes);   // discos mojados
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

	// agua: flotación por columnas (cada una con su nivel y su corriente), arrastre, ola de proa, inundación y toma de aire
	WaterDepthM = 0.f; BowWave = 0.f;
	bool bWet = false; float TopLv = -1e9f, FlowSum = 0.f; int32 FlowN = 0; FVector2D FlowAcc = FVector2D::ZeroVector;
	if (bNearWater)
	{
		const float CL = Sp.BodyL / 3.f, CW = Sp.BodyW / 3.f, HBody = Sp.BodyTop - Sp.BodyBot;
		// velocidad respecto al agua, en ejes del coche (para la ola de proa)
		float Lc; FVector Fc; WaterHere(P, Lc, Fc);
		const float vFwdW = FVector::DotProduct(Vel - Fc, Fwd);
		for (int32 ix = 0; ix < 3; ++ix) for (int32 iy = 0; iy < 3; ++iy)
		{
			const FVector Lp((ix - 1) * CL - ComX, (iy - 1) * CW, Sp.BodyBot);
			const FVector Wb = P + Q.RotateVector(Lp), Wt = P + Q.RotateVector(FVector(Lp.X, Lp.Y, Sp.BodyTop));
			float Lv; FVector Flow; if (!WaterHere(Wb, Lv, Flow)) continue;
			// columna de la carrocería entre los bajos y el techo (vale también volcado o de lado)
			const FVector Lo = Wb.Z <= Wt.Z ? Wb : Wt, Hi = Wb.Z <= Wt.Z ? Wt : Wb;
			const float Hc = FMath::Max((float)(Hi.Z - Lo.Z), 0.05f);
			const float h = FMath::Clamp(Lv - (float)Lo.Z, 0.f, Hc);
			if (h <= 0.f) continue;
			bWet = true; TopLv = FMath::Max(TopLv, Lv); FlowAcc += FVector2D(Flow.X, Flow.Y); ++FlowN;
			WaterDepthM = FMath::Max(WaterDepthM, FMath::Min(h, HBody));
			// el empuje actúa en el centro de la parte sumergida de la columna (a lo largo de su eje): al escorar se va al lado
			// que se hunde y endereza el coche (estabilidad de flotación)
			const FVector Pt = Lo + (Hi - Lo) * (0.5f * h / Hc), rp = Pt - P;
			// ~42 % de la caja desplaza agua: pasos de rueda, vano motor y bajos se llenan enseguida
			const float Vol = CL * CW * HBody * (h / Hc) * 0.42f * (1.f - 0.85f * Flood);
			const FVector Fb(0, 0, Rho * Gr * Vol);
			F += Fb; T += FVector::CrossProduct(rp, Fb);
			// arrastre: de frente solo la fila de columnas que empuja el agua, de lado solo la columna del lado que avanza
			const FVector vrel = Vel + FVector::CrossProduct(AngVel, rp) - Flow;
			const FVector vb = Q.UnrotateVector(vrel);
			const bool bFront = (vb.X > 0.f && ix == 2) || (vb.X < 0.f && ix == 0), bSide = (vb.Y > 0.f && iy == 2) || (vb.Y < 0.f && iy == 0);
			const FVector Db(bFront ? -0.5f * Rho * 0.9f * (CW * h) * vb.X * FMath::Abs(vb.X) : -0.5f * Rho * 0.05f * (CW * h) * vb.X * FMath::Abs(vb.X),
				bSide ? -0.5f * Rho * 1.1f * (CL * h) * vb.Y * FMath::Abs(vb.Y) : 0.f, -0.5f * Rho * 1.3f * (CL * CW) * vb.Z * FMath::Abs(vb.Z));
			const FVector Fd = Q.RotateVector(Db);
			// el agua empuja la carrocería por delante a media altura de lo sumergido, no por los bajos (no lo "zancadillea")
			const FVector rd(rp.X, rp.Y, rp.Z * 0.35f);
			F += Fd; T += FVector::CrossProduct(rd, Fd);
		}
		// ruedas en el agua: de canto ofrecen mucha superficie (hacen de timón al flotar), de frente poca; y el empuje del dibujo
		for (int32 i = 0; i < 4; ++i) if (WDep[i] > 0.02f)
		{
			bWet = true; TopLv = FMath::Max(TopLv, WLv[i]); FlowAcc += FVector2D(WFl[i].X, WFl[i].Y); ++FlowN;
			const FVector Ctr = WAnchor[i] - UpB * Wh[i].Length, rc = Ctr - P;
			FVector wd = Fwd; if (i < 2) wd = Q.RotateVector(FVector(FMath::Cos(SteerAngle), -FMath::Sin(SteerAngle), 0));
			const FVector side = FVector::CrossProduct(UpB, wd).GetSafeNormal();
			const FVector vrel = Vel + FVector::CrossProduct(AngVel, rc) - WFl[i];
			const float vf = FVector::DotProduct(vrel, wd), vs = FVector::DotProduct(vrel, side);
			const FVector Fd = -wd * (0.5f * Rho * 0.8f * Sp.Width * WDep[i] * vf * FMath::Abs(vf)) - side * (0.5f * Rho * 1.0f * 1.6f * Sp.R * WDep[i] * vs * FMath::Abs(vs))
				+ PaddleDir[i] * PaddleF[i];
			F += Fd; T += FVector::CrossProduct(rc, Fd);
		}
		if (bWet)
		{
			// ola de proa: al avanzar el agua se apila delante (~v²/2g); a paso de hombre deja un seno detrás que baja el nivel en el vano motor
			const float vf = FMath::Max(vFwdW, 0.f);
			const float Trough = 0.15f * Smooth(0.7f, 1.6f, vf) * (1.f - Smooth(2.4f, 3.8f, vf));
			// la ola no puede ser más alta que el agua que empuja (agua somera): profundidad delante = la de las ruedas delanteras
			const float DFront = FMath::Max(WDep[0], WDep[1]) + FMath::Max(0.f, WaterDepthM - Sp.R);
			BowWave = FMath::Min3(0.6f * vf * vf / (2.f * Gr), 1.0f * DFront, 0.9f);
			const FVector IntakeP = P + Q.RotateVector(FVector(0.3f * Sp.BodyL - ComX, 0.f, Sp.IntakeZ));
			float LvI; FVector FlI; if (!WaterHere(IntakeP, LvI, FlI)) LvI = TopLv;
			const float LvIntake = LvI + BowWave * 0.6f - Trough;
			const float Sill = Sp.BodyBot + 0.35f;
			const float OverSill = TopLv + BowWave * 0.3f - (float)(P + Q.RotateVector(FVector(0, 0, Sill))).Z;
			if (OverSill > 0.f) Flood = FMath::Min(1.f, Flood + dt * 0.012f * FMath::Clamp(OverSill / 0.5f, 0.f, 1.f));
			// gasolina: el agua en el encendido hace que falle antes de ahogarse (el diésel no tiene encendido)
			const float BayZ = (float)(P + Q.RotateVector(FVector(0.35f * Sp.BodyL - ComX, 0.f, Sp.BodyBot + 0.3f * (Sp.BodyTop - Sp.BodyBot)))).Z;
			EngineBayWater = FMath::Max(0.f, LvI + BowWave * 0.6f - Trough - BayZ);
			const float IntakeWZ = (float)IntakeP.Z;
			if (LvIntake > IntakeWZ && !bStalled)
			{
				bStalled = true;
				// acelerado y con vueltas el motor aspira agua y se rompe (hidrobloqueo); al ralentí solo se cala
				if (Rpm > 1800.f && thr > 0.3f) { bHydrolock = true; Damage[0] = FMath::Max(Damage[0], 0.6f); }
				UE_LOG(LogDustlineDrive, Display, TEXT("DRIVE motor ahogado (agua %.2f > toma %.2f, ola %.2f, rpm %.0f, hidrobloqueo %d)"), LvIntake, IntakeWZ, BowWave, Rpm, bHydrolock ? 1 : 0);
			}
			if (LvIntake < IntakeWZ) StallT += dt; else StallT = 0.f;
			WetTyres = 1.f;
			for (int32 i = 0; i < 4; ++i) if (WDep[i] > Sp.R * 0.7f) WetBrakes = 1.f;   // el buje bajo el agua moja los discos
		}
	}
	bInWater = bWet;
	if (bWet) { WaterZ = TopLv; if (FlowN > 0) { WaterFlow = FlowAcc / FlowN; CurrentMs = WaterFlow.Size(); } }
	else { WaterZ = -1e9f; WaterFlow = FVector2D::ZeroVector; CurrentMs = 0.f; EngineBayWater = 0.f; StallT += dt; Flood = FMath::Max(0.f, Flood - dt * 0.01f); WetTyres = FMath::Max(0.f, WetTyres - dt * 0.08f); }
	// los discos se secan con el calor de frenar (y muy poco con el aire)
	WetBrakes = FMath::Max(0.f, WetBrakes - dt * (0.006f + 0.22f * brake * Smooth(1.f, 8.f, Vel.Size())));
	// flota si las ruedas apenas cargan peso y hay agua bajo el coche
	{ float Ld = 0.f; for (int32 i = 0; i < 4; ++i) Ld += Wh[i].Contact ? Wh[i].Load : 0.f; bFloating = bWet && WaterDepthM > 0.2f && Ld < 0.2f * mass * Gr; }
	// volcado: boca abajo el motor se queda sin aceite ni gasolina y se para; de lado o boca abajo y casi parado = "Volcado"
	{
		const float UpZ = (float)UpB.Z;
		InvertT = UpZ < -0.3f ? InvertT + dt : 0.f;
		if (InvertT > 3.f && !bStalled) { bStalled = true; StallT = 0.f; UE_LOG(LogDustlineDrive, Display, TEXT("DRIVE motor parado: coche boca abajo")); }
		if (InvertT > 0.f) StallT = 0.f;
		OverT = (UpZ < 0.4f && Vel.Size() < 3.f) ? OverT + dt : 0.f;
		bOverturned = OverT > 1.5f;
	}
	if (bStalled && !bHydrolock && StallT > 2.5f) bStalled = false;

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

void DustlineCareerSelfTest(ADustlineCar* Car);
void ADustlineCar::UpdateDirt(float Dt, float RainNow)
{
	static const float Forced = [] { float v = -1.f; FParse::Value(FCommandLine::Get(), TEXT("ddirt="), v); return v; }();
	if (Forced >= 0.f) { Dirt = Forced; DirtH = 75.f; }
	else if (bGarage || bShowcase) { Dirt = 0.f; DirtH = 40.f; }   // en el taller lo lavan
	else if (!bPaused)
	{
		// las ruedas levantan barro según lo blando y mojado del suelo, la velocidad y el patinaje
		float Soil = 0.f, Sl = 0.f;
		for (const FWheelPhys& W : Wh) if (W.Contact && !W.bWater)
		{
			Soil += W.WMud + W.WDirt * (0.15f + 0.55f * W.Wet) + W.WSand * 0.25f + W.WSnow * 0.05f + W.WGrass * 0.08f * W.Wet;
			Sl = FMath::Max(Sl, FMath::Abs(W.Slip));
		}
		const float V = FVector2D(Vel).Size();
		const float Gain = 0.0016f * Soil * FMath::Min(V, 20.f) * (1.f + 2.5f * FMath::Min(Sl, 1.f));
		Dirt = FMath::Min(1.f, Dirt + Dt * Gain * (1.1f - Dirt));
		// cuanto más rápido y más patina, más alto salpica (hasta la cintura)
		const float Splash = 35.f + 4.5f * FMath::Min(V, 20.f) + 30.f * FMath::Min(Sl, 1.f);
		if (Gain > 0.f) DirtH = FMath::Min(130.f, FMath::Max(DirtH, FMath::Lerp(DirtH, Splash, FMath::Min(1.f, Dt * 0.6f))));
		// el agua lava lo que cubre (vadear) y la lluvia, poco a poco
		if (WaterDepthM > 0.2f) { Dirt = FMath::Max(0.f, Dirt - Dt * 0.06f * FMath::Clamp(WaterDepthM / 0.6f, 0.f, 1.f) * (0.5f + 0.1f * V)); DirtH = FMath::Max(40.f, DirtH - Dt * 8.f); }
		Dirt = FMath::Max(0.f, Dirt - Dt * 0.0035f * RainNow);
	}
	if (FMath::Abs(Dirt - DirtShown) > 0.01f || FMath::Abs(DirtH - DirtHShown) > 2.f)
	{
		DirtShown = Dirt; DirtHShown = DirtH;
		for (const TPair<int32, UMaterialInterface*>& KV : Mats)
			if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(KV.Value))
			{
				// los cristales y las luces se ensucian menos (se limpian con el limpiaparabrisas / no se ve el barro emisivo)
				const float K = (KV.Key == SGlass) ? 0.35f : ((KV.Key == SLight || KV.Key == STail || KV.Key == SAmber) ? 0.2f : 1.f);
				MI->SetScalarParameterValue(TEXT("Dirt"), Dirt * K); MI->SetScalarParameterValue(TEXT("DirtH"), DirtH);
			}
	}
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
	else if (float WtKmh = 0.f; FParse::Value(FCommandLine::Get(), TEXT("watertest="), WtKmh))
	{
		// prueba de vadeo: recto a velocidad fija (control de crucero sencillo) cruzando el agua; registra lo que pasa
		const float T = Clock - 2.f;
		if (T > 0.f) Throttle = FMath::Clamp((WtKmh - SpeedKmh) * 0.15f, 0.f, 1.f);
		static float PT = 0.f; PT += dt;
		if (PT > 0.5f && T > 0.f)
		{
			PT = 0.f; float Ld = 0.f; for (int32 i = 0; i < 4; ++i) Ld += Wh[i].Contact ? Wh[i].Load : 0.f;
			UE_LOG(LogDustlineDrive, Display, TEXT("WT t=%.1f v=%.1f thr=%.2f surf=%s depth=%.2f wd=%.2f/%.2f bow=%.2f cur=%.2f flood=%.2f stall=%d hydro=%d float=%d load=%.2f wbrk=%.2f clog=%.2f bay=%.2f rpm=%.0f gear=%d yaw=%.0f up=%.2f slip=%.2f/%.2f/%.2f/%.2f sink=%.2f tc=%.2f loc=%s"),
				T, SpeedKmh, Throttle, *SurfaceName, WaterDepthM, Wh[0].WaterD, Wh[2].WaterD, BowWave, CurrentMs, Flood, bStalled ? 1 : 0, bHydrolock ? 1 : 0, bFloating ? 1 : 0, Ld / (Sp.Mass * 9.81f), WetBrakes, TyreClog(), EngineBayWater, Rpm, Gear,
				GetActorRotation().Yaw, GetActorUpVector().Z, Wh[0].Slip, Wh[1].Slip, Wh[2].Slip, Wh[3].Slip, MaxSinkM(), TcCut, *GetActorLocation().ToString());
		}
		if (T > 28.f) FPlatformMisc::RequestExit(false);
	}
	else if (float TtKmh = 0.f; FParse::Value(FCommandLine::Get(), TEXT("trailtest="), TtKmh))
	{
		// prueba de pistas: piloto automático (persecución pura) por la pista que sale del campamento; registra progreso y estado
		static TArray<FVector2D> Path; static int32 Seg = 0; static float Done = 0.f, PT = 0.f, MaxPitch = 0.f, MaxRoll = 0.f; static FVector LastL;
		if (Path.Num() == 0)
		{
			TArray<FString> Lines; FFileHelper::LoadFileToStringArray(Lines, *(FPaths::ProjectContentDir() / TEXT("Data") / TEXT("trails.txt")));
			int32 Which = 0; FParse::Value(FCommandLine::Get(), TEXT("dtrail="), Which);
			const FVector2D L(GetActorLocation()); float Bd = 1e30f; int32 Best = -1; bool bRev = false;
			for (int32 k = 0; k < Lines.Num(); ++k)
			{
				TArray<FString> P; Lines[k].ParseIntoArray(P, TEXT(";")); if (P.Num() < 2) continue;
				auto Pt = [&](const FString& S) { FString A, B; S.Split(TEXT(","), &A, &B); return FVector2D(FCString::Atof(*A), FCString::Atof(*B)); };
				const float D0 = FVector2D::Distance(Pt(P[0]), L), D1 = FVector2D::Distance(Pt(P.Last()), L);
				if (Which > 0) { if (k == Which - 1) { Best = k; bRev = D1 < D0; } continue; }
				if (FMath::Min(D0, D1) < Bd) { Bd = FMath::Min(D0, D1); Best = k; bRev = D1 < D0; }
			}
			if (Best >= 0)
			{
				TArray<FString> P; Lines[Best].ParseIntoArray(P, TEXT(";"));
				for (const FString& S : P) { FString A, B; S.Split(TEXT(","), &A, &B); Path.Add(FVector2D(FCString::Atof(*A), FCString::Atof(*B))); }
				if (bRev) Algo::Reverse(Path);
				// empieza en el principio de la pista, mirando hacia ella
				FHitResult H; const FVector St(Path[0].X, Path[0].Y, 0.f);
				if (GetWorld()->LineTraceSingleByChannel(H, St + FVector(0, 0, 80000), St - FVector(0, 0, 30000), ECC_Visibility))
				{
					const FVector2D D = Path[FMath::Min(3, Path.Num() - 1)] - Path[0];
					SetActorLocationAndRotation(H.ImpactPoint + FVector(0, 0, ZCom + 30.f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f), false, nullptr, ETeleportType::TeleportPhysics);
					Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector;
				}
				LastL = GetActorLocation();
				UE_LOG(LogDustlineDrive, Display, TEXT("TT pista %d con %d puntos, invertida %d"), Best + 1, Path.Num(), bRev ? 1 : 0);
			}
		}
		const float T = Clock - 2.f;
		if (Path.Num() > 1 && T > 0.f)
		{
			const FVector2D L(GetActorLocation());
			// proyección sobre el tramo más cercano (ventana alrededor del último) y punto objetivo 9 m más adelante
			{
				float Bd = 1e30f; int32 Bs = Seg;
				for (int32 k = FMath::Max(0, Seg - 4); k < FMath::Min(Path.Num() - 1, Seg + 30); ++k)
				{
					const FVector2D A = Path[k], B = Path[k + 1]; const float t = FMath::Clamp(FVector2D::DotProduct(L - A, B - A) / FMath::Max(1.f, (B - A).SizeSquared()), 0.f, 1.f);
					const float D = FVector2D::DistSquared(L, A + (B - A) * t); if (D < Bd) { Bd = D; Bs = k; }
				}
				Seg = Bs;
			}
			auto Ahead = [&](float Dist) { float Acc = 0.f; for (int32 k = Seg; k < Path.Num() - 1; ++k) { const float l = FVector2D::Distance(Path[k], Path[k + 1]); if (Acc + l >= Dist) return FMath::Lerp(Path[k], Path[k + 1], (Dist - Acc) / FMath::Max(l, 1.f)); Acc += l; } return Path.Last(); };
			const FVector2D Tgt = Ahead(900.f);
			const float Yaw = GetActorRotation().Yaw, Want = FMath::RadiansToDegrees(FMath::Atan2(Tgt.Y - L.Y, Tgt.X - L.X));
			const float Err = FMath::FindDeltaAngleDegrees(Yaw, Want);
			Steer = FMath::Clamp(-Err / 22.f, -1.f, 1.f);
			// velocidad según la curva de los próximos 25 m (aceleración lateral ~0,3 g)
			const FVector2D P1 = Ahead(500.f), P2 = Ahead(1500.f), P3 = Ahead(2500.f);
			const float Turn = FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(P2.Y - P1.Y, P2.X - P1.X)), FMath::RadiansToDegrees(FMath::Atan2(P3.Y - P2.Y, P3.X - P2.X))));
			const float Rad = 10.f / FMath::Max(FMath::DegreesToRadians(Turn), 0.01f);
			const float Want2 = FMath::Min(TtKmh, FMath::Sqrt(3.f * Rad) * 3.6f) * FMath::Lerp(1.f, 0.5f, FMath::Clamp(FMath::Abs(Err) / 45.f, 0.f, 1.f));
			// agua delante: se entra al paso (como haría un conductor) para no levantar ola
			float Wv = Want2; { float Lv; FVector2D Fl; for (const float Da : { 1000.f, 2000.f, 3000.f }) { const FVector2D Aw = Ahead(Da); if (DustlineWorld::WaterAt(Aw.X, Aw.Y, Lv, Fl)) Wv = FMath::Min(Wv, 6.f + Da / 300.f); } if (bInWater) Wv = FMath::Min(Wv, 6.f); }
			Throttle = FMath::Clamp((FMath::Max(Wv, 5.f) - SpeedKmh) * 0.12f, -1.f, 1.f);
			Done += FVector2D::Distance(L, FVector2D(LastL)) / 100.f; LastL = GetActorLocation();
			MaxPitch = FMath::Max(MaxPitch, FMath::Abs(GetActorRotation().Pitch)); MaxRoll = FMath::Max(MaxRoll, FMath::Abs(GetActorRotation().Roll));
			PT += dt;
			if (PT > 3.f)
			{
				PT = 0.f; float Off = 1e9f;   // distancia a la pista
				for (int32 k = FMath::Max(0, Seg - 2); k < FMath::Min(Path.Num() - 1, Seg + 3); ++k) { const FVector2D A = Path[k], B = Path[k + 1]; const float t = FMath::Clamp(FVector2D::DotProduct(L - A, B - A) / FMath::Max(1.f, (B - A).SizeSquared()), 0.f, 1.f); Off = FMath::Min(Off, FVector2D::Distance(L, A + (B - A) * t) / 100.f); }
				UE_LOG(LogDustlineDrive, Display, TEXT("TT t=%.0f v=%.0f recorrido=%.0f m tramo=%d/%d fuera=%.1f m pitch=%.0f roll=%.0f max=%.0f/%.0f surf=%s up=%.2f agua=%.2f barro=%.2f/%.0fcm cr=%d"), T, SpeedKmh, Done, Seg, Path.Num(), Off, GetActorRotation().Pitch, GetActorRotation().Roll, MaxPitch, MaxRoll, *SurfaceName, GetActorUpVector().Z, WaterDepthM, Dirt, DirtH, DustlineCareer::Credits());
			}
			if (Seg >= Path.Num() - 2 && FVector2D::Distance(L, Path.Last()) < 1500.f) { UE_LOG(LogDustlineDrive, Display, TEXT("TT FIN pista completa: %.0f m en %.0f s"), Done, T); FPlatformMisc::RequestExit(false); }
		}
		float Lim = 240.f; FParse::Value(FCommandLine::Get(), TEXT("ttmax="), Lim);
		if (T > Lim) { UE_LOG(LogDustlineDrive, Display, TEXT("TT FIN tiempo: %.0f m, tramo %d/%d"), Done, Seg, Path.Num()); FPlatformMisc::RequestExit(false); }
	}
	else if (bAutoDrive) { Throttle = 1.f; Steer = 0.55f * FMath::Sin(Clock * 0.45f); Boost = Clock > 3.f; }
	else if (PC && !bPaused && !bShowcase && !bGarage && !(Menu.IsValid() && Menu->IsOpen()))
	{
		// teclado: gas y volante progresivos (como pisar un pedal), para poder dosificar en barro, nieve o trial
		const float KT = (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) ? 1.f : 0.f);
		const float KS = (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right) ? 1.f : 0.f);
		auto Ramp = [dt](float& V, float Tgt, float Up, float Down) { const float R = (FMath::Abs(Tgt) > FMath::Abs(V) && Tgt * V >= 0.f) ? Up : Down; V = FMath::FInterpConstantTo(V, Tgt, dt, R); };
		Ramp(KeyThr, KT, 2.6f, 6.f); Ramp(KeySteer, KS, 4.5f, 7.f);
		// mando: gatillo derecho gas, izquierdo freno / marcha atrás, stick izquierdo volante (con zona muerta y curva suave)
		const float RT = PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis), LT = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
		float LX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		LX = FMath::Sign(LX) * FMath::Pow(FMath::Clamp((FMath::Abs(LX) - 0.08f) / 0.92f, 0.f, 1.f), 1.4f);
		const float PadT = RT - LT, PadS = -LX;
		Throttle = FMath::Abs(PadT) > 0.03f ? PadT : KeyThr;
		Steer = FMath::Abs(PadS) > 0.01f ? PadS : KeySteer;
		Hand = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
		Boost = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Left);
		auto Pressed = [PC](FKey K1, FKey K2) { return PC->WasInputKeyJustPressed(K1) || PC->WasInputKeyJustPressed(K2); };
		if (Pressed(EKeys::R, EKeys::Gamepad_Special_Left)) ResetUpright();
		if (Pressed(EKeys::C, EKeys::Gamepad_FaceButton_Top)) CamMode = (CamMode + 1) % 3;
		if (Pressed(EKeys::B, EKeys::Gamepad_FaceButton_Right)) bDiffLock = !bDiffLock;
		if (Pressed(EKeys::L, EKeys::Gamepad_DPad_Down) && Sp.bHasLow && SpeedKmh < 8.f) { bLowRange = !bLowRange; Gear = Gear < 0 ? -1 : 1; }
		if (Pressed(EKeys::M, EKeys::Gamepad_DPad_Up)) bMapOpen = !bMapOpen;
		if (bManual)
		{
			if (Pressed(EKeys::E, EKeys::Gamepad_RightShoulder)) { Gear = Gear < 0 ? 0 : FMath::Min(Gear + 1, Sp.NGears); ShiftT = 0.25f; }
			if (Pressed(EKeys::Q, EKeys::Gamepad_LeftShoulder)) { Gear = Gear <= 0 ? -1 : Gear - 1; ShiftT = 0.25f; if (Gear == 0 && SpeedKmh > 5.f) Gear = 1; }
		}
	}
	else { KeyThr = KeySteer = 0.f; }
	// puntos de interés: se descubren al llegar (a menos de 35 m)
	if (!bGarage && !bShowcase)
	{
		// modo carrera: créditos por kilómetro recorrido con las ruedas en el suelo (no cuenta derrapar parado ni volar)
		int32 NC = 0; for (const FWheelPhys& W : Wh) NC += W.Contact ? 1 : 0;
		if (DustlineCareer::Enabled() && NC >= 2 && !bStalled) DustlineCareer::AddDistance(FVector2D(Vel).Size() * dt);
		TArray<DustlineWorld::FPoi>& P = DustlineWorld::Pois();
		const FVector L = GetActorLocation();
		for (DustlineWorld::FPoi& Pt : P)
			if (!Pt.bFound && FVector2D::DistSquared(FVector2D(L), FVector2D(Pt.P)) < 3500.f * 3500.f)
			{
				Pt.bFound = true; DustlineWorld::SaveDiscovered();
				int32 Nf = 0; for (const DustlineWorld::FPoi& Q : P) Nf += Q.bFound ? 1 : 0;
				Toast = FString::Printf(TEXT("Descubierto: %s  \u00B7  %d de %d"), *Pt.Name, Nf, P.Num()); ToastT = Clock;
				if (DustlineCareer::Enabled()) { DustlineCareer::Earn(DustlineCareer::PoiReward); Toast += TEXT("  \u00B7  +") + DustlineCareer::Fmt(DustlineCareer::PoiReward); }
				UE_LOG(LogDustlineDrive, Display, TEXT("POI descubierto %s (%d/%d)"), *Pt.Name, Nf, P.Num());
			}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("dmap")) && Clock > 4.f && Clock < 4.1f) bMapOpen = true;   // capturas del mapa
	{ FString Dl; if (FParse::Value(FCommandLine::Get(), TEXT("dlock="), Dl)) bDiffLock = Dl == TEXT("1"); if (FParse::Param(FCommandLine::Get(), TEXT("dlow"))) bLowRange = Sp.bHasLow; }
	// -probe=X,Y: rejilla de alturas de 60x60 m cada 1 m alrededor de un punto (diagnóstico de pistas) -> /Volumes/SSD/UE-Cache/probe.txt
	if (FString Pr; FParse::Value(FCommandLine::Get(), TEXT("probe="), Pr, false) && Clock > 3.f)
	{
		static bool bDone = false;
		if (!bDone)
		{
			bDone = true; FString A, B; Pr.Split(TEXT(","), &A, &B); const float X0 = FCString::Atof(*A), Y0 = FCString::Atof(*B);
			FString Out; FCollisionQueryParams Pq(SCENE_QUERY_STAT(DustlineProbe), false, this);
			for (int32 j = -30; j <= 30; ++j) { for (int32 i = -30; i <= 30; ++i) { FHitResult H; const FVector P(X0 + i * 100.f, Y0 + j * 100.f, 0.f); float Z = -1e5f; float Lv; FVector2D Fl;
				if (GetWorld()->LineTraceSingleByChannel(H, P + FVector(0, 0, 90000), P - FVector(0, 0, 30000), ECC_Visibility, Pq)) Z = H.ImpactPoint.Z;
				const float W = DustlineWorld::WaterAt(P.X, P.Y, Lv, Fl) ? Lv : -1e5f;
				Out += FString::Printf(TEXT("%.0f %.0f "), Z, W); } Out += TEXT("\n"); }
			FFileHelper::SaveStringToFile(Out, TEXT("/Volumes/SSD/UE-Cache/probe.txt"));
			FPlatformMisc::RequestExit(false);
		}
	}
	// -trailprofile: perfil real de cada pista sobre el terreno (rampa en 6 m e inclinación lateral a ±2,5 m), cada 2 m
	if (FParse::Param(FCommandLine::Get(), TEXT("trailprofile")) && Clock > 3.f)
	{
		static bool bDone = false;
		if (!bDone)
		{
			bDone = true;
			TArray<FString> Lines; FFileHelper::LoadFileToStringArray(Lines, *(FPaths::ProjectContentDir() / TEXT("Data") / TEXT("trails.txt")));
			FCollisionQueryParams Pq(SCENE_QUERY_STAT(DustlineTrailProf), false, this);
			auto Gz = [&](const FVector2D& P, bool& bLand) { FHitResult H; bLand = false; if (!GetWorld()->LineTraceSingleByChannel(H, FVector(P.X, P.Y, 90000), FVector(P.X, P.Y, -30000), ECC_Visibility, Pq)) return 0.f; bLand = H.GetComponent() && H.GetComponent()->IsA<ULandscapeHeightfieldCollisionComponent>(); return (float)H.ImpactPoint.Z; };
			FString Csv;
			for (int32 k = 0; k < Lines.Num(); ++k)
			{
				TArray<FString> P; Lines[k].ParseIntoArray(P, TEXT(";")); TArray<FVector2D> Pts;
				for (const FString& S : P) { FString A, B; S.Split(TEXT(","), &A, &B); Pts.Add(FVector2D(FCString::Atof(*A), FCString::Atof(*B))); }
				TArray<FVector2D> Sm; for (int32 i = 0; i + 1 < Pts.Num(); ++i) { const int32 K = FMath::Max(1, (int32)(FVector2D::Distance(Pts[i], Pts[i + 1]) / 200.f)); for (int32 j = 0; j < K; ++j) Sm.Add(FMath::Lerp(Pts[i], Pts[i + 1], j / (double)K)); }
				const int32 M = Sm.Num(); TArray<float> Z; Z.SetNum(M); int32 Obst = 0;
				for (int32 i = 0; i < M; ++i) { bool bL; Z[i] = Gz(Sm[i], bL); Obst += bL ? 0 : 1; }
				float MaxG = 0.f, MaxS = 0.f; int32 Steep = 0, Side = 0; FVector2D WorstG, WorstS;
				TArray<float> Gs;
				for (int32 i = 3; i < M - 3; ++i)
				{
					const float G = FMath::Abs(Z[i + 3] - Z[i - 3]) / FMath::Max(1.f, (float)FVector2D::Distance(Sm[i + 3], Sm[i - 3]));
					Gs.Add(G); if (G > MaxG) { MaxG = G; WorstG = Sm[i]; } if (G > 0.25f) ++Steep;
					const FVector2D D = (Sm[i + 1] - Sm[i - 1]).GetSafeNormal(), Pp(-D.Y, D.X); bool b1, b2;
					const float Sd = FMath::Abs(Gz(Sm[i] + Pp * 250.f, b1) - Gz(Sm[i] - Pp * 250.f, b2)) / 500.f;
					if (Sd > MaxS) { MaxS = Sd; WorstS = Sm[i]; } if (Sd > 0.2f) ++Side;
				}
				Gs.Sort(); const float P95 = Gs.Num() ? Gs[(int32)(Gs.Num() * 0.95f)] : 0.f;
				UE_LOG(LogDustlineDrive, Display, TEXT("TP pista %d: %.0f m  rampa p95=%.0f%% max=%.0f%% en %.0f,%.0f  tramos>25%%=%d  lateral max=%.0f%% en %.0f,%.0f  tramos lateral>20%%=%d  obstaculos=%d"),
					k + 1, M * 2.f, P95 * 100.f, MaxG * 100.f, WorstG.X, WorstG.Y, Steep * 2, MaxS * 100.f, WorstS.X, WorstS.Y, Side * 2, Obst);
			}
			FPlatformMisc::RequestExit(false);
		}
	}
	// -mapstats: muestrea todo el mapa cada 8 m (altura del primer impacto, qué es y agua) para analizar si se puede conducir
	if (FParse::Param(FCommandLine::Get(), TEXT("mapstats")) && Clock > 3.f)
	{
		static bool bDone = false;
		if (!bDone)
		{
			bDone = true;
			const int32 N = 504; const float Step = 800.f, X0 = -201600.f + 400.f;
			TArray<uint8> Out; Out.SetNumZeroed(N * N * 12);
			FCollisionQueryParams Pq(SCENE_QUERY_STAT(DustlineMapStats), false, this);
			for (int32 j = 0; j < N; ++j) for (int32 i = 0; i < N; ++i)
			{
				const float X = X0 + i * Step, Y = X0 + j * Step;
				FHitResult H; float Z = -1e9f; uint8 Kind = 3;
				if (GetWorld()->LineTraceSingleByChannel(H, FVector(X, Y, 90000), FVector(X, Y, -30000), ECC_Visibility, Pq))
				{
					Z = H.ImpactPoint.Z;
					Kind = (H.GetComponent() && H.GetComponent()->IsA<ULandscapeHeightfieldCollisionComponent>()) ? 0 : (H.GetComponent() && H.GetComponent()->IsA<UInstancedStaticMeshComponent>() ? 1 : 2);
				}
				float Lv; FVector2D Fl; const float W = DustlineWorld::WaterAt(X, Y, Lv, Fl) ? Lv : -1e9f;
				uint8* C = &Out[(j * N + i) * 12]; FMemory::Memcpy(C, &Z, 4); FMemory::Memcpy(C + 4, &W, 4); C[8] = Kind;
			}
			const FVector L = GetActorLocation();
			UE_LOG(LogDustlineDrive, Display, TEXT("MAPSTATS spawn=%.0f,%.0f"), L.X, L.Y);
			FFileHelper::SaveArrayToFile(Out, TEXT("/Volumes/SSD/UE-Cache/mapstats.bin"));
			FPlatformMisc::RequestExit(false);
		}
	}
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
	const bool bDemoRun = bAutoDrive || bPhysTest || FParse::Param(FCommandLine::Get(), TEXT("garagedemo")) || FParse::Param(FCommandLine::Get(), TEXT("nomenu")) || FCString::Strifind(FCommandLine::Get(), TEXT("watertest=")) || FCString::Strifind(FCommandLine::Get(), TEXT("trailtest=")) || FParse::Param(FCommandLine::Get(), TEXT("trailprofile")) || FCString::Strifind(FCommandLine::Get(), TEXT("probe=")) || FParse::Value(FCommandLine::Get(), TEXT("dshot="), Shot0);
	if (!bMenuStarted && Clock > 0.3f && PC) { bMenuStarted = true; Menu = MakeShared<FDustlineMenu>(this); Menu->Start(!bDemoRun); }
	if (PC && Menu.IsValid())
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape) && bMapOpen && !Menu->IsOpen()) bMapOpen = false;   // Esc cierra el mapa
		else if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::Gamepad_Special_Right)) Menu->OnEscape();
		else if ((PC->WasInputKeyJustPressed(EKeys::G) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right)) && !Menu->IsOpen()) ToggleGarage();
		if (Menu->IsOpen() || bGarage) bMapOpen = false;
	}
	else if (PC && PC->WasInputKeyJustPressed(EKeys::G)) ToggleGarage();
	if (FParse::Param(FCommandLine::Get(), TEXT("uidemo"))) UiDemo();
	if (FParse::Param(FCommandLine::Get(), TEXT("careertest")) && Clock > 2.f) { static bool bOnce = false; if (!bOnce) { bOnce = true; DustlineCareerSelfTest(this); FPlatformMisc::RequestExit(false); } }
	if (FParse::Param(FCommandLine::Get(), TEXT("wheeltest"))) WheelTest();
	if (FParse::Param(FCommandLine::Get(), TEXT("enginetest"))) EngineTest();
	if (FParse::Param(FCommandLine::Get(), TEXT("bodytest"))) BodyTest();
	if (bSetupDirty && Clock - LastRebuild > 0.04f) { bSetupDirty = false; LastRebuild = Clock; if (!NoteSetupChange()) Rebuild(); }
	StepAnims(dt);
	{
		const FVector L = GetActorLocation();
		const FLocal Wx = WeatherAt(L.X, L.Y);
		WeatherLabel = WeatherName(Wx); TempC = Wx.TempC;
		UpdateDirt(dt, Wx.Rain);
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
