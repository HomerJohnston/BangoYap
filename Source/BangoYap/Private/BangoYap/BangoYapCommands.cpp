// BangoYap's Bango commands (BangoYap Design, in the project): dialogue written in scripts, played by YapBase.
//
//   Speech(RunMode, Config, Interruptible, Advance) { ... }        a group of lines
//   Speaker Say(Text, Mood, TimeMode, Padding, Interruptible, Advance);   in a Speech, or on its own (Speech(RUN_ALL))
//   Speaker SayM(MatureText, ChildSafeText, Mood, ...);
//   Conversation(Name) { ... }                                      speech in it is the conversation's
//   Prompt(Config) { ... }                                          the player chooses one of its entries
//   Speaker AddPrompt(Text, TitleText, Mood, ...) { ... }           said when chosen, then its block runs
//   Character SetSpeechFocus(Character);                            who the character talks to (saved)
//   Character ClearSpeechFocus();                                   they talk to no one
//   GetSpeechFocus(Character) -> string                             who they talk to, as an ID (nil: no one)
//
// A character is its ID (a string, "HoJo") or an actor with a Yap Character Component (an entity, @hojo). A speaker is
// a character, [speaker, directedAt] (two strings or two entities), or [speaker] (the same as [speaker, nil]: directed
// at no one). With only a speaker, the line is directed at the speaker's speech focus (SetSpeechFocus), taken when the
// line starts and kept with it. Numbered settings start at 1; nil or left out is
// the default, and 0 is an error.
//
// Everything a command needs after its Start is kept as Bango values (its latent state, or its block's state), so a
// script saved in the middle of a line, a prompt or a conversation carries on from there when it's loaded: the line
// starts again from how far it had played. Those values are texts, one per field (a localized text stays one, so it
// still translates; everything else is a culture-invariant text of its string).

#include "BangoYapCommands.h"

#include "Bango.h"
#include "BangoCommand.h"
#include "BangoLog.h"
#include "BangoTypes.h"
#include "BangoYap/BangoYapLog.h"
#include "BangoYap/BangoYapSubsystem.h"
#include "Engine/World.h"
#include "Internationalization/Text.h"
#include "BangoActorRegistry.h"
#include "GameFramework/Actor.h"
#include "YapBase/YapCharacterComponent.h"
#include "YapBase/YapCharacterManager.h"
#include "YapBase/YapSettings.h"
#include "YapBase/YapSpeechConfig.h"
#include "YapBase/YapSubsystem.h"

namespace BangoYapPrivate
{
	static const TCHAR* Category = TEXT("Yap");

	enum class ERunMode : int32
	{
		RunAll = 1,
		SelectOne = 2,
		SelectRandom = 3,
	};

	// ============================================================================================
	// Fields: a record kept as a Bango text[]

	using FFields = TArray<FText>;

	static FText ToField(const FString& String) { return FText::FromString(String); }
	static FText ToField(int32 Number) { return FText::FromString(FString::FromInt(Number)); }
	static FText ToField(float Number) { return FText::FromString(FString::SanitizeFloat(Number)); }
	static FText ToField(bool bValue) { return FText::FromString(bValue ? TEXT("1") : TEXT("0")); }

	static FString FieldString(const FFields& Fields, int32 Index) { return Fields.IsValidIndex(Index) ? Fields[Index].ToString() : FString(); }
	static FText FieldText(const FFields& Fields, int32 Index) { return Fields.IsValidIndex(Index) ? Fields[Index] : FText::GetEmpty(); }
	static int32 FieldInt(const FFields& Fields, int32 Index) { return FCString::Atoi(*FieldString(Fields, Index)); }
	static float FieldFloat(const FFields& Fields, int32 Index) { return FCString::Atof(*FieldString(Fields, Index)); }
	static bool FieldBool(const FFields& Fields, int32 Index) { return FieldInt(Fields, Index) != 0; }

	static FBangoValue FieldsToValue(const FFields& Fields)
	{
		TArray<FBangoValue> Values;
		Values.Reserve(Fields.Num());
		for (const FText& Field : Fields)
		{
			Values.Add(FBangoValue::MakeText(Field));
		}
		return FBangoValue::MakeArray(FBangoType::Text(), Values);
	}

	static bool ValueToFields(const FBangoValue& Value, FFields& OutFields)
	{
		OutFields.Reset();
		if (Value.IsNil() || !(Value.GetType() == FBangoType::ArrayOf(EBangoBaseType::Text)))
		{
			return false;
		}
		for (const FBangoValue& Element : Value.GetArray())
		{
			OutFields.Add(Element.IsNil() ? FText::GetEmpty() : Element.GetText());
		}
		return true;
	}

	/** A block's state: rows of fields (a text[][]). Row 0 is the block's own; the rest are its lines or entries. */
	static FBangoValue RowsToValue(const TArray<FFields>& Rows)
	{
		TArray<FBangoValue> Values;
		Values.Reserve(Rows.Num());
		for (const FFields& Row : Rows)
		{
			Values.Add(FieldsToValue(Row));
		}
		return FBangoValue::MakeArray(FBangoType::ArrayOf(EBangoBaseType::Text), Values);
	}

	static bool ValueToRows(const FBangoValue& Value, TArray<FFields>& OutRows)
	{
		OutRows.Reset();
		if (Value.IsNil() || !(Value.GetType() == FBangoType::ArrayOf(EBangoBaseType::Text, 2)))
		{
			return false;
		}
		for (const FBangoValue& Element : Value.GetArray())
		{
			FFields& Row = OutRows.AddDefaulted_GetRef();
			ValueToFields(Element, Row);
		}
		return OutRows.Num() > 0;
	}

	// ============================================================================================
	// A line (a speech line, or a prompt entry)

	struct FLineRecord
	{
		FString Speaker;
		FString DirectedAt;
		FString LineID;
		FText MatureText;
		FText ChildSafeText;
		FText TitleText;
		FString Mood;
		int32 TimeMode = 0;
		bool bHasPadding = false;
		float Padding = 0.0f;
		int32 Interruptible = 0;
		int32 Advance = 0;
		FString ConfigName;
		FString ConversationID;
		FString ConversationName;
		/** Set once YapBase is playing it. */
		FString SpeechHandle;
		float Elapsed = 0.0f;
		/** A prompt entry's block. */
		int32 BlockNumber = -1;
		/** A prompt entry: the Prompt block's entry it's written in (entry if (...):), or -1. */
		int32 EntryNumber = -1;

		FFields ToFields() const
		{
			return {
				ToField(Speaker), ToField(DirectedAt), ToField(LineID), MatureText, ChildSafeText, TitleText, ToField(Mood),
				ToField(TimeMode), ToField(bHasPadding), ToField(Padding), ToField(Interruptible), ToField(Advance),
				ToField(ConfigName), ToField(ConversationID), ToField(ConversationName), ToField(SpeechHandle),
				ToField(Elapsed), ToField(BlockNumber), ToField(EntryNumber),
			};
		}

		void FromFields(const FFields& Fields)
		{
			Speaker = FieldString(Fields, 0);
			DirectedAt = FieldString(Fields, 1);
			LineID = FieldString(Fields, 2);
			MatureText = FieldText(Fields, 3);
			ChildSafeText = FieldText(Fields, 4);
			TitleText = FieldText(Fields, 5);
			Mood = FieldString(Fields, 6);
			TimeMode = FieldInt(Fields, 7);
			bHasPadding = FieldBool(Fields, 8);
			Padding = FieldFloat(Fields, 9);
			Interruptible = FieldInt(Fields, 10);
			Advance = FieldInt(Fields, 11);
			ConfigName = FieldString(Fields, 12);
			ConversationID = FieldString(Fields, 13);
			ConversationName = FieldString(Fields, 14);
			SpeechHandle = FieldString(Fields, 15);
			Elapsed = FieldFloat(Fields, 16);
			BlockNumber = Fields.IsValidIndex(17) ? FieldInt(Fields, 17) : -1;
			EntryNumber = Fields.IsValidIndex(18) ? FieldInt(Fields, 18) : -1;
		}
	};

