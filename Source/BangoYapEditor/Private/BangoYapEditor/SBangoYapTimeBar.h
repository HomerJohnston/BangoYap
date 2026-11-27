#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SOverlay;

/**
 * Yap's time progression bar (SYapTimeProgressionWidget), under a line: its speech time, then its padding, drawn to
 * scale (MaxTime seconds fill it; a longer line is shrunk to fit). Negative padding is drawn over the end of the
 * speech. While a game plays in the editor, a marker shows where the line is up to.
 *
 * The round handle at the end is the padding: drag it to change it (for manual time, the time itself), with the
 * times shown above the bar while dragging or hovering; Ctrl+click it to go back to the default. The change is made
 * when the mouse is let go (one change, not one per pixel).
 */
class SBangoYapTimeBar : public SCompoundWidget
{
public:
	/** The new time at the handle (the speech's end plus the padding), in seconds from the line's start. */
	DECLARE_DELEGATE_OneParam(FOnHandleCommitted, float /*HandleTime*/);

	SLATE_BEGIN_ARGS(SBangoYapTimeBar)
		: _MaxTime(5.0f)
		, _bEditable(true)
	{}
		SLATE_ATTRIBUTE(FLinearColor, BarColor)
		SLATE_ATTRIBUTE(bool, PaddingIsSet)
		SLATE_ATTRIBUTE(float, SpeechTime)
		SLATE_ATTRIBUTE(float, PaddingTime)
		SLATE_ARGUMENT(float, MaxTime)
		/** Unset unless the line is playing (in a game in the editor): seconds it has played. */
		SLATE_ATTRIBUTE(TOptional<float>, PlaybackTime)
		SLATE_ARGUMENT(bool, bEditable)
		SLATE_EVENT(FOnHandleCommitted, OnHandleCommitted)
		SLATE_EVENT(FSimpleDelegate, OnHandleReset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual TOptional<EMouseCursor::Type> GetCursor() const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** The times as drawn: the padding being dragged, while it is. */
	float GetDrawnSpeechTime() const;
	float GetDrawnPadding() const;
	/** 1, or less to fit a line longer than MaxTime. */
	float GetDownScaling() const;
	/** Where the handle is (local units), and whether the cursor is on it. */
	FVector2f GetHandleCentre(const FGeometry& Geometry) const;
	bool IsCursorOnHandle() const;
	bool CanDrag() const;

	void DrawBar(float Left, float Right, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32& LayerId, const FSlateBrush* Brush, const FLinearColor& Color) const;

	void ShowTimeDisplay();
	void HideTimeDisplay();
	FText GetTimeText() const;

	TAttribute<FLinearColor> BarColor;
	TAttribute<bool> PaddingIsSet;
	TAttribute<float> SpeechTime;
	TAttribute<float> PaddingTime;
	TAttribute<TOptional<float>> PlaybackTime;
	float MaxTime = 5.0f;
	bool bEditable = true;
	FOnHandleCommitted OnHandleCommitted;
	FSimpleDelegate OnHandleReset;

	/** Dragging: the padding as it'll be, and how many seconds a local unit is (fixed for the drag). */
	bool bDragging = false;
	float DragPadding = 0.0f;
	float SecondsPerUnit = 0.0f;
	bool bDragMoved = false;

	TSharedPtr<SOverlay> TimeDisplay;
	bool bTimeDisplayShown = false;
};
