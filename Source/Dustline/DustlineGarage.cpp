#include "DustlineCar.h"
#include "DustlineCarParts.h"
#include "UnrealClient.h"
#include "Misc/App.h"
#include "Widgets/Layout/SUniformGridPanel.h"
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

// el fichero va por estilo de carrocería (no por posición en la lista), así quitar o reordenar modelos no mezcla ajustes
static FString SetupPath(int32 Model) { return FPaths::ProjectSavedDir() / TEXT("Garage") / FString::Printf(TEXT("setup_%d.json"), ADustlineCar::GetModel(Model).Style); }

void ADustlineCar::SaveSetupToDisk() const
{
	FString Json; if (FJsonObjectConverter::UStructToJsonObjectString(Setup, Json)) FFileHelper::SaveStringToFile(Json, *SetupPath(Setup.Model));
	FFileHelper::SaveStringToFile(FString::FromInt(GetModel(Setup.Model).Style), *(FPaths::ProjectSavedDir() / TEXT("Garage") / TEXT("last_model.txt")));
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

ADustlineCar::FGarageStats ADustlineCar::StatsFor(const FDustlineSetup& S) const
{
	const FDustlineModel& M = GetModel(S.Model);
	FGarageStats R;
	R.HP = S.EngineCC * 0.065f; R.PeakHP = R.HP * (1.f + 0.5f * S.Turbo);
	const float wheelMass = S.TireRadiusM * S.TireRadiusM * S.TireWidthM * 520.f, stock = M.Radius * M.Radius * M.Width * 520.f;
	R.Mass = FMath::Max(450.f, M.Mass + (S.EngineCC - M.CC) * 0.065f + (S.Turbo - M.Turbo) * 30.f + (wheelMass - stock) + S.Ballast + (S.TravelM - M.Travel) * 60.f);
	R.VMaxKmh = 34.f * (S.TireRadiusM / 0.46f) * (1.f + 0.12f * S.Turbo) * (M.Style == 2 ? 0.8f : 1.f) * 3.6f;
	R.PowerToWeight = R.PeakHP / R.Mass * 1000.f;
	R.Clearance = GroundClearance();
	int32 cyl = FMath::Clamp(FMath::RoundToInt(S.EngineCC / 650.f), 2, 12); if (cyl > 6 && (cyl % 2)) cyl += 1;
	R.Cyl = cyl;
	return R;
}

ADustlineCar::FGarageStats ADustlineCar::GarageStats() const { return StatsFor(Setup); }

namespace
{
struct FPart { FString Name, Desc; TFunction<void(FDustlineSetup&)> Apply; TFunction<bool(const FDustlineSetup&)> Match; };
bool Eq(float a, float b) { return FMath::IsNearlyEqual(a, b, 0.006f); }
FString EngDesc(float cc)
{
	int32 cyl = FMath::Clamp(FMath::RoundToInt(cc / 650.f), 2, 12); if (cyl > 6 && (cyl % 2)) cyl += 1;
	const TCHAR* Rasgo = cc < 2000.f ? TEXT("Tapa de plástico lisa") : (cc < 2800.f ? TEXT("Cubierta con nervios") : (cc < 3600.f ? TEXT("Pleno pulido") : (cc < 4800.f ? TEXT("Filtro redondo clásico") : (cc < 6300.f ? TEXT("Filtro cónico rojo") : (cc < 8000.f ? TEXT("Trompetas de admisión") : TEXT("Supercargador"))))));
	return FString::Printf(TEXT("%.1f L  ·  %d cil.  ·  %.0f CV  ·  %s"), cc / 1000.f, cyl, cc * 0.065f, Rasgo);
}
const TCHAR* GCatNames[] = { TEXT("Motor"), TEXT("Turbo"), TEXT("Ruedas"), TEXT("Neumáticos"), TEXT("Suspensión"), TEXT("Tracción"), TEXT("Chasis"), TEXT("Pintura") };

TArray<FPart> MakeParts(int32 Cat, const FDustlineModel& M)
{
	TArray<FPart> P;
	auto Add = [&P](const FString& N, const FString& D, TFunction<void(FDustlineSetup&)> A, TFunction<bool(const FDustlineSetup&)> Mt) { P.Add({ N, D, A, Mt }); };
	if (Cat == 0)
	{
		const float Stock = M.CC;
		Add(TEXT("Original"), TEXT("Motor de fábrica  ·  ") + EngDesc(Stock), [Stock](FDustlineSetup& S) { S.EngineCC = Stock; }, [Stock](const FDustlineSetup& S) { return Eq(S.EngineCC, Stock); });
		struct E { const TCHAR* N; float CC; };
		for (const E& e : { E{ TEXT("Compacto"), 1600.f }, E{ TEXT("Turismo"), 2400.f }, E{ TEXT("Deportivo V6"), 3200.f }, E{ TEXT("V8 todoterreno"), 4000.f }, E{ TEXT("V8 alto rendimiento"), 5700.f }, E{ TEXT("V8 competición"), 7000.f }, E{ TEXT("V10 extremo"), 9000.f } })
		{ const float CC = e.CC; Add(e.N, EngDesc(CC), [CC](FDustlineSetup& S) { S.EngineCC = CC; }, [CC](const FDustlineSetup& S) { return Eq(S.EngineCC, CC); }); }
	}
	else if (Cat == 1)
	{
		const float Stock = M.Turbo;
		Add(TEXT("Original"), Stock < 0.05f ? FString(TEXT("Sin turbo de fábrica")) : FString::Printf(TEXT("Turbo de fábrica  ·  +%.0f %% de potencia"), Stock * 50.f), [Stock](FDustlineSetup& S) { S.Turbo = Stock; }, [Stock](const FDustlineSetup& S) { return Eq(S.Turbo, Stock); });
		struct T { const TCHAR* N; const TCHAR* D; float V; };
		for (const T& t : { T{ TEXT("Sin turbo"), TEXT("Sin turbo ni intercooler: respuesta inmediata y menos peso"), 0.f }, T{ TEXT("Turbo pequeño"), TEXT("Un turbo compacto  ·  +30 % de potencia"), 0.6f }, T{ TEXT("Turbo medio"), TEXT("Un turbo con intercooler grande  ·  +60 %"), 1.2f },
			T{ TEXT("Doble turbo"), TEXT("Dos turbos  ·  +90 % de potencia"), 1.8f }, T{ TEXT("Competición"), TEXT("Dos turbos grandes, compresor cromado y fundas térmicas  ·  +120 %"), 2.4f }, T{ TEXT("Extremo"), TEXT("Tres turbos y botella de nitro  ·  +150 %"), 3.0f } })
		{ const float V = t.V; Add(t.N, t.D, [V](FDustlineSetup& S) { S.Turbo = V; }, [V](const FDustlineSetup& S) { return Eq(S.Turbo, V); }); }
	}
	else if (Cat == 2)
	{
		const float R = M.Radius, W = M.Width;
		Add(TEXT("Originales"), FString::Printf(TEXT("De fábrica  ·  %.0f cm de radio  ·  %.0f cm de ancho"), R * 100.f, W * 100.f), [R, W](FDustlineSetup& S) { S.TireRadiusM = R; S.TireWidthM = W; }, [R, W](const FDustlineSetup& S) { return Eq(S.TireRadiusM, R) && Eq(S.TireWidthM, W); });
		struct Wh { const TCHAR* N; float R, W; };
		for (const Wh& w : { Wh{ TEXT("Urbanas 15\""), 0.33f, 0.22f }, Wh{ TEXT("Estándar 17\""), 0.40f, 0.28f }, Wh{ TEXT("Todoterreno 33\""), 0.42f, 0.29f }, Wh{ TEXT("Grandes 35\""), 0.445f, 0.31f }, Wh{ TEXT("Monster 40\""), 0.51f, 0.35f }, Wh{ TEXT("Gigantes 44\""), 0.56f, 0.38f } })
		{ const float r = w.R, wd = w.W; Add(w.N, FString::Printf(TEXT("%.0f cm de radio  ·  %.0f cm de ancho"), r * 100.f, wd * 100.f), [r, wd](FDustlineSetup& S) { S.TireRadiusM = r; S.TireWidthM = wd; }, [r, wd](const FDustlineSetup& S) { return Eq(S.TireRadiusM, r) && Eq(S.TireWidthM, wd); }); }
	}
	else if (Cat == 3)
	{
		struct Ty { const TCHAR* N; const TCHAR* D; };
		int32 i = 0;
		for (const Ty& t : { Ty{ TEXT("Carretera"), TEXT("Mucho agarre en asfalto y firme duro; resbala en barro y nieve") }, Ty{ TEXT("Todo terreno"), TEXT("Buen equilibrio entre tierra, hierba y carretera") }, Ty{ TEXT("Barro"), TEXT("Tacos profundos: arrastran en barro y tierra blanda") },
			Ty{ TEXT("Clavos"), TEXT("Imprescindibles sobre nieve y hielo") }, Ty{ TEXT("Balón"), TEXT("Gran superficie: flotan sobre arena y nieve blanda") } })
		{ const int32 k = i++; Add(t.N, t.D, [k](FDustlineSetup& S) { S.TireType = k; }, [k](const FDustlineSetup& S) { return S.TireType == k; }); }
	}
	else if (Cat == 4)
	{
		const float St = M.Stiff, Tr = M.Travel;
		Add(TEXT("Original"), FString::Printf(TEXT("De fábrica  ·  dureza ×%.1f  ·  %.0f cm de recorrido"), St, Tr * 100.f), [St, Tr](FDustlineSetup& S) { S.Stiffness = St; S.TravelM = Tr; }, [St, Tr](const FDustlineSetup& S) { return Eq(S.Stiffness, St) && Eq(S.TravelM, Tr); });
		struct Su { const TCHAR* N; const TCHAR* D; float St, Tr; };
		for (const Su& s : { Su{ TEXT("Deportiva baja"), TEXT("Dura y baja: precisa en firme, mala en terreno roto"), 1.9f, 0.30f }, Su{ TEXT("Confort"), TEXT("Blanda y filtrante, algo de balanceo"), 0.55f, 0.55f }, Su{ TEXT("Todoterreno"), TEXT("Recorrido largo y dureza media"), 0.95f, 0.70f },
			Su{ TEXT("Competición"), TEXT("Dura con mucho recorrido: la mejor para saltos"), 1.5f, 0.80f }, Su{ TEXT("Rock crawler"), TEXT("Recorrido máximo para trepar rocas"), 0.8f, 0.95f } })
		{ const float a = s.St, b = s.Tr; Add(s.N, FString::Printf(TEXT("%s  ·  ×%.1f  ·  %.0f cm"), s.D, a, b * 100.f), [a, b](FDustlineSetup& S) { S.Stiffness = a; S.TravelM = b; }, [a, b](const FDustlineSetup& S) { return Eq(S.Stiffness, a) && Eq(S.TravelM, b); }); }
	}
	else if (Cat == 5)
	{
		struct Dr { const TCHAR* N; const TCHAR* D; };
		int32 i = 0;
		for (const Dr& d : { Dr{ TEXT("4×4 integral"), TEXT("Tracción en las cuatro ruedas: máximo agarre y estabilidad") }, Dr{ TEXT("Trasera"), TEXT("Más divertida y deslizante, exige más pilotaje") }, Dr{ TEXT("Delantera"), TEXT("Estable y fácil, pero se queda sin tracción en cuesta") } })
		{ const int32 k = i++; Add(d.N, d.D, [k](FDustlineSetup& S) { S.Drive = k; }, [k](const FDustlineSetup& S) { return S.Drive == k; }); }
	}
	else if (Cat == 6)
	{
		struct Ch { const TCHAR* N; const TCHAR* D; float B; };
		for (const Ch& c : { Ch{ TEXT("Aligerado"), TEXT("Sin extras: −250 kg, más ágil pero menos estable"), -250.f }, Ch{ TEXT("Estándar"), TEXT("El peso de serie"), 0.f }, Ch{ TEXT("Reforzado"), TEXT("Barras y protecciones: +350 kg"), 350.f }, Ch{ TEXT("Blindado"), TEXT("Lastre máximo: +700 kg, muy estable"), 700.f } })
		{ const float B = c.B; Add(c.N, c.D, [B](FDustlineSetup& S) { S.Ballast = B; }, [B](const FDustlineSetup& S) { return Eq(S.Ballast, B); }); }
	}
	return P;
}
}

TSharedRef<SWidget> ADustlineCar::BuildGarageWidget()
{
	using namespace DUI;
	ADustlineCar* Car = this;
	FDustlineSettings* St = &FDustlineSettings::Get();
	TSharedRef<int32> Tab = MakeShared<int32>(0);
	TSharedRef<TSharedPtr<SBox>> Holder = MakeShared<TSharedPtr<SBox>>();

	auto Tile = [](const FString& Caption, TFunction<float()> Target, TFunction<FString(float)> Fmt) -> TSharedRef<SWidget>
	{
		TSharedRef<float> Shown = MakeShared<float>(Target());
		return SNew(SBorder).BorderImage(Box(Card(), 18.f, Hair(), 1.f)).Padding(FMargin(18, 12))
			[ SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(Font(30.f, 1)).ColorAndOpacity(Label()).Text_Lambda([Shown, Target, Fmt]()
					{ const float T = Target(); *Shown += (T - *Shown) * (1.f - FMath::Exp(-11.f * FApp::GetDeltaTime())); if (FMath::Abs(*Shown - T) < 0.05f) *Shown = T; return FText::FromString(Fmt(*Shown)); }) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 1, 0, 0)[ Text(Caption, 14.f, Label2()) ] ];
	};
	auto DeltaOf = [Car](const FPart& P, int32 Kind) -> float
	{
		FDustlineSetup Cand = Car->Setup; P.Apply(Cand);
		const FGarageStats A = Car->StatsFor(Car->Setup), B = Car->StatsFor(Cand);
		return Kind == 0 ? B.PeakHP - A.PeakHP : (Kind == 1 ? B.VMaxKmh - A.VMaxKmh : B.Mass - A.Mass);
	};
	auto DeltaChip = [Car, DeltaOf](const FPart& P, int32 Kind) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock).Font(Font(17.f))
			.Visibility_Lambda([=]() { return (!P.Match(Car->Setup) && FMath::Abs(DeltaOf(P, Kind)) >= (Kind == 1 ? 1.f : 2.f)) ? EVisibility::Visible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([=]() { const float D = DeltaOf(P, Kind); const bool bGood = Kind == 2 ? D < 0.f : D > 0.f; return bGood ? Green() : (Kind == 2 ? Orange() : Red()); })
			.Text_Lambda([=]() { const float D = DeltaOf(P, Kind); return FText::FromString(FString::Printf(TEXT("%s%.0f %s"), D > 0.f ? TEXT("+") : TEXT("\u2212"), FMath::Abs(D), Kind == 0 ? TEXT("CV") : (Kind == 1 ? TEXT("km/h") : TEXT("kg")))); });
	};
	auto PartCard = [Car, DeltaChip](const FPart& P) -> TSharedRef<SWidget>
	{
		TSharedPtr<SButton> Btn;
		SAssignNew(Btn, SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0))
			.OnClicked(FOnClicked::CreateLambda([Car, P]() { P.Apply(Car->Setup); Car->MarkSetupDirty(); Car->SaveSetupToDisk(); return FReply::Handled(); }))
			[ SNew(SBorder).BorderImage_Lambda([Car, P, W = TWeakPtr<SButton>(Btn)]() { if (P.Match(Car->Setup)) return Box(Col(10, 132, 255, 0.20f), 18.f, Accent(), 2.f); TSharedPtr<SButton> B = W.Pin(); return (B.IsValid() && B->IsHovered()) ? Box(Col(255, 255, 255, 0.13f), 18.f, Col(255, 255, 255, 0.20f), 1.f) : Box(Card(), 18.f, Hair(), 1.f); }).Padding(FMargin(20, 15, 16, 15))
				[ SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[ SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ Text(P.Name, 21.f, Label()) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)[ Text(P.Desc, 15.f, Label2(), 0, true) ] ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(12, 0, 0, 0)
					[ SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ DeltaChip(P, 0) ]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ DeltaChip(P, 1) ]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ DeltaChip(P, 2) ] ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14, 0, 0, 0)
					[ SNew(SBox).WidthOverride(26.f).HeightOverride(26.f)
						[ SNew(SOverlay)
							+ SOverlay::Slot()[ SNew(SBorder).BorderImage_Lambda([Car, P]() { return P.Match(Car->Setup) ? Box(Accent(), 13.f) : Box(DUI::Clear(), 13.f, Label3(), 2.f); }) ]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(10.f).HeightOverride(10.f).Visibility_Lambda([Car, P]() { return P.Match(Car->Setup) ? EVisibility::Visible : EVisibility::Collapsed; })[ SNew(SBorder).BorderImage(Box(FLinearColor::White, 5.f)) ] ] ] ] ] ];
		return Btn.ToSharedRef();
	};

	// contenido de cada pestaña
	auto BuildTab = [Car, PartCard](int32 T) -> TSharedRef<SWidget>
	{
		TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
		if (T == 7)
		{
			TSharedRef<SHorizontalBox> Paints = SNew(SHorizontalBox);
			const FLinearColor Cols[] = { FLinearColor(0.50f, 0.52f, 0.55f), FLinearColor(0.02f, 0.02f, 0.025f), FLinearColor(0.85f, 0.85f, 0.82f), FLinearColor(0.70f, 0.03f, 0.02f), FLinearColor(0.95f, 0.62f, 0.02f),
				FLinearColor(0.03f, 0.17f, 0.07f), FLinearColor(0.02f, 0.10f, 0.45f), FLinearColor(0.95f, 0.20f, 0.01f) };
			for (const FLinearColor& C : Cols)
			{
				FLinearColor Opaque = C; Opaque.A = 1.f;
				Paints->AddSlot().AutoWidth().Padding(0, 0, 14, 0)
				[ SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0))
					.OnClicked(FOnClicked::CreateLambda([Car, C]() { Car->Setup.Paint = C; Car->MarkSetupDirty(); Car->SaveSetupToDisk(); return FReply::Handled(); }))
					[ SNew(SBox).WidthOverride(42.f).HeightOverride(42.f)
						[ SNew(SBorder).BorderImage_Lambda([Car, C, Opaque]() { const bool Sel = FLinearColor::Dist(Car->Setup.Paint, C) < 0.02f; return Sel ? Box(Opaque, 21.f, FLinearColor::White, 3.f) : Box(Opaque, 21.f, Col(255, 255, 255, 0.22f), 1.f); }) ] ] ];
			}
			V->AddSlot().AutoHeight().Padding(4, 8, 0, 0)[ Paints ];
			return V;
		}
		const TArray<FPart> Parts = MakeParts(T, GetModel(Car->Setup.Model));
		int32 Idx = 0; for (const FPart& P : Parts) V->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[ SNew(SAppear).Rise(12.f).Delay(0.03f * Idx++)[ PartCard(P) ] ];
		return V;
	};
	auto SetTab = [Car, Tab, Holder, BuildTab](int32 T)
	{
		*Tab = T;
		static const int32 Focus[] = { 1, 1, 2, 2, 3, 3, 0, 0 };
		Car->GarageFocus = Focus[T]; Car->SetXRay(T == 0 || T == 1 || T == 4);
		if (Holder->IsValid()) (*Holder)->SetContent(BuildTab(T));
	};
	SAssignNew(*Holder, SBox)[ BuildTab(0) ];
	Car->GarageFocus = 1; Car->SetXRay(true);

	TArray<FString> TabNames; for (const TCHAR* N : GCatNames) TabNames.Add(N);

	TSharedRef<SUniformGridPanel> TabGrid = SNew(SUniformGridPanel).SlotPadding(FMargin(2.5f));
	for (int32 i = 0; i < TabNames.Num(); ++i)
	{
		TSharedPtr<SButton> Btn;
		SAssignNew(Btn, SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(FMargin(0))
			.OnClicked(FOnClicked::CreateLambda([SetTab, i]() { SetTab(i); return FReply::Handled(); }))
			[ SNew(SBorder).BorderImage_Lambda([Tab, i, W = TWeakPtr<SButton>(Btn)]() { if (*Tab == i) return Box(Accent(), 14.f); TSharedPtr<SButton> B = W.Pin(); return (B.IsValid() && B->IsHovered()) ? Box(Col(255, 255, 255, 0.20f), 14.f) : Box(Col(255, 255, 255, 0.10f), 14.f); }).Padding(FMargin(0, 10)).HAlign(HAlign_Center)
				[ Text(TabNames[i], 14.5f, Label()) ] ];
		TabGrid->AddSlot(i % 4, i / 4)[ Btn.ToSharedRef() ];
	}

	TSharedRef<SWidget> Header = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[ SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Text(TEXT("Taller"), 38.f, Label(), 1) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)[ SNew(STextBlock).Font(Font(18.f)).ColorAndOpacity(Label2()).Text_Lambda([Car]() { return FText::FromString(ModelName(Car->Setup.Model)); }) ] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Button(TEXT("Listo"), [Car]() { Car->CloseGarage(); }, EBtn::Primary, 0.f, 20.f) ];

	TSharedRef<SWidget> Stats = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 6, 0)[ Tile(TEXT("Potencia (con turbo)"), [Car]() { return Car->GarageStats().PeakHP; }, [](float V) { return FString::Printf(TEXT("%.0f CV"), V); }) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6, 0, 0, 0)[ Tile(TEXT("Peso"), [Car]() { return Car->GarageStats().Mass; }, [](float V) { return FString::Printf(TEXT("%.0f kg"), V); }) ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[ SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 6, 0)[ Tile(TEXT("Velocidad máxima"), [Car, St]() { const float V2 = Car->GarageStats().VMaxKmh; return St->Units == 1 ? V2 * 0.621371f : V2; }, [St](float V) { return St->Units == 1 ? FString::Printf(TEXT("%.0f mph"), V) : FString::Printf(TEXT("%.0f km/h"), V); }) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6, 0, 0, 0)[ Tile(TEXT("Altura libre"), [Car]() { return Car->GarageStats().Clearance; }, [](float V) { return FString::Printf(TEXT("%.0f cm"), V); }) ] ];

	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)[ Button(TEXT("Restaurar coche de serie"), [Car, Tab, SetTab]() { const int32 m = Car->Setup.Model; Car->Setup = StockSetup(m); Car->MarkSetupDirty(); Car->SaveSetupToDisk(); }, EBtn::Secondary, 0.f, 18.f) ];

	return SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(FMargin(0, 28, 28, 28))
		[ SNew(SAppear).Rise(0.f)
			[ SNew(SBox).WidthOverride(640.f)
				[ Glass(SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[ Header ]
					+ SVerticalBox::Slot().AutoHeight()[ Stats ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 12)[ TabGrid ]
					+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed) + SScrollBox::Slot()[ SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[ Holder->ToSharedRef() ] + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 24)[ Footer ] ] ], 34.f, FMargin(28, 26, 28, 0)) ] ] ];
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

