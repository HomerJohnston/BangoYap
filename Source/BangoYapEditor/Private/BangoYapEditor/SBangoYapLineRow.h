#pragma once

#include "CoreMinimal.h"
#include "BangoYapFieldWriter.h"
#include "BangoYapLineTiming.h"
#include "BangoYapNodeFields.h"
#include "Widgets/SCompoundWidget.h"

class SComboButton;
struct FYapActiveLine;
class UTexture2D;
class UYapCharacterAsset;

/** What a line's node says, read from its fields (BangoYapNodeFields.h). */
struct FBangoYapLineData
{
	/** Say, SayM or AddPrompt. */
	FString CommandName;

	BangoYapEditor::FSpeakerTarget Target;

	/** Say and AddPrompt: Text. SayM: MatureText. */
	BangoYapEditor::FTextValue Text;
	/** SayM: ChildSafeText. */
	BangoYapEditor::FTextValue ChildSafeText;
	/** AddPrompt: TitleText. */
	BangoYapEditor::FTextValue TitleText;

	/** The mood as written: a string's value (`Happy`), or the expression. */
	FString Mood;
	bool bMoodIsLiteral = true;

	/** TimeMode, Padding, Interruptible and Advance (stage 3). */
	BangoYapEditor::FSettingValue TimeMode;
	BangoYapEditor::FPaddingValue Padding;
	BangoYapEditor::FSettingValue Interruptible;
	BangoYapEditor::FSettingValue Advance;

	bool bMature = false;
	bool bPrompt = false;
	bool bDisabled = false;

	/** The node's layout and the target's text as written, for writing fields back. */
	FBangoNodeLayout Layout;
	FString TargetText;
	/** Sets a field (FBangoNodeDecoratorContext::SetFieldValue); unbound where the node can't be changed. */
	TFunction<bool(int32 FieldIndex, const FString& Value, FString& OutError)> SetFieldValue;

	static FBangoYapLineData FromContext(const struct FBangoNodeDecoratorContext& Context);

	/** Whether the row shows (and edits) a field, so the node can leave its own row out. */
	static bool RowCoversField(const struct FBangoNodeDecoratorContext& Context, int32 FieldIndex);
};

/**
 * A character's portrait as Yap's dialogue node draws it: the mood's portrait (or the default one) in a thick
 * rounded border of the character's colour. Shows what it can for a character it can't find (an actor, a variable,
 * an ID with no asset).
 */
class SBangoYapPortrait : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBangoYapPortrait)
		: _PortraitSize(64)
		, _BorderSize(2)
		, _bBorder(true)
		, _bPrompt(false)
	{}
		SLATE_ARGUMENT(BangoYapEditor::FCharacterRef, Character)
		SLATE_ARGUMENT(FString, Mood)
		SLATE_ARGUMENT(int32, PortraitSize)
		SLATE_ARGUMENT(int32, BorderSize)
		/** False for the small directed-at portrait, which has a round backing instead. */
		SLATE_ARGUMENT(bool, bBorder)
		/** Prompts show the character's colour at full strength (Yap dims it on other nodes). */
		SLATE_ARGUMENT(bool, bPrompt)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	UYapCharacterAsset* GetCharacter() const;
	const FSlateBrush* GetPortraitBrush() const;
	FSlateColor GetBorderColor() const;
	FText GetLabel() const;
	FSlateColor GetLabelColor() const;
	FText GetPortraitToolTip() const;

	BangoYapEditor::FCharacterRef Character;
	/** Found once: looking moods up reads the speech configs. */
	FGameplayTag MoodTag;
	int32 PortraitSize = 64;
	bool bPrompt = false;

	/** The brush for the current portrait texture, kept in step with it as the asset changes. */
	mutable FSlateBrush PortraitBrush;
	mutable TWeakObjectPtr<UTexture2D> BrushTexture;
};

/**
 * One line of dialogue drawn like a fragment of Yap's Flow dialogue node (SFlowGraphNode_YapFragmentWidget): the mood
 * icon on the left, the speaker's portrait, the text box (with the directed-at portrait on its corner and the line
 * ID at its foot), and the title text below it for a prompt entry. Hold Shift to see a SayM's child-safe text.
 *
 * Stage 2: it edits what it shows: the portrait opens a character picker (and takes a dropped character asset), the
 * directed-at portrait on the text box's corner (shown on hover, or when set) picks who it's said to, the mood icon
 * opens the mood menu, and the text box opens the text editor (text, line ID, child-safe text, title text). The node
 * leaves out the fields the row covers (RowCoversField), but keeps any written as an expression the row can't edit.
 *
 * Stage 3: above the row, Yap's upper bar: the progression toggles at its right (interruptible, advance), and a
 * summary of the line's time settings when they aren't the default (clicking it opens the text editor, which edits
 * them as Yap's editor does).
 *
 * Stage 4: under the row, Yap's time bar (SBangoYapTimeBar): the line's speech time and padding to scale, worked out
 * as YapBase will (BangoYapLineTiming), with a handle to drag the padding, and a marker while it plays in the editor.
 *
 * Stage 6: the line playing in a game in the editor is highlighted (as Yap's: white while it plays, yellow while it
 * waits to be advanced, fading after); problems (an unknown character or mood, mismatched mature and child-safe
 * texts or keys, a specified time with no time) give the text box a red border, and its tooltip says what they are;
 * the line ID tag says whether the broker finds audio for it (and plays it when clicked); zoomed out, the row is
 * drawn plainly (SBangoYapDetailSwitch), as Yap's is.
 */
class SBangoYapLineRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBangoYapLineRow) {}
		SLATE_ARGUMENT(FBangoYapLineData, Data)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Yap's sizes: 64 px portraits with a 2 px border, rows at least 72 px tall (the graph's snapping grid). */
	static constexpr int32 PortraitSize = 64;
	static constexpr int32 PortraitBorder = 2;
	static constexpr int32 RowHeight = 72;
	static constexpr int32 RowWidth = 360;

private:
	TSharedRef<SWidget> MakeSpeaker();
	TSharedRef<SWidget> MakeMoodColumn();
	TSharedRef<SWidget> MakeTextColumn();
	TSharedRef<SWidget> MakeDialogueText();
	TSharedRef<SWidget> MakeTitleText();
	TSharedRef<SWidget> MakeDirectedAt();
	TSharedRef<SWidget> MakeLineIDTag();
	TSharedRef<SWidget> MakeUpperBar();
	TSharedRef<SWidget> MakeTimeSummary();
	TSharedRef<SWidget> MakeTimeBar();

	/** The timing shown (the child-safe one while Shift shows a SayM's child-safe text). */
	const FBangoYapLineTiming& GetShownTiming() const;
	FLinearColor GetTimeBarColor() const;
	/** Seconds the line has played, if it's playing in a game in the editor (found by its line ID). */
	TOptional<float> GetPlaybackTime() const;

	// ---- Stage 6
	/** The line, if it's running in a game in the editor (found by its line ID). */
	const FYapActiveLine* FindActiveLine() const;
	FSlateColor GetHighlightColor() const;
	TSharedRef<SWidget> MakeHighlight();
	TSharedRef<SWidget> MakeErrorBorder();
	void FindProblems();
	FText GetDialogueToolTip() const;
	/** The audio shown (the child-safe audio while Shift shows a SayM's child-safe text). */
	UObject* GetShownAudio() const;
	TSharedRef<SWidget> MakeLowDetailSpeaker() const;

	bool ShowsChildSafe() const;
	FText GetDialogueText() const;
	FSlateColor GetDialogueTextColor() const;
	FText GetEmptyText() const;

	// ---- Editing
	bool CanEdit() const;
	/** Sets a field by its label, first filling any optional field before it that isn't there with nil. */
	bool SetField(const TCHAR* Label, const FString& Value);
	/**
	 * Sets several fields (label, script text), in the order they're written, then removes any nil left at the end
	 * (`Say(Text, nil, nil)` becomes `Say(Text)`). Stops at the first that's refused.
	 */
	bool SetFields(const TArray<BangoYapEditor::FFieldChange>& Changes);
	void SetTarget(const BangoYapEditor::FSpeakerTarget& Target);
	void ReportError(const FString& Error) const;

	TSharedRef<SWidget> MakeSpeakerMenu();
	TSharedRef<SWidget> MakeDirectedAtMenu();
	TSharedRef<SWidget> MakeMoodMenu();
	TSharedRef<SWidget> MakeTextMenu();
	void CloseMenus();

	FBangoYapLineData Data;
	/** Writes the node's fields (BangoYapFieldWriter.h). */
	BangoYapEditor::FFieldWriter Writer;

	FBangoYapLineTiming Timing;
	FBangoYapLineTiming ChildSafeTiming;

	/** What the broker finds for the line ID (and the child-safe key), when the row is made. */
	TWeakObjectPtr<UObject> MatureAudio;
	TWeakObjectPtr<UObject> ChildSafeAudio;

	/** What's wrong with the line, for the red border and the tooltip. */
	TArray<FText> Problems;
	/** A SayM's child-safe key doesn't follow its line ID (Yap's audio ID error). */
	bool bKeyMismatch = false;

	/** When the line was last seen playing (for the highlight's fade). */
	mutable double LastPlayingTime = -1.0;

	TSharedPtr<SComboButton> SpeakerCombo;
	TSharedPtr<SComboButton> DirectedAtCombo;
	TSharedPtr<SComboButton> MoodCombo;
	TSharedPtr<SComboButton> TextCombo;
};
