#include "SBangoYapBlockHeader.h"

#include "BangoNodeDecorator.h"
#include "BangoYapEditorStyle.h"
#include "BangoYapLineTiming.h"
#include "SBangoYapLineSettings.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "YapBase/YapSpeechConfig.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

using namespace BangoYapEditor;

namespace BangoYapBlockHeaderPrivate
{
	// Speech's RunMode values (EBangoYapRunMode).
	constexpr int32 RunAll = 1;
	constexpr int32 SelectOne = 2;
	constexpr int32 SelectRandom = 3;

	// As Yap's style: the node header (Bold 15), the group label (Normal 15), the sequencing text (Italic 9).
	static FSlateFontInfo HeaderFont() { return FCoreStyle::GetDefaultFontStyle("Bold", 15); }
	static FSlateFontInfo LabelFont() { return FCoreStyle::GetDefaultFontStyle("Regular", 15); }
	static FSlateFontInfo SequencingFont() { return FCoreStyle::GetDefaultFontStyle("Italic", 9); }

	/** The run mode as it'll run: the speech's, or run all (an expression: unknown, 0). */
	static int32 GetRunMode(const FSettingValue& Value)
	{
		switch (Value.Kind)
		{
			case FSettingValue::EKind::Value: return Value.Value;
			case FSettingValue::EKind::Default: return RunAll;
			default: return 0;
		}
	}

	/** Yap's sequencing button: its icon, colour, label and tooltip for each mode. */
	static const FSlateBrush* SequencingBrush(int32 Mode)
	{
		switch (Mode)
		{
			case RunAll: return FAppStyle::Get().GetBrush("Icons.SortDown");
			case SelectOne: return FAppStyle::Get().GetBrush("LevelEditor.Profile");
			case SelectRandom: return FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Icon_Random);
			default: return FAppStyle::Get().GetBrush("Icons.Help");
		}
	}

	static FLinearColor SequencingColor(int32 Mode)
	{
		switch (Mode)
		{
			case RunAll: return BangoYapColor::LightBlue;
			case SelectOne: return BangoYapColor::LightOrange;
			case SelectRandom: return BangoYapColor::LightCyan;
			default: return BangoYapColor::LightYellow;
		}
	}

	static FText SequencingLabel(int32 Mode)
	{
		switch (Mode)
		{
			case RunAll: return LOCTEXT("Sequencing_RunAll", "Run All");
			case SelectOne: return LOCTEXT("Sequencing_SelectOne", "Select One");
			case SelectRandom: return LOCTEXT("Sequencing_SelectRandom", "Select Random");
			default: return FText::GetEmpty();
		}
	}

	static FText SequencingToolTip(int32 Mode)
	{
		switch (Mode)
		{
			case RunAll: return LOCTEXT("Sequencing_RunAll_ToolTip", "Starting from the top, run every line reached");
			case SelectOne: return LOCTEXT("Sequencing_SelectOne_ToolTip", "Starting from the top, run the first line reached, and only that one");
			case SelectRandom: return LOCTEXT("Sequencing_SelectRandom_ToolTip", "Run one of the lines reached, chosen at random");
			default: return FText::GetEmpty();
		}
	}
}

// ================================================================================================ Data

TOptional<EBangoYapBlockKind> FBangoYapBlockData::GetKind(const FString& CommandName)
{
	if (CommandName == TEXT("Speech"))
	{
		return EBangoYapBlockKind::Speech;
	}
	if (CommandName == TEXT("Prompt"))
	{
		return EBangoYapBlockKind::Prompt;
	}
	if (CommandName == TEXT("Conversation"))
	{
		return EBangoYapBlockKind::Conversation;
	}
	return TOptional<EBangoYapBlockKind>();
}

const TCHAR* FBangoYapBlockData::GetNameLabel() const
{
	return Kind == EBangoYapBlockKind::Conversation ? TEXT("Name") : TEXT("Config");
}

