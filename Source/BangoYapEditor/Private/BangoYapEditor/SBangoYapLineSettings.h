#pragma once

#include "CoreMinimal.h"
#include "BangoYapNodeFields.h"
#include "Widgets/SCompoundWidget.h"

struct FSlateBrush;

/**
 * A line's playback settings, drawn as Yap draws them.
 *
 * - SBangoYapProgressionSettings: Yap's progression toggles (SYapProgressionSettingsWidget), the two small icons at
 *   the right of the upper bar: whether the line can be interrupted, and whether it moves on by itself.
 * - SBangoYapTimeSettings: the time rows of Yap's dialogue editor (SYapDialogueEditor's time and padding settings),
 *   for the text editor popup.
 */
namespace BangoYapEditor
{
	/** Yap's icon and colour for a time mode (1 text, 2 audio, 3 manual, 4 none; 0 the default). */
	const FSlateBrush* GetTimeModeBrush(int32 TimeMode);
	FLinearColor GetTimeModeColor(int32 TimeMode);
	FText GetTimeModeLabel(int32 TimeMode);
}

/**
 * Two stacked toggles, Interruptible above Advance. Click to switch between the two values (interruptible / forced,
 * auto / manual advance); Ctrl+click goes back to the default (nil: the speech's). Grey: the default. Yellow: written
 * as an expression (clicking replaces it).
 */
class SBangoYapProgressionSettings : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_TwoParams(FOnSet, BangoYapEditor::ELineSetting /*Setting*/, int32 /*Value: 0 the default*/);

	SLATE_BEGIN_ARGS(SBangoYapProgressionSettings)
		: _bEditable(true)
		, _bSpeech(false)
	{}
		SLATE_ARGUMENT(BangoYapEditor::FSettingValue, Interruptible)
		SLATE_ARGUMENT(BangoYapEditor::FSettingValue, Advance)
		SLATE_ARGUMENT(bool, bEditable)
		/** A Speech's settings (their default is the speech config's), not a line's (the speech's). */
		SLATE_ARGUMENT(bool, bSpeech)
		SLATE_EVENT(FOnSet, OnSet)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeToggle(BangoYapEditor::ELineSetting Setting);
	const BangoYapEditor::FSettingValue& GetValue(BangoYapEditor::ELineSetting Setting) const;

	FReply OnToggleClicked(BangoYapEditor::ELineSetting Setting) const;
	const FSlateBrush* GetToggleImage(BangoYapEditor::ELineSetting Setting) const;
	FSlateColor GetToggleColor(BangoYapEditor::ELineSetting Setting) const;
	FText GetToggleToolTip(BangoYapEditor::ELineSetting Setting) const;

	BangoYapEditor::FSettingValue Interruptible;
	BangoYapEditor::FSettingValue Advance;
	bool bEditable = true;
	bool bSpeech = false;
	FOnSet OnSet;
};

/**
 * The time settings of a line, as Yap's dialogue editor lays them out: a row each for the default time, audio time,
 * text time, a specified (manual) time with its seconds, and no time (until advanced); then the padding, with its
 * use-the-default button. Holds its choices until the popup applies them.
 *
 * BangoYap's manual time is the Padding argument (Say's TimeMode 3: "the padding is the time"), so the manual row's
 * seconds and the padding row are the same value, and the padding row hides while manual time is chosen.
 */
class SBangoYapTimeSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBangoYapTimeSettings) {}
		SLATE_ARGUMENT(BangoYapEditor::FSettingValue, TimeMode)
		SLATE_ARGUMENT(BangoYapEditor::FPaddingValue, PaddingSetting)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Script text for the fields that changed (empty: unchanged; `nil`: back to the default). */
	FString GetTimeModeScript() const;
	FString GetPaddingScript() const;

private:
	TSharedRef<SWidget> MakeTimeModeRow(int32 TimeMode);
	TSharedRef<SWidget> MakePaddingRow();
	TSharedRef<SWidget> MakeSecondsBox(bool bManualRow);

	FSlateColor GetButtonColor(int32 TimeMode) const;
	FSlateColor GetButtonForeground(int32 TimeMode) const;
	void SetSeconds(float NewSeconds);

	BangoYapEditor::FSettingValue InitialTimeMode;
	BangoYapEditor::FPaddingValue InitialPadding;

	/** The choices so far: 0 the default time mode. */
	int32 ChosenTimeMode = 0;
	bool bChosenPadding = false;
	float ChosenSeconds = 0.0f;

	bool bTimeModeChanged = false;
	bool bPaddingChanged = false;
};
