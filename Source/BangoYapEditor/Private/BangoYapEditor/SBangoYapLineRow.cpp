#include "SBangoYapLineRow.h"

#include "BangoNodeDecorator.h"
#include "BangoYapEditorStyle.h"
#include "SAssetDropTarget.h"
#include "SBangoYapLineSettings.h"
#include "SBangoYapTimeBar.h"
#include "SBangoYapDetailSwitch.h"
#include "Sound/SoundBase.h"
#include "HAL/PlatformTime.h"
#include "BangoYap/BangoYapSettings.h"
#include "Editor.h"
#include "Engine/World.h"
#include "YapBase/YapSubsystem.h"
#include "SBangoYapPickers.h"
#include "SBangoYapTextEditor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "YapBase/YapCharacterAsset.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

using namespace BangoYapEditor;

// ================================================================================================ Data

FBangoYapLineData FBangoYapLineData::FromContext(const FBangoNodeDecoratorContext& Context)
{
	FBangoYapLineData Data;
	Data.CommandName = Context.CommandName;
	Data.bDisabled = Context.bDisabled;
	Data.bMature = Context.CommandName == TEXT("SayM");
	Data.bPrompt = Context.CommandName == TEXT("AddPrompt");

	if (!Context.Layout)
	{
		return Data;
	}
	const FBangoNodeLayout& Layout = *Context.Layout;
	Data.Layout = Layout;
	Data.SetFieldValue = Context.SetFieldValue;
	Data.TargetText = GetFieldText(Layout, TEXT("Target"));

	Data.Target = ParseSpeakerTarget(Data.TargetText);
	Data.Text = ReadTextField(Layout, Data.bMature ? TEXT("MatureText") : TEXT("Text"));
	if (Data.bMature)
	{
		Data.ChildSafeText = ReadTextField(Layout, TEXT("ChildSafeText"));
	}
	if (Data.bPrompt)
	{
		Data.TitleText = ReadTextField(Layout, TEXT("TitleText"));
	}

	Data.TimeMode = ReadSetting(Layout, ELineSetting::TimeMode);
	Data.Padding = ReadPadding(Layout);
	Data.Interruptible = ReadSetting(Layout, ELineSetting::Interruptible);
	Data.Advance = ReadSetting(Layout, ELineSetting::Advance);

	const FString MoodText = GetFieldText(Layout, TEXT("Mood"));
	if (!MoodText.IsEmpty() && MoodText != TEXT("nil"))
	{
		Data.bMoodIsLiteral = ParseStringLiteral(MoodText, Data.Mood);
		if (!Data.bMoodIsLiteral)
		{
			Data.Mood = MoodText;
		}
	}
	return Data;
}

bool FBangoYapLineData::RowCoversField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex)
{
	if (!Context.Layout || !Context.Layout->Fields.IsValidIndex(FieldIndex) || !Context.SetFieldValue)
	{
		return false;
	}

	const FBangoNodeLayout& Layout = *Context.Layout;
	const FString& Label = Layout.Fields[FieldIndex].Label;

	if (Label == TEXT("Target"))
	{
		return IsTargetEditableInRow(ParseSpeakerTarget(GetFieldText(Layout, TEXT("Target"))));
	}

	if (Label == TEXT("Text") || Label == TEXT("MatureText") || Label == TEXT("ChildSafeText") || Label == TEXT("TitleText"))
	{
		const FTextValue Value = ReadTextField(Layout, *Label);
		return !Value.bPresent || Value.bLiteral;
	}

	if (Label == TEXT("TimeMode"))
	{
		return ReadSetting(Layout, ELineSetting::TimeMode).Kind != FSettingValue::EKind::Expression;
	}
	if (Label == TEXT("Interruptible"))
	{
		return ReadSetting(Layout, ELineSetting::Interruptible).Kind != FSettingValue::EKind::Expression;
	}
	if (Label == TEXT("Advance"))
	{
		return ReadSetting(Layout, ELineSetting::Advance).Kind != FSettingValue::EKind::Expression;
	}
	if (Label == TEXT("Padding"))
	{
		return !ReadPadding(Layout).bExpression;
	}

	if (Label == TEXT("Mood"))
	{
		const FString Mood = GetFieldText(Layout, TEXT("Mood"));
		FString Unused;
		return Mood.IsEmpty() || Mood == TEXT("nil") || ParseStringLiteral(Mood, Unused);
	}

	return false;
}

// ================================================================================================ Portrait

void SBangoYapPortrait::Construct(const FArguments& InArgs)
{
	Character = InArgs._Character;
	MoodTag = FindMoodTag(InArgs._Mood);
	PortraitSize = InArgs._PortraitSize;
	bPrompt = InArgs._bPrompt;

	PortraitBrush.DrawAs = ESlateBrushDrawType::Image;
	PortraitBrush.ImageSize = FVector2D(PortraitSize, PortraitSize);

	const int32 Border = InArgs._BorderSize;
	const float BoxSize = InArgs._bBorder ? FMath::Max(PortraitSize + 2 * Border, SBangoYapLineRow::RowHeight) : PortraitSize;

	TSharedRef<SOverlay> Overlay = SNew(SOverlay);

	if (InArgs._bBorder)
	{
		Overlay->AddSlot()
		[
			SNew(SBorder)
			.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Border_Thick_RoundedSquare))
			.BorderBackgroundColor(this, &SBangoYapPortrait::GetBorderColor)
		];
	}

	Overlay->AddSlot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	.Padding(InArgs._bBorder ? Border : 0)
	[
		SNew(SImage)
		.DesiredSizeOverride(FVector2D(PortraitSize, PortraitSize))
		.Image(this, &SBangoYapPortrait::GetPortraitBrush)
	];

	Overlay->AddSlot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Visibility(EVisibility::HitTestInvisible)
		.Text(this, &SBangoYapPortrait::GetLabel)
		.Font(FCoreStyle::GetDefaultFontStyle("Normal", InArgs._bBorder ? 8 : 6))
		.ColorAndOpacity(this, &SBangoYapPortrait::GetLabelColor)
		.Justification(ETextJustify::Center)
	];

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(BoxSize)
		.HeightOverride(BoxSize)
		.ToolTipText(this, &SBangoYapPortrait::GetPortraitToolTip)
		[
			Overlay
		]
	];
}

