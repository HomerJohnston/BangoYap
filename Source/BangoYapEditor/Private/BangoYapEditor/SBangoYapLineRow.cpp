#include "SBangoYapLineRow.h"

#include "BangoNodeDecorator.h"
#include "BangoYapEditorStyle.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Images/SImage.h"
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

	Data.Target = ParseSpeakerTarget(GetFieldText(Layout, TEXT("Target")));
	Data.Text = ReadTextField(Layout, Data.bMature ? TEXT("MatureText") : TEXT("Text"));
	if (Data.bMature)
	{
		Data.ChildSafeText = ReadTextField(Layout, TEXT("ChildSafeText"));
	}
	if (Data.bPrompt)
	{
		Data.TitleText = ReadTextField(Layout, TEXT("TitleText"));
	}

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
		.ToolTipText(this, &SBangoYapPortrait::GetToolTip)
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

FText SBangoYapPortrait::GetToolTip() const
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

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(RowWidth)
		.RenderOpacity(Data.bDisabled ? 0.4f : 1.0f)
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
					MakeMoodColumn()
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			[
				SNew(SBangoYapPortrait)
				.Character(Data.Target.Speaker)
				.Mood(Data.bMoodIsLiteral ? Data.Mood : FString())
				.PortraitSize(PortraitSize)
				.BorderSize(PortraitBorder)
				.bPrompt(Data.bPrompt)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4, 0, 0, 0)
			[
				SNew(SBox)
				.HeightOverride(RowHeight)
				[
					MakeTextColumn()
				]
			]
		]
	];
}

TSharedRef<SWidget> SBangoYapLineRow::MakeMoodColumn()
{
	const bool bHasMood = !Data.Mood.IsEmpty();
	const FString IconName = Data.bMoodIsLiteral ? Data.Mood : TEXT("Unknown");

	FText ToolTip;
	if (!bHasMood)
	{
		ToolTip = LOCTEXT("Mood_Default", "Default (no mood)");
	}
	else if (!Data.bMoodIsLiteral)
	{
		ToolTip = FText::Format(LOCTEXT("Mood_Expression", "Mood: {0} (only known while playing)"), FText::FromString(Data.Mood));
	}
	else if (!FindMoodTag(Data.Mood).IsValid())
	{
		ToolTip = FText::Format(LOCTEXT("Mood_Unknown", "Mood: {0} (not a mood under any speech config's mood root)"), FText::FromString(Data.Mood));
	}
	else
	{
		ToolTip = FText::Format(LOCTEXT("Mood_Known", "Mood: {0}"), FText::FromString(Data.Mood));
	}

	return SNew(SBox)
		.WidthOverride(22)
		.HeightOverride(22)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ToolTipText(ToolTip)
		[
			SNew(SImage)
			.Image(FBangoYapEditorStyle::GetMoodBrush(bHasMood ? IconName : FString()))
			.DesiredSizeOverride(FVector2D(16, 16))
			.ColorAndOpacity(bHasMood ? BangoYapColor::White_Trans : BangoYapColor::LightGray)
		];
}

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

	return Column;
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
	if (Value.bPresent)
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
		.BorderBackgroundColor(BangoYapColor::DarkGray_SemiGlass)
		.ToolTipText(Data.bMature ? LOCTEXT("DialogueText_ShiftForChildSafe", "Hold Shift to show the child-safe text") : FText::GetEmpty())
		.Padding(0)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.Padding(4, 4, 4, 2)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(Data.Text.bLiteral ? FBangoYapEditorStyle::DialogueFont() : FBangoYapEditorStyle::TitleFont())
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
	const bool bHas = Data.TitleText.bPresent;

	return SNew(SBorder)
		.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite))
		.BorderBackgroundColor(BangoYapColor::DarkGray_SemiGlass)
		.ToolTipText(LOCTEXT("TitleText_ToolTip", "Title text: what the player sees in the list of choices (without one, the line's text)"))
		.Padding(0)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.Padding(4, 0)
			[
				SNew(STextBlock)
				.Font(FBangoYapEditorStyle::TitleFont())
				.Text(FText::FromString(Data.TitleText.Source))
				.ColorAndOpacity(Data.TitleText.bLiteral ? BangoYapColor::YellowGray : BangoYapColor::LightYellow)
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

TSharedRef<SWidget> SBangoYapLineRow::MakeDirectedAt()
{
	if (!Data.Target.DirectedAt.IsSet())
	{
		return SNullWidget::NullWidget;
	}

	const int32 Size = PortraitSize / 3;
	const UYapCharacterAsset* Asset = FindCharacter(Data.Target.DirectedAt);

	return SNew(SBorder)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_FilledCircle))
		.BorderBackgroundColor(Asset ? Asset->GetCharacterColor() : BangoYapColor::Gray)
		.Padding(3)
		[
			SNew(SBox)
			.WidthOverride(Size + 3)
			.HeightOverride(Size + 3)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBangoYapPortrait)
				.Character(Data.Target.DirectedAt)
				.PortraitSize(Size)
				.bBorder(false)
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

	return SNew(SBox)
		.MinDesiredWidth(28)
		.MinDesiredHeight(20)
		.ToolTipText(FText::Format(LOCTEXT("LineID_ToolTip", "Line ID {0}: the text's key, and the ID its audio is found by"), FText::FromString(Data.Text.Key)))
		[
			SNew(SBorder)
			.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_IDTag))
			.BorderBackgroundColor(BangoYapColor::DarkGray)
			.Padding(5, 3, 5, 3)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(BangoYapColor::LightGray)
				.Text(FText::FromString(Data.Text.Key))
			]
		];
}

#undef LOCTEXT_NAMESPACE
