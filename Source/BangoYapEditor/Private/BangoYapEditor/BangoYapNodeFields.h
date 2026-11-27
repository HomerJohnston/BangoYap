#pragma once

#include "CoreMinimal.h"
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
	};
	FTextValue ReadTextField(const FBangoNodeLayout& Layout, const TCHAR* Label);

	/** The project's character with this ID (its Character ID, or its asset's name), loading it if need be. */
	UYapCharacterAsset* FindCharacter(FName CharacterID);

	/** The character a reference names, if it's an ID. */
	UYapCharacterAsset* FindCharacter(const FCharacterRef& Ref);

	/** The mood tag for a mood as scripts write it (the tag's last part under a speech config's mood root, or its full name). */
	FGameplayTag FindMoodTag(const FString& Mood);
}