	/** A Speech block's own row. */
	struct FSpeechHeader
	{
		int32 RunMode = (int32)ERunMode::RunAll;
		FString ConfigName;
		int32 Interruptible = 0;
		int32 Advance = 0;
		FString ConversationID;
		FString ConversationName;
		/** Select random: the row of the line chosen to play, or 0. */
		int32 RunningRow = 0;
		/** The block is a list of entries (entry if (...): / entry:): the Speech chooses the entries, and its lines just play. */
		bool bEntries = false;
		/** Run all with entries: an entry other than the default has played (so the default doesn't). */
		bool bPlayedEntry = false;

		FFields ToFields() const
		{
			return { ToField(RunMode), ToField(ConfigName), ToField(Interruptible), ToField(Advance), ToField(ConversationID), ToField(ConversationName), ToField(RunningRow), ToField(bEntries), ToField(bPlayedEntry) };
		}

		void FromFields(const FFields& Fields)
		{
			RunMode = FieldInt(Fields, 0);
			ConfigName = FieldString(Fields, 1);
			Interruptible = FieldInt(Fields, 2);
			Advance = FieldInt(Fields, 3);
			ConversationID = FieldString(Fields, 4);
			ConversationName = FieldString(Fields, 5);
			RunningRow = FieldInt(Fields, 6);
			bEntries = FieldBool(Fields, 7);
			bPlayedEntry = Fields.IsValidIndex(8) && FieldBool(Fields, 8);
		}
	};

	/** A Prompt block's own row. */
	struct FPromptHeader
	{
		FString ConfigName;
		FString ConversationID;
		FString ConversationName;
		FString PromptHandle;
		/** 0: not shown yet. 1: waiting for the player. 2: the chosen entry is being said. */
		int32 Phase = 0;
		int32 ChosenRow = 0;
		/** The block is a list of entries: each AddPrompt's entry says whether it's shown, watched while the player chooses. */
		bool bEntries = false;

		FFields ToFields() const
		{
			return { ToField(ConfigName), ToField(ConversationID), ToField(ConversationName), ToField(PromptHandle), ToField(Phase), ToField(ChosenRow), ToField(bEntries) };
		}

		void FromFields(const FFields& Fields)
		{
			ConfigName = FieldString(Fields, 0);
			ConversationID = FieldString(Fields, 1);
			ConversationName = FieldString(Fields, 2);
			PromptHandle = FieldString(Fields, 3);
			Phase = FieldInt(Fields, 4);
			ChosenRow = FieldInt(Fields, 5);
			bEntries = FieldBool(Fields, 6);
		}
	};

	// ============================================================================================
	// Helpers

	static UYapSubsystem* GetYap(const FBangoCommandCall& Call)
	{
		return Call.WorldContext ? UYapSubsystem::Get(Call.WorldContext) : nullptr;
	}

	static UBangoYapSubsystem* GetBangoYap(const FBangoCommandCall& Call)
	{
		return Call.WorldContext ? UBangoYapSubsystem::Get(Call.WorldContext) : nullptr;
	}

	static const TCHAR* NoYapError = TEXT("Yap isn't running here (it only runs in game and Play In Editor worlds).");

	/** Argument Index, or null if it wasn't given or is nil. */
	static const FBangoValue* Arg(const FBangoCommandCall& Call, int32 Index)
	{
		return (Call.Args.IsValidIndex(Index) && !Call.Args[Index].IsNil()) ? &Call.Args[Index] : nullptr;
	}

	/** A numbered setting: nil or left out is 0 (the default); otherwise it must be from 1 to Max. */
	static bool ReadSetting(const FBangoCommandCall& Call, int32 Index, int32 Max, const TCHAR* Name, int32& OutValue, FString& OutError)
	{
		OutValue = 0;
		const FBangoValue* Value = Arg(Call, Index);
		if (!Value)
		{
			return true;
		}

		const int32 Number = Value->GetInt();
		if (Number < 1 || Number > Max)
		{
			OutError = FString::Printf(TEXT("%s must be from 1 to %d, or nil for the default, but it's %d."), Name, Max, Number);
			return false;
		}

		OutValue = Number;
		return true;
	}

	static FString ReadString(const FBangoCommandCall& Call, int32 Index)
	{
		const FBangoValue* Value = Arg(Call, Index);
		return Value ? Value->GetString() : FString();
	}

	/** The line's ID: its text's key. Empty for a text with no key (INVTEXT, ToText). */
	static FString GetLineID(const FText& Text)
	{
		const TOptional<FString> Key = FTextInspector::GetKey(Text);
		return Key.IsSet() ? Key.GetValue() : FString();
	}

	// ---- Characters: by ID, or by entity
	//
	// A character is named by its ID (a string, "HoJo") or by an actor that has a Yap Character Component (an entity,
	// @hojo). Kept (in a line's saved state, and in speech focus) as a *speaker key*: the character ID if it's the same
	// every time (a project character, or a component's custom ID), otherwise '@' and the entity's name, which Bango
	// keeps through saving and loading where the component's made-up ID isn't. A key becomes a character ID each time a
	// line starts (KeyToCharacterID).

	static const TCHAR EntityKeyPrefix = TEXT('@');

	static bool IsEntityKey(const FString& Key) { return Key.Len() > 1 && Key[0] == EntityKeyPrefix; }

	/** The Yap Character Component an entity stands for: the actor's, or the entity itself if it's one. */
	static UYapCharacterComponent* FindCharacterComponent(const FBangoCommandCall& Call, const FString& EntityName, FString& OutError)
	{
		UBangoActorRegistry* Registry = Call.WorldContext ? UBangoActorRegistry::Get(Call.WorldContext) : nullptr;
		UObject* Object = Registry ? Registry->FindEntity(EntityName) : nullptr;
		if (!Object)
		{
			OutError = FString::Printf(TEXT("%s isn't here, so it can't speak."), *Bango::GetEntityDisplayName(EntityName));
			return nullptr;
		}

		if (UYapCharacterComponent* Component = Cast<UYapCharacterComponent>(Object))
		{
			return Component;
		}

		AActor* Actor = Cast<AActor>(Object);
		if (!Actor)
		{
			if (const UActorComponent* OtherComponent = Cast<UActorComponent>(Object))
			{
				Actor = OtherComponent->GetOwner();
			}
		}

		UYapCharacterComponent* Component = Actor ? Actor->FindComponentByClass<UYapCharacterComponent>() : nullptr;
		if (!Component)
		{
			OutError = FString::Printf(TEXT("%s has no Yap Character Component, so it can't speak."), *Bango::GetEntityDisplayName(EntityName));
		}
		return Component;
	}

	/** The speaker key for a string or entity value (What says which, for errors: "The speaker"). */
	static bool ValueToSpeakerKey(const FBangoCommandCall& Call, const FBangoValue& Value, const TCHAR* What, FString& OutKey, FString& OutError)
	{
		if (Value.IsNil())
		{
			OutError = FString::Printf(TEXT("%s is nil."), What);
			return false;
		}

		const FBangoType& Type = Value.GetType();
		if (Type == FBangoType::String())
		{
			OutKey = Value.GetString();
			if (OutKey.IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s is an empty string."), What);
				return false;
			}
			if (OutKey[0] == EntityKeyPrefix)
			{
				OutError = FString::Printf(TEXT("%s is \"%s\": a character ID can't start with '@'. For an actor, write its entity without quotes (@name)."), What, *OutKey);
				return false;
			}
			return true;
		}

		if (Type == FBangoType::Entity())
		{
			const FString& EntityName = Value.GetEntityName();
			UYapCharacterComponent* Component = FindCharacterComponent(Call, EntityName, OutError);
			if (!Component)
			{
				OutError = FString::Printf(TEXT("%s: %s"), What, *OutError);
				return false;
			}
			OutKey = Component->HasFixedCharacterID() ? Component->GetCharacterID().ToString() : FString(1, &EntityKeyPrefix) + EntityName;
			return true;
		}

		OutError = FString::Printf(TEXT("%s is a %s: it should be a character's ID (a string) or an actor with a Yap Character Component (an entity)."), What, *Type.ToString());
		return false;
	}

