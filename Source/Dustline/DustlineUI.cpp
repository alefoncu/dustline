#include "DustlineUI.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "Rendering/DrawElements.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"

namespace DUI
{
FLinearColor Col(int32 R, int32 G, int32 B, float A)
{
	FLinearColor C = FLinearColor::FromSRGBColor(FColor((uint8)R, (uint8)G, (uint8)B, 255)); C.A = A; return C;
}

const FSlateBrush* Box(const FLinearColor& F, float Radius, const FLinearColor& O, float OW)
{
	static TMap<FString, TSharedPtr<FSlateRoundedBoxBrush>> Cache;
	const FString Key = FString::Printf(TEXT("%.3f,%.3f,%.3f,%.3f|%.1f|%.3f,%.3f,%.3f,%.3f|%.1f"), F.R, F.G, F.B, F.A, Radius, O.R, O.G, O.B, O.A, OW);
	if (TSharedPtr<FSlateRoundedBoxBrush>* Found = Cache.Find(Key)) return Found->Get();
	TSharedPtr<FSlateRoundedBoxBrush> B = MakeShared<FSlateRoundedBoxBrush>(F, Radius, O, OW);
	Cache.Add(Key, B);
	return B.Get();
}

FSlateFontInfo Font(float Size, int32 Weight)
{
	static TSharedPtr<FStandaloneCompositeFont> SF; static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		const TCHAR* Paths[] = { TEXT("/System/Library/Fonts/SFNS.ttf"), TEXT("/System/Library/Fonts/SFNSRounded.ttf"), TEXT("/System/Library/Fonts/Helvetica.ttc") };
		for (const TCHAR* P : Paths)
			if (IFileManager::Get().FileExists(P))
			{
				SF = MakeShared<FStandaloneCompositeFont>();
				SF->DefaultTypeface.AppendFont(TEXT("Regular"), P, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				break;
			}
	}
	Size = FMath::Max(Size, 17.f);
	FSlateFontInfo F = SF.IsValid() ? FSlateFontInfo(SF, (int32)Size, TEXT("Regular")) : FCoreStyle::GetDefaultFontStyle(Weight ? "Bold" : "Regular", (int32)Size);
	if (Weight > 0 && SF.IsValid() && Size >= 28.f)
	{
		F.OutlineSettings.OutlineSize = Size >= 64.f ? 3 : 1;
		F.OutlineSettings.OutlineColor = FLinearColor::White;
	}
	return F;
}

TSharedRef<STextBlock> Text(const FString& S, float Size, const FLinearColor& C, int32 Weight, bool bWrap)
{
	return SNew(STextBlock).Text(FText::FromString(S)).Font(Font(Size, Weight)).ColorAndOpacity(C).AutoWrapText(bWrap);
}

static void DrawBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f& Size, const FVector2f& Pos, const FSlateBrush* B, const FWidgetStyle& St, float Alpha = 1.f)
{
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), B, ESlateDrawEffect::None, St.GetColorAndOpacityTint() * B->GetTint(St) * FLinearColor(1.f, 1.f, 1.f, Alpha));
}
// ------------------------------------------------------------------ slider
void SSlider2::Construct(const FArguments& A)
{
	Min = A._Min; Max = A._Max; Step = A._Step; Value = A._Value; OnChanged = A._OnValueChanged; OnBegin = A._OnCaptureBegin; OnEnd = A._OnCaptureEnd;
}
static const float GThumb = 28.f, GTrack = 6.f;
int32 SSlider2::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const
{
	const float W = G.GetLocalSize().X, H = G.GetLocalSize().Y, Pad = GThumb * 0.5f + 2.f;
	const float T = FMath::Clamp((Value.Get() - Min) / FMath::Max(Max - Min, 0.0001f), 0.f, 1.f);
	const float X = Pad + T * (W - 2.f * Pad);
	DrawBox(Out, Layer, G, FVector2f(W - 2.f * Pad, GTrack), FVector2f(Pad, H * 0.5f - GTrack * 0.5f), Box(Fill(), GTrack * 0.5f), Style);
	DrawBox(Out, Layer + 1, G, FVector2f(FMath::Max(X - Pad, 1.f), GTrack), FVector2f(Pad, H * 0.5f - GTrack * 0.5f), Box(Accent(), GTrack * 0.5f), Style);
	const float D = bDrag ? GThumb + 3.f : GThumb;
	DrawBox(Out, Layer + 2, G, FVector2f(D + 4.f, D + 4.f), FVector2f(X - D * 0.5f - 2.f, H * 0.5f - D * 0.5f - 1.f), Box(Col(0, 0, 0, 0.22f), (D + 4.f) * 0.5f), Style);
	DrawBox(Out, Layer + 3, G, FVector2f(D, D), FVector2f(X - D * 0.5f, H * 0.5f - D * 0.5f), Box(Col(255, 255, 255, 1.f), D * 0.5f, Col(0, 0, 0, 0.12f), 0.5f), Style);
	return Layer + 4;
}
void SSlider2::SetFromPointer(const FGeometry& G, const FPointerEvent& E)
{
	const float W = G.GetLocalSize().X, Pad = GThumb * 0.5f + 2.f;
	const float X = (float)G.AbsoluteToLocal(E.GetScreenSpacePosition()).X;
	const float T = FMath::Clamp((X - Pad) / FMath::Max(W - 2.f * Pad, 1.f), 0.f, 1.f);
	float V = Min + T * (Max - Min);
	if (Step > 0.f) V = FMath::Clamp(FMath::RoundToFloat((V - Min) / Step) * Step + Min, Min, Max);
	OnChanged.ExecuteIfBound(V);
}
FReply SSlider2::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
	if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	bDrag = true; OnBegin.ExecuteIfBound(); SetFromPointer(G, E);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}
