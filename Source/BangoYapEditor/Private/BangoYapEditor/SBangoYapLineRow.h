#pragma once

#include "CoreMinimal.h"
#include "BangoYapNodeFields.h"
#include "Widgets/SCompoundWidget.h"

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

	bool bMature = false;
	bool bPrompt = false;
	bool bDisabled = false;

	static FBangoYapLineData FromContext(const struct FBangoNodeDecoratorContext& Context);
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
	FText GetToolTip() const;

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
 * Stage 1: it only shows the node; the node's own fields still edit it.
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
	TSharedRef<SWidget> MakeMoodColumn();
	TSharedRef<SWidget> MakeTextColumn();
	TSharedRef<SWidget> MakeDialogueText();
	TSharedRef<SWidget> MakeTitleText();
	TSharedRef<SWidget> MakeDirectedAt();
	TSharedRef<SWidget> MakeLineIDTag();

	bool ShowsChildSafe() const;
	FText GetDialogueText() const;
	FSlateColor GetDialogueTextColor() const;
	FText GetEmptyText() const;

	FBangoYapLineData Data;
};