FBangoYapBlockData FBangoYapBlockData::FromContext(const FBangoNodeDecoratorContext& Context)
{
	FBangoYapBlockData Data;
	Data.Kind = GetKind(Context.CommandName).Get(EBangoYapBlockKind::Speech);
	Data.bDisabled = Context.bDisabled;

	if (!Context.Layout)
	{
		return Data;
	}
	const FBangoNodeLayout& Layout = *Context.Layout;
	Data.Writer = FFieldWriter(Layout, Context.SetFieldValue);

	if (Data.Kind == EBangoYapBlockKind::Speech)
	{
		Data.RunMode = ReadSetting(Layout, ELineSetting::RunMode);
		Data.Interruptible = ReadSetting(Layout, ELineSetting::Interruptible);
		Data.Advance = ReadSetting(Layout, ELineSetting::Advance);
	}

	const FString NameText = GetFieldText(Layout, Data.GetNameLabel());
	if (!NameText.IsEmpty() && NameText != TEXT("nil"))
	{
		Data.bNameIsLiteral = ParseStringLiteral(NameText, Data.Name);
		if (!Data.bNameIsLiteral)
		{
			Data.Name = NameText;
		}
	}
	return Data;
}

bool FBangoYapBlockData::HeaderCoversField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex)
{
	if (!Context.Layout || !Context.Layout->Fields.IsValidIndex(FieldIndex) || !Context.SetFieldValue)
	{
		return false;
	}

	const FBangoNodeLayout& Layout = *Context.Layout;
	const FString& Label = Layout.Fields[FieldIndex].Label;
	const TOptional<EBangoYapBlockKind> Kind = GetKind(Context.CommandName);
	const bool bConversation = Kind.IsSet() && Kind.GetValue() == EBangoYapBlockKind::Conversation;

	if (Kind.IsSet() && Kind.GetValue() == EBangoYapBlockKind::Speech)
	{
		if (Label == TEXT("RunMode"))
		{
			return ReadSetting(Layout, ELineSetting::RunMode).Kind != FSettingValue::EKind::Expression;
		}
		if (Label == TEXT("Interruptible"))
		{
			return ReadSetting(Layout, ELineSetting::Interruptible).Kind != FSettingValue::EKind::Expression;
		}
		if (Label == TEXT("Advance"))
		{
			return ReadSetting(Layout, ELineSetting::Advance).Kind != FSettingValue::EKind::Expression;
		}
	}

	const TCHAR* NameLabel = bConversation ? TEXT("Name") : TEXT("Config");
	if (Label == NameLabel)
	{
		const FString Text = GetFieldText(Layout, NameLabel);
		FString Unused;
		return Text.IsEmpty() || Text == TEXT("nil") || ParseStringLiteral(Text, Unused);
	}
	return false;
}

// ================================================================================================ Header

void SBangoYapBlockHeader::Construct(const FArguments& InArgs)
{
	Data = InArgs._Data;

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 0)
		[
			MakeKindLabel()
		];

	if (Data.Kind == EBangoYapBlockKind::Speech)
	{
		Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 0)
		[
			MakeSequencingButton()
		];
	}

	// As Yap's group label: on the right, grey.
	Row->AddSlot()
	.FillWidth(1.0f)
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	.Padding(8, 0, Data.Kind == EBangoYapBlockKind::Speech ? 6 : 2, 0)
	[
		Data.Kind == EBangoYapBlockKind::Conversation ? MakeNameBox() : MakeConfigButton()
	];

	if (Data.Kind == EBangoYapBlockKind::Speech)
	{
		Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBangoYapProgressionSettings)
			.Interruptible(Data.Interruptible)
			.Advance(Data.Advance)
			.bSpeech(true)
			.bEditable(Data.Writer.CanEdit())
			.OnSet_Lambda([this](ELineSetting Setting, int32 Value)
			{
				SetFields({ { GetSettingLabel(Setting), MakeSettingScript(Setting, Value) } });
			})
		];
	}

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(HeaderWidth)
		.Padding(0, 2, 0, 4)
		.RenderOpacity(Data.bDisabled ? 0.4f : 1.0f)
		[
			Row
		]
	];
}

bool SBangoYapBlockHeader::SetFields(const TArray<FFieldChange>& Changes)
{
	// Kept alive in case setting a field draws the node again.
	TSharedRef<SWidget> Self = AsShared();
	return Data.Writer.SetFields(Changes);
}

