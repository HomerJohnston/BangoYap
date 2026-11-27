#include "SBangoYapLineSettings.h"

#include "BangoYapEditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

using namespace BangoYapEditor;

namespace BangoYapLineSettingsPrivate
{
	// Say's TimeMode values (EBangoYapTimeMode).
	constexpr int32 DefaultTime = 0;
	constexpr int32 TextTime = 1;
	constexpr int32 AudioTime = 2;
	constexpr int32 ManualTime = 3;
	constexpr int32 NoTime = 4;

	// Interruptible: 1 interruptible, 2 forced. Advance: 1 auto, 2 manual.
	constexpr int32 FirstValue = 1;
	constexpr int32 SecondValue = 2;

	static bool IsCtrlDown()
	{
		return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsControlDown();
	}

	/** "TEXT_TIME" or "1": how the value is written in this project. */
	static FText ScriptName(ELineSetting Setting, int32 Value)
	{
		return FText::FromString(MakeSettingScript(Setting, Value));
	}
}

// ================================================================================================ Time modes

const FSlateBrush* BangoYapEditor::GetTimeModeBrush(int32 TimeMode)
{
	using namespace BangoYapLineSettingsPrivate;
	switch (TimeMode)
	{
		case TextTime: return FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_TextTime);
		case AudioTime: return FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_AudioTime);
		case ManualTime: return FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_Timer);
		case NoTime: return FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_CrossX);
		default: return FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_Default);
	}
}

FLinearColor BangoYapEditor::GetTimeModeColor(int32 TimeMode)
{
	using namespace BangoYapLineSettingsPrivate;
	// As Yap's dialogue editor (SYapDialogueEditor::TimeModeButtonColors).
	switch (TimeMode)
	{
		case TextTime: return BangoYapColor::LightBlue;
		case AudioTime: return BangoYapColor::Cyan;
		case ManualTime: return BangoYapColor::Orange;
		case NoTime: return BangoYapColor::Red;
		default: return BangoYapColor::Green;
	}
}

FText BangoYapEditor::GetTimeModeLabel(int32 TimeMode)
{
	using namespace BangoYapLineSettingsPrivate;
	switch (TimeMode)
	{
		case TextTime: return LOCTEXT("TimeMode_Text", "Use Text Time");
		case AudioTime: return LOCTEXT("TimeMode_Audio", "Use Audio Time");
		case ManualTime: return LOCTEXT("TimeMode_Manual", "Use Specified Time");
		case NoTime: return LOCTEXT("TimeMode_None", "No Time (until advanced)");
		default: return LOCTEXT("TimeMode_Default", "Use Default Time");
	}
}

// ================================================================================================ Progression

void SBangoYapProgressionSettings::Construct(const FArguments& InArgs)
{
	Interruptible = InArgs._Interruptible;
	Advance = InArgs._Advance;
	bEditable = InArgs._bEditable;
	bSpeech = InArgs._bSpeech;
	OnSet = InArgs._OnSet;

	// As Yap: a 20 px square, the two toggles 8 px tall, one above the other.
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(20)
		.HeightOverride(20)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				MakeToggle(ELineSetting::Interruptible)
			]
			+ SVerticalBox::Slot()
			[
				MakeToggle(ELineSetting::Advance)
			]
		]
	];
}

TSharedRef<SWidget> SBangoYapProgressionSettings::MakeToggle(ELineSetting Setting)
{
	return SNew(SBox)
		.HeightOverride(8)
		[
			SNew(SButton)
			.Cursor(EMouseCursor::Default)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ContentPadding(0)
			.IsEnabled(bEditable)
			.OnClicked(this, &SBangoYapProgressionSettings::OnToggleClicked, Setting)
			.ToolTipText(this, &SBangoYapProgressionSettings::GetToggleToolTip, Setting)
			[
				SNew(SImage)
				.Image(this, &SBangoYapProgressionSettings::GetToggleImage, Setting)
				.ColorAndOpacity(this, &SBangoYapProgressionSettings::GetToggleColor, Setting)
			]
		];
}

const FSettingValue& SBangoYapProgressionSettings::GetValue(ELineSetting Setting) const
{
	return Setting == ELineSetting::Interruptible ? Interruptible : Advance;
}

