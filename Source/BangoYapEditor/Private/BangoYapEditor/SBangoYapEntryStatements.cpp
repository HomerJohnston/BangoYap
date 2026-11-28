#include "SBangoYapEntryStatements.h"

#include "BangoNodeDecorator.h"
#include "BangoYapEditorStyle.h"
#include "BangoYapFieldWriter.h"
#include "BangoYapNodeFields.h"
#include "Framework/Application/SlateApplication.h"
#include "SBangoYapLineRow.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

using namespace BangoYapEditor;

namespace BangoYapEntryStatementsPrivate
{
	/** How long a statement just added or moved flashes. */
	constexpr double FlashSeconds = 1.5;

	/** A speaker's target as written, from a line's context ("" for none). */
	FString GetTargetText(const TSharedPtr<FBangoNodeDecoratorContext>& Statement)
	{
		return Statement.IsValid() ? FBangoYapLineData::FromContext(*Statement).TargetText : FString();
	}
}

void SBangoYapEntryStatements::Construct(const FArguments& InArgs, const FBangoNodeDecoratorContext& Context, int32 InEntry)
{
	using namespace BangoYapEntryStatementsPrivate;

	Entry = InEntry;
	bPrompt = Context.CommandName == TEXT("Prompt");
	InsertEntryStatement = Context.InsertEntryStatement;
	RemoveEntryStatement = Context.RemoveEntryStatement;
	MoveEntryStatement = Context.MoveEntryStatement;
	SetEntryStatementDisabled = Context.SetEntryStatementDisabled;

	EntryDraws = Context.EntryDrawsStatements;
	StatementCounts.SetNumZeroed(Context.EntryStatements.Num());
	for (int32 Index = 0; Index < Context.EntryStatements.Num(); ++Index)
	{
		StatementCounts[Index] = Context.EntryStatements[Index].Num();
	}

	// A new line's speaker: this entry's last line's, else the nearest entry's before it, else any.
	const TArray<TSharedPtr<FBangoNodeDecoratorContext>> NoStatements;
	const TArray<TSharedPtr<FBangoNodeDecoratorContext>>& Statements = Context.EntryStatements.IsValidIndex(Entry) ? Context.EntryStatements[Entry] : NoStatements;
	for (int32 From = Entry; From >= 0 && NewTarget.IsEmpty(); --From)
	{
		if (Context.EntryStatements.IsValidIndex(From) && Context.EntryStatements[From].Num() > 0)
		{
			NewTarget = GetTargetText(Context.EntryStatements[From].Last());
		}
	}
	for (int32 From = Entry + 1; From < Context.EntryStatements.Num() && NewTarget.IsEmpty(); ++From)
	{
		if (Context.EntryStatements[From].Num() > 0)
		{
			NewTarget = GetTargetText(Context.EntryStatements[From][0]);
		}
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (int32 Index = 0; Index < Statements.Num(); ++Index)
	{
		if (Statements[Index].IsValid())
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				MakeStatement(Context, Index)
			];
		}
	}
	Box->AddSlot()
	.AutoHeight()
	.Padding(0, 2, 0, 0)
	[
		MakeAddButtons()
	];

	ChildSlot
	[
		Box
	];
}