	/** The character ID a speaker key stands for now (empty key: None). */
	static bool KeyToCharacterID(const FBangoCommandCall& Call, const FString& Key, FName& OutID, FString& OutError)
	{
		if (Key.IsEmpty())
		{
			OutID = NAME_None;
			return true;
		}
		if (!IsEntityKey(Key))
		{
			OutID = FName(*Key);
			return true;
		}

		UYapCharacterComponent* Component = FindCharacterComponent(Call, Key.RightChop(1), OutError);
		if (!Component)
		{
			return false;
		}
		OutID = Component->GetCharacterID();
		return true;
	}

	/**
	 * The speaker (and who it's said to) from a line's target:
	 * - a character ID or an entity: said to the speaker's speech focus (if any);
	 * - [speaker, directedAt] (two strings, or two entities): said to directedAt;
	 * - [speaker], or [speaker, nil]: said to no one.
	 */
	static bool ReadSpeaker(const FBangoCommandCall& Call, FLineRecord& Line, FString& OutError)
	{
		if (!Call.bHasTarget || Call.Target.IsNil())
		{
			OutError = TEXT("The speaker is nil.");
			return false;
		}

		const FBangoType& Type = Call.Target.GetType();
		if (Type == FBangoType::String() || Type == FBangoType::Entity())
		{
			if (!ValueToSpeakerKey(Call, Call.Target, TEXT("The speaker"), Line.Speaker, OutError))
			{
				return false;
			}

			// Only a speaker: directed at their speech focus, as it is now (kept with the line, so a saved line keeps it).
			UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
			const FName Focus = BangoYap ? BangoYap->GetSpeechFocus(FName(*Line.Speaker)) : NAME_None;
			Line.DirectedAt = Focus.IsNone() ? FString() : Focus.ToString();
			return true;
		}

		if (Type == FBangoType::ArrayOf(EBangoBaseType::String) || Type == FBangoType::ArrayOf(EBangoBaseType::Entity))
		{
			// [speaker, directedAt], or [speaker] / [speaker, nil]: directed at no one (not at the speaker's focus).
			const TArray<FBangoValue>& Who = Call.Target.GetArray();
			if (Who.Num() < 1 || Who.Num() > 2)
			{
				OutError = FString::Printf(TEXT("A line's speaker is a character (an ID or an entity), or [speaker, directedAt], or [speaker]: an array of one or two, but this has %d."), Who.Num());
				return false;
			}
			if (!ValueToSpeakerKey(Call, Who[0], TEXT("The speaker"), Line.Speaker, OutError))
			{
				return false;
			}
			Line.DirectedAt.Reset();
			if (Who.Num() == 2 && !Who[1].IsNil()
				&& !ValueToSpeakerKey(Call, Who[1], TEXT("Who it's said to"), Line.DirectedAt, OutError))
			{
				return false;
			}
			return true;
		}

		OutError = FString::Printf(TEXT("A line's speaker is a character's ID (a string), an actor with a Yap Character Component (an entity), [speaker, directedAt] or [speaker], but this is a %s."), *Type.ToString());
		return false;
	}

	/** Mood, TimeMode, Padding, Interruptible and Advance, from argument First on. */
	static bool ReadLineSettings(const FBangoCommandCall& Call, int32 First, FLineRecord& Line, FString& OutError)
	{
		Line.Mood = ReadString(Call, First);

		if (!ReadSetting(Call, First + 1, 4, TEXT("TimeMode (1 text, 2 audio, 3 manual, 4 none)"), Line.TimeMode, OutError))
		{
			return false;
		}

		if (const FBangoValue* Padding = Arg(Call, First + 2))
		{
			Line.bHasPadding = true;
			Line.Padding = (float)Padding->GetNumberAsDouble();
		}

		return ReadSetting(Call, First + 3, 2, TEXT("Interruptible (1 interruptible, 2 forced)"), Line.Interruptible, OutError)
			&& ReadSetting(Call, First + 4, 2, TEXT("Advance (1 auto, 2 manual)"), Line.Advance, OutError);
	}

	/** A child-safe text's key should be its line's ID with the suffix (Project Settings > Yap): a warning if not. */
	static void CheckChildSafeKey(const FLineRecord& Line)
	{
		const FString ChildSafeKey = GetLineID(Line.ChildSafeText);
		const FString Expected = Line.LineID + GetDefault<UYapSettings>()->ChildSafeKeySuffix;
		if (!Line.LineID.IsEmpty() && ChildSafeKey != Expected)
		{
			Bango::ReportNote(FString::Printf(TEXT("SayM: the child-safe text's key is '%s', but for the line '%s' it should be '%s'."), *ChildSafeKey, *Line.LineID, *Expected), FColor::Yellow, true);
		}
	}

	static EYapToggle ToToggle(int32 Setting)
	{
		return Setting == 1 ? EYapToggle::Yes : (Setting == 2 ? EYapToggle::No : EYapToggle::Default);
	}

	/**
	 * The YapBase request for a line. bOutWaiting: its conversation isn't open yet (it's queued, or opening, or being
	 * opened again after a load): try again next frame.
	 */
	static bool BuildRequest(const FBangoCommandCall& Call, const FLineRecord& Line, FYapLineRequest& OutRequest, bool& bOutWaiting, FString& OutError)
	{
		bOutWaiting = false;

		UYapSubsystem* Yap = GetYap(Call);
		UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
		if (!Yap || !BangoYap)
		{
			OutError = NoYapError;
			return false;
		}

		UYapSpeechConfig* Config = Yap->FindSpeechConfig(Line.ConfigName.IsEmpty() ? NAME_None : FName(*Line.ConfigName), OutError);
		if (!Config)
		{
			return false;
		}

		OutRequest = FYapLineRequest();
		OutRequest.Config = Config;

		if (!Line.ConversationID.IsEmpty())
		{
			OutRequest.Conversation = BangoYap->EnsureConversation(Line.ConversationID, FName(*Line.ConversationName));
			if (Yap->GetConversationState(OutRequest.Conversation) != EYapConversationState::Open)
			{
				bOutWaiting = true;
				return true;
			}
		}

		// Characters named by entity are found again now (their IDs can change between plays and after a load).
		if (!KeyToCharacterID(Call, Line.Speaker, OutRequest.Speaker, OutError)
			|| !KeyToCharacterID(Call, Line.DirectedAt, OutRequest.DirectedAt, OutError))
		{
			return false;
		}
		OutRequest.LineID = Line.LineID.IsEmpty() ? NAME_None : FName(*Line.LineID);
		OutRequest.MatureText = Line.MatureText;
		OutRequest.ChildSafeText = Line.ChildSafeText;
		OutRequest.TitleText = Line.TitleText;
		OutRequest.Mood = Line.Mood.IsEmpty() ? NAME_None : FName(*Line.Mood);
		OutRequest.TimeMode = static_cast<EYapTimeMode>(Line.TimeMode);
		OutRequest.bHasPadding = Line.bHasPadding;
		OutRequest.Padding = Line.Padding;
		OutRequest.Interruptible = ToToggle(Line.Interruptible);
		OutRequest.AutoAdvance = ToToggle(Line.Advance);
		OutRequest.StartOffset = Line.Elapsed;
		return true;
	}