FReply SBangoYapProgressionSettings::OnToggleClicked(ELineSetting Setting) const
{
	using namespace BangoYapLineSettingsPrivate;
	const FSettingValue& Value = GetValue(Setting);

	// As Yap: Ctrl+click resets to the default; a click goes on to the next value.
	int32 Next = FirstValue;
	if (IsCtrlDown())
	{
		if (Value.IsDefault())
		{
			return FReply::Handled();
		}
		Next = 0;
	}
	else if (Value.Is(FirstValue))
	{
		Next = SecondValue;
	}

	OnSet.ExecuteIfBound(Setting, Next);
	return FReply::Handled();
}

const FSlateBrush* SBangoYapProgressionSettings::GetToggleImage(ELineSetting Setting) const
{
	using namespace BangoYapLineSettingsPrivate;
	const bool bSecond = GetValue(Setting).Is(SecondValue);

	if (Setting == ELineSetting::Interruptible)
	{
		return FBangoYapEditorStyle::GetBrush(bSecond ? FBangoYapEditorStyle::Icon_Interruptible_None : FBangoYapEditorStyle::Icon_Interruptible_All);
	}
	return FBangoYapEditorStyle::GetBrush(bSecond ? FBangoYapEditorStyle::Icon_AutoAdvance_None : FBangoYapEditorStyle::Icon_AutoAdvance_All);
}

FSlateColor SBangoYapProgressionSettings::GetToggleColor(ELineSetting Setting) const
{
	using namespace BangoYapLineSettingsPrivate;
	const FSettingValue& Value = GetValue(Setting);

	switch (Value.Kind)
	{
		case FSettingValue::EKind::Default:
			// Yap draws the default dark grey; a little lighter here, to show on a Bango node.
			return BangoYapColor::Gray;
		case FSettingValue::EKind::Expression:
			return BangoYapColor::LightYellow;
		default:
			// As Yap: green where it's let go (interruptible, auto advance), orange where it's held (forced, manual).
			return Value.Is(FirstValue) ? BangoYapColor::LightGreen : BangoYapColor::Orange;
	}
}

FText SBangoYapProgressionSettings::GetToggleToolTip(ELineSetting Setting) const
{
	using namespace BangoYapLineSettingsPrivate;
	const FSettingValue& Value = GetValue(Setting);
	const bool bInterruptible = Setting == ELineSetting::Interruptible;

	FText Base;
	switch (Value.Kind)
	{
		case FSettingValue::EKind::Default:
			if (bSpeech)
			{
				Base = bInterruptible
					? LOCTEXT("Interruptible_Default_Speech", "Interruptible: the speech config's")
					: LOCTEXT("Advance_Default_Speech", "Advance: the speech config's");
			}
			else
			{
				Base = bInterruptible
					? LOCTEXT("Interruptible_Default", "Interruptible: the speech's (or its config's)")
					: LOCTEXT("Advance_Default", "Advance: the speech's (or its config's)");
			}
			break;

		case FSettingValue::EKind::Expression:
			Base = FText::Format(bInterruptible
				? LOCTEXT("Interruptible_Expression", "Interruptible: {0} (only known while playing)")
				: LOCTEXT("Advance_Expression", "Advance: {0} (only known while playing)"), FText::FromString(Value.Text));
			break;

		default:
			if (bInterruptible)
			{
				Base = FText::Format(Value.Is(FirstValue)
					? LOCTEXT("Interruptible_Yes", "Playback is interruptible ({0})")
					: LOCTEXT("Interruptible_No", "Playback is not interruptible ({0})"), ScriptName(Setting, Value.Value));
			}
			else
			{
				Base = FText::Format(Value.Is(FirstValue)
					? LOCTEXT("Advance_Auto", "Auto-advances ({0})")
					: LOCTEXT("Advance_Manual", "Requires manual advancing ({0})"), ScriptName(Setting, Value.Value));
			}
			break;
	}

	const FText Hint = Value.IsDefault()
		? (bSpeech
			? LOCTEXT("Progression_Hint_Default_Speech", "Click to set it for this speech's lines. In conversations only.")
			: LOCTEXT("Progression_Hint_Default", "Click to set it for this line. In conversations only."))
		: LOCTEXT("Progression_Hint_Set", "Click to switch, Ctrl+click for the default. In conversations only.");

	return FText::Format(LOCTEXT("Progression_ToolTip", "{0}\n{1}"), Base, Hint);
}

// ================================================================================================ Time settings

