#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FBangoNodeDecoratorContext;

/**
 * The lines or options of one entry of a Speech or Prompt, drawn in the entry's row (Bango v0.143,
 * IBangoNodeDecorator::DrawsEntryStatements) rather than as nodes wired from its pin: each statement is an
 * SBangoYapLineRow, as its own node would be (it edits the line in place), with, on hover, buttons to move it up or
 * down (into the entry before or after at the ends), disable it and remove it; an AddPrompt's output pin (to its
 * block) is at its right end, on the node's edge (Bango v0.144, MakePinWidget). Under them, buttons to add a line (Say), a mature line (SayM) or an option (AddPrompt).
 *
 * A disabled statement is faded; a statement's comment is its tooltip; an error gives it a red outline (the error's
 * in the tooltip). While the debugger follows a copy of the script at one of them, it has Yap's fragment highlight; a
 * statement just added or moved flashes.
 *
 * Every change is one undoable change made by the graph editor, which draws the node again after it.
 */
class SBangoYapEntryStatements : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBangoYapEntryStatements) {}
	SLATE_END_ARGS()

	/** Must be made in MakeRowsArea (the statements' pins are placed there, at the rows' right ends: the node's edge). */
	void Construct(const FArguments& InArgs, const FBangoNodeDecoratorContext& Context, int32 InEntry);

private:
	TSharedRef<SWidget> MakeStatement(const FBangoNodeDecoratorContext& Context, int32 Index);
	TSharedRef<SWidget> MakeTool(const FName& Icon, const FText& ToolTipText, TFunction<void()> OnClicked, bool bEnabled = true) const;
	TSharedRef<SWidget> MakeAddButtons();

	/** A new statement's text: a new line ID, said by the speaker of the line before it (or of any line in the node). */
	FString MakeNewStatement(const TCHAR* Command) const;
	void Insert(const TCHAR* Command);
	void Move(int32 Index, int32 Delta);
	void Remove(int32 Index);
	void SetDisabled(int32 Index, bool bDisabled);
	static void Report(bool bDone, const FString& Error);

	int32 Entry = 0;
	bool bPrompt = false;
	/** Statements in each entry (0 for entries not drawn in their rows), to move a statement across entries. */
	TArray<int32> StatementCounts;
	TArray<bool> EntryDraws;
	/** The speaker to give a new line: the last line's target as written. */
	FString NewTarget;

	TFunction<bool(int32, int32, const FString&, FString&)> InsertEntryStatement;
	TFunction<bool(int32, int32, FString&)> RemoveEntryStatement;
	TFunction<bool(int32, int32, int32, int32, FString&)> MoveEntryStatement;
	TFunction<bool(int32, int32, bool, FString&)> SetEntryStatementDisabled;
};
