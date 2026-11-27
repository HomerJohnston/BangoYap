#pragma once

#include "CoreMinimal.h"
#include "BangoYapFieldWriter.h"
#include "BangoYapNodeFields.h"
#include "Widgets/SCompoundWidget.h"

class SComboButton;
struct FBangoNodeDecoratorContext;

/** Which block command a header is for. */
enum class EBangoYapBlockKind : uint8
{
	Speech,
	Prompt,
	Conversation,
};

/** What a block command's node says, read from its fields. */
struct FBangoYapBlockData
{
	EBangoYapBlockKind Kind = EBangoYapBlockKind::Speech;
	bool bDisabled = false;

	/** Speech: RunMode, Interruptible, Advance. */
	BangoYapEditor::FSettingValue RunMode;
	BangoYapEditor::FSettingValue Interruptible;
	BangoYapEditor::FSettingValue Advance;

	/**
	 * Speech and Prompt: Config. Conversation: Name. A string's value (bNameIsLiteral), or the expression as written;
	 * empty: not there or nil (the project's default config, no name).
	 */
	FString Name;
	bool bNameIsLiteral = true;

	BangoYapEditor::FFieldWriter Writer;

	/** Speech, Prompt or Conversation (other commands: unset). */
	static TOptional<EBangoYapBlockKind> GetKind(const FString& CommandName);

	static FBangoYapBlockData FromContext(const FBangoNodeDecoratorContext& Context);

	/** Whether the header shows (and edits) a field, so the node can leave its own row out. */
	static bool HeaderCoversField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex);

	/** The field Name is read from: "Config", or "Name" for a Conversation. */
	const TCHAR* GetNameLabel() const;
};

/**
 * The header of Yap's Flow dialogue node (SFlowGraphNode_YapDialogueWidget::CreateContentHeader), for BangoYap's
 * block commands, under their fields:
 *
 * - Speech: "Talk" (Yap's talk mode title), the sequencing button (Run All / Select One / Select Random: click to
 *   go on to the next), the config on the right (Yap's group label; click for the project's speech configs), and
 *   the progression toggles for the speech's interruptible and advance settings.
 * - Prompt: "Prompt" and its config.
 * - Conversation: "Conversation" and its name (click to type it).
 *
 * Fields written as expressions are shown (in yellow) but left for the node's own fields to edit.
 */
class SBangoYapBlockHeader : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBangoYapBlockHeader) {}
		SLATE_ARGUMENT(FBangoYapBlockData, Data)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** As the line rows (SBangoYapLineRow::RowWidth). */
	static constexpr int32 HeaderWidth = 360;

private:
	TSharedRef<SWidget> MakeKindLabel();
	TSharedRef<SWidget> MakeSequencingButton();
	TSharedRef<SWidget> MakeConfigButton();
	TSharedRef<SWidget> MakeConfigMenu();
	TSharedRef<SWidget> MakeNameBox();

	bool SetFields(const TArray<BangoYapEditor::FFieldChange>& Changes);

	FBangoYapBlockData Data;
	TSharedPtr<SComboButton> ConfigCombo;
};
