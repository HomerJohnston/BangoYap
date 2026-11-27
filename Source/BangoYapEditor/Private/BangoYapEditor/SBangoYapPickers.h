#pragma once

#include "CoreMinimal.h"
#include "BangoYapNodeFields.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;
class SEditableTextBox;
class UYapCharacterAsset;

/**
 * A character picker, as Yap's speaker and directed-at menus: a search box and the project's characters (portrait,
 * name and ID). For who a line is said to, it also offers "their speech focus" and "no one". At its foot, the
 * target as script text, for what the list can't pick (an actor, a variable).
 */
class SBangoYapCharacterPicker : public SCompoundWidget
{
public:
	/** What was picked: a character, or for directed-at, one of the two special choices. */
	enum class EChoice : uint8 { Character, Focus, NoOne };

	DECLARE_DELEGATE_TwoParams(FOnPicked, EChoice /*Choice*/, const BangoYapEditor::FCharacterRef& /*Character*/);
	DECLARE_DELEGATE_OneParam(FOnScriptCommitted, const FString& /*Text*/);

	SLATE_BEGIN_ARGS(SBangoYapCharacterPicker)
		: _bDirectedAt(false)
		, _CurrentChoice(EChoice::Character)
	{}
		/** The character picked now (highlighted). */
		SLATE_ARGUMENT(BangoYapEditor::FCharacterRef, Current)
		/** For who a line is said to: adds "their speech focus" and "no one". */
		SLATE_ARGUMENT(bool, bDirectedAt)
		/** For directed-at: which special choice is current (if Current isn't set). */
		SLATE_ARGUMENT(EChoice, CurrentChoice)
		/** The target's text, for the script box at the foot (empty: no box). */
		SLATE_ARGUMENT(FString, ScriptText)
		SLATE_EVENT(FOnPicked, OnPicked)
		SLATE_EVENT(FOnScriptCommitted, OnScriptCommitted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** What the menu should focus when it opens. */
	TSharedPtr<SWidget> GetFocusWidget() const;

private:
	void RefreshList();
	TSharedRef<SWidget> MakeCharacterRow(UYapCharacterAsset* Character);
	TSharedRef<SWidget> MakeChoiceRow(EChoice Choice, const FText& Label, const FText& InToolTip);

	BangoYapEditor::FCharacterRef Current;
	EChoice CurrentChoice = EChoice::Character;
	bool bDirectedAt = false;
	FString Filter;
	FOnPicked OnPicked;
	FOnScriptCommitted OnScriptCommitted;

	TSharedPtr<SScrollBox> List;
	TSharedPtr<SWidget> SearchBox;
};

/** Yap's mood menu: a grid of mood icons under each mood root, and Default (no mood). */
class SBangoYapMoodPicker : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnPicked, const FString& /*Mood: the tag's last part, or empty for none*/);

	SLATE_BEGIN_ARGS(SBangoYapMoodPicker) {}
		SLATE_ARGUMENT(FString, Current)
		SLATE_EVENT(FOnPicked, OnPicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeMoodButton(const FString& Mood, const FText& InToolTip);

	FString Current;
	FOnPicked OnPicked;
};