static constexpr float kOut = 0.30f, kIn = 0.70f;
static float EaseBack(float T)
{
	T = FMath::Clamp(T, 0.f, 1.f); const float C1 = 1.15f, C3 = C1 + 1.f, U = T - 1.f;
	return 1.f + C3 * U * U * U + C1 * U * U;
}
// desplazamiento de una pieza: 0 = montada, 1 = fuera. Shift retrasa la pieza (escalonado de ruedas).
static float SwapD(const ADustlineCar::FSwap& W, float Shift)
{
	if (W.S == ADustlineCar::ESwap::Idle) return 0.f;
	if (W.S == ADustlineCar::ESwap::Out) { const float t = FMath::Clamp((W.T - Shift) / kOut, 0.f, 1.f); return W.D0 + (1.f - W.D0) * t * t * t; }
	const float t = FMath::Clamp((W.T - Shift) / kIn, 0.f, 1.f); return W.D0 * (1.f - EaseBack(t));
}
bool ADustlineCar::NoteSetupChange()
{
	if (!bHaveLast) { LastSetup = Setup; PaintShown = Setup.Paint; bHaveLast = true; return false; }
	const FDustlineSetup& A = LastSetup; const FDustlineSetup& B = Setup;
	auto Ne = [](float a, float b) { return !FMath::IsNearlyEqual(a, b, 0.004f); };
	auto Start = [this](FSwap& W, float Stag) { if (W.S != ESwap::Out) { W.S = ESwap::Out; W.T = 0.f; W.D0 = W.D; } W.Stag = Stag; bRebuildPending = true; };
	if (A.Model != B.Model) Start(SwModel, 0.f);
	else
	{
		if (Ne(A.EngineCC, B.EngineCC) || Ne(A.Turbo, B.Turbo)) Start(SwEngine, 0.f);
		if (Ne(A.TireRadiusM, B.TireRadiusM) || Ne(A.TireWidthM, B.TireWidthM) || A.TireType != B.TireType) Start(SwWheels, 0.15f);
		if (Ne(A.Stiffness, B.Stiffness) || Ne(A.TravelM, B.TravelM) || A.Drive != B.Drive) Start(SwSusp, 0.f);
		if (Ne(A.Ballast, B.Ballast)) BounceT = 0.f;
	}
	if (!A.Paint.Equals(B.Paint, 0.002f)) { PaintFrom = PaintShown; AnimPaint = 0.f; }
	LastSetup = B;
	return bRebuildPending;
}
void ADustlineCar::StepAnims(float Dt)
{
	FSwap* All[4] = { &SwEngine, &SwWheels, &SwSusp, &SwModel };
	bool bAllOutDone = true;
	for (FSwap* W : All)
	{
		if (W->S == ESwap::Idle) continue;
		W->T += Dt;
		if (W->S == ESwap::Out) { W->D = SwapD(*W, 0.f); if (W->T < kOut + W->Stag) bAllOutDone = false; }
		else if (W->T >= kIn + W->Stag) { W->S = ESwap::Idle; W->D = 0.f; W->D0 = 0.f; }
	}
	if (bRebuildPending && bAllOutDone)
	{
		Rebuild(); bRebuildPending = false;
		for (FSwap* W : All) if (W->S == ESwap::Out) { W->S = ESwap::In; W->T = 0.f; W->D0 = 1.f; W->D = 1.f; }
		if (SwWheels.S == ESwap::In || SwSusp.S == ESwap::In || SwModel.S == ESwap::In) BounceT = -0.40f;
	}
	AnimPaint = FMath::Min(1.f, AnimPaint + Dt / 0.45f); BounceT += Dt;
	{
		const float Tg = bXRay ? 1.f : 0.f; XLift += (Tg - XLift) * (1.f - FMath::Exp(-7.f * Dt)); if (FMath::Abs(Tg - XLift) < 0.003f) XLift = Tg;
	}
}
void ADustlineCar::ApplyAnims()
{
	if (!Visual || !Engine || !Suspension) return;
	const float Bounce = (BounceT >= 0.f && BounceT < 1.4f) ? -6.f * FMath::Exp(-5.f * BounceT) * FMath::Cos(15.f * BounceT) : 0.f;
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -ZCom + SwapD(SwModel, 0.f) * 170.f + Bounce));
	Engine->SetRelativeLocation(FVector(0.f, 0.f, SwapD(SwEngine, 0.f) * 120.f));
	Suspension->SetRelativeLocation(FVector(0.f, 0.f, -SwapD(SwSusp, 0.f) * 75.f));
	const float Reach = Setup.TireWidthM * 50.f + 140.f;
	for (int32 i = 0; i < Wheels.Num(); ++i)
	{
		UProceduralMeshComponent* W = Wheels[i]; if (!W) continue;
		const float D = SwapD(SwWheels, i * SwWheels.Stag / 3.f);
		if (D == 0.f) continue;
		FVector L = W->GetRelativeLocation(); const float Side = L.Y >= 0.f ? 1.f : -1.f;
		L.Y += Side * Reach * D; L.Z += 18.f * FMath::Sin(PI * FMath::Clamp(D, 0.f, 1.f));
		W->SetRelativeLocationAndRotation(L, W->GetRelativeRotation().Quaternion() * FQuat(FVector(0, 1, 0), D * 4.5f * -Side));
	}
	// rayos X: la carrocería se eleva y se oculta (sin transparencia punteada)
	{
		const float Lift = FMath::SmoothStep(0.f, 1.f, XLift) * 280.f; const bool bVis = XLift < 0.96f;
		if (bXRay || XLift > 0.f) for (const TPair<int32, UMaterialInterface*>& KV : Mats) if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(KV.Value)) MI->SetScalarParameterValue(TEXT("Fade"), 1.f);
		if (Body) { Body->SetRelativeLocation(FVector(0.f, 0.f, Lift)); Body->SetVisibility(bVis); }
		if (KBody) { KBody->SetRelativeLocation(FVector(0.f, 0.f, KBodyZ + Lift)); KBody->SetVisibility(bVis); }
	}
	if (AnimPaint < 1.f || PaintShown != Setup.Paint)
	{
		const float U = FMath::SmoothStep(0.f, 1.f, AnimPaint);
		PaintShown = AnimPaint >= 1.f ? Setup.Paint : FMath::Lerp(PaintFrom, Setup.Paint, U);
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(KBody->GetMaterial(0))) M->SetVectorParameterValue(TEXT("Paint"), PaintShown);
	}
}

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

