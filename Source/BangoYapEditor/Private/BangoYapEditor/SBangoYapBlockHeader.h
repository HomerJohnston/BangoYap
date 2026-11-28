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

/**
 * One entry's row of a Speech or Prompt whose block is a list of entries (Bango v0.140), beside the entry's output
 * pin: a numbered badge in the run mode's colour (a prompt's in Yap's title colour; a grey "D" for the default), then the entry's condition box
 * (the graph editor's own, from FBangoNodeDecoratorContext::MakeEntryConditionWidget: typing, Tab, the hint Always,
 * the red outline for an error), on Yap's dark rounded panel.
 *
 * While the debugger follows a copy of the script with the block open: a row whose entry didn't pass is dimmed, and
 * the running one has Yap's fragment highlight. A disabled entry is dimmed too.
 */
class SBangoYapEntryRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBangoYapEntryRow)
		: _Entry(0)
		, _bDefault(false)
		, _bDisabled(false)
		, _bHasDebug(false)
		, _bDebugPassed(true)
		, _bDebugRunning(false)
	{}
		SLATE_ARGUMENT(FBangoYapBlockData, Data)
		SLATE_ARGUMENT(int32, Entry)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, ConditionWidget)
		/** The default entry (default:), which passes when none of the others does. */
		SLATE_ARGUMENT(bool, bDefault)
		SLATE_ARGUMENT(bool, bDisabled)
		SLATE_ARGUMENT(bool, bHasDebug)
		SLATE_ARGUMENT(bool, bDebugPassed)
		SLATE_ARGUMENT(bool, bDebugRunning)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/**
	 * A Speech's or Prompt's rows area (Bango v0.144, IBangoNodeDecorator::MakeRowsArea): a row per entry (MakeRow),
	 * then buttons to add an entry and the default. Must be made in MakeRowsArea (the pins are placed there).
	 */
	static TSharedRef<SWidget> MakeRowsArea(const struct FBangoNodeDecoratorContext& Context);

	/**
	 * Entry Entry's row, the rows area's whole width: the badge and condition (an SBangoYapEntryRow), with, on hover,
	 * buttons to move, disable and remove the entry, and its pin at the right end, on the node's edge; then, for an
	 * entry drawn in its row, its lines or options (SBangoYapEntryStatements), each option's pin on the edge too. An
	 * entry drawn in its row has nothing wired to its own pin, so it's placed but hidden.
	 */
	static TSharedRef<SWidget> MakeRow(const struct FBangoNodeDecoratorContext& Context, int32 Entry);

	/** The badge and condition alone. */
	static TSharedRef<SWidget> Make(const struct FBangoNodeDecoratorContext& Context, int32 Entry);
};
