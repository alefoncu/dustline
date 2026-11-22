#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "DustlineMeshBuilder.h"
#include "DustlineCar.generated.h"

USTRUCT(BlueprintType)
struct FDustlineSetup
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Model = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Paint = FLinearColor(0.02f, 0.02f, 0.025f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float EngineCC = 4000.f;      // 600..9000
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Turbo = 1.f;           // 0..3
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stiffness = 1.f;       // 0.4..3
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TravelM = 0.55f;       // 0.25..1.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TireRadiusM = 0.40f;   // 0.2..0.85
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TireWidthM = 0.28f;    // 0.12..0.55
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TireType = 1;          // 0 road,1 AT,2 mud,3 studded,4 balloon
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Drive = 0;             // 0 awd,1 rwd,2 fwd
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Ballast = 0.f;
};

struct FDustlineModel
{
	const TCHAR* Name; int32 Style; float L, W, H, Wheelbase; float Mass, CC, Turbo, Radius, Width, Travel, Stiff; int32 Tire, Drive;
	FLinearColor Colors[3];
};

UCLASS()
class DUSTLINE_API ADustlineCar : public APawn
{
	GENERATED_BODY()
public:
	ADustlineCar();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UBoxComponent* Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) USceneComponent* Visual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UCameraComponent* Cam;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UProceduralMeshComponent* Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* KBody;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dustline") bool bUseKenney = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UProceduralMeshComponent* Engine;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UProceduralMeshComponent* Suspension;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<UProceduralMeshComponent*> Wheels;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dustline") FDustlineSetup Setup;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dustline") bool bXRay = false;

	UFUNCTION(BlueprintCallable, Category = "Dustline") void ApplySetup(const FDustlineSetup& S);
	UFUNCTION(BlueprintCallable, Category = "Dustline") void SetXRay(bool bOn);
	UFUNCTION(BlueprintCallable, Category = "Dustline") void Rebuild();
	UFUNCTION(BlueprintPure, Category = "Dustline") static int32 NumModels();
	UFUNCTION(BlueprintPure, Category = "Dustline") static FString ModelName(int32 Index);
	/** Stock setup for a model index. */
	UFUNCTION(BlueprintPure, Category = "Dustline") static FDustlineSetup StockSetup(int32 Index);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override {}

	// ---- driving ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dustline") bool bAutoDrive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Dustline") float SpeedKmh = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Dustline") FString SurfaceName = TEXT("Hierba");
	static float GWet; static float GSnow;      // weather state shared with the weather system (0..1)
	void ResetUpright();
	// ---- garage / menús ----
	UFUNCTION(BlueprintCallable, Category = "Dustline") void ToggleGarage();
	UPROPERTY(BlueprintReadOnly, Category = "Dustline") bool bGarage = false;
	bool bShowcase = false;   // menú principal: coche quieto y cámara orbitando
	bool bPaused = false;     // pausa: física congelada
	void OpenGarage(TFunction<void()> OnClose = nullptr);
	void CloseGarage();
	void EnterShowcase();
	void ExitShowcase();
	TSharedPtr<class FDustlineMenu> Menu;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	struct FGarageStats { float HP, PeakHP, Mass, PowerToWeight, VMaxKmh, Clearance; int32 Cyl; };
	FGarageStats GarageStats() const;
	FGarageStats StatsFor(const FDustlineSetup& S) const;
	void SelectModel(int32 Index);
	void MarkSetupDirty() { bSetupDirty = true; }
	void SaveSetupToDisk() const;
	static bool LoadSetupFromDisk(int32 Model, FDustlineSetup& Out);
	static const FDustlineModel& GetModel(int32 Index);

private:
	friend class FDustlineMenu;
	TFunction<void()> OnGarageClosed; bool bMenuStarted = false;
	// physics state (SI units; position of the centre of mass in cm)
	FVector Vel = FVector::ZeroVector, AngVel = FVector::ZeroVector;
	float ZCom = 100.f, SteerAngle = 0.f, TurboSpool = 0.f, CamYaw = 0.f, Clock = 0.f, Accum = 0.f;
	int32 CamMode = 0;
	struct FWheelPhys { float Length = 0.5f; float Spin = 0.f; bool Contact = false; };
	FWheelPhys Wh[4];
	struct FPhysSpec { float Mass, HP, FMax, VMaxGear, SpringK, SpringC, RestLen, R, Width, Inertia[3], Cd; } Sp;
	void ComputeSpec();
	void StepPhysics(float Dt, float Throttle, float Steer, bool bHandbrake, bool bBoost);
	void UpdateVisuals(float Throttle);
	void UpdateCamera(float Dt);
	TSharedPtr<class SWidget> GarageWidget;
	bool bSetupDirty = false; float LastRebuild = -1.f; int32 GarageFocus = 0; float GarageOrbit = 0.5f; float OrbitSpeed = 1.f;
	FVector GarageCamPos = FVector::ZeroVector, GarageCamTgt = FVector::ZeroVector; bool bGarageCamInit = false;
	TSharedRef<class SWidget> BuildGarageWidget();
	void UpdateGarageCamera(float Dt);
	void UiDemo();
	void WheelTest();
	void EngineTest();
	void BodyTest();
public:
	// animaciones de montaje de piezas: la pieza vieja sale, se cambia la malla y la nueva entra
	enum class ESwap : uint8 { Idle, Out, In };
	struct FSwap { ESwap S = ESwap::Idle; float T = 0.f, D0 = 0.f, D = 0.f, Stag = 0.f; };
	FSwap SwEngine, SwWheels, SwSusp, SwModel; bool bRebuildPending = false;
	float AnimPaint = 1.f, BounceT = 9.f, XLift = 0.f;
	FLinearColor PaintFrom = FLinearColor::Black, PaintShown = FLinearColor::Black;
	FDustlineSetup LastSetup; bool bHaveLast = false;
	bool NoteSetupChange();   // true = la reconstrucción queda aplazada hasta que salgan las piezas viejas
	void StepAnims(float Dt);
	void ApplyAnims();
private:
	FString GarageReadout() const;
	FString LogLine; float LogT = 0.f; float FpsAcc = 0.f; int32 FpsN = 0;
	void BuildBody(); void BuildWheels(); void BuildSuspension(); void BuildEngine();
	void BuildShellV2(FDustlineMeshBuilder& B); void BuildBuggyBody(FDustlineMeshBuilder& B);
	float BodyH() const; void BuildBodyKen();
	UMaterialInstanceDynamic* MakeMat(const TCHAR* BasePath, const FLinearColor& Color, float Metallic, float Roughness);
	TMap<int32, UMaterialInterface*> Mats;
	void RefreshMaterials();
	float GroundClearance() const;
	void ComputeKenney(); bool bKen = false; float KSx = 1, KSy = 1, KSz = 1, KBodyZ = 0;
	float FrontX() const; float RearX() const; float BodyFront() const;
	FVector WheelPos(int32 I) const;
};
