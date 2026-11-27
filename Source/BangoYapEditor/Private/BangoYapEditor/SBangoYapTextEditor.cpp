#include "SBangoYapTextEditor.h"

#include "BangoYapEditorStyle.h"
#include "SBangoYapLineSettings.h"
#include "Widgets/Layout/SSeparator.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

using namespace BangoYapEditor;

namespace BangoYapTextEditorPrivate
{
	static TSharedRef<SWidget> Label(const FText& Text, const FText& ToolTip = FText::GetEmpty())
	{
		return SNew(STextBlock)
			.Text(Text)
			.ToolTipText(ToolTip)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.ColorAndOpacity(BangoYapColor::LightGray);
	}

	static FText ExpressionNote(const FTextValue& Value)
	{
		return (Value.bPresent && !Value.bLiteral)
			? FText::Format(LOCTEXT("TextEditor_Expression", "Written as an expression ({0}): applying a change here replaces it with a text."), FText::FromString(Value.Source))
			: FText::GetEmpty();
	}
}

void SBangoYapTextEditor::Construct(const FArguments& InArgs)
{
	using namespace BangoYapTextEditorPrivate;

	Text = InArgs._Text;
	ChildSafeText = InArgs._ChildSafeText;
	TitleText = InArgs._TitleText;
	bMature = InArgs._bMature;
	bPrompt = InArgs._bPrompt;
	OnApply = InArgs._OnApply;
	OnCancel = InArgs._OnCancel;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	// The text
	Box->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
	[
		Label(bMature ? LOCTEXT("TextEditor_Mature", "Mature text") : LOCTEXT("TextEditor_Text", "Text"))
	];
	Box->AddSlot().AutoHeight()
	[
		SNew(SBox)
		.MinDesiredHeight(60)
		[
			SAssignNew(TextBox, SMultiLineEditableTextBox)
			.Text(FText::FromString(Text.Source))
			.Font(FBangoYapEditorStyle::DialogueFont())
			.AutoWrapText(true)
		]
	];
	if (!ExpressionNote(Text).IsEmpty())
	{
		Box->AddSlot().AutoHeight().Padding(0, 2, 0, 0)
		[
			SNew(STextBlock).Text(ExpressionNote(Text)).AutoWrapText(true).ColorAndOpacity(BangoYapColor::LightYellow).Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
		];
	}

	// The line ID
	Box->AddSlot().AutoHeight().Padding(0, 8, 0, 2)
	[
		Label(LOCTEXT("TextEditor_LineID", "Line ID"), LOCTEXT("TextEditor_LineID_ToolTip", "The text's key: the line's ID, which its audio is found by. Empty: a line with no ID or audio (TEXT_INV)."))
	];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(IDBox, SEditableTextBox)
			.Text(FText::FromString(Text.Key))
			.HintText(LOCTEXT("TextEditor_NoID", "No ID"))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0, 0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("TextEditor_NewID", "New ID"))
			.ToolTipText(LOCTEXT("TextEditor_NewID_ToolTip", "A new ID in the project's form (Project Settings > Plugins > Yap > Line ID Format). Its digits are random: check it doesn't clash."))
			.OnClicked_Lambda([this]()
			{
				IDBox->SetText(FText::FromString(MakeLineID()));
				return FReply::Handled();
			})
		]
	];

	// Child-safe text
	if (bMature)
	{
		Box->AddSlot().AutoHeight().Padding(0, 8, 0, 2)
		[
			Label(LOCTEXT("TextEditor_ChildSafe", "Child-safe text"), LOCTEXT("TextEditor_ChildSafe_ToolTip", "What child-safe players see and hear. Its key is the line ID with the project's child-safe suffix."))
		];
		Box->AddSlot().AutoHeight()
		[
			SNew(SBox)
			.MinDesiredHeight(60)
			[
				SAssignNew(ChildSafeBox, SMultiLineEditableTextBox)
				.Text(FText::FromString(ChildSafeText.Source))
				.Font(FBangoYapEditorStyle::DialogueFont())
				.AutoWrapText(true)
				.ForegroundColor(BangoYapColor::LightBlue)
			]
		];
	}

	// Title text
	if (bPrompt)
	{
		Box->AddSlot().AutoHeight().Padding(0, 8, 0, 2)
		[
			Label(LOCTEXT("TextEditor_Title", "Title text"), LOCTEXT("TextEditor_Title_ToolTip", "What the player sees in the list of choices. Empty: the line's text. Its key is the line ID with _Title."))
		];
		Box->AddSlot().AutoHeight()
		[
			SAssignNew(TitleBox, SEditableTextBox)
			.Text(FText::FromString(TitleText.Source))
			.Font(FBangoYapEditorStyle::TitleFont())
			.HintText(LOCTEXT("TextEditor_Title_None", "Title Text (None)"))
		];
	}

	// Time settings, under a line as in Yap's editor
	Box->AddSlot().AutoHeight().Padding(0, 10, 0, 4)
	[
		SNew(SSeparator).Thickness(2.0f)
	];
	Box->AddSlot().AutoHeight()
	[
		SAssignNew(TimeSettings, SBangoYapTimeSettings)
		.TimeMode(InArgs._TimeMode)
		.PaddingSetting(InArgs._PaddingSetting)
	];

	// Buttons
	Box->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0, 10, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 4, 0)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
			.Text(LOCTEXT("TextEditor_Apply", "Apply"))
			.ToolTipText(LOCTEXT("TextEditor_Apply_ToolTip", "Write the changes into the script (Ctrl+Enter)"))
			.OnClicked_Lambda([this]() { Apply(); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("TextEditor_Cancel", "Cancel"))
			.OnClicked_Lambda([this]() { OnCancel.ExecuteIfBound(); return FReply::Handled(); })
		]
	];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(10)
		[
			SNew(SBox)
			.WidthOverride(460)
			[
				Box
			]
		]
	];
}