UYapCharacterAsset* SBangoYapPortrait::GetCharacter() const
{
	return FindCharacter(Character);
}

const FSlateBrush* SBangoYapPortrait::GetPortraitBrush() const
{
	UTexture2D* Texture = nullptr;
	if (const UYapCharacterAsset* Asset = GetCharacter())
	{
		Texture = Asset->GetCharacterPortrait(MoodTag);
	}

	if (!Texture)
	{
		return FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Portrait_Missing);
	}

	if (BrushTexture.Get() != Texture)
	{
		BrushTexture = Texture;
		PortraitBrush.SetResourceObject(Texture);
	}
	return &PortraitBrush;
}

FSlateColor SBangoYapPortrait::GetBorderColor() const
{
	const UYapCharacterAsset* Asset = GetCharacter();
	FLinearColor Color = Asset ? Asset->GetCharacterColor() : BangoYapColor::Gray;

	// As Yap: dimmed, but for a player prompt.
	if (!bPrompt)
	{
		Color = BangoYapColor::Desaturate(Color, 0.25f);
		Color.A *= 0.65f;
	}
	return Color;
}

FText SBangoYapPortrait::GetLabel() const
{
	switch (Character.Kind)
	{
		case ECharacterRefKind::None:
			return LOCTEXT("Portrait_NoSpeaker", "No\nspeaker");

		case ECharacterRefKind::ID:
			return GetCharacter() ? FText::GetEmpty() : FText::Format(LOCTEXT("Portrait_NotFound", "{0}\n(not found)"), FText::FromString(Character.Value));

		default:
			return FText::FromString(Character.ToDisplayString());
	}
}

FSlateColor SBangoYapPortrait::GetLabelColor() const
{
	switch (Character.Kind)
	{
		case ECharacterRefKind::None:
		case ECharacterRefKind::ID:
		case ECharacterRefKind::Other:
			return BangoYapColor::Red;
		case ECharacterRefKind::Variable:
			return BangoYapColor::LightYellow;
		default:
			return BangoYapColor::White;
	}
}

FText SBangoYapPortrait::GetPortraitToolTip() const
{
	if (const UYapCharacterAsset* Asset = GetCharacter())
	{
		return FText::Format(LOCTEXT("Portrait_ToolTip", "{0} ({1})"), Asset->GetCharacterName(), FText::FromName(Asset->GetCharacterID()));
	}

	switch (Character.Kind)
	{
		case ECharacterRefKind::None: return LOCTEXT("Portrait_ToolTip_None", "No character.");
		case ECharacterRefKind::ID: return FText::Format(LOCTEXT("Portrait_ToolTip_NotFound", "No Yap character has the ID '{0}'."), FText::FromString(Character.Value));
		case ECharacterRefKind::Entity: return FText::Format(LOCTEXT("Portrait_ToolTip_Entity", "The actor @{0}: its Yap Character Component says who it is, while playing."), FText::FromString(Character.Value));
		case ECharacterRefKind::Variable: return FText::Format(LOCTEXT("Portrait_ToolTip_Variable", "The variable '{0}': who it is is only known while playing."), FText::FromString(Character.Value));
		default: return FText::Format(LOCTEXT("Portrait_ToolTip_Other", "{0}: only known while playing."), FText::FromString(Character.Value));
	}
}

// ================================================================================================ Row

void SBangoYapLineRow::Construct(const FArguments& InArgs)
{
	Data = InArgs._Data;
	Writer = FFieldWriter(Data.Layout, Data.SetFieldValue);

	Timing = ComputeLineTiming(Data.Text, Data.TimeMode, Data.Padding, false);
	if (Data.bMature)
	{
		ChildSafeTiming = ComputeLineTiming(Data.ChildSafeText, Data.TimeMode, Data.Padding, true);
	}

	if (Data.Text.bLiteral)
	{
		MatureAudio = FindLineAudio(Data.Text.Key, false);
	}
	if (Data.bMature && Data.ChildSafeText.bLiteral)
	{
		ChildSafeAudio = FindLineAudio(Data.ChildSafeText.Key, true);
	}
	FindProblems();

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(RowWidth)
		.RenderOpacity(Data.bDisabled ? 0.4f : 1.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 3)
			[
				SNew(SBangoYapDetailSwitch)
				.HighDetail()
				[
					MakeUpperBar()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(32)
					.HAlign(HAlign_Center)
					[
						SNew(SBangoYapDetailSwitch)
						.HighDetail()
						[
							MakeMoodColumn()
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				[
					SNew(SBangoYapDetailSwitch)
					.HighDetail()
					[
						MakeSpeaker()
					]
					.OnMakeLowDetail(this, &SBangoYapLineRow::MakeLowDetailSpeaker)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4, 0, 0, 0)
				[
					SNew(SBox)
					.HeightOverride(RowHeight)
					[
						SNew(SBangoYapDetailSwitch)
						.HighDetail()
						[
							MakeTextColumn()
						]
						.OnMakeLowDetail_Lambda([this]() -> TSharedRef<SWidget>
						{
							// As Yap's low detail: the text box without its text.
							return SNew(SBorder)
								.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite))
								.BorderBackgroundColor(Problems.Num() > 0 ? BangoYapColor::Red.CopyWithNewOpacity(0.4f) : BangoYapColor::DarkGray_SemiGlass);
						})
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(32, 3, 0, 0)
			[
				SNew(SBangoYapDetailSwitch)
				.HighDetail()
				[
					MakeTimeBar()
				]
			]
		]
	];
}

// ------------------------------------------------------------------------------------------------ Editing

bool SBangoYapLineRow::CanEdit() const
{
	return Writer.CanEdit();
}

void SBangoYapLineRow::ReportError(const FString& Error) const
{
	FFieldWriter::ReportError(Error);
}

bool SBangoYapLineRow::SetField(const TCHAR* Label, const FString& Value)
{
	return SetFields({ { Label, Value } });
}

bool SBangoYapLineRow::SetFields(const TArray<FFieldChange>& Changes)
{
	// Kept alive in case setting a field draws the node again.
	TSharedRef<SWidget> Self = AsShared();
	return Writer.SetFields(Changes);
}

void SBangoYapLineRow::SetTarget(const FSpeakerTarget& Target)
{
	bool bDropped = false;
	const FString Script = MakeTargetScript(Target, &bDropped);
	if (bDropped)
	{
		ReportError(TEXT("a speaker and who it's said to must be the same kind (two IDs, or two actors), so who it's said to was left out."));
	}
	SetField(TEXT("Target"), Script);
}

void SBangoYapLineRow::CloseMenus()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().DismissAllMenus();
	}
}

