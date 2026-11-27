#include "SBangoYapPickers.h"

#include "BangoYapEditorStyle.h"
#include "GameplayTagsManager.h"
#include "SBangoYapLineRow.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "YapBase/YapCharacterAsset.h"
#include "YapBase/YapMoodTags.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

using namespace BangoYapEditor;

// ================================================================================================ Characters

void SBangoYapCharacterPicker::Construct(const FArguments& InArgs)
{
	Current = InArgs._Current;
	CurrentChoice = InArgs._CurrentChoice;
	bDirectedAt = InArgs._bDirectedAt;
	OnPicked = InArgs._OnPicked;
	OnScriptCommitted = InArgs._OnScriptCommitted;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot()
	.AutoHeight()
	.Padding(4)
	[
		SAssignNew(SearchBox, SSearchBox)
		.HintText(LOCTEXT("Picker_Search", "Search characters"))
		.OnTextChanged_Lambda([this](const FText& Text)
		{
			Filter = Text.ToString();
			RefreshList();
		})
	];

	if (bDirectedAt)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(4, 0)
		[
			MakeChoiceRow(EChoice::Focus, LOCTEXT("Picker_Focus", "Their speech focus"),
				LOCTEXT("Picker_Focus_ToolTip", "Said to whoever the speaker is focused on (SetSpeechFocus), or to no one if they have no focus. Writes the speaker on its own."))
		];
		Box->AddSlot()
		.AutoHeight()
		.Padding(4, 0, 4, 4)
		[
			MakeChoiceRow(EChoice::NoOne, LOCTEXT("Picker_NoOne", "No one"),
				LOCTEXT("Picker_NoOne_ToolTip", "Said to no one, whatever the speaker's focus. Writes [speaker]."))
		];
	}

	Box->AddSlot()
	.AutoHeight()
	.Padding(4, 0)
	[
		SNew(SBox)
		.MaxDesiredHeight(320)
		[
			SAssignNew(List, SScrollBox)
		]
	];

	if (!InArgs._ScriptText.IsEmpty() || !bDirectedAt)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(4, 6, 4, 0)
		[
			SNew(SSeparator)
		];
		Box->AddSlot()
		.AutoHeight()
		.Padding(4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 6, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Picker_Script", "As script"))
				.ToolTipText(LOCTEXT("Picker_Script_ToolTip", "The target as it's written in the script: an ID (\"HoJo\"), an actor (@elf), a variable, or [speaker, directedAt]. Enter sets it."))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(InArgs._ScriptText))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type Commit)
				{
					if (Commit == ETextCommit::OnEnter)
					{
						OnScriptCommitted.ExecuteIfBound(Text.ToString());
					}
				})
			]
		];
	}

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(280)
		[
			Box
		]
	];

	RefreshList();
}

TSharedPtr<SWidget> SBangoYapCharacterPicker::GetFocusWidget() const
{
	return SearchBox;
}

void SBangoYapCharacterPicker::RefreshList()
{
	List->ClearChildren();

	int32 Shown = 0;
	for (UYapCharacterAsset* Character : GetAllCharacters())
	{
		const FString Name = Character->GetCharacterName().ToString();
		const FString ID = Character->GetCharacterID().ToString();
		if (!Filter.IsEmpty() && !Name.Contains(Filter) && !ID.Contains(Filter))
		{
			continue;
		}
		List->AddSlot()
		[
			MakeCharacterRow(Character)
		];
		++Shown;
	}

	if (Shown == 0)
	{
		List->AddSlot()
		.Padding(6)
		[
			SNew(STextBlock)
			.Text(Filter.IsEmpty() ? LOCTEXT("Picker_NoCharacters", "No Yap characters in the project (Add > Yap > Yap Character).") : LOCTEXT("Picker_NoMatch", "No characters match."))
			.ColorAndOpacity(BangoYapColor::LightGray)
			.AutoWrapText(true)
		];
	}
}