	/**
	 * Plays a line to where what follows it may start: starts it in YapBase (or again, from how far it had played, if
	 * YapBase doesn't have it: after a load), then waits. Line is updated (its handle and how far it's played) for
	 * saving.
	 */
	static EBangoCommandResult DriveLine(FBangoCommandCall& Call, FLineRecord& Line)
	{
		UYapSubsystem* Yap = GetYap(Call);
		if (!Yap)
		{
			return Call.Fail(NoYapError);
		}

		FYapSpeechHandle Speech = FYapSpeechHandle::FromString(Line.SpeechHandle);
		EYapLineState State = Speech.IsValid() ? Yap->GetLineState(Speech) : EYapLineState::Unknown;

		if (State == EYapLineState::Unknown)
		{
			FYapLineRequest Request;
			bool bWaiting = false;
			FString Error;
			if (!BuildRequest(Call, Line, Request, bWaiting, Error))
			{
				return Call.Fail(Error);
			}
			if (bWaiting)
			{
				return EBangoCommandResult::Pending;
			}
			if (!Yap->RunLine(Request, Speech, Error))
			{
				return Call.Fail(Error);
			}
			Line.SpeechHandle = Speech.ToString();
			State = Yap->GetLineState(Speech);
		}

		if (State == EYapLineState::Finished)
		{
			Yap->ReleaseLine(Speech);
			return EBangoCommandResult::Done;
		}

		Line.Elapsed = FMath::Max(Yap->GetLineElapsed(Speech), 0.0f);
		return EBangoCommandResult::Pending;
	}

	static void CancelLine(const FBangoCommandCall& Call, const FLineRecord& Line)
	{
		UYapSubsystem* Yap = GetYap(Call);
		const FYapSpeechHandle Speech = FYapSpeechHandle::FromString(Line.SpeechHandle);
		if (Yap && Speech.IsValid())
		{
			Yap->CancelSpeech(Speech);
			Yap->ReleaseLine(Speech);
		}
	}

	/** A Conversation block's state: [ID, Name]. */
	static bool ReadConversationFields(const FBangoValue* State, FString& OutID, FString& OutName)
	{
		FFields Fields;
		if (!State || !ValueToFields(*State, Fields) || Fields.Num() < 2)
		{
			return false;
		}
		OutID = Fields[0].ToString();
		OutName = Fields[1].ToString();
		return true;
	}

	/**
	 * The innermost Conversation block open in the calling script, through the functions it's in (not a spawn copy's
	 * or another script's): its ID and name, empty if there's none.
	 */
	static void GetCurrentConversation(const FBangoCommandCall& Call, FString& OutID, FString& OutName)
	{
		OutID.Reset();
		OutName.Reset();
		if (!ReadConversationFields(Call.FindOpenBlock(TEXT("Conversation")), OutID, OutName))
		{
			OutID.Reset();
			OutName.Reset();
		}
	}

	static bool CheckConfigExists(const FBangoCommandCall& Call, const FString& ConfigName, FString& OutError)
	{
		UYapSubsystem* Yap = GetYap(Call);
		if (!Yap)
		{
			OutError = NoYapError;
			return false;
		}
		return Yap->FindSpeechConfig(ConfigName.IsEmpty() ? NAME_None : FName(*ConfigName), OutError) != nullptr;
	}

	static void Register(const FBangoCommand& Command)
	{
		FBangoModule& Bango = FBangoModule::Get();
		if (Bango.GetCommands().FindCommand(Command.Signature.Name))
		{
			return; // Already registered (the module was started again, e.g. by Live Coding). Commands can't be removed.
		}
		if (!Bango.RegisterCommand(Command))
		{
			UE_LOG(LogBangoYap, Error, TEXT("Couldn't register the Bango command '%s'."), *Command.Signature.Name);
		}
	}

	/** The optional settings every line and entry ends with. */
	static FBangoCommandSignature& AddLineSettings(FBangoCommandSignature& Signature)
	{
		return Signature
			.Param(TEXT("Mood"), FBangoType::String(), TEXT("The mood's name under the config's mood root (\"Happy\"); nil: the default mood")).MayBeNil()
			.Param(TEXT("TimeMode"), FBangoType::Int(), TEXT("1 text, 2 audio, 3 manual (the padding is the time), 4 none (until advanced); nil: the config's")).MayBeNil()
			.Param(TEXT("Padding"), FBangoType::Double(), TEXT("Seconds after the line; negative: what follows starts before it ends; nil: the config's")).MayBeNil()
			.Param(TEXT("Interruptible"), FBangoType::Int(), TEXT("Conversations: 1 interruptible, 2 forced; nil: the speech's")).MayBeNil()
			.Param(TEXT("Advance"), FBangoType::Int(), TEXT("Conversations: 1 auto, 2 manual; nil: the speech's")).MayBeNil();
	}

	// ============================================================================================
	// Speech(RunMode, Config, Interruptible, Advance) { ... }

