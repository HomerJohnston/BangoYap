#include "SBangoYapTimeBar.h"

#include "BangoYapEditorStyle.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

namespace BangoYapTimeBarPrivate
{
	/** As Yap: the bars are 5 units tall, centred. */
	constexpr float BarHeight = 5.0f;
}

void SBangoYapTimeBar::Construct(const FArguments& InArgs)
{
	BarColor = InArgs._BarColor;
	PaddingIsSet = InArgs._PaddingIsSet;
	SpeechTime = InArgs._SpeechTime;
	PaddingTime = InArgs._PaddingTime;
	PlaybackTime = InArgs._PlaybackTime;
	MaxTime = FMath::Max(InArgs._MaxTime, 0.1f);
	bEditable = InArgs._bEditable;
	OnHandleCommitted = InArgs._OnHandleCommitted;
	OnHandleReset = InArgs._OnHandleReset;

	// As Yap: the times are shown in a box above the bar while its handle is hovered or dragged.
	ChildSlot
	.Padding(0, -64, 0, 6)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Top)
	[
		SAssignNew(TimeDisplay, SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
	];
}

// ------------------------------------------------------------------------------------------------ Times

float SBangoYapTimeBar::GetDrawnSpeechTime() const
{
	return FMath::Max(SpeechTime.Get(), 0.0f);
}

float SBangoYapTimeBar::GetDrawnPadding() const
{
	return bDragging || bDragMoved ? DragPadding : PaddingTime.Get();
}

float SBangoYapTimeBar::GetDownScaling() const
{
	const float Speech = GetDrawnSpeechTime();
	const float Largest = FMath::Max(Speech, Speech + GetDrawnPadding());
	return Largest > MaxTime ? MaxTime / Largest : 1.0f;
}

FVector2f SBangoYapTimeBar::GetHandleCentre(const FGeometry& Geometry) const
{
	const FVector2f Size = Geometry.GetLocalSize();
	const float End = GetDrawnSpeechTime() + GetDrawnPadding();
	return FVector2f(Size.X * GetDownScaling() * End / MaxTime, 0.5f * Size.Y);
}

bool SBangoYapTimeBar::CanDrag() const
{
	// As Yap: not while a game is playing in the editor.
	return bEditable && OnHandleCommitted.IsBound() && !(GEditor && GEditor->IsPlaySessionInProgress());
}

bool SBangoYapTimeBar::IsCursorOnHandle() const
{
	if (!CanDrag() || !IsHovered() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FGeometry& Geometry = GetTickSpaceGeometry();
	const FVector2f CursorPos = Geometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	const float Radius = FMath::Max(Geometry.GetLocalSize().Y - 2.0f, 4.0f);

	// As Yap: it can be grabbed at twice its drawn size.
	return FVector2f::DistSquared(CursorPos, GetHandleCentre(Geometry)) < Radius * Radius;
}

// ------------------------------------------------------------------------------------------------ Painting

void SBangoYapTimeBar::DrawBar(float Left, float Right, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32& LayerId, const FSlateBrush* Brush, const FLinearColor& Color) const
{
	using namespace BangoYapTimeBarPrivate;

	const FVector2f Size = Geometry.GetLocalSize();
	const float Scale = GetDownScaling() * Size.X / MaxTime;
	const float LeftPos = Left * Scale;
	const float Width = (Right - Left) * Scale;
	if (Width <= 0.0f)
	{
		return;
	}

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		Geometry.ToPaintGeometry(FVector2f(Width, BarHeight), FSlateLayoutTransform(FVector2f(LeftPos, 0.5f * (Size.Y - BarHeight)))),
		Brush,
		ESlateDrawEffect::None,
		Color);
}

int32 SBangoYapTimeBar::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FLinearColor Color = BarColor.Get() * InWidgetStyle.GetColorAndOpacityTint();
	const FLinearColor PaddingColor = (PaddingIsSet.Get() || bDragMoved) ? BangoYapColor::White * InWidgetStyle.GetColorAndOpacityTint() : Color;

	const float Speech = GetDrawnSpeechTime();
	const float Padding = GetDrawnPadding();

	// As Yap: the speech (shortened by negative padding), the negative padding over its end, then positive padding.
	const float SpeechEnd = Speech + FMath::Min(0.0f, Padding);
	const float PaddingEnd = Speech + Padding;

	DrawBar(0.0f, SpeechEnd, AllottedGeometry, OutDrawElements, Layer, FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Bar_NormalPadding), Color);
	DrawBar(SpeechEnd, Speech, AllottedGeometry, OutDrawElements, Layer, FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Bar_NegativePadding), PaddingColor);
	DrawBar(Speech, PaddingEnd, AllottedGeometry, OutDrawElements, Layer, FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Bar_PositivePadding), PaddingColor);

	const FVector2f Size = AllottedGeometry.GetLocalSize();

	// Where the line is up to, while it plays.
	const TOptional<float> Playback = PlaybackTime.Get();
	if (Playback.IsSet())
	{
		const FSlateBrush* Marker = FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_PlaybackTimeHandle);
		const FVector2f MarkerSize(Marker->GetImageSize());
		const float X = Size.X * GetDownScaling() * Playback.GetValue() / MaxTime;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Layer++,
			AllottedGeometry.ToPaintGeometry(MarkerSize, FSlateLayoutTransform(FVector2f(X - 0.5f * MarkerSize.X, 0.5f * (Size.Y - MarkerSize.Y)))),
			Marker);
	}

	// The padding handle.
	if (CanDrag())
	{
		const float Diameter = FMath::Max(Size.Y - 2.0f, 2.0f);
		const FVector2f Centre = GetHandleCentre(AllottedGeometry);
		const FLinearColor HandleColor = (PaddingIsSet.Get() || bDragMoved) ? BangoYapColor::LightGray : Color;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Layer++,
			AllottedGeometry.ToPaintGeometry(FVector2f(Diameter, Diameter), FSlateLayoutTransform(Centre - FVector2f(0.5f * Diameter))),
			FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_FilledCircle),
			ESlateDrawEffect::None,
			HandleColor * InWidgetStyle.GetColorAndOpacityTint());
	}

	return Layer;
}

