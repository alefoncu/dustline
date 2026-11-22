#include "DustlineCar.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SScrollBox.h"
#include "DustlineUI.h"
#include "DustlineMenu.h"
#include "DustlineSettings.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Styling/CoreStyle.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static FString SetupPath(int32 Model) { return FPaths::ProjectSavedDir() / TEXT("Garage") / FString::Printf(TEXT("setup_%d.json"), Model); }

void ADustlineCar::SaveSetupToDisk() const
{
	FString Json; if (FJsonObjectConverter::UStructToJsonObjectString(Setup, Json)) FFileHelper::SaveStringToFile(Json, *SetupPath(Setup.Model));
	FFileHelper::SaveStringToFile(FString::FromInt(Setup.Model), *(FPaths::ProjectSavedDir() / TEXT("Garage") / TEXT("last_model.txt")));
}
bool ADustlineCar::LoadSetupFromDisk(int32 Model, FDustlineSetup& Out)
{
	FString Json; if (!FFileHelper::LoadFileToString(Json, *SetupPath(Model))) return false;
	return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Out, 0, 0);
}

void ADustlineCar::SelectModel(int32 Index)
{
	FDustlineSetup S = StockSetup(Index);
	LoadSetupFromDisk(Index, S);
	S.Model = Index;
	Setup = S; bSetupDirty = true;
}

ADustlineCar::FGarageStats ADustlineCar::GarageStats() const
{
	const FDustlineModel& M = GetModel(Setup.Model);
	FGarageStats R;
	R.HP = Setup.EngineCC * 0.065f; R.PeakHP = R.HP * (1.f + 0.5f * Setup.Turbo);
	const float wheelMass = Setup.TireRadiusM * Setup.TireRadiusM * Setup.TireWidthM * 520.f, stock = M.Radius * M.Radius * M.Width * 520.f;
	R.Mass = FMath::Max(450.f, M.Mass + (Setup.EngineCC - M.CC) * 0.065f + (Setup.Turbo - M.Turbo) * 30.f + (wheelMass - stock) + Setup.Ballast + (Setup.TravelM - M.Travel) * 60.f);
	R.VMaxKmh = 34.f * (Setup.TireRadiusM / 0.46f) * (1.f + 0.12f * Setup.Turbo) * (M.Style == 2 ? 0.8f : 1.f) * 3.6f;
	R.PowerToWeight = R.PeakHP / R.Mass * 1000.f;
	R.Clearance = GroundClearance();
	int32 cyl = FMath::Clamp(FMath::RoundToInt(Setup.EngineCC / 650.f), 2, 12); if (cyl > 6 && (cyl % 2)) cyl += 1;
	R.Cyl = cyl;
	return R;
}

