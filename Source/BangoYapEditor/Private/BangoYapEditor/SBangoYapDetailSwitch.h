#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Shows one thing when the graph is zoomed in and another (cheaper, plainer) when it's zoomed out, as Yap's node does
 * with the graph editor's SLevelOfDetailBranchNode. A decorator's widget can't ask the graph panel its level of
 * detail, so this works it out from its own scale (the graph's zoom, with the window's DPI scale taken out): below
 * LowDetailZoom it's zoomed out. Yap's node goes plain below the graph's default detail (zoom 0.675); here, below
 * 0.6 by default.
 *
 * The low-detail content is made only when it's first needed.
 */
class SBangoYapDetailSwitch : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(TSharedRef<SWidget>, FOnMakeLowDetail);

	SLATE_BEGIN_ARGS(SBangoYapDetailSwitch)
		: _LowDetailZoom(0.6f)
	{}
		SLATE_ARGUMENT(float, LowDetailZoom)
		SLATE_NAMED_SLOT(FArguments, HighDetail)
		/** Made when the graph is first zoomed out. Unbound: an empty box the high-detail content's size. */
		SLATE_EVENT(FOnMakeLowDetail, OnMakeLowDetail)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** True while it's showing its low-detail content. */
	bool IsLowDetail() const { return bLowDetail; }

private:
	float GetZoom(const FGeometry& Geometry);

	float LowDetailZoom = 0.6f;
	TSharedPtr<SWidget> HighDetail;
	TSharedPtr<SWidget> LowDetail;
	FOnMakeLowDetail OnMakeLowDetail;
	bool bLowDetail = false;

	/** The window's DPI scale, found once. */
	float DPIScale = 0.0f;
	/** The high-detail content's size when it was last shown, for the default low-detail box. */
	FVector2D LastHighDetailSize = FVector2D::ZeroVector;
};