FReply SSlider2::OnMouseButtonUp(const FGeometry&, const FPointerEvent& E)
{
	if (E.GetEffectingButton() != EKeys::LeftMouseButton || !bDrag) return FReply::Unhandled();
	bDrag = false; OnEnd.ExecuteIfBound();
	return FReply::Handled().ReleaseMouseCapture();
}
FReply SSlider2::OnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
	if (bDrag) { SetFromPointer(G, E); return FReply::Handled(); }
	return FReply::Unhandled();
}

// ------------------------------------------------------------------ toggle
void SToggle::Construct(const FArguments& A)
{
	IsOn = A._IsOn; OnToggled = A._OnToggled; Anim = IsOn.Get(false) ? 1.f : 0.f;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SToggle::AnimTick));
}
EActiveTimerReturnType SToggle::AnimTick(double, float Dt)
{
	const float Target = IsOn.Get(false) ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Anim, Target, 0.002f)) { Anim = FMath::FInterpTo(Anim, Target, Dt, 18.f); Invalidate(EInvalidateWidgetReason::Paint); }
	return EActiveTimerReturnType::Continue;
}
int32 SToggle::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const
{
	const float TW = 52.f, TH = 32.f, X0 = (G.GetLocalSize().X - TW) * 0.5f, Y0 = (G.GetLocalSize().Y - TH) * 0.5f;
	DrawBox(Out, Layer, G, FVector2f(TW, TH), FVector2f(X0, Y0), Box(Fill(), TH * 0.5f), Style);
	DrawBox(Out, Layer + 1, G, FVector2f(TW, TH), FVector2f(X0, Y0), Box(Green(), TH * 0.5f), Style, Anim);
	const float D = 28.f, KX = X0 + 2.f + Anim * (TW - 4.f - D);
	DrawBox(Out, Layer + 2, G, FVector2f(D + 4.f, D + 4.f), FVector2f(KX - 2.f, Y0 + 1.f), Box(Col(0, 0, 0, 0.20f), (D + 4.f) * 0.5f), Style);
	DrawBox(Out, Layer + 3, G, FVector2f(D, D), FVector2f(KX, Y0 + 2.f), Box(Col(255, 255, 255, 1.f), D * 0.5f), Style);
	return Layer + 4;
}
FReply SToggle::OnMouseButtonDown(const FGeometry&, const FPointerEvent& E)
{
	if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	OnToggled.ExecuteIfBound(!IsOn.Get(false));
	return FReply::Handled();
}

// ------------------------------------------------------------------ gradient / appear
int32 SGradient::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const
{
	const float Op = Style.GetColorAndOpacityTint().A;
	FLinearColor C0 = A, C1 = B; C0.A *= Op; C1.A *= Op;
	const FVector2f S = G.GetLocalSize();
	TArray<FSlateGradientStop> Stops;
	Stops.Add(FSlateGradientStop(FVector2f(0.f, 0.f), C0));
	Stops.Add(FSlateGradientStop(bH ? FVector2f(S.X, 0.f) : FVector2f(0.f, S.Y), C1));
	FSlateDrawElement::MakeGradient(Out, Layer, G.ToPaintGeometry(), Stops, bH ? Orient_Vertical : Orient_Horizontal, ESlateDrawEffect::None, FVector4f(0.f));
	return Layer + 1;
}