TSharedRef<SWidget> ADustlineCar::BuildGarageWidget()
{
	using namespace DUI;
	ADustlineCar* Car = this;
	FDustlineSettings* St = &FDustlineSettings::Get();

	auto Sl = [Car](const FString& Name, float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FString(float)> Fmt, int32 Focus, bool bXRayOnDrag) -> TSharedRef<SWidget>
	{
		return SliderRow(Name, Min, Max, Get, [Car, Set](float V) { Set(V); Car->MarkSetupDirty(); }, Fmt,
			FSimpleDelegate::CreateLambda([Car, Focus, bXRayOnDrag]() { Car->GarageFocus = Focus; if (bXRayOnDrag) Car->SetXRay(true); }),
			FSimpleDelegate::CreateLambda([Car, bXRayOnDrag]() { Car->GarageFocus = 0; if (bXRayOnDrag) Car->SetXRay(false); Car->SaveSetupToDisk(); }));
	};
	auto Tile = [Car](const FString& Caption, TFunction<FString()> Value) -> TSharedRef<SWidget>
	{
		return SNew(SBorder).BorderImage(Box(Card(), 18.f, Hair(), 1.f)).Padding(FMargin(18, 14))
			[ SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(Font(34.f, 1)).ColorAndOpacity(Label()).Text_Lambda([Value]() { return FText::FromString(Value()); }) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)[ Text(Caption, 15.f, Label2()) ] ];
	};

	// ---- vehículo
	TArray<FString> Names; for (int32 i = 0; i < NumModels(); ++i) Names.Add(ModelName(i));
	TSharedRef<SWidget> Models = Pills(Names, [Car]() { return Car->Setup.Model; }, [Car](int32 I) { Car->SelectModel(I); Car->SaveSetupToDisk(); }, 17.f);

	// ---- color
	TSharedRef<SHorizontalBox> Paints = SNew(SHorizontalBox);
	const FLinearColor Cols[] = { FLinearColor(0.50f, 0.52f, 0.55f), FLinearColor(0.02f, 0.02f, 0.025f), FLinearColor(0.85f, 0.85f, 0.82f), FLinearColor(0.70f, 0.03f, 0.02f), FLinearColor(0.95f, 0.62f, 0.02f),
		FLinearColor(0.03f, 0.17f, 0.07f), FLinearColor(0.02f, 0.10f, 0.45f), FLinearColor(0.95f, 0.20f, 0.01f) };
	for (const FLinearColor& C : Cols)
	{
		FLinearColor Opaque = C; Opaque.A = 1.f;
		Paints->AddSlot().AutoWidth().Padding(0, 0, 14, 0)
		[ SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0))
			.OnClicked(FOnClicked::CreateLambda([Car, C]() { Car->Setup.Paint = C; Car->MarkSetupDirty(); Car->SaveSetupToDisk(); return FReply::Handled(); }))
			[ SNew(SBox).WidthOverride(38.f).HeightOverride(38.f)
				[ SNew(SBorder).BorderImage_Lambda([Car, C, Opaque]() { const bool Sel = FLinearColor::Dist(Car->Setup.Paint, C) < 0.02f; return Sel ? Box(Opaque, 19.f, FLinearColor::White, 3.f) : Box(Opaque, 19.f, Col(255, 255, 255, 0.22f), 1.f); }) ] ] ];
	}

	// ---- cuerpo
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	auto Add = [&V](TSharedRef<SWidget> W) { V->AddSlot().AutoHeight()[ W ]; };

	Add(Section(TEXT("Vehículo")));
	Add(Models);
	Add(Section(TEXT("Color")));
	Add(SNew(SBox).Padding(FMargin(2, 0, 0, 0))[ Paints ]);
	Add(Section(TEXT("Rendimiento")));
	Add(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 6, 0)[ Tile(TEXT("Potencia (con turbo)"), [Car]() { const FGarageStats G = Car->GarageStats(); return FString::Printf(TEXT("%.0f CV"), G.PeakHP); }) ]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6, 0, 0, 0)[ Tile(TEXT("Peso"), [Car]() { return FString::Printf(TEXT("%.0f kg"), Car->GarageStats().Mass); }) ]);
	Add(SNew(SBox).Padding(FMargin(0, 12, 0, 0))[ SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 6, 0)[ Tile(TEXT("Velocidad máxima"), [Car, St]() { const float V2 = Car->GarageStats().VMaxKmh; return St->Units == 1 ? FString::Printf(TEXT("%.0f mph"), V2 * 0.621371f) : FString::Printf(TEXT("%.0f km/h"), V2); }) ]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6, 0, 0, 0)[ Tile(TEXT("Altura libre"), [Car]() { return FString::Printf(TEXT("%.0f cm"), Car->GarageStats().Clearance); }) ] ]);

	Add(Section(TEXT("Motor")));
	Add(Group({
		Sl(TEXT("Cilindrada"), 600.f, 9000.f, [Car]() { return Car->Setup.EngineCC; }, [Car](float v) { Car->Setup.EngineCC = v; }, [Car](float v) { return FString::Printf(TEXT("%d cil.  ·  %.0f cc"), Car->GarageStats().Cyl, v); }, 1, true),
		Sl(TEXT("Turbo"), 0.f, 3.f, [Car]() { return Car->Setup.Turbo; }, [Car](float v) { Car->Setup.Turbo = v; }, [](float v) { return v < 0.05f ? FString(TEXT("Sin turbo")) : FString::Printf(TEXT("Nivel %.1f"), v); }, 1, true),
		Sl(TEXT("Lastre / aligerar"), -300.f, 800.f, [Car]() { return Car->Setup.Ballast; }, [Car](float v) { Car->Setup.Ballast = v; }, [](float v) { return FString::Printf(TEXT("%+.0f kg"), v); }, 0, false) }));
	Add(Section(TEXT("Suspensión")));
	Add(Group({
		Sl(TEXT("Dureza"), 0.4f, 3.f, [Car]() { return Car->Setup.Stiffness; }, [Car](float v) { Car->Setup.Stiffness = v; }, [](float v) { return FString::Printf(TEXT("×%.2f"), v); }, 3, true),
		Sl(TEXT("Recorrido y altura"), 0.25f, 1.f, [Car]() { return Car->Setup.TravelM; }, [Car](float v) { Car->Setup.TravelM = v; }, [](float v) { return FString::Printf(TEXT("%.0f cm"), v * 100.f); }, 3, true) }));
	Add(Section(TEXT("Ruedas")));
	Add(Group({
		Sl(TEXT("Radio"), 0.2f, 0.85f, [Car]() { return Car->Setup.TireRadiusM; }, [Car](float v) { Car->Setup.TireRadiusM = v; }, [](float v) { return FString::Printf(TEXT("%.0f cm"), v * 100.f); }, 2, false),
		Sl(TEXT("Ancho"), 0.12f, 0.55f, [Car]() { return Car->Setup.TireWidthM; }, [Car](float v) { Car->Setup.TireWidthM = v; }, [](float v) { return FString::Printf(TEXT("%.0f cm"), v * 100.f); }, 2, false) }));
	Add(SNew(SBox).Padding(FMargin(0, 14, 0, 0))[ Pills({ TEXT("Carretera"), TEXT("Todo terreno"), TEXT("Barro"), TEXT("Clavos"), TEXT("Balón") }, [Car]() { return Car->Setup.TireType; },
		[Car](int32 I) { Car->Setup.TireType = I; Car->MarkSetupDirty(); Car->SaveSetupToDisk(); Car->GarageFocus = 2; }, 17.f) ]);
	Add(Section(TEXT("Tracción")));
	Add(Segmented({ TEXT("4×4"), TEXT("Trasera"), TEXT("Delantera") }, [Car]() { return Car->Setup.Drive; }, [Car](int32 I) { Car->Setup.Drive = I; Car->MarkSetupDirty(); Car->SaveSetupToDisk(); Car->GarageFocus = 3; }, 18.f));
	Add(Section(TEXT("Vista")));
	Add(Group({
		Row(TEXT("Rayos X"), SNew(SToggle).IsOn_Lambda([Car]() { return Car->bXRay; }).OnToggled(FOnBooleanValueChanged::CreateLambda([Car](bool B) { Car->SetXRay(B); })), TEXT("Muestra motor, turbo y suspensión")),
		Row(TEXT("Configuración original"), Button(TEXT("Restaurar"), [Car]() { const int32 m = Car->Setup.Model; Car->Setup = StockSetup(m); Car->MarkSetupDirty(); Car->SaveSetupToDisk(); }, EBtn::Secondary, 0.f, 17.f)) }));
	Add(Spacer(30.f));

	TSharedRef<SWidget> Header = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[ SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Text(TEXT("Garaje"), 38.f, Label(), 1) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)[ SNew(STextBlock).Font(Font(18.f)).ColorAndOpacity(Label2()).Text_Lambda([Car]() { return FText::FromString(ModelName(Car->Setup.Model)); }) ] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Button(TEXT("Listo"), [Car]() { Car->CloseGarage(); }, EBtn::Primary, 0.f, 20.f) ];

	return SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(FMargin(0, 28, 28, 28))
		[ SNew(SAppear).Rise(0.f)
			[ SNew(SBox).WidthOverride(520.f)
				[ Glass(SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[ Header ]
					+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed) + SScrollBox::Slot()[ V ] ], 34.f, FMargin(28, 26, 28, 0)) ] ] ];
}