TSharedPtr<SWidget> SBangoYapTextEditor::GetFocusWidget() const
{
	return TextBox;
}

FReply SBangoYapTextEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter && InKeyEvent.IsControlDown())
	{
		Apply();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCancel.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FString SBangoYapTextEditor::MakeLiteral(const FTextValue& Original, const FString& Source, const FString& Key)
{
	FBangoTextLiteral Literal = Original.bLiteral ? Original.Literal : FBangoTextLiteral();
	Literal.Source = Source;
	Literal.bInvariant = Key.IsEmpty();
	Literal.Key = Key;
	return Bango::MakeTextLiteral(Literal);
}

void SBangoYapTextEditor::Apply()
{
	FResult Result;

	const FString NewID = IDBox->GetText().ToString().TrimStartAndEnd();
	const bool bIDChanged = NewID != Text.Key;

	const FString NewText = TextBox->GetText().ToString();
	if (bIDChanged || NewText != Text.Source || !Text.bLiteral)
	{
		Result.Text = MakeLiteral(Text, NewText, NewID);
	}

	if (bMature && ChildSafeBox.IsValid())
	{
		const FString NewChildSafe = ChildSafeBox->GetText().ToString();
		if (bIDChanged || NewChildSafe != ChildSafeText.Source || !ChildSafeText.bLiteral)
		{
			Result.ChildSafeText = MakeLiteral(ChildSafeText, NewChildSafe, MakeChildSafeKey(NewID));
		}
	}

	if (bPrompt && TitleBox.IsValid())
	{
		const FString NewTitle = TitleBox->GetText().ToString();
		const bool bTitleChanged = NewTitle != TitleText.Source || (TitleText.bPresent && !TitleText.bLiteral);
		if (bTitleChanged || (bIDChanged && TitleText.bPresent))
		{
			// An empty title clears the field: the line's own text is shown.
			Result.TitleText = NewTitle.IsEmpty() ? FString(TEXT("nil")) : MakeLiteral(TitleText, NewTitle, NewID.IsEmpty() ? FString() : NewID + TEXT("_Title"));
		}
	}

	if (TimeSettings.IsValid())
	{
		Result.TimeMode = TimeSettings->GetTimeModeScript();
		Result.Padding = TimeSettings->GetPaddingScript();
	}

	OnApply.ExecuteIfBound(Result);
}

#undef LOCTEXT_NAMESPACE
