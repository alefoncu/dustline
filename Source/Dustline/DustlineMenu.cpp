#include "DustlineMenu.h"
#include "DustlineCar.h"
#include "DustlineUI.h"
#include "DustlineSettings.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"

using namespace DUI;

static APlayerController* PCOf(ADustlineCar* C) { return C && C->GetWorld() ? C->GetWorld()->GetFirstPlayerController() : nullptr; }
static const TCHAR* kVersion = TEXT("0.2");

void FDustlineMenu::SetUIMode(bool bUI)
{
	APlayerController* PC = PCOf(Car.Get()); if (!PC) return;
	if (bUI) { PC->SetShowMouseCursor(true); FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(M); }
	else { PC->SetShowMouseCursor(false); PC->SetInputMode(FInputModeGameOnly()); }
}

void FDustlineMenu::Clear()
{
	if (GEngine && GEngine->GameViewport && Current.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(Current.ToSharedRef());
	Current.Reset();
}

void FDustlineMenu::Present(EScreen S, TSharedRef<SWidget> W)
{
	Clear();
	if (S != EScreen::Settings) PageBox.Reset();
	Screen = S; Current = W;
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->AddViewportWidgetContent(W, 20);
	SetUIMode(true);
}

void FDustlineMenu::Start(bool bShowMain)
{
	FDustlineSettings::Get().ApplyGlobal();
	if (!HudWidget.IsValid() && GEngine && GEngine->GameViewport) { HudWidget = BuildHud(); GEngine->GameViewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), 5); }
	if (!FadeW.IsValid() && GEngine && GEngine->GameViewport)
	{
		FadeA = MakeShared<float>(1.f); TSharedPtr<float> A = FadeA;
		FadeW = SNew(SBorder).BorderImage(Box(FLinearColor::Black, 0.f)).Visibility_Lambda([A]() { return *A > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.BorderBackgroundColor_Lambda([A]() { *A = FMath::Max(0.f, *A - FApp::GetDeltaTime() * 1.5f); return FLinearColor(1.f, 1.f, 1.f, FMath::SmoothStep(0.f, 1.f, *A)); });
		GEngine->GameViewport->AddViewportWidgetContent(FadeW.ToSharedRef(), 100);
	}
	RestartHintTimer();
	if (bShowMain) ShowMain();
}
void FDustlineMenu::Flash() { if (FadeA.IsValid()) *FadeA = 1.f; }

void FDustlineMenu::Shutdown()
{
	Clear();
	if (GEngine && GEngine->GameViewport && HudWidget.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(HudWidget.ToSharedRef());
	if (GEngine && GEngine->GameViewport && FadeW.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(FadeW.ToSharedRef());
	FadeW.Reset(); HudWidget.Reset(); Screen = EScreen::None;
}

void FDustlineMenu::RestartHintTimer() { DriveStart = FPlatformTime::Seconds(); }

void FDustlineMenu::ShowMain()
{
	ADustlineCar* C = Car.Get(); if (!C) return;
	Flash(); C->bPaused = false; C->EnterShowcase();
	Present(EScreen::Main, BuildMain());
}
void FDustlineMenu::ShowPause()
{
	ADustlineCar* C = Car.Get(); if (!C) return;
	C->bPaused = true;
	Present(EScreen::Pause, BuildPause());
}
void FDustlineMenu::ShowSettings()
{
	if (Screen == EScreen::Main || Screen == EScreen::Pause) Prev = Screen;
	Present(EScreen::Settings, BuildSettings());
}
void FDustlineMenu::StartDriving()
{
	ADustlineCar* C = Car.Get(); if (!C) return;
	Clear(); Screen = EScreen::None; SetUIMode(false);
	Flash(); C->bPaused = false; C->ExitShowcase(); RestartHintTimer();
}
void FDustlineMenu::Resume()
{
	ADustlineCar* C = Car.Get(); if (!C) return;
	Clear(); Screen = EScreen::None; SetUIMode(false);
	C->bPaused = false; RestartHintTimer();
}
void FDustlineMenu::OpenGarageFrom(EScreen Back)
{
	ADustlineCar* C = Car.Get(); if (!C) return;
	Clear(); Screen = EScreen::None;
	TWeakPtr<FDustlineMenu> Weak = AsShared();
	C->OpenGarage([Weak, Back]() { if (TSharedPtr<FDustlineMenu> M = Weak.Pin()) { if (Back == EScreen::Pause) M->ShowPause(); else M->ShowMain(); } });
}
void FDustlineMenu::OnEscape()
{
	ADustlineCar* C = Car.Get(); if (!C) return;
	if (C->bGarage) { C->CloseGarage(); return; }
	switch (Screen)
	{
	case EScreen::Settings: if (Prev == EScreen::Pause) ShowPause(); else ShowMain(); break;
	case EScreen::Pause: Resume(); break;
	case EScreen::None: ShowPause(); break;
	default: break;
	}
}
void FDustlineMenu::SetPage(int32 P)
{
	Page = P;
	if (PageBox.IsValid()) PageBox->SetContent(SNew(SAppear).Rise(10.f)[ BuildPage(P) ]);
}

// ------------------------------------------------------------------ pantalla principal
TSharedRef<SWidget> FDustlineMenu::BuildMain()
{
	ADustlineCar* C = Car.Get();
	TWeakPtr<FDustlineMenu> Weak = AsShared();
	auto Appear = [](TSharedRef<SWidget> W, float Delay) -> TSharedRef<SWidget> { return SNew(SAppear).Delay(Delay)[ W ]; };

	TSharedRef<SVerticalBox> Stack = SNew(SVerticalBox);
	Stack->AddSlot().AutoHeight()[ Appear(Text(TEXT("OFF-ROAD  ·  MUNDO ABIERTO"), 17.f, Label2()), 0.00f) ];
	Stack->AddSlot().AutoHeight().Padding(0, 4, 0, 0)[ Appear(Text(TEXT("Dustline"), 120.f, Label(), 1), 0.05f) ];
	Stack->AddSlot().AutoHeight().Padding(0, 2, 0, 0)[ Appear(Text(TEXT("Conduce. Modifica. Domina el terreno."), 26.f, Label2()), 0.10f) ];
	Stack->AddSlot().AutoHeight()[ Spacer(60.f) ];
	Stack->AddSlot().AutoHeight()[ Appear(Button(TEXT("Conducir"), [Weak]() { if (auto M = Weak.Pin()) M->StartDriving(); }, EBtn::Primary, 400.f, 24.f), 0.16f) ];
	Stack->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[ Appear(Button(TEXT("Configurar coche"), [Weak]() { if (auto M = Weak.Pin()) M->OpenGarageFrom(EScreen::Main); }, EBtn::Secondary, 400.f, 24.f), 0.20f) ];
	Stack->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[ Appear(Button(TEXT("Ajustes"), [Weak]() { if (auto M = Weak.Pin()) M->ShowSettings(); }, EBtn::Secondary, 400.f, 24.f), 0.24f) ];
	Stack->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[ Appear(Button(TEXT("Salir"), [Weak]() { if (auto M = Weak.Pin()) if (APlayerController* PC = PCOf(M->Car.Get())) PC->ConsoleCommand(TEXT("quit")); }, EBtn::Plain, 400.f, 22.f), 0.28f) ];

	// selector de vehículo
	auto Step = [C](int32 D) { const int32 N = ADustlineCar::NumModels(); const int32 I = (C->Setup.Model + D + N) % N; C->SelectModel(I); C->SaveSetupToDisk(); };
	auto Bar = [C](const FString& Caption, TFunction<float()> Frac) -> TSharedRef<SWidget>
	{
		TSharedRef<float> Shown = MakeShared<float>(0.f);
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Text(Caption, 17.f, Label2()) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
			[ SNew(SBox).WidthOverride(150.f).HeightOverride(8.f)
				[ SNew(SOverlay)
					+ SOverlay::Slot()[ SNew(SBorder).BorderImage(Box(Fill(), 4.f)) ]
					+ SOverlay::Slot().HAlign(HAlign_Left)[ SNew(SBox).WidthOverride_Lambda([Shown, Frac]() { *Shown += (FMath::Clamp(Frac(), 0.02f, 1.f) - *Shown) * (1.f - FMath::Exp(-10.f * FApp::GetDeltaTime())); return 150.f * *Shown; })[ SNew(SBorder).BorderImage(Box(Label(), 4.f)) ] ] ] ];
	};
	TSharedRef<SWidget> Switcher = Glass(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Button(TEXT("\u2039"), [Step]() { Step(-1); }, EBtn::Round, 0.f, 30.f) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(18, 0)
		[ SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SNew(STextBlock).Font(Font(30.f)).ColorAndOpacity(Label()).Text_Lambda([C]() { return FText::FromString(C ? ADustlineCar::ModelName(C->Setup.Model) : FString()); }) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 16, 0, 0)
			[ SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 26, 0)[ Bar(TEXT("Potencia"), [C]() { return C ? C->GarageStats().PeakHP / 480.f : 0.f; }) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 26, 0)[ Bar(TEXT("Velocidad"), [C]() { return C ? C->GarageStats().VMaxKmh / 230.f : 0.f; }) ]
				+ SHorizontalBox::Slot().AutoWidth()[ Bar(TEXT("Ligereza"), [C]() { return C ? 1.f - (C->GarageStats().Mass - 700.f) / 2800.f : 0.f; }) ] ] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Button(TEXT("\u203A"), [Step]() { Step(1); }, EBtn::Round, 0.f, 30.f) ],
		40.f, FMargin(18, 18));

	return SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SGradient).Horizontal(true).From(Col(0, 0, 0, 0.82f)).To(Col(0, 0, 0, 0.0f)) ]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)[ SNew(SBox).HeightOverride(360.f)[ SNew(SGradient).Horizontal(false).From(Col(0, 0, 0, 0.f)).To(Col(0, 0, 0, 0.62f)) ] ]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(150, 0, 0, 30)[ Stack ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 60)[ SNew(SAppear).Delay(0.35f)[ Switcher ] ]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0, 0, 44, 36)[ Text(FString::Printf(TEXT("Versión %s  ·  Unreal Engine 5.8"), kVersion), 15.f, Label3()) ];
}