void ADustlineCar::OpenGarage(TFunction<void()> OnClose)
{
	if (bGarage) return;
	bGarage = true; OnGarageClosed = OnClose;
	Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector; bGarageCamInit = false; GarageFocus = 0;
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) { PC->SetShowMouseCursor(true); FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(M); }
	if (GEngine && GEngine->GameViewport) { GarageWidget = BuildGarageWidget(); GEngine->GameViewport->AddViewportWidgetContent(GarageWidget.ToSharedRef(), 10); }
}

void ADustlineCar::CloseGarage()
{
	if (!bGarage) return;
	bGarage = false;
	if (GEngine && GEngine->GameViewport && GarageWidget.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(GarageWidget.ToSharedRef());
	GarageWidget.Reset(); SetXRay(false); SaveSetupToDisk();
	TFunction<void()> Cb = MoveTemp(OnGarageClosed); OnGarageClosed = nullptr;
	if (Cb) { Cb(); return; }
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) { PC->SetShowMouseCursor(false); PC->SetInputMode(FInputModeGameOnly()); }
	ResetUpright();
}

void ADustlineCar::ToggleGarage() { if (bGarage) CloseGarage(); else OpenGarage(); }

void ADustlineCar::EnterShowcase()
{
	bShowcase = true; bGarageCamInit = false; GarageFocus = 0; Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector; SetXRay(false);
}
void ADustlineCar::ExitShowcase()
{
	bShowcase = false; Accum = 0.f;
	const FVector F = GetActorForwardVector(); CamYaw = FMath::Atan2(F.Y, F.X);
	ResetUpright();
}