void ADustlineCar::BodyTest()
{
	// -nomenu -bodytest: cada modelo de serie desde 4 ángulos (3/4 delantero, lateral, frontal, 3/4 trasero)
	static int32 Phase = -1; const float T = Clock - 5.f; if (T < 0.f) return;
	int32 BM = -1; FParse::Value(FCommandLine::Get(), TEXT("bodymodel="), BM);
	const float Per = 14.f; const int32 Mi = BM >= 0 ? BM : (int32)(T / Per); const float Lt = FMath::Fmod(T, Per);
	if (Mi >= NumModels() || (BM >= 0 && T >= Per)) { FPlatformMisc::RequestExit(false); return; }
	static const bool bOrtho = FParse::Param(FCommandLine::Get(), TEXT("bodyortho"));
	static const float AngP[] = { 0.62f, 1.5708f, 0.f, 2.5f }, AngO[] = { 0.62f, 1.5708f, 0.f, 3.75f };
	static const TCHAR* TagP[] = { TEXT("f34"), TEXT("side"), TEXT("front"), TEXT("r34") };
	static const TCHAR* TagO[] = { TEXT("o34"), TEXT("oside"), TEXT("ofront"), TEXT("orear") };
	const float* Ang = bOrtho ? AngO : AngP; const TCHAR* const* Tag = bOrtho ? TagO : TagP;
	const int32 Step = (int32)(Lt / 3.5f); const float St = FMath::Fmod(Lt, 3.5f);
	if (Phase != Mi * 10 + Step)
	{
		Phase = Mi * 10 + Step;
		if (Step == 0) { SelectModel(Mi); Setup = StockSetup(Mi); bSetupDirty = true; if (!bShowcase) EnterShowcase(); OrbitSpeed = 0.f; }
		GarageOrbit = Ang[Step];
	}
	if (St > 2.7f && St < 2.8f) { static int32 Last = -1; if (Last != Phase) { Last = Phase; FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("/Volumes/SSD/UE-Cache/bd_%d_%s.png"), Mi, Tag[Step]), false, false); } }
}