// ------------------------------------------------------------------------------------------------ Speaker

TSharedRef<SWidget> SBangoYapLineRow::MakeSpeaker()
{
	TSharedRef<SWidget> Portrait = SNew(SBangoYapPortrait)
		.Character(Data.Target.Speaker)
		.Mood(Data.bMoodIsLiteral ? Data.Mood : FString())
		.PortraitSize(PortraitSize)
		.BorderSize(PortraitBorder)
		.bPrompt(Data.bPrompt);

	if (!CanEdit())
	{
		return Portrait;
	}

	return SNew(SAssetDropTarget)
		.bSupportsMultiDrop(false)
		.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> Assets)
		{
			return Assets.Num() == 1 && Assets[0].IsInstanceOf(UYapCharacterAsset::StaticClass());
		})
		.OnAssetsDropped_Lambda([this](const FDragDropEvent&, TArrayView<FAssetData> Assets)
		{
			if (Assets.Num() == 1)
			{
				if (const UYapCharacterAsset* Character = Cast<UYapCharacterAsset>(Assets[0].GetAsset()))
				{
					FSpeakerTarget Target = Data.Target;
					Target.bValid = true;
					Target.Speaker.Kind = ECharacterRefKind::ID;
					Target.Speaker.Value = Character->GetCharacterID().ToString();
					SetTarget(Target);
				}
			}
		})
		[
			SAssignNew(SpeakerCombo, SComboButton)
			.HasDownArrow(false)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.MenuPlacement(MenuPlacement_BelowAnchor)
			.OnGetMenuContent(this, &SBangoYapLineRow::MakeSpeakerMenu)
			.ToolTipText(LOCTEXT("Speaker_ToolTip", "Who says it: click to pick a character, or drop a Yap character asset here"))
			.ButtonContent()
			[
				Portrait
			]
		];
}

TSharedRef<SWidget> SBangoYapLineRow::MakeSpeakerMenu()
{
	TSharedRef<SBangoYapCharacterPicker> Picker = SNew(SBangoYapCharacterPicker)
		.Current(Data.Target.Speaker)
		.ScriptText(Data.TargetText)
		.OnPicked_Lambda([this](SBangoYapCharacterPicker::EChoice, const FCharacterRef& Character)
		{
			CloseMenus();
			FSpeakerTarget Target = Data.Target;
			Target.bValid = true;
			Target.Speaker = Character;
			SetTarget(Target);
		})
		.OnScriptCommitted_Lambda([this](const FString& Text)
		{
			CloseMenus();
			SetField(TEXT("Target"), Text);
		});

	if (SpeakerCombo.IsValid())
	{
		SpeakerCombo->SetMenuContentWidgetToFocus(Picker->GetFocusWidget());
	}
	return Picker;
}

// ------------------------------------------------------------------------------------------------ Directed at

