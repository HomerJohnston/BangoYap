#pragma once

#include "CoreMinimal.h"
#include "BangoGraphLayout.h"
#include "GameplayTagContainer.h"

struct FBangoNodeLayout;
struct FBangoNodeDecoratorContext;
class UYapCharacterAsset;

/**
 * Reading BangoYap's commands from a graph node: its fields hold script text (`"HoJo"`, `["HoJo", "Santa"]`,
 * `TEXT("XMS010", "Hello")`, `"Happy"`, `MANUAL_TIME`), and these helpers turn that into what the node draws.
 */
namespace BangoYapEditor
{
	/** How a character is written in a field. */
	enum class ECharacterRefKind : uint8
	{
		/** Not there, or `nil`. */
		None,
		/** A character ID, as a string literal: `"HoJo"`. */
		ID,
		/** An actor: `@elf` (Value is the entity name, without the @). */
		Entity,
		/** A variable (`hojo`): its value isn't known in the editor. */
		Variable,
		/** Anything else (an expression). */
		Other,
	};

	struct FCharacterRef
	{
		ECharacterRefKind Kind = ECharacterRefKind::None;
		/** The ID, the entity's name, the variable's name, or the expression as written. */
		FString Value;

		bool IsSet() const { return Kind != ECharacterRefKind::None; }
		/** How to show it when there's no character asset: `HoJo`, `@elf`, `hojo`. */
		FString ToDisplayString() const;
	};

	/** A line's target: the speaker, and who it's said to. */
	struct FSpeakerTarget
	{
		FCharacterRef Speaker;
		FCharacterRef DirectedAt;
		/** Written as an array (`[speaker]`, `[speaker, directedAt]`): said to DirectedAt or no one, not to a focus. */
		bool bArray = false;
		/** False if the text couldn't be read as a target at all. */
		bool bValid = true;
	};

	/** Reads a target field's text. */
	FSpeakerTarget ParseSpeakerTarget(const FString& Text);

	/** Reads one character element: `"HoJo"`, `@elf`, `hojo`, `nil`. */
	FCharacterRef ParseCharacterRef(const FString& Text);

	/** Reads a string literal (`"Happy"`), with its escapes. False if Text isn't one. */
	bool ParseStringLiteral(const FString& Text, FString& OutValue);

	/** The index of the layout's field with this label ("Target", "Text", "Mood", ...), or INDEX_NONE. */
	int32 FindField(const FBangoNodeLayout& Layout, const TCHAR* Label);

	/** That field's text, trimmed, or empty if it isn't there (an optional argument not passed). */
	FString GetFieldText(const FBangoNodeLayout& Layout, const TCHAR* Label);

	/** A text field, read: the literal's source text and key, or the expression as written. */
	struct FTextValue
	{
		/** The field has something in it. */
		bool bPresent = false;
		/** It's a text literal (TEXT, TEXT_NS, TEXT_INV); otherwise Source is the expression as written. */
		bool bLiteral = false;
		/** TEXT_INV: no key, so no line ID and no audio. */
		bool bInvariant = false;
		FString Source;
		FString Key;
		/** The literal as read (its namespace and notes), to write back changed. */
		FBangoTextLiteral Literal;
	};
	FTextValue ReadTextField(const FBangoNodeLayout& Layout, const TCHAR* Label);

	/** The project's character with this ID (its Character ID, or its asset's name), loading it if need be. */
	UYapCharacterAsset* FindCharacter(FName CharacterID);

	/** The character a reference names, if it's an ID. */
	UYapCharacterAsset* FindCharacter(const FCharacterRef& Ref);

	// ---- Writing (stage 2): what a field is set to, as script text

	/** Script text for a string: `"Hello \"you\""`. */
	FString MakeStringLiteral(const FString& Value);

	/** Script text for one character: `"HoJo"`, `@elf`, a variable's name, or the expression as it was. */
	FString CharacterRefToScript(const FCharacterRef& Ref);

