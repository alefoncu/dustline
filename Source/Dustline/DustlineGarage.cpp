#include "DustlineCar.h"
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

FString ADustlineCar::GarageReadout() const
{
	const FDustlineModel& M = GetModel(Setup.Model);
	const float hp = Setup.EngineCC * 0.065f, peak = hp * (1.f + 0.5f * Setup.Turbo);
	const float wheelMass = Setup.TireRadiusM * Setup.TireRadiusM * Setup.TireWidthM * 520.f, stock = M.Radius * M.Radius * M.Width * 520.f;
	const float mass = FMath::Max(450.f, M.Mass + (Setup.EngineCC - M.CC) * 0.065f + (Setup.Turbo - M.Turbo) * 30.f + (wheelMass - stock) + Setup.Ballast + (Setup.TravelM - M.Travel) * 60.f);
	const float vmax = 34.f * (Setup.TireRadiusM / 0.46f) * (1.f + 0.12f * Setup.Turbo) * (M.Style == 2 ? 0.8f : 1.f) * 3.6f;
	int32 cyl = FMath::Clamp(FMath::RoundToInt(Setup.EngineCC / 650.f), 2, 12); if (cyl > 6 && (cyl % 2)) cyl += 1;
	return FString::Printf(TEXT("%s\nMotor: %d cilindros, %.0f cc  ->  %.0f CV  (%.0f CV con turbo a pleno)\nPeso: %.0f kg   Potencia/peso: %.0f CV/t\nVelocidad max. (marchas): ~%.0f km/h\nAltura libre aprox.: %.0f cm"),
		*ModelName(Setup.Model), cyl, Setup.EngineCC, hp, peak, mass, peak / mass * 1000.f, vmax, GroundClearance());
}