TSharedRef<SWidget> SBangoYapBlockHeader::MakeKindLabel()
{
	// Yap's node header: its talk / prompt mode title. (Yap's is a button that switches the node between them; here
	// they're different commands.)
	FText Label;
	FText HeaderToolTip;
	switch (Data.Kind)
	{
		case EBangoYapBlockKind::Speech:
			Label = LOCTEXT("Header_Talk", "Talk");
			HeaderToolTip = LOCTEXT("Header_Talk_ToolTip", "A speech: the lines in its block, said one after another (run all) or one of them. In a Conversation, part of it; otherwise free speech.");
			break;
		case EBangoYapBlockKind::Prompt:
			Label = LOCTEXT("Header_Prompt", "Prompt");
			HeaderToolTip = LOCTEXT("Header_Prompt_ToolTip", "A player prompt: the AddPrompt entries reached in its block are the player's choices. Must be in a Conversation.");
			break;
		default:
			Label = LOCTEXT("Header_Conversation", "Conversation");
			HeaderToolTip = LOCTEXT("Header_Conversation_ToolTip", "A conversation: the speech and prompts run in its block are part of it. Waits while another conversation is open.");
			break;
	}

	return SNew(STextBlock)
		.Text(Label)
		.ToolTipText(HeaderToolTip)
		.Font(BangoYapBlockHeaderPrivate::HeaderFont())
		.ColorAndOpacity(BangoYapColor::White)
		.SimpleTextMode(true);
}

TSharedRef<SWidget> SBangoYapBlockHeader::MakeSequencingButton()
{
	using namespace BangoYapBlockHeaderPrivate;

	const int32 Mode = GetRunMode(Data.RunMode);
	const bool bExpression = Data.RunMode.Kind == FSettingValue::EKind::Expression;

	FText Label = bExpression ? FText::FromString(Data.RunMode.Text) : SequencingLabel(Mode);
	FText HeaderToolTip = bExpression
		? FText::Format(LOCTEXT("Sequencing_Expression", "Run mode: {0} (only known while playing; change it in the node's RunMode field)"), FText::FromString(Data.RunMode.Text))
		: FText::Format(LOCTEXT("Sequencing_ToolTip", "{0} ({1})\nClick for the next run mode."), SequencingToolTip(Mode),
			FText::FromString(Data.RunMode.IsDefault() ? FString(TEXT("the default")) : MakeSettingScript(ELineSetting::RunMode, Mode)));

	// As Yap's: 120 wide, the icon then the italic label, both in the mode's colour.
	return SNew(SBox)
		.WidthOverride(120)
		[
			SNew(SButton)
			.Cursor(EMouseCursor::Default)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(2, 1, 2, 1))
			.HAlign(HAlign_Left)
			.IsEnabled(Data.Writer.CanEdit() && !bExpression)
			.ToolTipText(HeaderToolTip)
			.OnClicked_Lambda([this, Mode]()
			{
				// Run all is the default, so it's written as nil.
				const int32 Next = Mode % 3 + 1;
				SetFields({ { TEXT("RunMode"), Next == RunAll ? FString(TEXT("nil")) : MakeSettingScript(ELineSetting::RunMode, Next) } });
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 4, 0)
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(16, 16))
					.Image(SequencingBrush(Mode))
					.ColorAndOpacity(SequencingColor(Mode))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(SequencingFont())
					.ColorAndOpacity(SequencingColor(Mode))
					.SimpleTextMode(true)
				]
			]
		];
}

// ------------------------------------------------------------------------------------------------ Config

TSharedRef<SWidget> SBangoYapBlockHeader::MakeConfigButton()
{
	using namespace BangoYapBlockHeaderPrivate;

	const bool bSet = !Data.Name.IsEmpty();
	const FName DefaultName = GetDefaultSpeechConfigName();

	FText Label;
	FText HeaderToolTip;
	FLinearColor Color = BangoYapColor::LightGray;

	if (!bSet)
	{
		// The default config, a little dimmer.
		Label = FText::FromName(DefaultName);
		HeaderToolTip = FText::Format(LOCTEXT("Config_Default", "Speech config: the project's default ({0})"), FText::FromName(DefaultName));
		Color.A = 0.6f;
	}
	else if (!Data.bNameIsLiteral)
	{
		Label = FText::FromString(Data.Name);
		HeaderToolTip = FText::Format(LOCTEXT("Config_Expression", "Speech config: {0} (only known while playing; change it in the node's Config field)"), FText::FromString(Data.Name));
		Color = BangoYapColor::LightYellow;
	}
	else
	{
		Label = FText::FromString(Data.Name);
		const bool bFound = FindSpeechConfig(FName(*Data.Name)) != nullptr;
		HeaderToolTip = bFound
			? FText::Format(LOCTEXT("Config_Set", "Speech config: {0}"), Label)
			: FText::Format(LOCTEXT("Config_NotFound", "Speech config: {0} (there's no speech config with this name)"), Label);
		if (!bFound)
		{
			Color = BangoYapColor::Red;
		}
	}

	TSharedRef<SWidget> Text = SNew(STextBlock)
		.Text(Label)
		.Font(LabelFont())
		.ColorAndOpacity(Color)
		.SimpleTextMode(true);

	if (!Data.Writer.CanEdit() || !Data.bNameIsLiteral)
	{
		return SNew(SBox).ToolTipText(HeaderToolTip)[Text];
	}

	return SAssignNew(ConfigCombo, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(4, 0))
		.MenuPlacement(MenuPlacement_BelowRightAnchor)
		.ToolTipText(FText::Format(LOCTEXT("Config_ToolTip_Edit", "{0}\nClick to choose another."), HeaderToolTip))
		.OnGetMenuContent(this, &SBangoYapBlockHeader::MakeConfigMenu)
		.ButtonContent()
		[
			Text
		];
}