// ------------------------------------------------------------------------------------------------ Dragging

FReply SBangoYapTimeBar::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsCursorOnHandle())
	{
		return FReply::Unhandled();
	}

	if (MouseEvent.IsControlDown())
	{
		if (PaddingIsSet.Get())
		{
			OnHandleReset.ExecuteIfBound();
		}
		return FReply::Handled();
	}

	bDragging = true;
	bDragMoved = false;
	DragPadding = PaddingTime.Get();
	SecondsPerUnit = MaxTime / FMath::Max(GetDownScaling() * MyGeometry.GetLocalSize().X, 1.0f);
	ShowTimeDisplay();
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SBangoYapTimeBar::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bDragging || !HasMouseCapture())
	{
		return SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
	}

	const float Scale = FMath::Max(MyGeometry.Scale, KINDA_SMALL_NUMBER);
	const float Delta = MouseEvent.GetCursorDelta().X / Scale * SecondsPerUnit;
	if (Delta != 0.0f)
	{
		// The line's end can't come before its start.
		DragPadding = FMath::Max(DragPadding + Delta, -GetDrawnSpeechTime());
		bDragMoved = true;
	}
	return FReply::Handled();
}

FReply SBangoYapTimeBar::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bDragging || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	bDragging = false;
	HideTimeDisplay();

	if (bDragMoved)
	{
		// The node is drawn again from the script; until then, the dragged padding stays drawn (bDragMoved).
		const float Snapped = FMath::RoundToFloat(DragPadding * 100.0f) / 100.0f;
		DragPadding = Snapped;
		OnHandleCommitted.ExecuteIfBound(GetDrawnSpeechTime() + Snapped);
	}
	return FReply::Handled().ReleaseMouseCapture();
}

void SBangoYapTimeBar::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// Lost without a release (the window lost focus): nothing is changed.
	if (bDragging)
	{
		bDragging = false;
		bDragMoved = false;
		HideTimeDisplay();
	}
	SCompoundWidget::OnMouseCaptureLost(CaptureLostEvent);
}

TOptional<EMouseCursor::Type> SBangoYapTimeBar::GetCursor() const
{
	if (bDragging || IsCursorOnHandle())
	{
		return EMouseCursor::ResizeLeftRight;
	}
	return SCompoundWidget::GetCursor();
}

void SBangoYapTimeBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bDragging || IsCursorOnHandle())
	{
		ShowTimeDisplay();
	}
	else
	{
		HideTimeDisplay();
	}
}

// ------------------------------------------------------------------------------------------------ Time display

FText SBangoYapTimeBar::GetTimeText() const
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 2;
	Options.MaximumFractionalDigits = 2;

	const float Padding = GetDrawnPadding();
	return FText::Format(LOCTEXT("TimeBar_Text", "Speech: {0} s\n{1} {2} s"),
		FText::AsNumber(GetDrawnSpeechTime(), &Options),
		Padding < 0.0f ? LOCTEXT("TimeBar_Minus", "-") : LOCTEXT("TimeBar_Plus", "+"),
		FText::AsNumber(FMath::Abs(Padding), &Options));
}

void SBangoYapTimeBar::ShowTimeDisplay()
{
	if (bTimeDisplayShown || !TimeDisplay.IsValid())
	{
		return;
	}

	TimeDisplay->AddSlot()
	[
		SNew(SBorder)
		.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite_Rounded))
		.BorderBackgroundColor(BangoYapColor::DarkGray)
		.Padding(12)
		[
			SNew(STextBlock)
			.Text(this, &SBangoYapTimeBar::GetTimeText)
			.Justification(ETextJustify::Center)
			.ColorAndOpacity(BangoYapColor::DimWhite)
		]
	];
	bTimeDisplayShown = true;
}

void SBangoYapTimeBar::HideTimeDisplay()
{
	if (!bTimeDisplayShown || bDragging || !TimeDisplay.IsValid())
	{
		return;
	}
	TimeDisplay->ClearChildren();
	bTimeDisplayShown = false;
}

#undef LOCTEXT_NAMESPACE