// ------------------------------------------------------------------ pausa
TSharedRef<SWidget> FDustlineMenu::BuildPause()
{
	ADustlineCar* C = Car.Get();
	TWeakPtr<FDustlineMenu> Weak = AsShared();
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	V->AddSlot().AutoHeight().HAlign(HAlign_Center)[ Text(TEXT("En pausa"), 40.f, Label(), 1) ];
	V->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 28)[ SNew(STextBlock).Font(Font(19.f)).ColorAndOpacity(Label2()).Text_Lambda([C]() { return FText::FromString(C ? ADustlineCar::ModelName(C->Setup.Model) : FString()); }) ];
	V->AddSlot().AutoHeight()[ Button(TEXT("Reanudar"), [Weak]() { if (auto M = Weak.Pin()) M->Resume(); }, EBtn::Primary, 0.f, 23.f) ];
	V->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[ Button(TEXT("Taller"), [Weak]() { if (auto M = Weak.Pin()) M->OpenGarageFrom(EScreen::Pause); }, EBtn::Secondary, 0.f, 23.f) ];
	V->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[ Button(TEXT("Ajustes"), [Weak]() { if (auto M = Weak.Pin()) M->ShowSettings(); }, EBtn::Secondary, 0.f, 23.f) ];
	V->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 24, 0, 0)
	[ SNew(STextBlock).Font(Font(18.f)).ColorAndOpacity(Label2()).Text_Lambda([C]() { return FText::FromString(C ? FString::Printf(TEXT("%s  \u00B7  %.0f \u00B0C  \u00B7  %s"), *C->WeatherLabel, C->TempC, *C->SurfaceName) : FString()); }) ];
	V->AddSlot().AutoHeight().Padding(0, 22, 0, 0)[ SNew(SBox).HeightOverride(1.f)[ SNew(SBorder).BorderImage(Box(Hair(), 0.f)) ] ];
	V->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[ Button(TEXT("Menú principal"), [Weak]() { if (auto M = Weak.Pin()) M->ShowMain(); }, EBtn::Plain, 0.f, 21.f) ];

	return SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SBorder).BorderImage(Box(Col(0, 0, 0, 0.42f), 0.f)) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[ SNew(SAppear).Rise(18.f)[ SNew(SBox).WidthOverride(500.f)[ Glass(V, 34.f, FMargin(34, 32)) ] ] ];
}