TSharedRef<SWidget> SBangoYapCharacterPicker::MakeCharacterRow(UYapCharacterAsset* Character)
{
	const FName ID = Character->GetCharacterID();
	const bool bSelected = Current.Kind == ECharacterRefKind::ID && FName(*Current.Value) == ID;

	FCharacterRef Ref;
	Ref.Kind = ECharacterRefKind::ID;
	Ref.Value = ID.ToString();

	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(2))
		.OnClicked_Lambda([this, Ref]()
		{
			OnPicked.ExecuteIfBound(EChoice::Character, Ref);
			return FReply::Handled();
		})
		[
			SNew(SBorder)
			.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite_Rounded))
			.BorderBackgroundColor(bSelected ? BangoYapColor::Gray : FLinearColor::Transparent)
			.Padding(3)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite_Rounded))
					.BorderBackgroundColor(Character->GetCharacterColor())
					.Padding(2)
					[
						SNew(SBangoYapPortrait)
						.Character(Ref)
						.PortraitSize(32)
						.bBorder(false)
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(8, 0, 0, 0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(Character->GetCharacterName().IsEmpty() ? FText::FromName(ID) : Character->GetCharacterName())
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromName(ID))
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
						.ColorAndOpacity(BangoYapColor::LightGray)
					]
				]
			]
		];
}

TSharedRef<SWidget> SBangoYapCharacterPicker::MakeChoiceRow(EChoice Choice, const FText& Label, const FText& InToolTip)
{
	const bool bSelected = !Current.IsSet() && CurrentChoice == Choice;

	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(2))
		.ToolTipText(InToolTip)
		.OnClicked_Lambda([this, Choice]()
		{
			OnPicked.ExecuteIfBound(Choice, FCharacterRef());
			return FReply::Handled();
		})
		[
			SNew(SBorder)
			.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite_Rounded))
			.BorderBackgroundColor(bSelected ? BangoYapColor::Gray : FLinearColor::Transparent)
			.Padding(FMargin(8, 4))
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
			]
		];
}

// ================================================================================================ Moods

void SBangoYapMoodPicker::Construct(const FArguments& InArgs)
{
	Current = InArgs._Current;
	OnPicked = InArgs._OnPicked;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	// Default: no mood.
	Box->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	.Padding(4)
	[
		MakeMoodButton(FString(), LOCTEXT("Mood_Default_ToolTip", "Default (no mood)"))
	];

	const FGameplayTagContainer Roots = Yap::GetMoodTagRoots();
	for (const FGameplayTag& Root : Roots)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(6, 4, 6, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromName(Root.GetTagName()))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
			.ColorAndOpacity(BangoYapColor::LightGray)
		];

		TSharedRef<SUniformWrapPanel> Grid = SNew(SUniformWrapPanel).NumColumnsOverride(4);
		for (const FGameplayTag& MoodTag : Yap::GetAllMoodTagsUnder(Root))
		{
			const TSharedPtr<FGameplayTagNode> Node = UGameplayTagsManager::Get().FindTagNode(MoodTag);
			const FString Leaf = Node.IsValid() ? Node->GetSimpleTagName().ToString() : MoodTag.ToString();
			Grid->AddSlot()
			[
				MakeMoodButton(Leaf, FText::FromString(Leaf))
			];
		}

		Box->AddSlot()
		.AutoHeight()
		.Padding(4)
		[
			Grid
		];
	}

	if (Roots.Num() == 0)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(6)
		[
			SNew(SBox)
			.WidthOverride(200)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT("Mood_NoRoots", "No moods: set a Mood Tags Root in a Yap Speech Config, with a tag under it for each mood."))
				.ColorAndOpacity(BangoYapColor::LightGray)
			]
		];
	}

	ChildSlot
	[
		Box
	];
}

TSharedRef<SWidget> SBangoYapMoodPicker::MakeMoodButton(const FString& Mood, const FText& InToolTip)
{
	const bool bSelected = Mood == Current;

	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ToolTipText(InToolTip)
		.ContentPadding(FMargin(2))
		.OnClicked_Lambda([this, Mood]()
		{
			OnPicked.ExecuteIfBound(Mood);
			return FReply::Handled();
		})
		[
			SNew(SBorder)
			.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite_Rounded))
			.BorderBackgroundColor(bSelected ? BangoYapColor::Gray : FLinearColor::Transparent)
			.Padding(4)
			[
				SNew(SImage)
				.Image(FBangoYapEditorStyle::GetMoodBrush(Mood))
				.DesiredSizeOverride(FVector2D(20, 20))
				.ColorAndOpacity(Mood.IsEmpty() ? BangoYapColor::LightGray : BangoYapColor::White_Trans)
			]
		];
}

#undef LOCTEXT_NAMESPACE