TSharedRef<SWidget> SBangoYapEntryStatements::MakeStatement(const FBangoNodeDecoratorContext& Context, int32 Index)
{
	using namespace BangoYapEntryStatementsPrivate;

	const FBangoNodeDecoratorContext& Statement = *Context.EntryStatements[Entry][Index];
	const bool bStatementDisabled = Statement.bDisabled;
	const bool bError = !Statement.ErrorMessage.IsEmpty();
	const bool bDebugHere = Context.DebugStatementEntry == Entry && Context.DebugStatementIndex == Index;
	const bool bFlash = Context.FocusStatementEntry == Entry && Context.FocusStatementIndex == Index;

	// The tooltip: the comment, and any error.
	FString StatementTip = Statement.Comment;
	if (bError)
	{
		StatementTip = StatementTip.IsEmpty() ? Statement.ErrorMessage : StatementTip + TEXT("\n\n") + Statement.ErrorMessage;
	}

	// Moving past the first or last statement goes on into the entry before or after, when it's drawn in its row too.
	const bool bCanUp = Index > 0 || (EntryDraws.IsValidIndex(Entry - 1) && EntryDraws[Entry - 1]);
	const bool bCanDown = Index < StatementCounts[Entry] - 1 || (EntryDraws.IsValidIndex(Entry + 1) && EntryDraws[Entry + 1]);

	TSharedPtr<SWidget> Pin;
	if (Statement.bBlock && Context.MakeEntryStatementPinWidget)
	{
		Pin = Context.MakeEntryStatementPinWidget(Entry, Index);
	}

	TSharedPtr<SBorder> Holder;

	TSharedRef<SVerticalBox> Tools = SNew(SVerticalBox);
	auto AddTool = [&](const FName& Icon, const FText& Tip, TFunction<void()> OnClicked, bool bEnabled)
	{
		Tools->AddSlot()
		.AutoHeight()
		.Padding(0, 1)
		[
			MakeTool(Icon, Tip, MoveTemp(OnClicked), bEnabled)
		];
	};
	const TWeakPtr<SBangoYapEntryStatements> WeakThis = SharedThis(this);
	AddTool(TEXT("Icons.ChevronUp"), LOCTEXT("Statement_Up", "Move up (into the entry before, from the top)"),
		[WeakThis, Index]() { if (TSharedPtr<SBangoYapEntryStatements> This = WeakThis.Pin()) { This->Move(Index, -1); } }, bCanUp);
	AddTool(TEXT("Icons.ChevronDown"), LOCTEXT("Statement_Down", "Move down (into the entry after, from the bottom)"),
		[WeakThis, Index]() { if (TSharedPtr<SBangoYapEntryStatements> This = WeakThis.Pin()) { This->Move(Index, 1); } }, bCanDown);
	AddTool(bStatementDisabled ? TEXT("Icons.Hidden") : TEXT("Icons.Visible"),
		bStatementDisabled ? LOCTEXT("Statement_Enable", "Enable") : LOCTEXT("Statement_Disable", "Disable (it's skipped)"),
		[WeakThis, Index, bStatementDisabled]() { if (TSharedPtr<SBangoYapEntryStatements> This = WeakThis.Pin()) { This->SetDisabled(Index, !bStatementDisabled); } }, true);
	AddTool(TEXT("Icons.Delete"), bPrompt ? LOCTEXT("Statement_RemoveOption", "Remove the option (and its block)") : LOCTEXT("Statement_RemoveLine", "Remove the line"),
		[WeakThis, Index]() { if (TSharedPtr<SBangoYapEntryStatements> This = WeakThis.Pin()) { This->Remove(Index); } }, true);

	const double FlashStart = FSlateApplication::Get().GetCurrentTime();

	TSharedRef<SWidget> Row = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(Holder, SBorder)
			.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Box_SolidWhite_Rounded))
			.BorderBackgroundColor(FLinearColor::Transparent)
			.Padding(0)
			.ToolTipText(StatementTip.IsEmpty() ? FText::GetEmpty() : FText::FromString(StatementTip))
			[
				SNew(SHorizontalBox)
				// The tools, shown while the pointer's over the statement.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 2, 0)
				[
					SNew(SBox)
					.WidthOverride(16)
					[
						Tools
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SBox)
					.RenderOpacity(bStatementDisabled ? 0.4f : 1.0f)
					[
						SNew(SBangoYapLineRow)
						.Data(FBangoYapLineData::FromContext(Statement))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0, 0, 0)
				[
					Pin.IsValid() ? Pin.ToSharedRef() : SNullWidget::NullWidget
				]
			]
		]
		// A red outline for an error.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(bError ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			.BorderImage(FBangoYapEditorStyle::GetBrush(FBangoYapEditorStyle::Border_Thick_RoundedSquare))
			.BorderBackgroundColor(BangoYapColor::Red)
		]
		// Yap's fragment highlight while the debugger is at it; a flash when it's just been added or moved.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(bDebugHere || bFlash ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
			.BorderBackgroundColor_Lambda([bDebugHere, bFlash, FlashStart]()
			{
				if (bDebugHere)
				{
					return FSlateColor(BangoYapColor::Yellow_Glass);
				}
				const double Age = FSlateApplication::Get().GetCurrentTime() - FlashStart;
				const float Alpha = bFlash ? 0.25f * FMath::Clamp(1.0f - float(Age / FlashSeconds), 0.0f, 1.0f) : 0.0f;
				return FSlateColor(BangoYapColor::LightBlue.CopyWithNewOpacity(Alpha));
			})
		];

	// The tools show while the pointer's over the statement (and stay hit-testable then).
	const TWeakPtr<SBorder> WeakHolder = Holder;
	Tools->SetVisibility(TAttribute<EVisibility>::CreateLambda([WeakHolder]()
	{
		const TSharedPtr<SBorder> Pinned = WeakHolder.Pin();
		return Pinned.IsValid() && Pinned->IsHovered() ? EVisibility::Visible : EVisibility::Hidden;
	}));

	return Row;
}

TSharedRef<SWidget> SBangoYapEntryStatements::MakeTool(const FName& Icon, const FText& ToolTipText, TFunction<void()> OnClicked, bool bEnabled) const
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(0)
		.IsEnabled(bEnabled)
		.ToolTipText(ToolTipText)
		.OnClicked_Lambda([OnClicked]()
		{
			OnClicked();
			return FReply::Handled();
		})
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush(Icon))
			.DesiredSizeOverride(FVector2D(12, 12))
			.ColorAndOpacity(FSlateColor::UseForeground())
		];
}