TSharedRef<SWidget> ADustlineCar::BuildGarageWidget()
{
	const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 11), Bold = FCoreStyle::GetDefaultFontStyle("Bold", 12), Title = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	ADustlineCar* Car = this;

	auto Slider = [Car, Small](const FString& Name, float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FString(float)> Fmt, int32 Focus, bool bXRayOnDrag) -> TSharedRef<SWidget>
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
			[ SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(STextBlock).Text(FText::FromString(Name)).Font(Small).ColorAndOpacity(FLinearColor(0.85f, 0.9f, 1.f)) ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(STextBlock).Font(Small).Text_Lambda([Get, Fmt]() { return FText::FromString(Fmt(Get())); }) ] ]
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(SSlider).MinValue(Min).MaxValue(Max).Value_Lambda([Get]() { return Get(); })
				.OnValueChanged(FOnFloatValueChanged::CreateLambda([Car, Set](float V) { Set(V); Car->MarkSetupDirty(); }))
				.OnMouseCaptureBegin(FSimpleDelegate::CreateLambda([Car, Focus, bXRayOnDrag]() { Car->GarageFocus = Focus; if (bXRayOnDrag) Car->SetXRay(true); }))
				.OnMouseCaptureEnd(FSimpleDelegate::CreateLambda([Car, bXRayOnDrag]() { Car->GarageFocus = 0; if (bXRayOnDrag) Car->SetXRay(false); Car->SaveSetupToDisk(); })) ];
	};
	auto Btn = [Small](const FString& Label, TFunction<void()> Click, TFunction<bool()> Selected) -> TSharedRef<SWidget>
	{
		return SNew(SButton).ButtonColorAndOpacity_Lambda([Selected]() { return Selected() ? FLinearColor(0.10f, 0.50f, 0.95f) : FLinearColor(0.16f, 0.16f, 0.18f); })
			.OnClicked(FOnClicked::CreateLambda([Click]() { Click(); return FReply::Handled(); }))
			[ SNew(STextBlock).Text(FText::FromString(Label)).Font(Small).Justification(ETextJustify::Center) ];
	};

	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	Col->AddSlot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(TEXT("GARAJE"))).Font(Title) ];
	Col->AddSlot().AutoHeight().Padding(0, 2, 0, 6)[ SNew(STextBlock).Text(FText::FromString(TEXT("G: volver a conducir"))).Font(Small).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f)) ];

	// models
	TSharedRef<SUniformGridPanel> Models = SNew(SUniformGridPanel).SlotPadding(FMargin(2));
	for (int32 i = 0; i < NumModels(); ++i)
		Models->AddSlot(i % 2, i / 2)[ Btn(ModelName(i), [Car, i]() { Car->SelectModel(i); }, [Car, i]() { return Car->Setup.Model == i; }) ];
	Col->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[ Models ];

	// paint swatches
	TSharedRef<SHorizontalBox> Paints = SNew(SHorizontalBox);
	const FLinearColor Cols[] = { FLinearColor(0.50f, 0.52f, 0.55f), FLinearColor(0.02f, 0.02f, 0.025f), FLinearColor(0.85f, 0.85f, 0.82f), FLinearColor(0.70f, 0.03f, 0.02f), FLinearColor(0.95f, 0.62f, 0.02f),
		FLinearColor(0.03f, 0.17f, 0.07f), FLinearColor(0.02f, 0.10f, 0.45f), FLinearColor(0.95f, 0.20f, 0.01f) };
	for (const FLinearColor& C : Cols)
		Paints->AddSlot().FillWidth(1.f).Padding(2)[ SNew(SButton).ButtonColorAndOpacity(C).OnClicked(FOnClicked::CreateLambda([Car, C]() { Car->Setup.Paint = C; Car->MarkSetupDirty(); Car->SaveSetupToDisk(); return FReply::Handled(); }))[ SNew(SBox).HeightOverride(16.f) ] ];
	Col->AddSlot().AutoHeight().Padding(0, 0, 0, 2)[ SNew(STextBlock).Text(FText::FromString(TEXT("COLOR"))).Font(Small).ColorAndOpacity(FLinearColor(0.85f, 0.9f, 1.f)) ];
	Col->AddSlot().AutoHeight()[ Paints ];

	// sliders
	Col->AddSlot().AutoHeight()[ Slider(TEXT("MOTOR (cilindrada)"), 600.f, 9000.f, [Car]() { return Car->Setup.EngineCC; }, [Car](float v) { Car->Setup.EngineCC = v; }, [](float v) { return FString::Printf(TEXT("%.0f cc"), v); }, 1, true) ];
	Col->AddSlot().AutoHeight()[ Slider(TEXT("TURBO"), 0.f, 3.f, [Car]() { return Car->Setup.Turbo; }, [Car](float v) { Car->Setup.Turbo = v; }, [](float v) { return v < 0.05f ? FString(TEXT("sin turbo")) : FString::Printf(TEXT("nivel %.1f"), v); }, 1, true) ];
	Col->AddSlot().AutoHeight()[ Slider(TEXT("LASTRE / ALIGERAR"), -300.f, 800.f, [Car]() { return Car->Setup.Ballast; }, [Car](float v) { Car->Setup.Ballast = v; }, [](float v) { return FString::Printf(TEXT("%+.0f kg"), v); }, 0, false) ];
	Col->AddSlot().AutoHeight()[ Slider(TEXT("SUSPENSION - dureza"), 0.4f, 3.f, [Car]() { return Car->Setup.Stiffness; }, [Car](float v) { Car->Setup.Stiffness = v; }, [](float v) { return FString::Printf(TEXT("x%.2f"), v); }, 3, true) ];
	Col->AddSlot().AutoHeight()[ Slider(TEXT("SUSPENSION - recorrido / altura"), 0.25f, 1.f, [Car]() { return Car->Setup.TravelM; }, [Car](float v) { Car->Setup.TravelM = v; }, [](float v) { return FString::Printf(TEXT("%.0f cm"), v * 100.f); }, 3, true) ];
	Col->AddSlot().AutoHeight()[ Slider(TEXT("RUEDAS - radio"), 0.2f, 0.85f, [Car]() { return Car->Setup.TireRadiusM; }, [Car](float v) { Car->Setup.TireRadiusM = v; }, [](float v) { return FString::Printf(TEXT("%.0f cm"), v * 100.f); }, 2, false) ];
	Col->AddSlot().AutoHeight()[ Slider(TEXT("RUEDAS - ancho"), 0.12f, 0.55f, [Car]() { return Car->Setup.TireWidthM; }, [Car](float v) { Car->Setup.TireWidthM = v; }, [](float v) { return FString::Printf(TEXT("%.0f cm"), v * 100.f); }, 2, false) ];

	// tyre type + drivetrain
	const TCHAR* TireNames[] = { TEXT("Carretera"), TEXT("Todo terreno"), TEXT("Barro"), TEXT("Clavos"), TEXT("Balon") };
	TSharedRef<SUniformGridPanel> Tires = SNew(SUniformGridPanel).SlotPadding(FMargin(2));
	for (int32 i = 0; i < 5; ++i) Tires->AddSlot(i % 3, i / 3)[ Btn(TireNames[i], [Car, i]() { Car->Setup.TireType = i; Car->MarkSetupDirty(); Car->SaveSetupToDisk(); Car->GarageFocus = 2; }, [Car, i]() { return Car->Setup.TireType == i; }) ];
	Col->AddSlot().AutoHeight().Padding(0, 8, 0, 2)[ SNew(STextBlock).Text(FText::FromString(TEXT("TIPO DE NEUMATICO"))).Font(Small).ColorAndOpacity(FLinearColor(0.85f, 0.9f, 1.f)) ];
	Col->AddSlot().AutoHeight()[ Tires ];
	const TCHAR* DriveNames[] = { TEXT("4x4"), TEXT("Trasera"), TEXT("Delantera") };
	TSharedRef<SHorizontalBox> Drives = SNew(SHorizontalBox);
	for (int32 i = 0; i < 3; ++i) Drives->AddSlot().FillWidth(1.f).Padding(2)[ Btn(DriveNames[i], [Car, i]() { Car->Setup.Drive = i; Car->MarkSetupDirty(); Car->SaveSetupToDisk(); Car->GarageFocus = 3; }, [Car, i]() { return Car->Setup.Drive == i; }) ];
	Col->AddSlot().AutoHeight().Padding(0, 8, 0, 2)[ SNew(STextBlock).Text(FText::FromString(TEXT("TRACCION"))).Font(Small).ColorAndOpacity(FLinearColor(0.85f, 0.9f, 1.f)) ];
	Col->AddSlot().AutoHeight()[ Drives ];

	// readout + actions
	Col->AddSlot().AutoHeight().Padding(0, 10, 0, 6)[ SNew(STextBlock).Font(Small).ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.5f)).Text_Lambda([Car]() { return FText::FromString(Car->GarageReadout()); }) ];
	TSharedRef<SHorizontalBox> Acts = SNew(SHorizontalBox);
	Acts->AddSlot().FillWidth(1.f).Padding(2)[ Btn(TEXT("Rayos X"), [Car]() { Car->SetXRay(!Car->bXRay); }, [Car]() { return Car->bXRay; }) ];
	Acts->AddSlot().FillWidth(1.f).Padding(2)[ Btn(TEXT("Original"), [Car]() { const int32 m = Car->Setup.Model; Car->Setup = StockSetup(m); Car->MarkSetupDirty(); Car->SaveSetupToDisk(); }, []() { return false; }) ];
	Acts->AddSlot().FillWidth(1.f).Padding(2)[ Btn(TEXT("Conducir"), [Car]() { Car->ToggleGarage(); }, []() { return false; }) ];
	Col->AddSlot().AutoHeight()[ Acts ];

	return SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 12, 12, 12))
		[ SNew(SBox).WidthOverride(380.f)
			[ SNew(SBorder).Padding(14.f).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.82f))
				[ SNew(SScrollBox) + SScrollBox::Slot()[ Col ] ] ] ];
}

void ADustlineCar::ToggleGarage()
{
	bGarage = !bGarage;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (bGarage)
	{
		Vel = FVector::ZeroVector; AngVel = FVector::ZeroVector; bGarageCamInit = false; GarageFocus = 0;
		if (PC) { PC->SetShowMouseCursor(true); FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(M); }
		if (GEngine && GEngine->GameViewport) { GarageWidget = BuildGarageWidget(); GEngine->GameViewport->AddViewportWidgetContent(GarageWidget.ToSharedRef(), 10); }
	}
	else
	{
		if (GEngine && GEngine->GameViewport && GarageWidget.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(GarageWidget.ToSharedRef());
		GarageWidget.Reset(); SetXRay(false); SaveSetupToDisk();
		if (PC) { PC->SetShowMouseCursor(false); PC->SetInputMode(FInputModeGameOnly()); }
		ResetUpright();
	}
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