void SBangoYapTimeSettings::Construct(const FArguments& InArgs)
{
	using namespace BangoYapLineSettingsPrivate;

	InitialTimeMode = InArgs._TimeMode;
	InitialPadding = InArgs._PaddingSetting;

	ChosenTimeMode = InitialTimeMode.Kind == FSettingValue::EKind::Value ? InitialTimeMode.Value : DefaultTime;
	bChosenPadding = InitialPadding.bSet && !InitialPadding.bExpression;
	ChosenSeconds = InitialPadding.Value;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	// As Yap's dialogue editor: default, audio, text, specified; and BangoYap's no time.
	for (const int32 Mode : { DefaultTime, AudioTime, TextTime, ManualTime, NoTime })
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(0, 2)
		[
			MakeTimeModeRow(Mode)
		];
	}

	if (InitialTimeMode.Kind == FSettingValue::EKind::Expression)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(0, 2)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
			.ColorAndOpacity(BangoYapColor::LightYellow)
			.Text(FText::Format(LOCTEXT("TimeMode_Expression", "The time mode is written as an expression ({0}): picking one here replaces it."), FText::FromString(InitialTimeMode.Text)))
		];
	}

	Box->AddSlot()
	.AutoHeight()
	.Padding(0, 6, 0, 0)
	[
		MakePaddingRow()
	];

	if (InitialPadding.bExpression)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(0, 2)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
			.ColorAndOpacity(BangoYapColor::LightYellow)
			.Text(FText::Format(LOCTEXT("Padding_Expression", "The padding is written as an expression ({0}): setting it here replaces it."), FText::FromString(InitialPadding.Text)))
		];
	}

	ChildSlot
	[
		Box
	];
}

TSharedRef<SWidget> SBangoYapTimeSettings::MakeTimeModeRow(int32 Mode)
{
	using namespace BangoYapLineSettingsPrivate;

	FText ModeToolTip;
	switch (Mode)
	{
		case TextTime: ModeToolTip = LOCTEXT("TimeMode_Text_ToolTip", "Use a time worked out from the text's length"); break;
		case AudioTime: ModeToolTip = LOCTEXT("TimeMode_Audio_ToolTip", "Use the length of the line's audio (its text's length if it has none)"); break;
		case ManualTime: ModeToolTip = LOCTEXT("TimeMode_Manual_ToolTip", "Use the seconds entered here (written as the line's Padding, so choosing another mode afterwards goes back to the default padding)"); break;
		case NoTime: ModeToolTip = LOCTEXT("TimeMode_None_ToolTip", "No time: the line plays until it's advanced"); break;
		default: ModeToolTip = LOCTEXT("TimeMode_Default_ToolTip", "Use the speech config's time mode"); break;
	}
	if (Mode != DefaultTime)
	{
		ModeToolTip = FText::Format(LOCTEXT("TimeMode_ToolTip_Script", "{0} ({1})"), ModeToolTip, FText::FromString(MakeSettingScript(ELineSetting::TimeMode, Mode)));
	}

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 8, 0)
		[
			SNew(STextBlock)
			.Text(GetTimeModeLabel(Mode))
			.SimpleTextMode(true)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 8, 0)
		[
			SNew(SButton)
			.Cursor(EMouseCursor::Default)
			.ButtonStyle(&FBangoYapEditorStyle::Get().GetWidgetStyle<FButtonStyle>(FBangoYapEditorStyle::ButtonStyle_TimeSetting))
			.ContentPadding(FMargin(4, 3))
			.ToolTipText(ModeToolTip)
			.OnClicked_Lambda([this, Mode]()
			{
				if (ChosenTimeMode != Mode)
				{
					// Manual time's seconds are the padding: leaving it goes back to the default padding.
					if (ChosenTimeMode == ManualTime && bChosenPadding)
					{
						bChosenPadding = false;
						bPaddingChanged = true;
					}
					ChosenTimeMode = Mode;
					bTimeModeChanged = true;
					// Manual time is the padding: give it a time to start from.
					if (Mode == ManualTime && !bChosenPadding)
					{
						SetSeconds(FMath::Max(ChosenSeconds, 1.0f));
					}
				}
				return FReply::Handled();
			})
			.ButtonColorAndOpacity(this, &SBangoYapTimeSettings::GetButtonColor, Mode)
			.ForegroundColor(this, &SBangoYapTimeSettings::GetButtonForeground, Mode)
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(16, 16))
				.Image(GetTimeModeBrush(Mode))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

	// A column for the seconds, as Yap's rows have (only the specified time's is used here).
	Row->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(60)
		[
			Mode == ManualTime ? MakeSecondsBox(true) : SNullWidget::NullWidget
		]
	];

	return SNew(SBox)
		.HeightOverride(24)
		[
			Row
		];
}