int32 SArc::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const
{
	const float Target = FMath::Clamp(Value.Get(0.f), 0.f, 1.f);
	Shown += (Target - Shown) * (1.f - FMath::Exp(-9.f * FApp::GetDeltaTime()));
	const FVector2f S = G.GetLocalSize(), C = S * 0.5f;
	const float R = FMath::Min(S.X, S.Y) * 0.5f - Inset - Thick * 0.5f;
	const float A0 = FMath::DegreesToRadians(135.f), Sweep = FMath::DegreesToRadians(270.f);
	auto Arc = [&](float T, int32 L, FLinearColor Tint)
	{
		const int32 N = FMath::Max(2, (int32)(120.f * T)); TArray<FVector2f> P; P.Reserve(N + 1);
		for (int32 i = 0; i <= N; ++i) { const float A = A0 + Sweep * T * (float)i / (float)N; P.Add(FVector2f(C.X + R * FMath::Cos(A), C.Y + R * FMath::Sin(A))); }
		FSlateDrawElement::MakeLines(Out, L, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Tint * Style.GetColorAndOpacityTint(), true, Thick);
	};
	Arc(1.f, Layer, Col(255, 255, 255, 0.16f));
	if (Shown > 0.004f) Arc(Shown, Layer + 1, Color);
	return Layer + 2;
}

void SAppear::Construct(const FArguments& A)
{
	Rise = A._Rise; Delay = A._Delay;
	ChildSlot[ A._Content.Widget ];
	SetRenderOpacity(0.f);
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAppear::AnimTick));
}
EActiveTimerReturnType SAppear::AnimTick(double, float Dt)
{
	T += Dt;
	const float U = FMath::Clamp((T - Delay) / 0.45f, 0.f, 1.f), E = 1.f - FMath::Pow(1.f - U, 3.f);
	SetRenderOpacity(E);
	SetRenderTransform(FSlateRenderTransform(FVector2f(0.f, (1.f - E) * Rise)));
	return U >= 1.f ? EActiveTimerReturnType::Stop : EActiveTimerReturnType::Continue;
}

// ------------------------------------------------------------------ builders
static const FButtonStyle& StyleFor(EBtn K)
{
	auto Make = [](const FLinearColor& N, const FLinearColor& H, const FLinearColor& P, float R)
	{
		FButtonStyle S; S.SetNormal(*Box(N, R)).SetHovered(*Box(H, R)).SetPressed(*Box(P, R)).SetDisabled(*Box(N, R)).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0, 1, 0, -1));
		return S;
	};
	static FButtonStyle Primary = Make(Col(255, 255, 255, 0.96f), Col(255, 255, 255, 1.f), Col(255, 255, 255, 0.70f), 30.f);
	static FButtonStyle Secondary = Make(Col(255, 255, 255, 0.14f), Col(255, 255, 255, 0.22f), Col(255, 255, 255, 0.08f), 30.f);
	static FButtonStyle Plain = Make(Clear(), Col(255, 255, 255, 0.09f), Col(255, 255, 255, 0.15f), 14.f);
	static FButtonStyle Round = Make(Col(255, 255, 255, 0.16f), Col(255, 255, 255, 0.26f), Col(255, 255, 255, 0.09f), 24.f);
	switch (K) { case EBtn::Primary: return Primary; case EBtn::Plain: return Plain; case EBtn::Round: return Round; default: return Secondary; }
}

TSharedRef<SWidget> Button(const FString& Caption, TFunction<void()> OnClick, EBtn Kind, float Width, float Size)
{
	FLinearColor Fg = FLinearColor::White; FMargin Pad(30, 16);
	if (Kind == EBtn::Primary) Fg = FLinearColor(0.f, 0.f, 0.f, 1.f);
	else if (Kind == EBtn::Danger) Fg = Red();
	else if (Kind == EBtn::Plain) Pad = FMargin(18, 9);
	else if (Kind == EBtn::Round) Pad = FMargin(0, 0);
	TSharedRef<SWidget> B = SNew(SButton).ButtonStyle(&StyleFor(Kind)).ContentPadding(Pad).HAlign(HAlign_Center).VAlign(VAlign_Center)
		.OnClicked(FOnClicked::CreateLambda([OnClick]() { OnClick(); return FReply::Handled(); }))
		[ Text(Caption, Size, Fg, 0) ];
	if (Kind == EBtn::Round) return SNew(SBox).WidthOverride(48.f).HeightOverride(48.f)[B];
	if (Width > 0.f) return SNew(SBox).WidthOverride(Width)[B];
	return B;
}

TSharedRef<SWidget> Glass(TSharedRef<SWidget> Content, float R, const FMargin& Pad, const FLinearColor& Tint)
{
	const FLinearColor T = Tint.A > 0.f ? Tint : Col(34, 34, 38, 0.56f);
	return SNew(SBackgroundBlur).BlurStrength(26.f).CornerRadius(FVector4(R, R, R, R)).Padding(FMargin(0)).LowQualityFallbackBrush(Box(Col(24, 24, 28, 0.92f), R))
		[ SNew(SBorder).BorderImage(Box(T, R, Col(255, 255, 255, 0.17f), 1.f)).Padding(Pad)[ Content ] ];
}