TSharedRef<SWidget> SBangoYapEntryStatements::MakeAddButtons()
{
	const TWeakPtr<SBangoYapEntryStatements> WeakThis = SharedThis(this);
	auto MakeAdd = [WeakThis](const FText& Label, const FText& Tip, const TCHAR* Command) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(4, 1))
			.ToolTipText(Tip)
			.OnClicked_Lambda([WeakThis, Command]()
			{
				if (TSharedPtr<SBangoYapEntryStatements> This = WeakThis.Pin())
				{
					This->Insert(Command);
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 3, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Plus"))
					.DesiredSizeOverride(FVector2D(10, 10))
					.ColorAndOpacity(BangoYapColor::LightGray)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(BangoYapColor::LightGray)
				]
			];
	};

	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (bPrompt)
	{
		Buttons->AddSlot().AutoWidth()
		[
			MakeAdd(LOCTEXT("Add_Option", "Option"), LOCTEXT("Add_OptionTip", "Add an option (AddPrompt) to this entry, with an empty block."), TEXT("AddPrompt"))
		];
	}
	else
	{
		Buttons->AddSlot().AutoWidth()
		[
			MakeAdd(LOCTEXT("Add_Line", "Line"), LOCTEXT("Add_LineTip", "Add a line (Say) to this entry."), TEXT("Say"))
		];
		Buttons->AddSlot().AutoWidth().Padding(4, 0, 0, 0)
		[
			MakeAdd(LOCTEXT("Add_Mature", "Mature line"), LOCTEXT("Add_MatureTip", "Add a line with mature and child-safe texts (SayM) to this entry."), TEXT("SayM"))
		];
	}
	return Buttons;
}

FString SBangoYapEntryStatements::MakeNewStatement(const TCHAR* Command) const
{
	const FString Target = NewTarget.IsEmpty() ? FString(TEXT("\"\"")) : NewTarget;
	const FString LineID = MakeLineID();
	const FString Text = FString::Printf(TEXT("TEXT(\"%s\", \"\")"), *LineID);

	if (FCString::Strcmp(Command, TEXT("AddPrompt")) == 0)
	{
		return FString::Printf(TEXT("%s AddPrompt(%s) { }"), *Target, *Text);
	}
	if (FCString::Strcmp(Command, TEXT("SayM")) == 0)
	{
		const FString ChildSafe = FString::Printf(TEXT("TEXT(\"%s\", \"\")"), *MakeChildSafeKey(LineID));
		return FString::Printf(TEXT("%s SayM(%s, %s)"), *Target, *Text, *ChildSafe);
	}
	return FString::Printf(TEXT("%s Say(%s)"), *Target, *Text);
}

void SBangoYapEntryStatements::Insert(const TCHAR* Command)
{
	if (!InsertEntryStatement)
	{
		return;
	}
	// Kept alive while the node is drawn again.
	const TSharedRef<SWidget> Self = AsShared();
	FString Error;
	const int32 Count = StatementCounts.IsValidIndex(Entry) ? StatementCounts[Entry] : 0;
	Report(InsertEntryStatement(Entry, Count, MakeNewStatement(Command), Error), Error);
}

void SBangoYapEntryStatements::Move(int32 Index, int32 Delta)
{
	if (!MoveEntryStatement)
	{
		return;
	}
	const TSharedRef<SWidget> Self = AsShared();
	FString Error;
	const int32 Count = StatementCounts[Entry];
	if (Delta < 0 && Index == 0)
	{
		// To the end of the entry before.
		Report(MoveEntryStatement(Entry, Index, Entry - 1, StatementCounts[Entry - 1], Error), Error);
	}
	else if (Delta > 0 && Index == Count - 1)
	{
		// To the start of the entry after.
		Report(MoveEntryStatement(Entry, Index, Entry + 1, 0, Error), Error);
	}
	else
	{
		// In this entry (ToIndex is counted without the one moved).
		Report(MoveEntryStatement(Entry, Index, Entry, Index + Delta, Error), Error);
	}
}

void SBangoYapEntryStatements::Remove(int32 Index)
{
	if (!RemoveEntryStatement)
	{
		return;
	}
	const TSharedRef<SWidget> Self = AsShared();
	FString Error;
	Report(RemoveEntryStatement(Entry, Index, Error), Error);
}

void SBangoYapEntryStatements::SetDisabled(int32 Index, bool bDisabled)
{
	if (!SetEntryStatementDisabled)
	{
		return;
	}
	const TSharedRef<SWidget> Self = AsShared();
	FString Error;
	Report(SetEntryStatementDisabled(Entry, Index, bDisabled, Error), Error);
}

void SBangoYapEntryStatements::Report(bool bDone, const FString& Error)
{
	if (!bDone && !Error.IsEmpty())
	{
		FFieldWriter::ReportError(Error);
	}
}

#undef LOCTEXT_NAMESPACE
