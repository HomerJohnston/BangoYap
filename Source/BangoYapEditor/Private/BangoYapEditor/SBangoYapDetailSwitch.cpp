#include "SBangoYapDetailSwitch.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"

void SBangoYapDetailSwitch::Construct(const FArguments& InArgs)
{
	LowDetailZoom = InArgs._LowDetailZoom;
	HighDetail = InArgs._HighDetail.Widget;
	if (!HighDetail.IsValid())
	{
		HighDetail = SNullWidget::NullWidget;
	}
	OnMakeLowDetail = InArgs._OnMakeLowDetail;

	ChildSlot
	[
		HighDetail.ToSharedRef()
	];
}

float SBangoYapDetailSwitch::GetZoom(const FGeometry& Geometry)
{
	if (DPIScale <= 0.0f && FSlateApplication::IsInitialized())
	{
		if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
		{
			DPIScale = Window->GetDPIScaleFactor();
		}
	}
	return Geometry.Scale / (DPIScale > 0.0f ? DPIScale : 1.0f);
}

void SBangoYapDetailSwitch::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const bool bWantLow = GetZoom(AllottedGeometry) < LowDetailZoom;
	if (bWantLow == bLowDetail)
	{
		if (!bLowDetail)
		{
			LastHighDetailSize = FVector2D(AllottedGeometry.GetLocalSize());
		}
		return;
	}

	bLowDetail = bWantLow;

	if (bLowDetail)
	{
		if (!LowDetail.IsValid())
		{
			LowDetail = OnMakeLowDetail.IsBound()
				? OnMakeLowDetail.Execute()
				: StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(LastHighDetailSize.X).HeightOverride(LastHighDetailSize.Y));
		}
		ChildSlot[LowDetail.ToSharedRef()];
	}
	else
	{
		ChildSlot[HighDetail.ToSharedRef()];
	}
}