TSharedRef<SWidget> SBangoYapLineRow::MakeDirectedAt()
{
	const int32 Size = PortraitSize / 3;

	TSharedRef<SWidget> Circle = SNew(SBorder)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_FilledCircle))
		.BorderBackgroundColor_Lambda([this]()
		{
			const UYapCharacterAsset* Asset = FindCharacter(Data.Target.DirectedAt);
			return FSlateColor(Asset ? Asset->GetCharacterColor() : BangoYapColor::Gray);
		})
		.Padding(3)
		[
			SNew(SBox)
			.WidthOverride(Size + 3)
			.HeightOverride(Size + 3)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				Data.Target.DirectedAt.IsSet()
				? StaticCastSharedRef<SWidget>(SNew(SBangoYapPortrait).Character(Data.Target.DirectedAt).PortraitSize(Size).bBorder(false))
				: StaticCastSharedRef<SWidget>(SNew(STextBlock)
					.Text(Data.Target.bArray ? LOCTEXT("DirectedAt_NoOne", "-") : LOCTEXT("DirectedAt_Focus", "@"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.ColorAndOpacity(BangoYapColor::LightGray))
			]
		];

	const auto DirectedAtVisibility = [this]()
	{
		// As Yap: shown when it's set, or while the row is hovered.
		return (Data.Target.DirectedAt.IsSet() || IsHovered() || (DirectedAtCombo.IsValid() && DirectedAtCombo->IsOpen())) ? EVisibility::Visible : EVisibility::Collapsed;
	};

	if (!CanEdit() || !IsTargetEditableInRow(Data.Target))
	{
		return SNew(SBox).Visibility_Lambda([this]() { return Data.Target.DirectedAt.IsSet() ? EVisibility::Visible : EVisibility::Collapsed; })[Circle];
	}

	return SNew(SBox)
		.Visibility_Lambda(DirectedAtVisibility)
		[
			SNew(SAssetDropTarget)
			.bSupportsMultiDrop(false)
			.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> Assets)
			{
				return Assets.Num() == 1 && Assets[0].IsInstanceOf(UYapCharacterAsset::StaticClass());
			})
			.OnAssetsDropped_Lambda([this](const FDragDropEvent&, TArrayView<FAssetData> Assets)
			{
				if (Assets.Num() == 1)
				{
					if (const UYapCharacterAsset* Character = Cast<UYapCharacterAsset>(Assets[0].GetAsset()))
					{
						FSpeakerTarget Target = Data.Target;
						Target.bArray = true;
						Target.DirectedAt.Kind = ECharacterRefKind::ID;
						Target.DirectedAt.Value = Character->GetCharacterID().ToString();
						SetTarget(Target);
					}
				}
			})
			[
				SAssignNew(DirectedAtCombo, SComboButton)
				.HasDownArrow(false)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.MenuPlacement(MenuPlacement_BelowAnchor)
				.OnGetMenuContent(this, &SBangoYapLineRow::MakeDirectedAtMenu)
				.ToolTipText_Lambda([this]()
				{
					if (Data.Target.DirectedAt.IsSet())
					{
						return FText::Format(LOCTEXT("DirectedAt_ToolTip_Set", "Said to {0}: click to change"), FText::FromString(Data.Target.DirectedAt.ToDisplayString()));
					}
					return Data.Target.bArray
						? LOCTEXT("DirectedAt_ToolTip_NoOne", "Said to no one: click to pick who it's said to")
						: LOCTEXT("DirectedAt_ToolTip_Focus", "Said to the speaker's speech focus: click to pick who it's said to");
				})
				.ButtonContent()
				[
					Circle
				]
			]
		];
}

TSharedRef<SWidget> SBangoYapLineRow::MakeDirectedAtMenu()
{
	TSharedRef<SBangoYapCharacterPicker> Picker = SNew(SBangoYapCharacterPicker)
		.Current(Data.Target.DirectedAt)
		.bDirectedAt(true)
		.CurrentChoice(Data.Target.bArray ? SBangoYapCharacterPicker::EChoice::NoOne : SBangoYapCharacterPicker::EChoice::Focus)
		.OnPicked_Lambda([this](SBangoYapCharacterPicker::EChoice Choice, const FCharacterRef& Character)
		{
			CloseMenus();
			FSpeakerTarget Target = Data.Target;
			switch (Choice)
			{
				case SBangoYapCharacterPicker::EChoice::Focus:
					Target.bArray = false;
					Target.DirectedAt = FCharacterRef();
					break;
				case SBangoYapCharacterPicker::EChoice::NoOne:
					Target.bArray = true;
					Target.DirectedAt = FCharacterRef();
					break;
				default:
					Target.bArray = true;
					Target.DirectedAt = Character;
					break;
			}
			SetTarget(Target);
		});

	if (DirectedAtCombo.IsValid())
	{
		DirectedAtCombo->SetMenuContentWidgetToFocus(Picker->GetFocusWidget());
	}
	return Picker;
}

// ------------------------------------------------------------------------------------------------ Mood

TSharedRef<SWidget> SBangoYapLineRow::MakeMoodColumn()
{
	const bool bHasMood = !Data.Mood.IsEmpty();
	const FString IconName = Data.bMoodIsLiteral ? Data.Mood : TEXT("Unknown");

	FText MoodToolTip;
	if (!bHasMood)
	{
		MoodToolTip = LOCTEXT("Mood_Default", "Default (no mood)");
	}
	else if (!Data.bMoodIsLiteral)
	{
		MoodToolTip = FText::Format(LOCTEXT("Mood_Expression", "Mood: {0} (only known while playing)"), FText::FromString(Data.Mood));
	}
	else if (!FindMoodTag(Data.Mood).IsValid())
	{
		MoodToolTip = FText::Format(LOCTEXT("Mood_Unknown", "Mood: {0} (not a mood under any speech config's mood root)"), FText::FromString(Data.Mood));
	}
	else
	{
		MoodToolTip = FText::Format(LOCTEXT("Mood_Known", "Mood: {0}"), FText::FromString(Data.Mood));
	}

	TSharedRef<SWidget> Icon = SNew(SBox)
		.WidthOverride(22)
		.HeightOverride(22)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FBangoYapEditorStyle::GetMoodBrush(bHasMood ? IconName : FString()))
			.DesiredSizeOverride(FVector2D(16, 16))
			.ColorAndOpacity(bHasMood ? BangoYapColor::White_Trans : BangoYapColor::LightGray)
		];

	if (!CanEdit() || !Data.bMoodIsLiteral)
	{
		return SNew(SBox).ToolTipText(MoodToolTip)[Icon];
	}

	return SAssignNew(MoodCombo, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(0)
		.MenuPlacement(MenuPlacement_CenteredBelowAnchor)
		.ToolTipText(MoodToolTip)
		.OnGetMenuContent(this, &SBangoYapLineRow::MakeMoodMenu)
		.ButtonContent()
		[
			Icon
		];
}

TSharedRef<SWidget> SBangoYapLineRow::MakeMoodMenu()
{
	return SNew(SBangoYapMoodPicker)
		.Current(Data.Mood)
		.OnPicked_Lambda([this](const FString& Mood)
		{
			CloseMenus();
			if (Mood == Data.Mood)
			{
				return;
			}
			SetFields({ { TEXT("Mood"), Mood.IsEmpty() ? FString(TEXT("nil")) : MakeStringLiteral(Mood) } });
		});
}

// ------------------------------------------------------------------------------------------------ Upper bar

