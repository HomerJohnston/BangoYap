#include "BangoYap/BangoYapSubsystem.h"

#include "BangoGlobalsSubsystem.h"
#include "BangoGlobalStore.h"
#include "BangoTypes.h"
#include "BangoYap/BangoYapLog.h"
#include "Engine/World.h"
#include "YapBase/YapSubsystem.h"

namespace BangoYapSubsystemPrivate
{
	/** Where focus is kept in Bango's globals: not a name a script can write. A string[][] of [character, focus]. */
	static const TCHAR* SpeechFocusKey = TEXT("BangoYap|SpeechFocus");

	static FBangoType SpeechFocusType() { return FBangoType::ArrayOf(EBangoBaseType::String, 2); }
}

UBangoYapSubsystem* UBangoYapSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBangoYapSubsystem>() : nullptr;
}

FYapConversationHandle UBangoYapSubsystem::EnsureConversation(const FString& ConversationID, FName Name)
{
	UYapSubsystem* Yap = UYapSubsystem::Get(this);
	if (!Yap || ConversationID.IsEmpty())
	{
		return FYapConversationHandle();
	}

	if (const FYapConversationHandle* Existing = Conversations.Find(ConversationID))
	{
		if (Yap->GetConversationState(*Existing) != EYapConversationState::None)
		{
			return *Existing;
		}
	}

	const FYapConversationHandle Handle = Yap->OpenConversation(Name);
	Conversations.Add(ConversationID, Handle);
	return Handle;
}

FYapConversationHandle UBangoYapSubsystem::FindConversation(const FString& ConversationID) const
{
	const FYapConversationHandle* Handle = Conversations.Find(ConversationID);
	return Handle ? *Handle : FYapConversationHandle();
}

void UBangoYapSubsystem::ForgetConversation(const FString& ConversationID)
{
	Conversations.Remove(ConversationID);
}

bool UBangoYapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ==================================================================================================== Speech focus

FBangoGlobalStore* UBangoYapSubsystem::GetGlobalStore() const
{
	UBangoGlobalsSubsystem* Globals = UBangoGlobalsSubsystem::Get(this);
	return Globals ? &Globals->GetStore() : nullptr;
}

void UBangoYapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// A new world: no one is talking to anyone yet. (Loading a save puts its focus back after this.)
	ClearSpeechFocus();
}

void UBangoYapSubsystem::Deinitialize()
{
	// Bango keeps its store on the game instance, across worlds: focus belongs to this one.
	ClearSpeechFocus();

	Super::Deinitialize();
}

void UBangoYapSubsystem::ClearSpeechFocus()
{
	if (FBangoGlobalStore* Store = GetGlobalStore())
	{
		if (Store->Find(BangoYapSubsystemPrivate::SpeechFocusKey))
		{
			WriteSpeechFocus({});
		}
	}
}

TMap<FName, FName> UBangoYapSubsystem::ReadSpeechFocus() const
{
	TMap<FName, FName> Focus;

	const FBangoGlobalStore* Store = GetGlobalStore();
	const FBangoValue* Value = Store ? Store->Find(BangoYapSubsystemPrivate::SpeechFocusKey) : nullptr;
	if (!Value || Value->IsNil() || Value->GetType() != BangoYapSubsystemPrivate::SpeechFocusType())
	{
		return Focus;
	}

	for (const FBangoValue& Pair : Value->GetArray())
	{
		if (Pair.IsNil())
		{
			continue;
		}
		const TArray<FBangoValue>& Fields = Pair.GetArray();
		if (Fields.Num() == 2 && !Fields[0].IsNil() && !Fields[1].IsNil())
		{
			Focus.Add(FName(*Fields[0].GetString()), FName(*Fields[1].GetString()));
		}
	}
	return Focus;
}

void UBangoYapSubsystem::WriteSpeechFocus(const TMap<FName, FName>& Focus)
{
	FBangoGlobalStore* Store = GetGlobalStore();
	if (!Store)
	{
		return;
	}

	TArray<FBangoValue> Pairs;
	for (const TPair<FName, FName>& Pair : Focus)
	{
		Pairs.Add(FBangoValue::MakeArray(FBangoType::String(), { FBangoValue::MakeString(Pair.Key.ToString()), FBangoValue::MakeString(Pair.Value.ToString()) }));
	}

	Store->DeclarePersistentLocal(BangoYapSubsystemPrivate::SpeechFocusKey, BangoYapSubsystemPrivate::SpeechFocusType());
	FString Error;
	if (!Store->Set(BangoYapSubsystemPrivate::SpeechFocusKey, FBangoValue::MakeArray(FBangoType::ArrayOf(EBangoBaseType::String), Pairs), Error))
	{
		UE_LOG(LogBangoYap, Error, TEXT("Couldn't keep the speech focus: %s"), *Error);
	}
}

bool UBangoYapSubsystem::SetSpeechFocus(FName CharacterID, FName FocusID)
{
	if (CharacterID.IsNone() || FocusID == CharacterID)
	{
		return false;
	}

	TMap<FName, FName> Focus = ReadSpeechFocus();
	const FName* Existing = Focus.Find(CharacterID);
	const FName OldFocus = Existing ? *Existing : NAME_None;
	if (OldFocus == FocusID)
	{
		return true;
	}

	if (FocusID.IsNone())
	{
		Focus.Remove(CharacterID);
	}
	else
	{
		Focus.Add(CharacterID, FocusID);
	}
	WriteSpeechFocus(Focus);

	OnSpeechFocusChanged.Broadcast(CharacterID, OldFocus, FocusID);
	return true;
}

FName UBangoYapSubsystem::GetSpeechFocus(FName CharacterID) const
{
	const TMap<FName, FName> Focus = ReadSpeechFocus();
	const FName* Found = Focus.Find(CharacterID);
	return Found ? *Found : NAME_None;
}