TSharedRef<SWidget> SBangoYapTimeSettings::MakePaddingRow()
{
	using namespace BangoYapLineSettingsPrivate;

	return SNew(SBox)
		.HeightOverride(24)
		.Visibility_Lambda([this]() { return ChosenTimeMode == ManualTime ? EVisibility::Collapsed : EVisibility::Visible; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PaddingTime_Header", "Padding Time"))
				.ToolTipText(LOCTEXT("PaddingTime_ToolTip", "Seconds after the line before what follows. Negative: what follows starts before it ends."))
				.SimpleTextMode(true)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Cursor(EMouseCursor::Default)
				.ButtonStyle(&FBangoYapEditorStyle::Get().GetWidgetStyle<FButtonStyle>(FBangoYapEditorStyle::ButtonStyle_TimeSetting))
				.ContentPadding(FMargin(4, 3))
				.ToolTipText(LOCTEXT("PaddingTime_Default", "Use the speech config's padding (green: it does). Click to switch."))
				.OnClicked_Lambda([this]()
				{
					if (bChosenPadding)
					{
						bChosenPadding = false;
						bPaddingChanged = true;
					}
					else
					{
						SetSeconds(ChosenSeconds);
					}
					return FReply::Handled();
				})
				.ButtonColorAndOpacity_Lambda([this]() { return FSlateColor(bChosenPadding ? BangoYapColor::DarkGray : BangoYapColor::Green); })
				.ForegroundColor_Lambda([this]() { return FSlateColor(bChosenPadding ? BangoYapColor::Gray : BangoYapColor::Green); })
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(16, 16))
					.Image(GetTimeModeBrush(DefaultTime))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(60)
				[
					MakeSecondsBox(false)
				]
			]
		];
}

TSharedRef<SWidget> SBangoYapTimeSettings::MakeSecondsBox(bool bManualRow)
{
	using namespace BangoYapLineSettingsPrivate;

	return SNew(SNumericEntryBox<float>)
		.AllowSpin(true)
		.Delta(bManualRow ? 0.05f : 0.01f)
		.MinValue(bManualRow ? TOptional<float>(0.0f) : TOptional<float>())
		.MinSliderValue(bManualRow ? 0.0f : -1.0f)
		.MaxSliderValue(bManualRow ? 10.0f : 2.0f)
		.MaxFractionalDigits(2)
		.IsEnabled_Lambda([this, bManualRow]() { return !bManualRow || ChosenTimeMode == ManualTime; })
		.ToolTipText(bManualRow
			? LOCTEXT("ManualTime_Box_ToolTip", "Seconds the line plays for")
			: LOCTEXT("Padding_Box_ToolTip", "Seconds of padding after the line (setting it stops using the default)"))
		.Value_Lambda([this, bManualRow]() -> TOptional<float>
		{
			// The padding box is empty while the default's used.
			if (!bManualRow && !bChosenPadding)
			{
				return TOptional<float>();
			}
			return ChosenSeconds;
		})
		.UndeterminedString(LOCTEXT("Padding_Default", "Default"))
		.OnValueChanged_Lambda([this](float NewValue) { SetSeconds(NewValue); })
		.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type) { SetSeconds(NewValue); });
}

void SBangoYapTimeSettings::SetSeconds(float NewSeconds)
{
	ChosenSeconds = NewSeconds;
	bChosenPadding = true;
	bPaddingChanged = true;
}

FSlateColor SBangoYapTimeSettings::GetButtonColor(int32 Mode) const
{
	// As Yap: the chosen mode in its colour, the others dark.
	return ChosenTimeMode == Mode ? FSlateColor(GetTimeModeColor(Mode)) : FSlateColor(BangoYapColor::DarkGray);
}

FSlateColor SBangoYapTimeSettings::GetButtonForeground(int32 Mode) const
{
	return ChosenTimeMode == Mode ? FSlateColor(GetTimeModeColor(Mode)) : FSlateColor(BangoYapColor::Gray);
}

FString SBangoYapTimeSettings::GetTimeModeScript() const
{
	if (!bTimeModeChanged)
	{
		return FString();
	}
	return MakeSettingScript(ELineSetting::TimeMode, ChosenTimeMode);
}

FString SBangoYapTimeSettings::GetPaddingScript() const
{
	if (!bPaddingChanged)
	{
		return FString();
	}
	if (!bChosenPadding)
	{
		return InitialPadding.bSet ? FString(TEXT("nil")) : FString();
	}
	return MakeSecondsScript(ChosenSeconds);
}

#undef LOCTEXT_NAMESPACE