// ------------------------------------------------------------------ ajustes
TSharedRef<SWidget> FDustlineMenu::BuildPage(int32 P)
{
	ADustlineCar* C = Car.Get();
	FDustlineSettings& S = FDustlineSettings::Get();
	UWorld* W = C ? C->GetWorld() : nullptr;
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	auto Add = [&V](TSharedRef<SWidget> X) { V->AddSlot().AutoHeight()[ X ]; };
	const FString Pct = TEXT("%");

	if (P == 0) // Gráficos
	{
		Add(Section(TEXT("Pantalla")));
		Add(Group({
			Row(TEXT("Modo de ventana"), Segmented({ TEXT("Ventana"), TEXT("Pantalla completa") }, []() { return DustlineGfx::IsFullscreen() ? 1 : 0; }, [W](int32 I) { DustlineGfx::SetFullscreen(W, I == 1); })),
			Row(TEXT("Sincronización vertical"), SNew(SToggle).IsOn_Lambda([]() { return DustlineGfx::GetVSync(); }).OnToggled(FOnBooleanValueChanged::CreateLambda([W](bool B) { DustlineGfx::SetVSync(W, B); })), TEXT("Evita cortes en la imagen a cambio de algo de latencia")),
			Row(TEXT("Límite de fotogramas"), Segmented({ TEXT("30"), TEXT("60"), TEXT("Sin límite") }, []() { const int32 L = DustlineGfx::GetFpsLimit(); return L == 30 ? 0 : (L == 60 ? 1 : 2); }, [W](int32 I) { DustlineGfx::SetFpsLimit(W, I == 0 ? 30 : (I == 1 ? 60 : 0)); })) }));
		Add(Section(TEXT("Calidad")));
		Add(Group({
			Row(TEXT("Calidad general"), Segmented({ TEXT("Baja"), TEXT("Media"), TEXT("Alta"), TEXT("Máxima") }, []() { return DustlineGfx::GetQuality(); }, [W](int32 I) { DustlineGfx::SetQuality(W, I); }), TEXT("Sombras, vegetación, texturas y distancia de dibujado")),
			SliderRow(TEXT("Escala de render"), 50.f, 100.f, [&S]() { return S.RenderScale; }, [&S](float V2) { S.RenderScale = V2; S.ApplyGlobal(); }, [](float V2) { return FString::Printf(TEXT("%.0f %%"), V2); }, FSimpleDelegate(), FSimpleDelegate::CreateLambda([&S]() { S.Save(); }), 5.f) }));
		Add(SNew(SBox).Padding(FMargin(20, 12, 20, 0))[ Text(TEXT("Bajar la escala de render es lo que más fotogramas por segundo gana en un Mac con poca memoria."), 15.f, Label2()) ]);
	}
	else if (P == 1) // Juego
	{
		Add(Section(TEXT("Conducción")));
		Add(Group({
			Row(TEXT("Cámara"), Segmented({ TEXT("Cercana"), TEXT("Media"), TEXT("Lejana") }, [C]() { return C ? C->CamMode : 0; }, [C](int32 I) { if (C) C->CamMode = I; }), TEXT("También con la tecla C")),
			SliderRow(TEXT("Campo de visión"), 60.f, 110.f, [&S]() { return S.Fov; }, [&S](float V2) { S.Fov = V2; }, [](float V2) { return FString::Printf(TEXT("%.0f°"), V2); }, FSimpleDelegate(), FSimpleDelegate::CreateLambda([&S]() { S.Save(); }), 1.f),
			Row(TEXT("Unidades de velocidad"), Segmented({ TEXT("km/h"), TEXT("mph") }, [&S]() { return S.Units; }, [&S](int32 I) { S.Units = I; S.Save(); })) }));
		Add(Section(TEXT("Asistencias")));
		Add(Group({
			Row(TEXT("Cambio"), Segmented({ TEXT("Automático"), TEXT("Manual") }, [&S]() { return S.bManual ? 1 : 0; }, [&S](int32 I) { S.bManual = I == 1; S.Save(); }), TEXT("En manual: E sube, Q baja (desde 1ª, marcha atrás)")),
			Row(TEXT("ABS"), SNew(SToggle).IsOn_Lambda([&S]() { return S.bAbs; }).OnToggled(FOnBooleanValueChanged::CreateLambda([&S](bool B) { S.bAbs = B; S.Save(); })), TEXT("Sin ABS las ruedas se bloquean al frenar fuerte y el coche desliza")),
			Row(TEXT("Control de tracción y estabilidad"), SNew(SToggle).IsOn_Lambda([&S]() { return S.bTc; }).OnToggled(FOnBooleanValueChanged::CreateLambda([&S](bool B) { S.bTc = B; S.Save(); })), TEXT("Recorta el gas si patinan las ruedas y frena una rueda si el coche se cruza (ESC en los modelos que lo traen). Sin él, a fondo en barro o nieve el coche se entierra")) }));
		Add(SNew(SBox).Padding(FMargin(20, 12, 20, 0))[ Text(TEXT("El clima no se elige: cambia solo y depende de la zona del mapa. Frentes de lluvia y nieve cruzan con el viento, la montaña es más fría y el suelo se moja, se embarra, se hiela o se cubre de nieve poco a poco."), 15.f, Label2()) ]);
		Add(Section(TEXT("Sonido")));
		Add(Group({ SliderRow(TEXT("Volumen"), 0.f, 100.f, [&S]() { return S.Volume; }, [&S](float V2) { S.Volume = V2; }, [](float V2) { return FString::Printf(TEXT("%.0f %%"), V2); }, FSimpleDelegate(), FSimpleDelegate::CreateLambda([&S]() { S.Save(); }), 5.f) }));
		Add(Section(TEXT("Interfaz")));
		Add(Group({
			Row(TEXT("Mostrar velocímetro"), SNew(SToggle).IsOn_Lambda([&S]() { return S.bHUD; }).OnToggled(FOnBooleanValueChanged::CreateLambda([&S](bool B) { S.bHUD = B; S.Save(); }))),
			Row(TEXT("Mostrar atajos de teclado"), SNew(SToggle).IsOn_Lambda([&S]() { return S.bHints; }).OnToggled(FOnBooleanValueChanged::CreateLambda([&S](bool B) { S.bHints = B; S.Save(); }))) }));
	}
	else if (P == 2) // Controles
	{
		auto Keys = [](std::initializer_list<const TCHAR*> L) -> TSharedRef<SWidget> { TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox); for (const TCHAR* K : L) H->AddSlot().AutoWidth().Padding(4, 0, 0, 0)[ KeyCap(K) ]; return H; };
		Add(Section(TEXT("Conducción")));
		Add(Group({ Row(TEXT("Acelerar / frenar y marcha atrás"), Keys({ TEXT("W"), TEXT("S") })), Row(TEXT("Girar"), Keys({ TEXT("A"), TEXT("D") })), Row(TEXT("Freno de mano"), Keys({ TEXT("Espacio") })),
			Row(TEXT("Turbo"), Keys({ TEXT("Shift") })), Row(TEXT("Enderezar el coche"), Keys({ TEXT("R") })) }));
		Add(Section(TEXT("Juego")));
		Add(Group({ Row(TEXT("Cambiar de cámara"), Keys({ TEXT("C") })), Row(TEXT("Bloquear diferenciales"), Keys({ TEXT("B") })), Row(TEXT("Reductora (parado o muy despacio)"), Keys({ TEXT("L") })), Row(TEXT("Cambio manual: subir / bajar"), Keys({ TEXT("E"), TEXT("Q") })), Row(TEXT("Taller"), Keys({ TEXT("G") })), Row(TEXT("Pausa y menú"), Keys({ TEXT("Esc") })) }));
	}
	else // Acerca de
	{
		Add(SNew(SBox).Padding(FMargin(0, 24, 0, 8)).HAlign(HAlign_Center)[ Text(TEXT("Dustline"), 64.f, Label(), 1) ]);
		Add(SNew(SBox).HAlign(HAlign_Center)[ Text(FString::Printf(TEXT("Versión %s  ·  Unreal Engine 5.8"), kVersion), 18.f, Label2()) ]);
		Add(Section(TEXT("Créditos")));
		Add(Group({
			Row(TEXT("Carrocerías"), Text(TEXT("Kenney · CC0"), 18.f, Label2())),
			Row(TEXT("Texturas del terreno"), Text(TEXT("ambientCG · CC0"), 18.f, Label2())),
			Row(TEXT("Vegetación y rocas"), Text(TEXT("Poly Haven · CC0"), 18.f, Label2())),
			Row(TEXT("Código"), Text(TEXT("MIT · github.com/alefoncu/dustline"), 18.f, Label2())) }));
		Add(Section(TEXT("Datos")));
		Add(Group({ Row(TEXT("Restablecer ajustes"), Button(TEXT("Restablecer"), [this]() { FDustlineSettings D; FDustlineSettings::Get() = D; D.Save(); D.ApplyGlobal(); SetPage(Page); }, EBtn::Danger, 0.f, 18.f), TEXT("Vuelve a los valores de fábrica (no borra tus coches del garaje)")) }));
	}
	return SNew(SBox).Padding(FMargin(0, 0, 0, 30))[ V ];
}