TSharedRef<SWidget> SBangoYapBlockHeader::MakeConfigMenu()
{
	FMenuBuilder Menu(true, nullptr);

	const FString Current = Data.Name;
	const FName DefaultName = GetDefaultSpeechConfigName();

	Menu.AddMenuEntry(
		FText::Format(LOCTEXT("ConfigMenu_Default", "The project's default ({0})"), FText::FromName(DefaultName)),
		LOCTEXT("ConfigMenu_Default_ToolTip", "No config given (nil): the one in Project Settings > Plugins > Yap > Default Speech Config"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { SetFields({ { Data.GetNameLabel(), FString(TEXT("nil")) } }); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([Current]() { return Current.IsEmpty(); })),
		NAME_None,
		EUserInterfaceActionType::RadioButton);

	Menu.BeginSection(NAME_None, LOCTEXT("ConfigMenu_Configs", "Speech Configs"));
	for (const UYapSpeechConfig* Config : GetAllSpeechConfigs())
	{
		const FString ConfigName = Config->GetConfigName().ToString();
		Menu.AddMenuEntry(
			FText::FromString(ConfigName),
			FText::Format(LOCTEXT("ConfigMenu_Config_ToolTip", "{0} ({1})"), FText::FromString(ConfigName), FText::FromString(Config->GetPathName())),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, ConfigName]() { SetFields({ { Data.GetNameLabel(), MakeStringLiteral(ConfigName) } }); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Current, ConfigName]() { return Current == ConfigName; })),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
	Menu.EndSection();

	return Menu.MakeWidget();
}

// ------------------------------------------------------------------------------------------------ Name

TSharedRef<SWidget> SBangoYapBlockHeader::MakeNameBox()
{
	using namespace BangoYapBlockHeaderPrivate;

	if (!Data.Writer.CanEdit() || !Data.bNameIsLiteral)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Data.Name))
			.ToolTipText(Data.bNameIsLiteral
				? LOCTEXT("Name_ToolTip", "The conversation's name, for the game")
				: FText::Format(LOCTEXT("Name_Expression", "The conversation's name: {0} (only known while playing; change it in the node's Name field)"), FText::FromString(Data.Name)))
			.Font(LabelFont())
			.ColorAndOpacity(Data.bNameIsLiteral ? BangoYapColor::LightGray : BangoYapColor::LightYellow);
	}

	return SNew(SEditableText)
		.Text(FText::FromString(Data.Name))
		.HintText(LOCTEXT("Name_Hint", "No name"))
		.ToolTipText(LOCTEXT("Name_ToolTip_Edit", "The conversation's name, for the game (its events have it). Click to type it; empty for none."))
		.Font(LabelFont())
		.ColorAndOpacity(BangoYapColor::LightGray)
		.Justification(ETextJustify::Right)
		.SelectAllTextWhenFocused(true)
		.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType)
		{
			if (CommitType == ETextCommit::OnCleared)
			{
				return;
			}
			const FString NewName = NewText.ToString().TrimStartAndEnd();
			if (NewName == Data.Name)
			{
				return;
			}
			SetFields({ { TEXT("Name"), NewName.IsEmpty() ? FString(TEXT("nil")) : MakeStringLiteral(NewName) } });
		});
}

#undef LOCTEXT_NAMESPACE