static const FButtonStyle& InvisibleStyle()
{
	static FButtonStyle S = FButtonStyle().SetNormal(*Box(Clear(), 0)).SetHovered(*Box(Clear(), 0)).SetPressed(*Box(Clear(), 0)).SetDisabled(*Box(Clear(), 0)).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
	return S;
}

TSharedRef<SWidget> Segmented(const TArray<FString>& Items, TFunction<int32()> Get, TFunction<void(int32)> Set, float Size)
{
	TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
	for (int32 i = 0; i < Items.Num(); ++i)
		H->AddSlot().AutoWidth()
		[ SNew(SButton).ButtonStyle(&InvisibleStyle()).ContentPadding(FMargin(0))
			.OnClicked(FOnClicked::CreateLambda([Set, i]() { Set(i); return FReply::Handled(); }))
			[ SNew(SBorder).BorderImage_Lambda([Get, i]() { return Get() == i ? Box(Col(108, 108, 114, 1.f), 10.f) : Box(Clear(), 10.f); }).Padding(FMargin(20, 8))
				[ Text(Items[i], Size, Label(), 0) ] ] ];
	return SNew(SBorder).BorderImage(Box(FillSoft(), 13.f)).Padding(FMargin(3))[ H ];
}

TSharedRef<SWidget> Pills(const TArray<FString>& Items, TFunction<int32()> Get, TFunction<void(int32)> Set, float Size)
{
	TSharedRef<SWrapBox> W = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 8));
	for (int32 i = 0; i < Items.Num(); ++i)
		W->AddSlot()
		[ SNew(SButton).ButtonStyle(&InvisibleStyle()).ContentPadding(FMargin(0))
			.OnClicked(FOnClicked::CreateLambda([Set, i]() { Set(i); return FReply::Handled(); }))
			[ SNew(SBorder).BorderImage_Lambda([Get, i]() { return Get() == i ? Box(Accent(), 17.f) : Box(Col(255, 255, 255, 0.12f), 17.f); }).Padding(FMargin(16, 8))
				[ Text(Items[i], Size, Label(), 0) ] ] ];
	return W;
}

TSharedRef<SWidget> Group(const TArray<TSharedRef<SWidget>>& Rows)
{
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		if (i > 0) V->AddSlot().AutoHeight().Padding(20, 0, 0, 0)[ SNew(SBox).HeightOverride(1.f)[ SNew(SBorder).BorderImage(Box(Hair(), 0.f)) ] ];
		V->AddSlot().AutoHeight()[ Rows[i] ];
	}
	return SNew(SBorder).BorderImage(Box(Card(), 18.f, Hair(), 1.f)).Padding(0)[ V ];
}

TSharedRef<SWidget> Row(const FString& Title, TSharedRef<SWidget> Control, const FString& Sub)
{
	TSharedRef<SVerticalBox> L = SNew(SVerticalBox);
	L->AddSlot().AutoHeight()[ Text(Title, 21.f, Label()) ];
	if (!Sub.IsEmpty()) L->AddSlot().AutoHeight().Padding(0, 3, 0, 0)[ Text(Sub, 15.f, Label2(), 0, true) ];
	return SNew(SBox).MinDesiredHeight(62.f)
		[ SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(20, 12, 12, 12)[ L ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 10, 18, 10)[ Control ] ];
}

TSharedRef<SWidget> SliderRow(const FString& Title, float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FString(float)> Fmt, FSimpleDelegate OnBegin, FSimpleDelegate OnEnd, float Step)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(20, 14, 20, 0)
		[ SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[ Text(Title, 21.f, Label()) ]
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(STextBlock).Font(Font(18.f)).ColorAndOpacity(Label2()).Text_Lambda([Get, Fmt]() { return FText::FromString(Fmt(Get())); }) ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(12, 4, 12, 12)
		[ SNew(SSlider2).Min(Min).Max(Max).Step(Step).Value_Lambda([Get]() { return Get(); })
			.OnValueChanged(FOnFloatValueChanged::CreateLambda([Set](float V) { Set(V); }))
			.OnCaptureBegin(OnBegin).OnCaptureEnd(OnEnd) ];
}

TSharedRef<SWidget> Section(const FString& Title)
{
	return SNew(SBox).Padding(FMargin(20, 26, 0, 9))[ Text(Title, 16.f, Label2()) ];
}

TSharedRef<SWidget> KeyCap(const FString& K, float Size)
{
	return SNew(SBorder).BorderImage(Box(Col(255, 255, 255, 0.14f), 9.f, Col(255, 255, 255, 0.14f), 1.f)).Padding(FMargin(11, 5))[ Text(K, Size, Label(), 0) ];
}

TSharedRef<SWidget> Spacer(float H) { return SNew(SBox).HeightOverride(H); }
}
