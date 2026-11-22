#pragma once
// Sistema de diseño de Dustline (estilo Apple): paleta, tipografía, cristal esmerilado y controles propios.
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace DUI
{
	// ---- paleta (colores sRGB 0-255 -> lineal; alfa directo) ----
	FLinearColor Col(int32 R, int32 G, int32 B, float A = 1.f);
	inline FLinearColor Label()    { return Col(255, 255, 255, 1.f); }
	inline FLinearColor Label2()   { return Col(235, 235, 245, 0.62f); }
	inline FLinearColor Label3()   { return Col(235, 235, 245, 0.32f); }
	inline FLinearColor Accent()   { return Col(10, 132, 255, 1.f); }
	inline FLinearColor Green()    { return Col(48, 209, 88, 1.f); }
	inline FLinearColor Red()      { return Col(255, 69, 58, 1.f); }
	inline FLinearColor Orange()   { return Col(255, 159, 10, 1.f); }
	inline FLinearColor Fill()     { return Col(120, 120, 128, 0.36f); }
	inline FLinearColor FillSoft() { return Col(118, 118, 128, 0.24f); }
	inline FLinearColor Card()     { return Col(255, 255, 255, 0.075f); }
	inline FLinearColor Hair()     { return Col(255, 255, 255, 0.10f); }
	inline FLinearColor Clear()    { return Col(0, 0, 0, 0.f); }

	/** Rectángulo redondeado (cacheado). Radio >= alto/2 => cápsula. */
	const FSlateBrush* Box(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor(0, 0, 0, 0), float OutlineW = 0.f);

	/** Tipografía: SF del sistema si existe; Peso 0 = regular, 1 = seminegrita (solo >= 28 pt). */
	FSlateFontInfo Font(float Size, int32 Weight = 0);
	TSharedRef<class STextBlock> Text(const FString& S, float Size = 20.f, const FLinearColor& C = FLinearColor(1, 1, 1, 1), int32 Weight = 0, bool bWrap = false);

	// ---- widgets propios ----
	class SSlider2 : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSlider2) : _Min(0.f), _Max(1.f), _Step(0.f) {}
			SLATE_ARGUMENT(float, Min) SLATE_ARGUMENT(float, Max) SLATE_ARGUMENT(float, Step)
			SLATE_ATTRIBUTE(float, Value)
			SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
			SLATE_EVENT(FSimpleDelegate, OnCaptureBegin)
			SLATE_EVENT(FSimpleDelegate, OnCaptureEnd)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs);
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(240.0, 34.0); }
		virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override;
		virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override;
		virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override;
		virtual void OnMouseEnter(const FGeometry& G, const FPointerEvent& E) override { bHover = true; SLeafWidget::OnMouseEnter(G, E); }
		virtual void OnMouseLeave(const FPointerEvent& E) override { bHover = false; SLeafWidget::OnMouseLeave(E); }
	private:
		float Min = 0.f, Max = 1.f, Step = 0.f; bool bHover = false, bDrag = false;
		TAttribute<float> Value; FOnFloatValueChanged OnChanged; FSimpleDelegate OnBegin, OnEnd;
		void SetFromPointer(const FGeometry& G, const FPointerEvent& E);
	};

	class SToggle : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SToggle) {}
			SLATE_ATTRIBUTE(bool, IsOn)
			SLATE_EVENT(FOnBooleanValueChanged, OnToggled)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs);
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(56.0, 34.0); }
		virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override;
	private:
		TAttribute<bool> IsOn; FOnBooleanValueChanged OnToggled; mutable float Anim = 0.f;
		EActiveTimerReturnType AnimTick(double, float Dt);
	};

	/** Degradado lineal (orientación horizontal o vertical). */
	class SGradient : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGradient) : _Horizontal(true) {}
			SLATE_ARGUMENT(bool, Horizontal) SLATE_ARGUMENT(FLinearColor, From) SLATE_ARGUMENT(FLinearColor, To)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { bH = InArgs._Horizontal; A = InArgs._From; B = InArgs._To; }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(10.0, 10.0); }
	private:
		bool bH = true; FLinearColor A, B;
	};

	/** Aparece con fundido + ligero desplazamiento. */
	class SAppear : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAppear) : _Rise(14.f), _Delay(0.f) {}
			SLATE_DEFAULT_SLOT(FArguments, Content) SLATE_ARGUMENT(float, Rise) SLATE_ARGUMENT(float, Delay)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs);
	private:
		float T = 0.f, Rise = 14.f, Delay = 0.f;
		EActiveTimerReturnType AnimTick(double, float Dt);
	};

	// ---- constructores de alto nivel ----
	enum class EBtn : uint8 { Primary, Secondary, Plain, Danger, Round };
	TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> OnClick, EBtn Kind = EBtn::Secondary, float Width = 0.f, float Size = 21.f);
	TSharedRef<SWidget> Glass(TSharedRef<SWidget> Content, float Radius = 28.f, const FMargin& Pad = FMargin(24.f), const FLinearColor& Tint = FLinearColor(0.f, 0.f, 0.f, 0.f));
	TSharedRef<SWidget> Segmented(const TArray<FString>& Items, TFunction<int32()> Get, TFunction<void(int32)> Set, float Size = 18.f);
	TSharedRef<SWidget> Pills(const TArray<FString>& Items, TFunction<int32()> Get, TFunction<void(int32)> Set, float Size = 17.f);
	TSharedRef<SWidget> Group(const TArray<TSharedRef<SWidget>>& Rows);
	TSharedRef<SWidget> Row(const FString& Title, TSharedRef<SWidget> Control, const FString& Sub = FString());
	TSharedRef<SWidget> SliderRow(const FString& Title, float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FString(float)> Fmt,
		FSimpleDelegate OnBegin = FSimpleDelegate(), FSimpleDelegate OnEnd = FSimpleDelegate(), float Step = 0.f);
	TSharedRef<SWidget> Section(const FString& Title);
	TSharedRef<SWidget> KeyCap(const FString& K, float Size = 16.f);
	TSharedRef<SWidget> Spacer(float H);
}