void ADustlineCar::EngineTest()
{
	// -nomenu -enginetest: cada motor (sin turbo) y cada turbo (con V8) en el taller con rayos X
	static int32 Phase = -1; const float T = Clock - 5.f; if (T < 0.f) return;
	const int32 N = (int32)(T / 4.f);
	if (N != Phase)
	{
		Phase = N;
		static const float CC[] = { 1600.f, 2400.f, 3200.f, 4000.f, 5700.f, 7000.f, 9000.f };
		static const float TB[] = { 0.f, 0.6f, 1.2f, 1.8f, 2.4f, 3.0f };
		if (N == 0) { SelectModel(2); OpenGarage(); GarageFocus = 1; SetXRay(true); }
		if (N < 7) { Setup.EngineCC = CC[N]; Setup.Turbo = 0.f; bSetupDirty = true; }
		else if (N < 13) { Setup.EngineCC = 4000.f; Setup.Turbo = TB[N - 7]; bSetupDirty = true; }
		else FPlatformMisc::RequestExit(false);
	}
	if (Phase >= 0 && Phase < 13 && FMath::Fmod(T, 4.f) > 3.2f && FMath::Fmod(T, 4.f) < 3.3f)
	{ static int32 Last = -1; if (Last != Phase) { Last = Phase; FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("/Volumes/SSD/UE-Cache/en_%02d.png"), Phase), false, false); } }
}