void ADustlineCar::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Menu.IsValid()) { Menu->Shutdown(); Menu.Reset(); }
	if (GEngine && GEngine->GameViewport && GarageWidget.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(GarageWidget.ToSharedRef());
	GarageWidget.Reset();
	Super::EndPlay(Reason);
}

void ADustlineCar::UiDemo()
{
	static int32 Step = 0;
	auto Shot = [&](int32 n) { FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("/Volumes/SSD/UE-Cache/ui_%d.png"), n), true, false); };
	if (!Menu.IsValid()) return;
	const float T = Clock;
	if (Step == 0 && T > 6.f) { Shot(1); Step = 1; }
	else if (Step == 1 && T > 8.f) { Menu->ShowSettings(); Menu->SetPage(0); Step = 2; }
	else if (Step == 2 && T > 11.f) { Shot(2); Step = 3; }
	else if (Step == 3 && T > 12.f) { Menu->SetPage(1); Step = 4; }
	else if (Step == 4 && T > 14.f) { Shot(3); Step = 5; }
	else if (Step == 5 && T > 15.f) { Menu->SetPage(2); Step = 6; }
	else if (Step == 6 && T > 17.f) { Shot(4); Step = 7; }
	else if (Step == 7 && T > 18.f) { Menu->SetPage(3); Step = 8; }
	else if (Step == 8 && T > 20.f) { Shot(5); Step = 9; }
	else if (Step == 9 && T > 21.f) { Menu->ShowPause(); Step = 10; }
	else if (Step == 10 && T > 23.f) { Shot(6); Step = 11; }
	else if (Step == 11 && T > 24.f) { Menu->StartDriving(); Step = 12; }
	else if (Step == 12 && T > 26.f) { Shot(7); Step = 13; }
	else if (Step == 13 && T > 27.f) { OpenGarage(); Step = 14; }
	else if (Step == 14 && T > 30.f) { Shot(8); Step = 15; }
	else if (Step == 15 && T > 33.f) { FPlatformMisc::RequestExit(false); }
}

void ADustlineCar::UpdateGarageCamera(float Dt)
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const FTransform T = GetActorTransform(); const FVector Off(0, 0, ZCom);
	auto W = [&](const FVector& Local) { return T.TransformPosition(Local - Off); };
	FVector Tgt, Pos;
	const float gc = GroundClearance();
	switch (GarageFocus)
	{
	case 1: Tgt = W(FVector(FrontX() - 10.f, 0, gc + 60.f)); Pos = W(FVector(FrontX() + 150.f, -170.f, gc + 190.f)); break;
	case 2: { const FVector WP = WheelPos(0); Tgt = W(WP); Pos = W(WP + FVector(110.f, -190.f - Setup.TireRadiusM * 80.f, 40.f + Setup.TireRadiusM * 60.f)); break; }
	case 3: { const FVector WP = WheelPos(0); Tgt = W(FVector(WP.X, WP.Y * 0.6f, gc * 0.5f)); Pos = W(FVector(WP.X + 70.f, -330.f, gc * 0.5f + 20.f)); break; }
	default:
	{
		GarageOrbit += Dt * 0.25f;
		const float d = M.L * 1.05f + 360.f;
		Tgt = W(FVector(0, 0, M.H * 0.45f));
		Pos = W(FVector(FMath::Cos(GarageOrbit) * d, FMath::Sin(GarageOrbit) * d, M.H * 0.55f + 120.f));
		break;
	}
	}
	if (!bGarageCamInit) { GarageCamPos = Pos; GarageCamTgt = Tgt; bGarageCamInit = true; }
	const float a = 1.f - FMath::Exp(-4.f * Dt);
	GarageCamPos = FMath::Lerp(GarageCamPos, Pos, a); GarageCamTgt = FMath::Lerp(GarageCamTgt, Tgt, a);
	Cam->SetWorldLocationAndRotation(GarageCamPos, (GarageCamTgt - GarageCamPos).Rotation());
}