TSharedRef<SWidget> FDustlineMenu::BuildSettings()
{
	TWeakPtr<FDustlineMenu> Weak = AsShared();
	const TArray<FString> Names = { TEXT("Gráficos"), TEXT("Juego"), TEXT("Controles"), TEXT("Acerca de") };

	TSharedRef<SVerticalBox> Side = SNew(SVerticalBox);
	Side->AddSlot().AutoHeight().Padding(10, 4, 0, 22)[ Text(TEXT("Ajustes"), 36.f, Label(), 1) ];
	static const FButtonStyle* Invis = nullptr;
	for (int32 i = 0; i < Names.Num(); ++i)
	{
		const int32 Idx = i;
		Side->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[ SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0))
			.OnClicked(FOnClicked::CreateLambda([Weak, Idx]() { if (auto M = Weak.Pin()) M->SetPage(Idx); return FReply::Handled(); }))
			[ SNew(SBorder).BorderImage_Lambda([Weak, Idx]() { auto M = Weak.Pin(); return (M.IsValid() && M->Page == Idx) ? Box(Accent(), 12.f) : Box(DUI::Clear(), 12.f); }).Padding(FMargin(16, 12))
				[ Text(Names[i], 23.f, Label()) ] ] ];
	}
	(void)Invis;

	SAssignNew(PageBox, SBox)[ BuildPage(Page) ];

	TSharedRef<SWidget> Header = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ SNew(STextBlock).Font(Font(34.f, 1)).ColorAndOpacity(Label()).Text_Lambda([Weak, Names]() { auto M = Weak.Pin(); return FText::FromString(M.IsValid() ? Names[FMath::Clamp(M->Page, 0, 3)] : FString()); }) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Button(TEXT("Listo"), [Weak]() { if (auto M = Weak.Pin()) M->OnEscape(); }, EBtn::Primary, 0.f, 20.f) ];

	TSharedRef<SWidget> Body = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(300.f).Padding(FMargin(22, 30, 22, 22))[ Side ] ]
		+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(1.f)[ SNew(SBorder).BorderImage(Box(Hair(), 0.f)) ] ]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[ SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(40, 30, 40, 4)[ Header ]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(40, 0, 40, 0)[ SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed) + SScrollBox::Slot()[ PageBox.ToSharedRef() ] ] ];

	return SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SBorder).BorderImage(Box(Col(0, 0, 0, 0.55f), 0.f)) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[ SNew(SAppear).Rise(18.f)[ SNew(SBox).WidthOverride(1400.f).HeightOverride(880.f)[ Glass(Body, 36.f, FMargin(0)) ] ] ];
}