void ADustlineCar::WheelTest()
{
	// -nomenu -wheeltest: cada modelo con ruedas de 44" (vista trasera y vista orbital)
	static int32 Phase = 0; const float T = Clock - 5.f; if (T < 0.f) return;
	const int32 Mi = (int32)(T / 9.f), Ph = (int32)(FMath::Fmod(T, 9.f) * 10.f);
	if (Mi >= NumModels()) { FPlatformMisc::RequestExit(false); return; }
	auto Shot = [&](const TCHAR* Tag) { FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("/Volumes/SSD/UE-Cache/wt_%d_%s.png"), Mi, Tag), false, false); };
	if (Phase != Mi * 100 + Ph / 10) { Phase = Mi * 100 + Ph / 10;
		const int32 S = Ph / 10;
		if (S == 0) { ExitShowcase(); SelectModel(Mi); Setup.TireRadiusM = 0.56f; Setup.TireWidthM = 0.38f; bSetupDirty = true; }
		else if (S == 4) Shot(TEXT("rear"));
		else if (S == 5) EnterShowcase();
		else if (S == 8) Shot(TEXT("orbit"));
	}
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
	else if (Step == 15 && T > 31.f) { Setup.EngineCC = 7000.f; Setup.Turbo = 2.4f; bSetupDirty = true; Step = 16; }
	else if (Step == 16 && T > 31.2f) { Shot(9); Step = 17; }
	else if (Step == 17 && T > 31.7f) { Shot(10); Step = 18; }
	else if (Step == 18 && T > 33.5f) { Setup.TireRadiusM = 0.68f; Setup.TireWidthM = 0.47f; GarageFocus = 2; bSetupDirty = true; Step = 19; }
	else if (Step == 19 && T > 34.25f) { Shot(11); Step = 21; }
	else if (Step == 21 && T > 34.7f) { Shot(12); Step = 20; }
	else if (Step == 20 && T > 35.5f) { GarageFocus = 0; SetXRay(false); Step = 22; }
	else if (Step == 22 && T > 40.f) { Shot(13); Step = 23; }
	else if (Step == 23 && T > 42.f) { FPlatformMisc::RequestExit(false); }
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
		GarageOrbit += Dt * 0.25f * OrbitSpeed;
		static const float Zoom = [] { float z = 1.f; FParse::Value(FCommandLine::Get(), TEXT("camzoom="), z); return FMath::Clamp(z, 0.3f, 2.f); }();
		static const bool bOrtho = FParse::Param(FCommandLine::Get(), TEXT("bodyortho"));
		if (bOrtho)
		{
			// casi ortográfica: cámara lejana a media altura y campo de visión estrecho, escala fija 560 cm de ancho de imagen
			const float d = 2400.f;
			Tgt = W(FVector(0, 0, 100.f)); Pos = W(FVector(FMath::Cos(GarageOrbit) * d, FMath::Sin(GarageOrbit) * d, 100.f));
			Cam->SetFieldOfView(2.f * FMath::RadiansToDegrees(FMath::Atan(280.f / d)));
			GarageCamPos = Pos; GarageCamTgt = Tgt; bGarageCamInit = true;
			break;
		}
		const float d = (M.L * 1.05f + 360.f) * Zoom;
		Tgt = W(FVector(0, 0, M.H * 0.45f));
		Pos = W(FVector(FMath::Cos(GarageOrbit) * d, FMath::Sin(GarageOrbit) * d, M.H * 0.55f + 120.f * Zoom));
		break;
	}
	}
	if (!bGarageCamInit) { GarageCamPos = Pos; GarageCamTgt = Tgt; bGarageCamInit = true; }
	const float a = 1.f - FMath::Exp(-4.f * Dt);
	GarageCamPos = FMath::Lerp(GarageCamPos, Pos, a); GarageCamTgt = FMath::Lerp(GarageCamTgt, Tgt, a);
	Cam->SetWorldLocationAndRotation(GarageCamPos, (GarageCamTgt - GarageCamPos).Rotation());
}