	/**
	 * Script text for a target: the speaker alone (said to their focus), `[speaker]` (said to no one) or
	 * `[speaker, directedAt]`. An array can only hold one type (two strings or two entities): a directed-at of the
	 * other kind is left out, and OutDropped says so.
	 */
	FString MakeTargetScript(const FSpeakerTarget& Target, bool* OutDropped = nullptr);

	/** True if the target can be shown and edited by the row alone (IDs, entities, or nothing; no variables or expressions). */
	bool IsTargetEditableInRow(const FSpeakerTarget& Target);

	/**
	 * What to set an optional field to so that it's cleared: empty (which removes it, with its separators) if no field
	 * after it is there, otherwise `nil` (an argument can't be left out from the middle).
	 */
	FString GetClearValue(const FBangoNodeLayout& Layout, int32 FieldIndex);

	/** Every character asset in the project, by name. */
	TArray<UYapCharacterAsset*> GetAllCharacters();

	/**
	 * A new line ID in the project's form (Project Settings > Plugins > Yap > Line ID Format): `*` a random capital
	 * letter, `?` a random letter or digit, `#` a random digit (a node can't see the script's other IDs, so it can't
	 * count on from them).
	 */
	FString MakeLineID();

	/** The child-safe key for a line ID: the ID with the project's child-safe suffix. */
	FString MakeChildSafeKey(const FString& LineID);

	/** The mood tag for a mood as scripts write it (the tag's last part under a speech config's mood root, or its full name). */
	FGameplayTag FindMoodTag(const FString& Mood);

	// ---- Line settings (stage 3): TimeMode, Padding, Interruptible and Advance

	/** A line's numbered settings, as Say, SayM and AddPrompt take them. */
	enum class ELineSetting : uint8
	{
		/** 1 text, 2 audio, 3 manual (the padding is the time), 4 none. */
		TimeMode,
		/** 1 interruptible, 2 forced. */
		Interruptible,
		/** 1 auto advance, 2 manual advance. */
		Advance,
		/** Speech's: 1 run all, 2 select one, 3 select random. */
		RunMode,
	};

	/** The field's label for a setting ("TimeMode", "Interruptible", "Advance", "RunMode"). */
	const TCHAR* GetSettingLabel(ELineSetting Setting);

	/** The highest value a setting takes (its values are 1 to this). */
	int32 GetSettingMax(ELineSetting Setting);

	/** A numbered setting as written in a field. */
	struct FSettingValue
	{
		enum class EKind : uint8
		{
			/** Not there, or nil: the speech's (or the speech config's). */
			Default,
			/** A number, or one of BangoYap's defines for one (Project Settings > Plugins > Bango Yap). */
			Value,
			/** Anything else (a variable, an expression, a name that isn't a define): only known while playing. */
			Expression,
		};
		EKind Kind = EKind::Default;
		/** 1.., for Value. */
		int32 Value = 0;
		/** The field's text as written. */
		FString Text;

		bool IsDefault() const { return Kind == EKind::Default; }
		bool Is(int32 InValue) const { return Kind == EKind::Value && Value == InValue; }
	};

	/** Reads a numbered setting's field: `TEXT_TIME`, `1`, `nil`, or an expression. */
	FSettingValue ReadSetting(const FBangoNodeLayout& Layout, ELineSetting Setting);

	/** Script text for a setting's value: its define's name (`FORCED`) if the project has one, else the number; 0: `nil`. */
	FString MakeSettingScript(ELineSetting Setting, int32 Value);

	/** The define's name for a setting's value, or empty if the project has none. */
	FString GetSettingDefineName(ELineSetting Setting, int32 Value);

	/** A line's Padding, as written in its field. */
	struct FPaddingValue
	{
		/** Not there, or nil: the speech config's. */
		bool bSet = false;
		/** Written as something other than a number (only known while playing). */
		bool bExpression = false;
		float Value = 0.0f;
		/** The field's text as written. */
		FString Text;
	};
	FPaddingValue ReadPadding(const FBangoNodeLayout& Layout);

	/** Script text for a number of seconds: `0.5`, `-0.25`, `2.0` (always with a decimal point: Padding is a double). */
	FString MakeSecondsScript(float Seconds);
}