TSharedRef<SWidget> SBangoYapLineRow::MakeUpperBar()
{
	// As Yap's CreateUpperFragmentBar: a 20 px bar over the line, the progression toggles at its right. (Yap's
	// activation counter and conditions have no BangoYap command yet; the line ID is on the text box.)
	return SNew(SBox)
		.HeightOverride(20)
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
			.Padding(0, 0, 4, 0)
			[
				MakeTimeSummary()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2, -2, 0, -2)
			[
				SNew(SBangoYapProgressionSettings)
				.Interruptible(Data.Interruptible)
				.Advance(Data.Advance)
				.bEditable(CanEdit())
				.OnSet_Lambda([this](ELineSetting Setting, int32 Value)
				{
					SetFields({ { GetSettingLabel(Setting), MakeSettingScript(Setting, Value) } });
				})
			]
		];
}

TSharedRef<SWidget> SBangoYapLineRow::MakeTimeSummary()
{
	const bool bTimeMode = !Data.TimeMode.IsDefault();
	if (!bTimeMode && !Data.Padding.bSet)
	{
		return SNullWidget::NullWidget;
	}

	const int32 Mode = Data.TimeMode.Kind == FSettingValue::EKind::Value ? Data.TimeMode.Value : 0;
	const bool bManual = Mode == 3;

	// What it says: the seconds for manual time, the padding otherwise ("+0.5 s"), or what's written.
	FString Label;
	if (Data.TimeMode.Kind == FSettingValue::EKind::Expression)
	{
		Label = Data.TimeMode.Text;
	}
	if (Data.Padding.bSet)
	{
		const FString Seconds = Data.Padding.bExpression ? Data.Padding.Text : MakeSecondsScript(Data.Padding.Value) + TEXT(" s");
		const FString Part = bManual || Data.Padding.bExpression || Data.Padding.Value < 0.0f ? Seconds : TEXT("+") + Seconds;
		Label = Label.IsEmpty() ? Part : Label + TEXT("  ") + Part;
	}

	FText SummaryToolTip = bTimeMode
		? FText::Format(LOCTEXT("TimeSummary_Mode", "{0}"), Mode ? GetTimeModeLabel(Mode) : FText::FromString(Data.TimeMode.Text))
		: LOCTEXT("TimeSummary_DefaultMode", "Use Default Time");
	if (Data.Padding.bSet && !bManual)
	{
		SummaryToolTip = FText::Format(LOCTEXT("TimeSummary_Padding", "{0}, padding {1}"), SummaryToolTip, FText::FromString(Data.Padding.Text));
	}
	if (CanEdit())
	{
		SummaryToolTip = FText::Format(LOCTEXT("TimeSummary_Edit", "{0}\nClick to change"), SummaryToolTip);
	}

	const FLinearColor Color = bTimeMode ? (Mode ? GetTimeModeColor(Mode) : BangoYapColor::LightYellow) : BangoYapColor::LightGray;

	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Visibility(bTimeMode ? EVisibility::Visible : EVisibility::Collapsed)
			.DesiredSizeOverride(FVector2D(14, 14))
			.Image(GetTimeModeBrush(Mode))
			.ColorAndOpacity(Color)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(bTimeMode ? 3 : 0, 0, 0, 0)
		[
			SNew(STextBlock)
			.Visibility(Label.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			.Text(FText::FromString(Label))
			.Font(FCoreStyle::GetDefaultFontStyle("Normal", 8))
			.ColorAndOpacity(Data.Padding.bExpression ? BangoYapColor::LightYellow : BangoYapColor::LightGray)
		];

	return SNew(SButton)
		.Cursor(EMouseCursor::Default)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ContentPadding(FMargin(2, 0))
		.IsEnabled(CanEdit())
		.ToolTipText(SummaryToolTip)
		.OnClicked_Lambda([this]()
		{
			if (TextCombo.IsValid())
			{
				TextCombo->SetIsOpen(true);
			}
			return FReply::Handled();
		})
		[
			Content
		];
}

// ------------------------------------------------------------------------------------------------ Time bar

const FBangoYapLineTiming& SBangoYapLineRow::GetShownTiming() const
{
	return ShowsChildSafe() ? ChildSafeTiming : Timing;
}

FLinearColor SBangoYapLineRow::GetTimeBarColor() const
{
	// As Yap (ColorAndOpacity_FragmentTimeIndicator): the line's time mode's colour, greyer when it's the default.
	FLinearColor Color;
	switch (Data.TimeMode.Kind)
	{
		case FSettingValue::EKind::Default:
			Color = BangoYapColor::Desaturate(BangoYapColor::LightGray, 0.4f);
			break;
		case FSettingValue::EKind::Expression:
			Color = BangoYapColor::Desaturate(BangoYapColor::LightYellow, 0.2f);
			break;
		default:
			Color = BangoYapColor::Desaturate(GetTimeModeColor(Data.TimeMode.Value), 0.2f);
			break;
	}

	// Darker while a game plays and it isn't the line playing.
	if (GEditor && GEditor->IsPlaySessionInProgress() && !GetPlaybackTime().IsSet())
	{
		Color *= BangoYapColor::Gray;
	}

	Color.A = 0.6f;
	return Color;
}

const FYapActiveLine* SBangoYapLineRow::FindActiveLine() const
{
	// A line is known by its ID (its text's key), so one without an ID can't be found.
	if (!GEditor || !GEditor->PlayWorld || !Data.Text.bLiteral || Data.Text.Key.IsEmpty())
	{
		return nullptr;
	}

	const UYapSubsystem* Subsystem = GEditor->PlayWorld->GetSubsystem<UYapSubsystem>();
	if (!Subsystem)
	{
		return nullptr;
	}

	const FName LineID(*Data.Text.Key);
	for (const FYapActiveLine& Line : Subsystem->GetActiveLines())
	{
		if (Line.Data.LineID == LineID)
		{
			return &Line;
		}
	}
	return nullptr;
}

TOptional<float> SBangoYapLineRow::GetPlaybackTime() const
{
	const FYapActiveLine* Line = FindActiveLine();
	if (Line && !Line->bFinished)
	{
		return Line->Elapsed;
	}
	return TOptional<float>();
}

TSharedRef<SWidget> SBangoYapLineRow::MakeTimeBar()
{
	FText BarToolTip = Timing.Describe();
	if (CanEdit() && Timing.bTimed)
	{
		BarToolTip = FText::Format(LOCTEXT("TimeBar_ToolTip_Edit", "{0}\nDrag the handle at its end to change the padding (for a specified time, the time); Ctrl+click it for the default."), BarToolTip);
	}

	const bool bManual = Timing.TimeMode == 3;

	return SNew(SBox)
		.HeightOverride(9)
		.Visibility_Lambda([this]() { return GetShownTiming().bTimed ? EVisibility::Visible : EVisibility::Collapsed; })
		.ToolTipText_Lambda([this, BarToolTip]()
		{
			return ShowsChildSafe() ? ChildSafeTiming.Describe() : BarToolTip;
		})
		[
			SNew(SBangoYapTimeBar)
			.MaxTime(GetDefault<UBangoYapSettings>()->TimeBarMaxSeconds)
			.bEditable(CanEdit() && !Data.Padding.bExpression)
			.BarColor(this, &SBangoYapLineRow::GetTimeBarColor)
			// A specified time's padding is its time: it's drawn as the speech, and its handle changes it.
			.PaddingIsSet(Data.Padding.bSet && !bManual)
			.SpeechTime_Lambda([this]() { return GetShownTiming().SpeechTime; })
			.PaddingTime_Lambda([this]() { return GetShownTiming().Padding; })
			.PlaybackTime(this, &SBangoYapLineRow::GetPlaybackTime)
			.OnHandleCommitted_Lambda([this, bManual](float HandleTime)
			{
				const float NewPadding = bManual ? FMath::Max(HandleTime, 0.0f) : HandleTime - GetShownTiming().SpeechTime;
				SetFields({ { TEXT("Padding"), MakeSecondsScript(NewPadding) } });
			})
			.OnHandleReset_Lambda([this]()
			{
				SetFields({ { TEXT("Padding"), FString(TEXT("nil")) } });
			})
		];
}

// ------------------------------------------------------------------------------------------------ Text

TSharedRef<SWidget> SBangoYapLineRow::MakeTextColumn()
{
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SBox)
			.MaxDesiredHeight(Data.bPrompt ? 49 : RowHeight)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					MakeDialogueText()
				]
				+ SOverlay::Slot()
				.Padding(-2)
				[
					MakeErrorBorder()
				]
				+ SOverlay::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Right)
				.Padding(-4)
				[
					MakeDirectedAt()
				]
				+ SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Right)
				.Padding(0, 0, -3, -2)
				[
					MakeLineIDTag()
				]
			]
		];

	if (Data.bPrompt)
	{
		Column->AddSlot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(SBox)
			.MaxDesiredHeight(20)
			[
				MakeTitleText()
			]
		];
	}

	// As Yap's fragment highlight: over the texts, while the line plays.
	TSharedRef<SWidget> Framed = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			Column
		]
		+ SOverlay::Slot()
		[
			MakeHighlight()
		];

	if (!CanEdit())
	{
		return Framed;
	}

	// Clicking anywhere on the texts opens the text editor (the corner widgets take their own clicks).
	return SAssignNew(TextCombo, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ContentPadding(0)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.MenuPlacement(MenuPlacement_BelowAnchor)
		.OnGetMenuContent(this, &SBangoYapLineRow::MakeTextMenu)
		.ButtonContent()
		[
			Framed
		];
}