	/** The first entry after After that passed (when last worked out), or INDEX_NONE. */
	static int32 NextPassedEntry(const FBangoCommandCall& Call, int32 After)
	{
		for (int32 Index = FMath::Max(After + 1, 0); Index < Call.GetEntryCount(); ++Index)
		{
			if (Call.EntryPassed(Index))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** One of the entries that passed, at random, or INDEX_NONE. */
	static int32 PickRandomPassedEntry(const FBangoCommandCall& Call)
	{
		TArray<int32> Passed;
		for (int32 Index = 0; Index < Call.GetEntryCount(); ++Index)
		{
			if (Call.EntryPassed(Index))
			{
				Passed.Add(Index);
			}
		}
		return Passed.Num() > 0 ? Passed[FMath::RandRange(0, Passed.Num() - 1)] : INDEX_NONE;
	}

	static void RegisterSpeech()
	{
		FBangoCommand Command;
		Command.Signature = FBangoCommandSignature::Make(TEXT("Speech"))
			.Optional()
			.Param(TEXT("RunMode"), FBangoType::Int(), TEXT("1 run all, 2 select one (the first reached), 3 select random; nil: run all")).MayBeNil()
			.Param(TEXT("Config"), FBangoType::String(), TEXT("The speech config's name; nil: the project's default")).MayBeNil()
			.Param(TEXT("Interruptible"), FBangoType::Int(), TEXT("Conversations: 1 interruptible, 2 forced; nil: the config's")).MayBeNil()
			.Param(TEXT("Advance"), FBangoType::Int(), TEXT("Conversations: 1 auto, 2 manual; nil: the config's")).MayBeNil()
			.Block()
			.Entries(/*bOptional*/ true)
			.InCategory(Category)
			.Describe(TEXT("A group of lines, said one after another (run all), or one of them (select one, select random). In a Conversation block, they're part of the conversation; otherwise free speech. Its block can be a list of entries (entry if (condition): / entry:), each a line or several: run all plays each entry that passes, select one the first, select random one of them."));

		Command.Start = [](FBangoCommandCall& Call)
		{
			FSpeechHeader Header;
			FString Error;
			if (!ReadSetting(Call, 0, 3, TEXT("RunMode (1 run all, 2 select one, 3 select random)"), Header.RunMode, Error)
				|| !ReadSetting(Call, 2, 2, TEXT("Interruptible (1 interruptible, 2 forced)"), Header.Interruptible, Error)
				|| !ReadSetting(Call, 3, 2, TEXT("Advance (1 auto, 2 manual)"), Header.Advance, Error))
			{
				return Call.Fail(Error);
			}
			if (Header.RunMode == 0)
			{
				Header.RunMode = (int32)ERunMode::RunAll;
			}

			Header.ConfigName = ReadString(Call, 1);
			if (!CheckConfigExists(Call, Header.ConfigName, Error))
			{
				return Call.Fail(Error);
			}

			GetCurrentConversation(Call, Header.ConversationID, Header.ConversationName);

			// A list of entries: the speech picks them, and their lines play as they come.
			if (Call.HasEntries())
			{
				Header.bEntries = true;
				const int32 Entry = Header.RunMode == (int32)ERunMode::SelectRandom ? PickRandomPassedEntry(Call) : NextPassedEntry(Call, -1);
				if (Entry != INDEX_NONE)
				{
					Header.bPlayedEntry = !Call.EntryIsDefault(Entry);
					Call.RunEntry(Entry);
				}
			}

			*Call.BlockState = RowsToValue({ Header.ToFields() });
			return EBangoCommandResult::Done;
		};

		// Run all goes on to the next entry that passes, checked again so that what this one did counts. Select one and
		// select random play only the one.
		Command.EntryEnded = [](FBangoCommandCall& Call)
		{
			TArray<FFields> Rows;
			if (!ValueToRows(*Call.BlockState, Rows))
			{
				return EBangoCommandResult::Done;
			}
			FSpeechHeader Header;
			Header.FromFields(Rows[0]);
			if (Header.RunMode != (int32)ERunMode::RunAll)
			{
				return EBangoCommandResult::Done;
			}
			Call.RecheckEntries();
			return EBangoCommandResult::Pending; // Poll comes straight away, with the new answers
		};

		// Select random: the lines reached were gathered in the block's state; one plays now.
		Command.Close = [](FBangoCommandCall& Call)
		{
			TArray<FFields> Rows;
			if (Call.bLeftEarly || !ValueToRows(*Call.BlockState, Rows))
			{
				return EBangoCommandResult::Done;
			}

			FSpeechHeader Header;
			Header.FromFields(Rows[0]);
			if (Header.bEntries || Header.RunMode != (int32)ERunMode::SelectRandom || Rows.Num() < 2)
			{
				return EBangoCommandResult::Done;
			}

			Header.RunningRow = FMath::RandRange(1, Rows.Num() - 1);
			FLineRecord Line;
			Line.FromFields(Rows[Header.RunningRow]);

			const EBangoCommandResult Result = DriveLine(Call, Line);
			Rows[0] = Header.ToFields();
			Rows[Header.RunningRow] = Line.ToFields();
			*Call.BlockState = RowsToValue(Rows);
			return Result;
		};

		Command.Poll = [](FBangoCommandCall& Call)
		{
			// Run all, after an entry's re-check: the next entry that passes now. The default (default:) plays only if no
			// other entry has: after one has, the re-check can find none of the rest passing, and the default with them.
			if (Call.bEntryEnding)
			{
				TArray<FFields> EntryRows;
				FSpeechHeader EntryHeader;
				const bool bHasHeader = ValueToRows(*Call.BlockState, EntryRows);
				if (bHasHeader)
				{
					EntryHeader.FromFields(EntryRows[0]);
				}

				const int32 Next = NextPassedEntry(Call, Call.EntryNumber);
				if (Next == INDEX_NONE || (Call.EntryIsDefault(Next) && EntryHeader.bPlayedEntry))
				{
					return EBangoCommandResult::Done;
				}

				if (bHasHeader && !Call.EntryIsDefault(Next))
				{
					EntryHeader.bPlayedEntry = true;
					EntryRows[0] = EntryHeader.ToFields();
					*Call.BlockState = RowsToValue(EntryRows);
				}
				Call.RunEntry(Next);
				return EBangoCommandResult::Done;
			}

			TArray<FFields> Rows;
			if (!Call.bClosing || !ValueToRows(*Call.BlockState, Rows))
			{
				return EBangoCommandResult::Done;
			}

			FSpeechHeader Header;
			Header.FromFields(Rows[0]);
			if (!Rows.IsValidIndex(Header.RunningRow) || Header.RunningRow == 0)
			{
				return EBangoCommandResult::Done;
			}

			FLineRecord Line;
			Line.FromFields(Rows[Header.RunningRow]);
			const EBangoCommandResult Result = DriveLine(Call, Line);
			Rows[Header.RunningRow] = Line.ToFields();
			*Call.BlockState = RowsToValue(Rows);
			return Result;
		};

		Command.Abandoned = [](FBangoCommandCall& Call)
		{
			TArray<FFields> Rows;
			if (Call.bBlockAbandoned || !Call.BlockState || !ValueToRows(*Call.BlockState, Rows))
			{
				return EBangoCommandResult::Done;
			}

			FSpeechHeader Header;
			Header.FromFields(Rows[0]);
			if (Rows.IsValidIndex(Header.RunningRow) && Header.RunningRow > 0)
			{
				FLineRecord Line;
				Line.FromFields(Rows[Header.RunningRow]);
				CancelLine(Call, Line);
			}
			return EBangoCommandResult::Done;
		};

		Register(Command);
	}

	// ============================================================================================
	// Speaker Say(Text, ...); and Speaker SayM(MatureText, ChildSafeText, ...);

	static void RegisterSay(bool bMature)
	{
		const FBangoTypeVar Who(TEXT("T"));

		FBangoCommandSignature Signature = FBangoCommandSignature::Make(bMature ? TEXT("SayM") : TEXT("Say"))
			.AnyTypeParam(Who, TEXT("speaker"))
			.Target(Who);

		if (bMature)
		{
			Signature
				.Param(TEXT("MatureText"), FBangoType::Text(), TEXT("What's said to mature players; its key is the line's ID"))
				.Param(TEXT("ChildSafeText"), FBangoType::Text(), TEXT("What's said to child-safe players; its key is the line's ID with the child-safe suffix"));
		}
		else
		{
			Signature.Param(TEXT("Text"), FBangoType::Text(), TEXT("What's said; its key is the line's ID"));
		}

		Signature.Optional();
		AddLineSettings(Signature)
			.Inside(TEXT("Speech"), /*bOptional*/ true)
			.InCategory(Category)
			.Describe(bMature
				? TEXT("A line with mature and child-safe text, said by a character (an ID, or an actor with a Yap Character Component; or [speaker, directedAt]). In a Speech block, one of its lines; on its own, as if in Speech(RUN_ALL).")
				: TEXT("A line said by a character (an ID, or an actor with a Yap Character Component; or [speaker, directedAt]). In a Speech block, one of its lines; on its own, as if in Speech(RUN_ALL)."));

		FBangoCommand Command;
		Command.Signature = Signature;

		Command.Start = [bMature](FBangoCommandCall& Call)
		{
			TArray<FFields> Rows;
			FSpeechHeader Header;
			FString Error;

			if (Call.BlockState)
			{
				// One of a Speech block's lines.
				if (!ValueToRows(*Call.BlockState, Rows))
				{
					return Call.Fail(TEXT("The Speech block this line is in has no state."));
				}
				Header.FromFields(Rows[0]);
			}
			else
			{
				// On its own: as if in Speech(RUN_ALL), with the default config, in the conversation it's in (if any).
				Header.RunMode = (int32)ERunMode::RunAll;
				if (!CheckConfigExists(Call, Header.ConfigName, Error))
				{
					return Call.Fail(Error);
				}
				GetCurrentConversation(Call, Header.ConversationID, Header.ConversationName);
			}

			FLineRecord Line;
			if (!ReadSpeaker(Call, Line, Error))
			{
				return Call.Fail(Error);
			}

			const int32 FirstSetting = bMature ? 2 : 1;
			const FBangoValue* Text = Arg(Call, 0);
			if (!Text)
			{
				return Call.Fail(TEXT("The line's text is nil."));
			}
			Line.MatureText = Text->GetText();
			Line.LineID = GetLineID(Line.MatureText);

			if (bMature)
			{
				const FBangoValue* ChildSafe = Arg(Call, 1);
				if (!ChildSafe)
				{
					return Call.Fail(TEXT("The line's child-safe text is nil."));
				}
				Line.ChildSafeText = ChildSafe->GetText();
				CheckChildSafeKey(Line);
			}

			if (!ReadLineSettings(Call, FirstSetting, Line, Error))
			{
				return Call.Fail(Error);
			}

			// The speech's settings, where the line has none of its own.
			Line.ConfigName = Header.ConfigName;
			Line.ConversationID = Header.ConversationID;
			Line.ConversationName = Header.ConversationName;
			if (Line.Interruptible == 0)
			{
				Line.Interruptible = Header.Interruptible;
			}
			if (Line.Advance == 0)
			{
				Line.Advance = Header.Advance;
			}

			// In a Speech whose block is a list of entries, the speech chose the entry, and its lines just play. Otherwise
			// the line is one of the speech's choices.
			const bool bSpeechChooses = Call.BlockState && !Header.bEntries;

			// Select random: gather it; the Speech block's Close plays one.
			if (bSpeechChooses && Header.RunMode == (int32)ERunMode::SelectRandom)
			{
				Rows.Add(Line.ToFields());
				*Call.BlockState = RowsToValue(Rows);
				return EBangoCommandResult::Done;
			}

			const EBangoCommandResult Result = DriveLine(Call, Line);
			if (Result == EBangoCommandResult::Pending)
			{
				Call.LatentState = { FieldsToValue(Line.ToFields()) };
			}
			else if (Result == EBangoCommandResult::Done && bSpeechChooses && Header.RunMode == (int32)ERunMode::SelectOne)
			{
				Call.EndBlock();
			}
			return Result;
		};

		Command.Poll = [](FBangoCommandCall& Call)
		{
			FFields Fields;
			if (Call.LatentState.Num() < 1 || !ValueToFields(Call.LatentState[0], Fields))
			{
				return Call.Fail(TEXT("The line's saved state is missing."));
			}

			FLineRecord Line;
			Line.FromFields(Fields);
			const EBangoCommandResult Result = DriveLine(Call, Line);
			Call.LatentState[0] = FieldsToValue(Line.ToFields());

			if (Result == EBangoCommandResult::Done)
			{
				TArray<FFields> Rows;
				if (Call.BlockState && ValueToRows(*Call.BlockState, Rows))
				{
					FSpeechHeader Header;
					Header.FromFields(Rows[0]);
					if (!Header.bEntries && Header.RunMode == (int32)ERunMode::SelectOne)
					{
						Call.EndBlock();
					}
				}
			}
			return Result;
		};

		Command.Abandoned = [](FBangoCommandCall& Call)
		{
			FFields Fields;
			if (Call.LatentState.Num() > 0 && ValueToFields(Call.LatentState[0], Fields))
			{
				FLineRecord Line;
				Line.FromFields(Fields);
				CancelLine(Call, Line);
			}
			return EBangoCommandResult::Done;
		};

		Register(Command);
	}

	// ============================================================================================
	// Conversation(Name) { ... }

	static bool ReadConversationState(const FBangoCommandCall& Call, FString& OutID, FString& OutName)
	{
		return ReadConversationFields(Call.BlockState, OutID, OutName);
	}

	/** Waits for the block's conversation to be open (opening it again after a load). */
	static EBangoCommandResult WaitForOpen(FBangoCommandCall& Call)
	{
		UYapSubsystem* Yap = GetYap(Call);
		UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
		if (!Yap || !BangoYap)
		{
			return Call.Fail(NoYapError);
		}

		FString ID;
		FString Name;
		if (!ReadConversationState(Call, ID, Name))
		{
			return Call.Fail(TEXT("The Conversation block has no state."));
		}

		const FYapConversationHandle Handle = BangoYap->EnsureConversation(ID, Name.IsEmpty() ? NAME_None : FName(*Name));
		return Yap->GetConversationState(Handle) == EYapConversationState::Open ? EBangoCommandResult::Done : EBangoCommandResult::Pending;
	}

	static EBangoCommandResult WaitForClosed(FBangoCommandCall& Call)
	{
		UYapSubsystem* Yap = GetYap(Call);
		UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
		FString ID;
		FString Name;
		if (!Yap || !BangoYap || !ReadConversationState(Call, ID, Name))
		{
			return EBangoCommandResult::Done;
		}

		const FYapConversationHandle Handle = BangoYap->FindConversation(ID);
		if (Yap->GetConversationState(Handle) == EYapConversationState::None)
		{
			BangoYap->ForgetConversation(ID);
			return EBangoCommandResult::Done;
		}
		return EBangoCommandResult::Pending;
	}

	static void RegisterConversation()
	{
		FBangoCommand Command;
		Command.Signature = FBangoCommandSignature::Make(TEXT("Conversation"))
			.Optional()
			.Param(TEXT("Name"), FBangoType::String(), TEXT("The conversation's name, for the game; nil: none")).MayBeNil()
			.Block()
			.InCategory(Category)
			.Describe(TEXT("Opens a conversation for its block: the speech and prompts the script runs in it (in functions it calls too) are the conversation's. Waits here while another conversation is open."));

		Command.Start = [](FBangoCommandCall& Call)
		{
			const FString Name = ReadString(Call, 0);
			const FString ID = FGuid::NewGuid().ToString(EGuidFormats::Digits);
			*Call.BlockState = FieldsToValue({ ToField(ID), ToField(Name) });
			return WaitForOpen(Call);
		};

		Command.Poll = [](FBangoCommandCall& Call)
		{
			return Call.bClosing ? WaitForClosed(Call) : WaitForOpen(Call);
		};

		Command.Close = [](FBangoCommandCall& Call)
		{
			UYapSubsystem* Yap = GetYap(Call);
			UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
			FString ID;
			FString Name;
			if (!Yap || !BangoYap || !ReadConversationState(Call, ID, Name))
			{
				return EBangoCommandResult::Done;
			}

			Yap->CloseConversation(BangoYap->FindConversation(ID));
			return WaitForClosed(Call);
		};

		Command.Abandoned = [](FBangoCommandCall& Call)
		{
			UYapSubsystem* Yap = GetYap(Call);
			UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
			FString ID;
			FString Name;
			if (!Yap || !BangoYap || !ReadConversationState(Call, ID, Name))
			{
				return EBangoCommandResult::Done;
			}

			Yap->CloseConversation(BangoYap->FindConversation(ID), true);
			BangoYap->ForgetConversation(ID);
			return EBangoCommandResult::Done;
		};

		Register(Command);
	}

	// ============================================================================================
	// Prompt(Config) { ... } and Speaker AddPrompt(Text, TitleText, ...) { ... }

	/** Shows the prompt (again, after a load), waits for the player's choice, then says the chosen entry and runs its block. */
	static EBangoCommandResult DrivePrompt(FBangoCommandCall& Call, TArray<FFields>& Rows)
	{
		UYapSubsystem* Yap = GetYap(Call);
		UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
		if (!Yap || !BangoYap)
		{
			return Call.Fail(NoYapError);
		}

		FPromptHeader Header;
		Header.FromFields(Rows[0]);

		// With entries: whether a row's entry passes now (the entries are watched while the player chooses).
		auto RowShown = [&Call, &Header](const FLineRecord& Entry)
		{
			return !Header.bEntries || Call.EntryPassed(Entry.EntryNumber);
		};

		if (Header.Phase < 2)
		{
			FYapPromptHandle Prompt = FYapPromptHandle::FromString(Header.PromptHandle);
			int32 Chosen = INDEX_NONE;

			// Watched entries changed while the player chooses: show the options that have come true, hide the rest.
			if (Header.bEntries && Call.bEntriesChanged && Prompt.IsValid() && Yap->GetPromptChoice(Prompt, Chosen) && Chosen == INDEX_NONE)
			{
				for (int32 Row = 1; Row < Rows.Num(); ++Row)
				{
					FLineRecord Entry;
					Entry.FromFields(Rows[Row]);
					Yap->SetPromptEntryShown(Prompt, Row - 1, RowShown(Entry));
				}
			}

			if (!Prompt.IsValid() || !Yap->GetPromptChoice(Prompt, Chosen))
			{
				// Show it.
				FString Error;
				UYapSpeechConfig* Config = Yap->FindSpeechConfig(Header.ConfigName.IsEmpty() ? NAME_None : FName(*Header.ConfigName), Error);
				if (!Config)
				{
					return Call.Fail(Error);
				}

				FYapPromptRequest Request;
				Request.Config = Config;
				Request.Conversation = BangoYap->EnsureConversation(Header.ConversationID, Header.ConversationName.IsEmpty() ? NAME_None : FName(*Header.ConversationName));
				if (Yap->GetConversationState(Request.Conversation) != EYapConversationState::Open)
				{
					Rows[0] = Header.ToFields();
					return EBangoCommandResult::Pending;
				}

				for (int32 Row = 1; Row < Rows.Num(); ++Row)
				{
					FLineRecord Entry;
					Entry.FromFields(Rows[Row]);
					Entry.ConversationID.Reset(); // the prompt's conversation is checked above
					if (!RowShown(Entry))
					{
						Request.HiddenEntries.Add(Row - 1);
					}

					FYapLineRequest& EntryRequest = Request.Entries.AddDefaulted_GetRef();
					bool bWaiting = false;
					if (!BuildRequest(Call, Entry, EntryRequest, bWaiting, Error))
					{
						return Call.Fail(FString::Printf(TEXT("Prompt entry %d: %s"), Row, *Error));
					}
				}

				// Every option's condition is false for now: wait for one to come true.
				if (Request.HiddenEntries.Num() == Request.Entries.Num())
				{
					Rows[0] = Header.ToFields();
					return EBangoCommandResult::Pending;
				}

				if (!Yap->StartPrompt(Request, Prompt, Error))
				{
					return Call.Fail(Error);
				}

				Header.PromptHandle = Prompt.ToString();
				Header.Phase = 1;
				Yap->GetPromptChoice(Prompt, Chosen);
			}

			if (Chosen == INDEX_NONE)
			{
				Rows[0] = Header.ToFields();
				return EBangoCommandResult::Pending;
			}

			// Chosen, but its condition stopped holding in the same frame: show the prompt again (next Poll).
			if (Header.bEntries && Rows.IsValidIndex(Chosen + 1))
			{
				FLineRecord Entry;
				Entry.FromFields(Rows[Chosen + 1]);
				if (!RowShown(Entry))
				{
					Yap->ReleasePrompt(Prompt);
					Header.PromptHandle.Reset();
					Header.Phase = 0;
					Rows[0] = Header.ToFields();
					return EBangoCommandResult::Pending;
				}
			}

			// Chosen: say it.
			Yap->ReleasePrompt(Prompt);
			Header.Phase = 2;
			Header.ChosenRow = Chosen + 1;
			if (Header.bEntries)
			{
				Call.WatchEntries(false);
			}

			FString Error;
			if (UYapSpeechConfig* Config = Yap->FindSpeechConfig(Header.ConfigName.IsEmpty() ? NAME_None : FName(*Header.ConfigName), Error))
			{
				if (Config->bPromptAdvancesImmediately && Rows.IsValidIndex(Header.ChosenRow))
				{
					FLineRecord Entry;
					Entry.FromFields(Rows[Header.ChosenRow]);
					Entry.Advance = 1;
					Rows[Header.ChosenRow] = Entry.ToFields();
				}
			}
		}

		if (!Rows.IsValidIndex(Header.ChosenRow) || Header.ChosenRow == 0)
		{
			return Call.Fail(TEXT("The chosen prompt entry is missing from the prompt's state."));
		}

		FLineRecord Entry;
		Entry.FromFields(Rows[Header.ChosenRow]);
		const EBangoCommandResult Result = DriveLine(Call, Entry);
		Rows[0] = Header.ToFields();
		Rows[Header.ChosenRow] = Entry.ToFields();

		if (Result == EBangoCommandResult::Done)
		{
			Call.RunBlock(Entry.BlockNumber);
		}
		return Result;
	}

	static void RegisterPrompt()
	{
		FBangoCommand Command;
		Command.Signature = FBangoCommandSignature::Make(TEXT("Prompt"))
			.Optional()
			.Param(TEXT("Config"), FBangoType::String(), TEXT("The speech config's name; nil: the project's default")).MayBeNil()
			.Block()
			.Entries(/*bOptional*/ true)
			.EntriesHold(TEXT("AddPrompt"))
			.InCategory(Category)
			.Describe(TEXT("Shows the player the AddPrompts reached in its block, and waits for their choice: the chosen one is said, then its block runs. Must be in a conversation. With none reached, does nothing. Its block can be a list of entries (entry if (condition): / entry:), each holding one AddPrompt: an option is shown while its condition holds, even if it comes true while the player chooses."));

		Command.Start = [](FBangoCommandCall& Call)
		{
			FPromptHeader Header;
			FString Error;
			Header.ConfigName = ReadString(Call, 0);
			if (!CheckConfigExists(Call, Header.ConfigName, Error))
			{
				return Call.Fail(Error);
			}

			GetCurrentConversation(Call, Header.ConversationID, Header.ConversationName);
			if (Header.ConversationID.IsEmpty())
			{
				return Call.Fail(TEXT("A Prompt must be in a Conversation block."));
			}

			// A list of entries: every one runs, passed or not, so each AddPrompt is gathered (and is there to show if
			// its condition comes true while the player chooses).
			if (Call.HasEntries())
			{
				Header.bEntries = true;
				if (Call.GetEntryCount() > 0)
				{
					Call.RunEntry(0);
				}
			}

			*Call.BlockState = RowsToValue({ Header.ToFields() });
			return EBangoCommandResult::Done;
		};

		Command.EntryEnded = [](FBangoCommandCall& Call)
		{
			if (Call.EntryNumber + 1 < Call.GetEntryCount())
			{
				Call.RunEntry(Call.EntryNumber + 1);
			}
			return EBangoCommandResult::Done;
		};

		Command.Close = [](FBangoCommandCall& Call)
		{
			TArray<FFields> Rows;
			if (Call.bLeftEarly || !ValueToRows(*Call.BlockState, Rows) || Rows.Num() < 2)
			{
				return EBangoCommandResult::Done; // left early, or no entry was reached
			}

			FPromptHeader Header;
			Header.FromFields(Rows[0]);
			if (Header.bEntries)
			{
				// As without entries, a prompt with no option to show at the start does nothing.
				bool bAnyShown = false;
				for (int32 Row = 1; Row < Rows.Num() && !bAnyShown; ++Row)
				{
					FLineRecord Entry;
					Entry.FromFields(Rows[Row]);
					bAnyShown = Call.EntryPassed(Entry.EntryNumber);
				}
				if (!bAnyShown)
				{
					return EBangoCommandResult::Done;
				}

				// Watch the conditions while the player chooses.
				Call.WatchEntries(true);
			}

			const EBangoCommandResult Result = DrivePrompt(Call, Rows);
			*Call.BlockState = RowsToValue(Rows);
			return Result;
		};

		Command.Poll = [](FBangoCommandCall& Call)
		{
			TArray<FFields> Rows;
			if (!Call.bClosing || !ValueToRows(*Call.BlockState, Rows) || Rows.Num() < 2)
			{
				return EBangoCommandResult::Done;
			}

			const EBangoCommandResult Result = DrivePrompt(Call, Rows);
			*Call.BlockState = RowsToValue(Rows);
			return Result;
		};

		Command.Abandoned = [](FBangoCommandCall& Call)
		{
			TArray<FFields> Rows;
			if (Call.bBlockAbandoned || !Call.BlockState || !ValueToRows(*Call.BlockState, Rows))
			{
				return EBangoCommandResult::Done;
			}

			FPromptHeader Header;
			Header.FromFields(Rows[0]);

			if (UYapSubsystem* Yap = GetYap(Call))
			{
				if (Header.Phase == 1)
				{
					Yap->CancelPrompt(FYapPromptHandle::FromString(Header.PromptHandle));
				}
			}
			if (Header.Phase == 2 && Rows.IsValidIndex(Header.ChosenRow) && Header.ChosenRow > 0)
			{
				FLineRecord Entry;
				Entry.FromFields(Rows[Header.ChosenRow]);
				CancelLine(Call, Entry);
			}
			return EBangoCommandResult::Done;
		};

		Register(Command);
	}

	static void RegisterAddPrompt()
	{
		const FBangoTypeVar Who(TEXT("T"));

		FBangoCommandSignature Signature = FBangoCommandSignature::Make(TEXT("AddPrompt"))
			.AnyTypeParam(Who, TEXT("speaker"))
			.Target(Who)
			.Param(TEXT("Text"), FBangoType::Text(), TEXT("What's said when it's chosen; its key is the line's ID"))
			.Optional()
			.Param(TEXT("TitleText"), FBangoType::Text(), TEXT("What the player picks; nil: the text")).MayBeNil();

		AddLineSettings(Signature)
			.Inside(TEXT("Prompt"))
			.Block()
			.InCategory(Category)
			.Describe(TEXT("One of a prompt's options: said by a character (an ID, or an actor with a Yap Character Component; or [speaker, directedAt]) when the player chooses it, then its block runs. In a Prompt's entry (entry if (condition):), it's shown while the condition holds."));

		FBangoCommand Command;
		Command.Signature = Signature;

		Command.Start = [](FBangoCommandCall& Call)
		{
			TArray<FFields> Rows;
			if (!Call.OuterBlockState || !ValueToRows(*Call.OuterBlockState, Rows))
			{
				return Call.Fail(TEXT("The Prompt block this entry is in has no state."));
			}

			FPromptHeader Header;
			Header.FromFields(Rows[0]);

			FLineRecord Entry;
			FString Error;
			if (!ReadSpeaker(Call, Entry, Error))
			{
				return Call.Fail(Error);
			}

			const FBangoValue* Text = Arg(Call, 0);
			if (!Text)
			{
				return Call.Fail(TEXT("The entry's text is nil."));
			}
			Entry.MatureText = Text->GetText();
			Entry.LineID = GetLineID(Entry.MatureText);

			if (const FBangoValue* Title = Arg(Call, 1))
			{
				Entry.TitleText = Title->GetText();
			}

			if (!ReadLineSettings(Call, 2, Entry, Error))
			{
				return Call.Fail(Error);
			}

			Entry.ConfigName = Header.ConfigName;
			Entry.ConversationID = Header.ConversationID;
			Entry.ConversationName = Header.ConversationName;
			Entry.BlockNumber = Call.BlockNumber;
			Entry.EntryNumber = Call.BlockEntryNumber;

			Rows.Add(Entry.ToFields());
			*Call.OuterBlockState = RowsToValue(Rows);

			// Its block runs only if it's chosen (the Prompt's Close runs it).
			Call.SkipBlock();
			return EBangoCommandResult::Done;
		};

		Register(Command);
	}
}

namespace BangoYapPrivate
{
	// ============================================================================================
	// Character SetSpeechFocus(Character);, Character ClearSpeechFocus(); and GetSpeechFocus(Character) -> string

	/** The character ID in a focus command's target, checked to be a known character. */
	/** A focus command's character (a string or an entity) as a speaker key, checked to be a known character. */
	static bool ReadFocusCharacter(const FBangoCommandCall& Call, UYapCharacterManager& Characters, const FBangoValue& Value, const TCHAR* What, FString& OutKey, FString& OutError)
	{
		if (!ValueToSpeakerKey(Call, Value, What, OutKey, OutError))
		{
			return false;
		}

		FName CharacterID;
		if (!KeyToCharacterID(Call, OutKey, CharacterID, OutError))
		{
			return false;
		}

		FString LookUpError;
		if (!Characters.LookUpCharacter(CharacterID, LookUpError))
		{
			OutError = FString::Printf(TEXT("%s: %s"), What, *LookUpError);
			return false;
		}
		return true;
	}

	static void RegisterSpeechFocus()
	{
		const FBangoTypeVar Who(TEXT("T"));
		const FBangoTypeVar Whom(TEXT("U"));

		{
			FBangoCommand Command;
			Command.Signature = FBangoCommandSignature::Make(TEXT("SetSpeechFocus"))
				.TypeParam(Who, { FBangoType::String(), FBangoType::Entity() }, TEXT("character"))
				.TypeParam(Whom, { FBangoType::String(), FBangoType::Entity() }, TEXT("character"))
				.Target(Who)
				.Param(TEXT("Character"), Whom, TEXT("Who the target character talks to (a character's ID, or an actor with a Yap Character Component)"))
				.InCategory(Category)
				.Describe(TEXT("Sets who a character (an ID, or an actor with a Yap Character Component) is talking to. Their lines that only name a speaker are directed at them, until it's changed (or ClearSpeechFocus)."));

			Command.Start = [](FBangoCommandCall& Call)
			{
				UYapCharacterManager* Characters = Call.WorldContext ? UYapCharacterManager::Get(Call.WorldContext) : nullptr;
				UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
				if (!Characters || !BangoYap)
				{
					return Call.Fail(NoYapError);
				}

				FString CharacterKey;
				FString Error;
				if (!ReadFocusCharacter(Call, *Characters, Call.Target, TEXT("The character"), CharacterKey, Error))
				{
					return Call.Fail(Error);
				}

				const FBangoValue* Focus = Arg(Call, 0);
				if (!Focus)
				{
					return Call.Fail(TEXT("Who they talk to is nil. To have them talk to no one, use ClearSpeechFocus."));
				}

				FString FocusKey;
				if (!ReadFocusCharacter(Call, *Characters, *Focus, TEXT("Who they talk to"), FocusKey, Error))
				{
					return Call.Fail(Error);
				}
				if (FocusKey == CharacterKey)
				{
					return Call.Fail(TEXT("A character can't talk to themselves (SetSpeechFocus)."));
				}

				BangoYap->SetSpeechFocus(FName(*CharacterKey), FName(*FocusKey));
				return EBangoCommandResult::Done;
			};

			Register(Command);
		}

		{
			FBangoCommand Command;
			Command.Signature = FBangoCommandSignature::Make(TEXT("ClearSpeechFocus"))
				.TypeParam(Who, { FBangoType::String(), FBangoType::Entity() }, TEXT("character"))
				.Target(Who)
				.InCategory(Category)
				.Describe(TEXT("The character talks to no one: their lines that only name a speaker aren't directed at anyone (SetSpeechFocus)."));

			Command.Start = [](FBangoCommandCall& Call)
			{
				UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
				if (!BangoYap)
				{
					return Call.Fail(NoYapError);
				}

				// No check that the character exists: clearing a focus that isn't there does nothing.
				FString CharacterKey;
				FString Error;
				if (!ValueToSpeakerKey(Call, Call.Target, TEXT("The character"), CharacterKey, Error))
				{
					return Call.Fail(Error);
				}

				BangoYap->SetSpeechFocus(FName(*CharacterKey), NAME_None);
				return EBangoCommandResult::Done;
			};

			Register(Command);
		}

		{
			FBangoCommand Command;
			Command.Signature = FBangoCommandSignature::Make(TEXT("GetSpeechFocus"))
				.TypeParam(Who, { FBangoType::String(), FBangoType::Entity() }, TEXT("character"))
				.Param(TEXT("Character"), Who, TEXT("The character (a character's ID, or an actor with a Yap Character Component)"))
				.Returns(FBangoType::String())
				.InCategory(Category)
				.Describe(TEXT("Who a character is talking to, as a character's ID, or nil if no one (SetSpeechFocus)."));

			Command.Start = [](FBangoCommandCall& Call)
			{
				UBangoYapSubsystem* BangoYap = GetBangoYap(Call);
				if (!BangoYap)
				{
					return Call.Fail(NoYapError);
				}

				const FBangoValue* Character = Arg(Call, 0);
				if (!Character)
				{
					return Call.Fail(TEXT("The character is nil."));
				}

				FString CharacterKey;
				FString Error;
				if (!ValueToSpeakerKey(Call, *Character, TEXT("The character"), CharacterKey, Error))
				{
					return Call.Fail(Error);
				}

				const FName Focus = BangoYap->GetSpeechFocus(FName(*CharacterKey));
				FName FocusID;
				if (Focus.IsNone() || !KeyToCharacterID(Call, Focus.ToString(), FocusID, Error))
				{
					// No one, or an actor that's gone.
					Call.Result = FBangoValue::MakeNil(FBangoType::String());
				}
				else
				{
					Call.Result = FBangoValue::MakeString(FocusID.ToString());
				}
				return EBangoCommandResult::Done;
			};

			Register(Command);
		}
	}
}

void BangoYap::RegisterCommands()
{
	using namespace BangoYapPrivate;

	RegisterSpeech();
	RegisterSay(false);
	RegisterSay(true);
	RegisterConversation();
	RegisterPrompt();
	RegisterAddPrompt();
	RegisterSpeechFocus();
}
