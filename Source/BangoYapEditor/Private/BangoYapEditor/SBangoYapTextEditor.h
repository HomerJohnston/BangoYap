#pragma once

#include "CoreMinimal.h"
#include "BangoYapNodeFields.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class SBangoYapTimeSettings;

/**
 * The popup that edits a line's texts, like Yap's dialogue editor (SYapDialogueEditor): its text, its line ID (the
 * text's key: empty for a line with no ID, TEXT_INV), and for SayM the child-safe text, for AddPrompt the title
 * text. Apply (or Ctrl+Enter) writes them back as text literals, keeping their namespaces and notes; the child-safe
 * and title texts' keys follow the line ID (with the child-safe suffix, and _Title).
 *
 * Below them, as in Yap's editor, the line's time settings (SBangoYapTimeSettings): its time mode and padding.
 */
class SBangoYapTextEditor : public SCompoundWidget
{
public:
	struct FResult
	{
		/** Script text for each field that changed (empty: unchanged). */
		FString Text;
		FString ChildSafeText;
		FString TitleText;
		FString TimeMode;
		FString Padding;
	};

	DECLARE_DELEGATE_OneParam(FOnApply, const FResult& /*Result*/);

	SLATE_BEGIN_ARGS(SBangoYapTextEditor)
		: _bMature(false)
		, _bPrompt(false)
	{}
		SLATE_ARGUMENT(BangoYapEditor::FTextValue, Text)
		SLATE_ARGUMENT(BangoYapEditor::FTextValue, ChildSafeText)
		SLATE_ARGUMENT(BangoYapEditor::FTextValue, TitleText)
		SLATE_ARGUMENT(bool, bMature)
		SLATE_ARGUMENT(bool, bPrompt)
		SLATE_ARGUMENT(BangoYapEditor::FSettingValue, TimeMode)
		SLATE_ARGUMENT(BangoYapEditor::FPaddingValue, PaddingSetting)
		SLATE_EVENT(FOnApply, OnApply)
		SLATE_EVENT(FSimpleDelegate, OnCancel)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedPtr<SWidget> GetFocusWidget() const;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	void Apply();

	/** Script text for a literal with this source and key, keeping what Original had (namespace, notes). */
	static FString MakeLiteral(const BangoYapEditor::FTextValue& Original, const FString& Source, const FString& Key);

	BangoYapEditor::FTextValue Text;
	BangoYapEditor::FTextValue ChildSafeText;
	BangoYapEditor::FTextValue TitleText;
	bool bMature = false;
	bool bPrompt = false;
	FOnApply OnApply;
	FSimpleDelegate OnCancel;

	TSharedPtr<SMultiLineEditableTextBox> TextBox;
	TSharedPtr<SMultiLineEditableTextBox> ChildSafeBox;
	TSharedPtr<SEditableTextBox> TitleBox;
	TSharedPtr<SEditableTextBox> IDBox;
	TSharedPtr<SBangoYapTimeSettings> TimeSettings;
};