TSharedRef<SWidget> SBangoYapLineRow::MakeTextMenu()
{
	TSharedRef<SBangoYapTextEditor> Editor = SNew(SBangoYapTextEditor)
		.Text(Data.Text)
		.ChildSafeText(Data.ChildSafeText)
		.TitleText(Data.TitleText)
		.bMature(Data.bMature)
		.bPrompt(Data.bPrompt)
		.TimeMode(Data.TimeMode)
		.PaddingSetting(Data.Padding)
		.OnApply_Lambda([this](const SBangoYapTextEditor::FResult& Result)
		{
			CloseMenus();
			// One change per field that changed (empty: unchanged), set in the order they're written.
			SetFields({
				{ Data.bMature ? TEXT("MatureText") : TEXT("Text"), Result.Text },
				{ TEXT("ChildSafeText"), Result.ChildSafeText },
				{ TEXT("TitleText"), Result.TitleText },
				{ TEXT("TimeMode"), Result.TimeMode },
				{ TEXT("Padding"), Result.Padding },
			});
		})
		.OnCancel_Lambda([this]() { CloseMenus(); });

	if (TextCombo.IsValid())
	{
		TextCombo->SetMenuContentWidgetToFocus(Editor->GetFocusWidget());
	}
	return Editor;
}

bool SBangoYapLineRow::ShowsChildSafe() const
{
	return Data.bMature && FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
}

FText SBangoYapLineRow::GetDialogueText() const
{
	const FTextValue& Value = ShowsChildSafe() ? Data.ChildSafeText : Data.Text;
	return FText::FromString(Value.Source);
}

FSlateColor SBangoYapLineRow::GetDialogueTextColor() const
{
	const FTextValue& Value = ShowsChildSafe() ? Data.ChildSafeText : Data.Text;
	FLinearColor Color = Value.bLiteral ? BangoYapColor::LightGray : BangoYapColor::LightYellow;
	if (ShowsChildSafe())
	{
		Color *= BangoYapColor::LightBlue;
	}
	return Color;
}