// ------------------------------------------------------------------ HUD
TSharedRef<SWidget> FDustlineMenu::BuildHud()
{
	ADustlineCar* C = Car.Get();
	TWeakPtr<FDustlineMenu> Weak = AsShared();
	FDustlineSettings* S = &FDustlineSettings::Get();
	auto Driving = [C, Weak]() { auto M = Weak.Pin(); return C && M.IsValid() && M->Screen == EScreen::None && !C->bGarage && !C->bShowcase; };
	auto Speed = [C, S]() { const float V = C ? C->SpeedKmh : 0.f; return S->Units == 1 ? V * 0.621371f : V; };

	auto SpeedFrac = [C, Speed]() { return C ? FMath::Clamp(Speed() / (C->Sp.VMaxGear * 3.6f * 1.2f * (C && FDustlineSettings::Get().Units == 1 ? 0.621371f : 1.f)), 0.f, 1.f) : 0.f; };
	TSharedRef<SWidget> SpeedCard = Glass(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[ SNew(SBox).WidthOverride(330.f).HeightOverride(300.f)
			[ SNew(SOverlay)
				+ SOverlay::Slot()[ SNew(SArc).Thickness(14.f).Inset(4.f).Color(Accent()).Value_Lambda(SpeedFrac) ]
				+ SOverlay::Slot().Padding(0, 0, 0, 0)[ SNew(SBox).Visibility_Lambda([C]() { return C && C->Setup.Turbo > 0.05f ? EVisibility::Visible : EVisibility::Collapsed; })[ SNew(SArc).Thickness(7.f).Inset(26.f).Color(Orange()).Value_Lambda([C]() { return C ? C->TurboSpool : 0.f; }) ] ]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0, 0, 0, 14)
				[ SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SNew(STextBlock).Font(Font(104.f, 1)).ColorAndOpacity(Label()).Text_Lambda([Speed]() { return FText::FromString(FString::FromInt(FMath::RoundToInt(Speed()))); }) ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SNew(STextBlock).Font(Font(26.f)).ColorAndOpacity(Label2()).Text_Lambda([S]() { return FText::FromString(S->Units == 1 ? TEXT("mph") : TEXT("km/h")); }) ] ] ] ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
		[ SNew(STextBlock).Font(Font(20.f)).ColorAndOpacity(Label2()).Text_Lambda([C]()
			{
				if (!C) return FText();
				return FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s %.0f\u00B0"), *C->SurfaceName, *C->WeatherLabel, C->TempC));
			}) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 0)
		[ SNew(STextBlock).Font(Font(18.f, 1)).ColorAndOpacity(Label()).Text_Lambda([C]()
			{
				if (!C) return FText();
				const FString G = C->Gear < 0 ? TEXT("R") : (C->Gear == 0 ? TEXT("N") : FString::FromInt(C->Gear));
				FString Tags;
				if (C->bLowRange) Tags += TEXT("  \u00B7  4L");
				if (C->bDiffLock) Tags += TEXT("  \u00B7  Bloqueo");
				if (C->bHdcActive) Tags += TEXT("  \u00B7  Descenso");
				if (C->bEscActive) Tags += TEXT("  \u00B7  ESC");
				if (C->DamageTotal() > 0.05f) Tags += FString::Printf(TEXT("  \u00B7  Daños %.0f %%"), C->DamageTotal() * 100.f);
				{ const FRotator R = C->GetActorRotation(); if (FMath::Abs(R.Pitch) > 8.f || FMath::Abs(R.Roll) > 8.f) Tags += FString::Printf(TEXT("  \u00B7  %s %.0f\u00B0 / lateral %.0f\u00B0"), R.Pitch > 0.f ? TEXT("Subida") : TEXT("Bajada"), FMath::Abs(R.Pitch), FMath::Abs(R.Roll)); }
				if (C->bStalled) Tags += TEXT("  \u00B7  Motor ahogado");
				else if (C->WaterDepthM > 0.05f) Tags += FString::Printf(TEXT("  \u00B7  Vadeo %.0f cm"), C->WaterDepthM * 100.f);
				else if (C->MaxSinkM() > 0.06f) Tags += FString::Printf(TEXT("  \u00B7  Hundido %.0f cm"), C->MaxSinkM() * 100.f);
				return FText::FromString(FString::Printf(TEXT("%s  \u00B7  %.0f rpm%s"), *G, C->Rpm, *Tags));
			}) ],
		40.f, FMargin(26, 24, 26, 22));

	TSharedRef<SWidget> Chip = Glass(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(STextBlock).Font(Font(20.f)).ColorAndOpacity(Label()).Text_Lambda([C]() { return FText::FromString(C ? ADustlineCar::ModelName(C->Setup.Model) : FString()); }) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14, 0, 0, 0)[ SNew(STextBlock).Font(Font(17.f)).ColorAndOpacity(Label2()).Text_Lambda([C]() { return FText::FromString(C ? FString::Printf(TEXT("%.0f CV  ·  %.0f kg"), C->Sp.HP * (1.f + 0.5f * C->Setup.Turbo * C->TurboSpool), C->Sp.Mass) : FString()); }) ],
		26.f, FMargin(22, 11));

	auto HintKey = [](const TCHAR* K, const TCHAR* L) -> TSharedRef<SWidget>
	{ return SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ KeyCap(K, 15.f) ] + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 22, 0)[ Text(L, 16.f, Label2()) ]; };
	TSharedRef<SWidget> Hints = SNew(SBorder).BorderImage(Box(DUI::Clear(), 0.f))
		.ColorAndOpacity_Lambda([Weak, S]() { auto M = Weak.Pin(); if (!M.IsValid() || !S->bHints) return FLinearColor(1, 1, 1, 0); const float T = (float)(FPlatformTime::Seconds() - M->DriveStart); return FLinearColor(1, 1, 1, FMath::Clamp(1.f - (T - 9.f) / 2.f, 0.f, 1.f) * FMath::Clamp(T / 0.6f, 0.f, 1.f)); })
		[ Glass(SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[ HintKey(TEXT("Esc"), TEXT("Pausa")) ] + SHorizontalBox::Slot().AutoWidth()[ HintKey(TEXT("G"), TEXT("Taller")) ]
			+ SHorizontalBox::Slot().AutoWidth()[ HintKey(TEXT("C"), TEXT("Cámara")) ] + SHorizontalBox::Slot().AutoWidth()[ HintKey(TEXT("R"), TEXT("Enderezar")) ] + SHorizontalBox::Slot().AutoWidth()[ HintKey(TEXT("Shift"), TEXT("Turbo")) ], 26.f, FMargin(20, 11, 0, 11)) ];

	return SNew(SBox).Visibility_Lambda([Driving, S]() { return Driving() && S->bHUD ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		[ SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0, 0, 48, 48)[ SpeedCard ]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(48, 44, 0, 0)[ Chip ]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 48)[ Hints ] ];
}