FText SBangoYapLineRow::GetEmptyText() const
{
	const FTextValue& Value = ShowsChildSafe() ? Data.ChildSafeText : Data.Text;
	if (Value.bPresent && !Value.Source.IsEmpty())
	{
		return FText::GetEmpty();
	}
	if (!Data.bMature)
	{
		return LOCTEXT("DialogueText_None", "Dialogue Text (None)");
	}
	return ShowsChildSafe() ? LOCTEXT("SafeDialogueText_None", "Child-Safe Dialogue Text (None)") : LOCTEXT("MatureDialogueText_None", "Mature Dialogue Text (None)");
}

TSharedRef<SWidget> SBangoYapLineRow::MakeDialogueText()
{
	return SNew(SBorder)
		.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite))
		.BorderBackgroundColor_Lambda([this]()
		{
			// A little lighter while hovered, as a button would be.
			return FSlateColor(TextCombo.IsValid() && TextCombo->IsHovered() ? BangoYapColor::Gray.CopyWithNewOpacity(0.6f) : BangoYapColor::DarkGray_SemiGlass);
		})
		.ToolTipText(this, &SBangoYapLineRow::GetDialogueToolTip)
		.Padding(0)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.Padding(4, 4, 4, 2)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(Data.Text.bLiteral || !Data.Text.bPresent ? FBangoYapEditorStyle::DialogueFont() : FBangoYapEditorStyle::TitleFont())
				.Text(this, &SBangoYapLineRow::GetDialogueText)
				.ColorAndOpacity(this, &SBangoYapLineRow::GetDialogueTextColor)
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Justification(ETextJustify::Center)
				.Font(FBangoYapEditorStyle::DialogueFont())
				.Text(this, &SBangoYapLineRow::GetEmptyText)
				.ColorAndOpacity(BangoYapColor::White_Glass)
			]
		];
}

TSharedRef<SWidget> SBangoYapLineRow::MakeTitleText()
{
	const bool bHas = Data.TitleText.bPresent && !Data.TitleText.Source.IsEmpty();

	return SNew(SBorder)
		.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite))
		.BorderBackgroundColor(BangoYapColor::DarkGray_SemiGlass)
		.ToolTipText(LOCTEXT("TitleText_ToolTip", "Title text: what the player sees in the list of choices (without one, the line's text). Click to edit."))
		.Padding(0)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.Padding(4, 0)
			[
				SNew(STextBlock)
				.Font(FBangoYapEditorStyle::TitleFont())
				.Text(FText::FromString(Data.TitleText.Source))
				.ColorAndOpacity(Data.TitleText.bLiteral || !Data.TitleText.bPresent ? BangoYapColor::YellowGray : BangoYapColor::LightYellow)
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Visibility(bHas ? EVisibility::Hidden : EVisibility::HitTestInvisible)
				.Font(FBangoYapEditorStyle::TitleFont())
				.Text(LOCTEXT("TitleText_None", "Title Text (None)"))
				.ColorAndOpacity(BangoYapColor::White_Glass)
			]
		];
}

TSharedRef<SWidget> SBangoYapLineRow::MakeLineIDTag()
{
	// The line's ID is its text's key; a line without one (TEXT_INV, or text that isn't a literal) has no ID or audio.
	if (!Data.Text.bLiteral || Data.Text.Key.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SWidget> IDTag = SNew(SBox)
		.MinDesiredWidth(28)
		.MinDesiredHeight(20)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_IDTag))
				.BorderBackgroundColor(BangoYapColor::DarkGray)
				.Padding(5, 3, 5, 3)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					// As Yap: white with audio, dark without (Yap's is black; a little lighter here, to read).
					.ColorAndOpacity_Lambda([this]()
					{
						return FSlateColor(bKeyMismatch ? BangoYapColor::OrangeRed : (GetShownAudio() ? BangoYapColor::White : BangoYapColor::Gray));
					})
					.Text_Lambda([this]()
					{
						return FText::FromString(ShowsChildSafe() && Data.ChildSafeText.bLiteral && !Data.ChildSafeText.Key.IsEmpty() ? Data.ChildSafeText.Key : Data.Text.Key);
					})
				]
			]
			// As Yap: a red "!" at its left for an audio ID that's wrong.
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(-12, 0)
			[
				SNew(SBox)
				.Visibility(bKeyMismatch ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				.WidthOverride(16)
				.HeightOverride(16)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_FilledCircle))
						.ColorAndOpacity(BangoYapColor::Red)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(INVTEXT("!"))
						.ColorAndOpacity(BangoYapColor::White)
					]
				]
			]
		];

	// As Yap's audio ID button: clicking plays the line's audio.
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ContentPadding(0)
		.Cursor(EMouseCursor::Default)
		.ToolTipText_Lambda([this]()
		{
			const FString& Key = ShowsChildSafe() && Data.ChildSafeText.bLiteral && !Data.ChildSafeText.Key.IsEmpty() ? Data.ChildSafeText.Key : Data.Text.Key;
			const UObject* Audio = GetShownAudio();
			return Audio
				? FText::Format(LOCTEXT("LineID_Audio", "Line ID: {0}\nAudio: {1} (click to play it)"), FText::FromString(Key), FText::FromString(Audio->GetName()))
				: FText::Format(LOCTEXT("LineID_NoAudio", "Line ID: {0}\nNo audio: the broker finds none for this ID."), FText::FromString(Key));
		})
		.OnClicked_Lambda([this]()
		{
			if (USoundBase* Sound = Cast<USoundBase>(GetShownAudio()))
			{
				if (GEditor)
				{
					GEditor->PlayPreviewSound(Sound);
				}
			}
			return FReply::Handled();
		})
		[
			IDTag
		];
}

// ------------------------------------------------------------------------------------------------ Stage 6: state

UObject* SBangoYapLineRow::GetShownAudio() const
{
	return ShowsChildSafe() ? ChildSafeAudio.Get() : MatureAudio.Get();
}

FSlateColor SBangoYapLineRow::GetHighlightColor() const
{
	// As Yap's BorderBackgroundColor_FragmentHighlight: white glass while it plays, yellow while it waits to be
	// advanced, fading out over half a second after.
	const double Now = FPlatformTime::Seconds();
	if (const FYapActiveLine* Line = FindActiveLine())
	{
		if (Line->bAwaitingAdvance)
		{
			LastPlayingTime = Now;
			return BangoYapColor::Yellow_Glass;
		}
		if (!Line->bFinished)
		{
			LastPlayingTime = Now;
			return BangoYapColor::White_Glass;
		}
	}

	if (LastPlayingTime < 0.0 || !(GEditor && GEditor->PlayWorld))
	{
		LastPlayingTime = -1.0;
		return FLinearColor::Transparent;
	}

	constexpr double FadeTime = 0.5;
	FLinearColor Color = BangoYapColor::White_Glass;
	Color.A *= 1.0f - (float)FMath::Clamp((Now - LastPlayingTime) / FadeTime, 0.0, 1.0);
	return Color;
}

TSharedRef<SWidget> SBangoYapLineRow::MakeHighlight()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
		.Visibility_Lambda([this]()
		{
			return GetHighlightColor().GetSpecifiedColor().A > 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		.BorderBackgroundColor(this, &SBangoYapLineRow::GetHighlightColor);
}

TSharedRef<SWidget> SBangoYapLineRow::MakeErrorBorder()
{
	// As Yap's error state: a red marquee round the text box (the reasons are in its tooltip).
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("MarqueeSelection"))
		.Visibility(Problems.Num() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
		.BorderBackgroundColor(BangoYapColor::Red);
}

void SBangoYapLineRow::FindProblems()
{
	Problems.Reset();
	bKeyMismatch = false;

	auto CheckCharacter = [this](const FCharacterRef& Ref, const FText& Format)
	{
		if (Ref.Kind == ECharacterRefKind::ID && !FindCharacter(Ref))
		{
			Problems.Add(FText::Format(Format, FText::FromString(Ref.Value)));
		}
	};
	CheckCharacter(Data.Target.Speaker, LOCTEXT("Problem_Speaker", "No Yap character has the ID '{0}' (the speaker)."));
	CheckCharacter(Data.Target.DirectedAt, LOCTEXT("Problem_DirectedAt", "No Yap character has the ID '{0}' (who it's said to)."));
	if (!Data.Target.bValid)
	{
		Problems.Add(FText::Format(LOCTEXT("Problem_Target", "The target ({0}) isn't a speaker, or [speaker, who it's said to]."), FText::FromString(Data.TargetText)));
	}

	if (Data.bMoodIsLiteral && !Data.Mood.IsEmpty() && !FindMoodTag(Data.Mood).IsValid())
	{
		Problems.Add(FText::Format(LOCTEXT("Problem_Mood", "'{0}' isn't a mood under any speech config's mood root."), FText::FromString(Data.Mood)));
	}

	if (Data.bMature && Data.Text.bLiteral && Data.ChildSafeText.bLiteral)
	{
		// As Yap: a line with one of its texts and not the other.
		if (Data.Text.Source.IsEmpty() != Data.ChildSafeText.Source.IsEmpty())
		{
			Problems.Add(LOCTEXT("Problem_ChildSafeText", "Only one of the mature and child-safe texts is written."));
		}

		// As BangoYap's SayM warns while playing: the child-safe key follows the line ID.
		const FString Expected = MakeChildSafeKey(Data.Text.Key);
		if (!Data.Text.Key.IsEmpty() && Data.ChildSafeText.Key != Expected)
		{
			bKeyMismatch = true;
			Problems.Add(FText::Format(LOCTEXT("Problem_ChildSafeKey", "The child-safe text's key is '{0}': for the line '{1}' it should be '{2}' (applying the text editor fixes it)."),
				FText::FromString(Data.ChildSafeText.Key), FText::FromString(Data.Text.Key), FText::FromString(Expected)));
		}
	}

	if (Timing.bManualMissingTime)
	{
		Problems.Add(LOCTEXT("Problem_ManualTime", "Specified time, but no time is given (it's the padding): the line won't run."));
	}
}

FText SBangoYapLineRow::GetDialogueToolTip() const
{
	FText Hint = Data.bMature
		? LOCTEXT("DialogueText_ShiftForChildSafe", "Click to edit. Hold Shift to show the child-safe text.")
		: LOCTEXT("DialogueText_ClickToEdit", "Click to edit");
	if (!CanEdit())
	{
		Hint = Data.bMature ? LOCTEXT("DialogueText_Shift", "Hold Shift to show the child-safe text.") : FText::GetEmpty();
	}

	if (Problems.Num() == 0)
	{
		return Hint;
	}

	TArray<FText> Lines(Problems);
	if (!Hint.IsEmpty())
	{
		Lines.Add(Hint);
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

TSharedRef<SWidget> SBangoYapLineRow::MakeLowDetailSpeaker() const
{
	// As Yap's CreateSpeakerImageWidget_LowDetail: a rounded box in the speaker's colour.
	const UYapCharacterAsset* Asset = FindCharacter(Data.Target.Speaker);
	FLinearColor Color = Asset ? Asset->GetCharacterColor() : BangoYapColor::Gray;
	if (!Data.bPrompt)
	{
		Color = BangoYapColor::Desaturate(Color, 0.25f);
		Color.A *= 0.65f;
	}

	const int32 Size = FMath::Max(PortraitSize + 2 * PortraitBorder, RowHeight);
	return SNew(SBox)
		.WidthOverride(Size)
		.HeightOverride(Size)
		[
			SNew(SBorder)
			.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite_Rounded))
			.BorderBackgroundColor(Color)
		];
}

#undef LOCTEXT_NAMESPACE
