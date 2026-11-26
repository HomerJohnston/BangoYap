#include "BangoYap/BangoYapSubsystem.h"

#include "Engine/World.h"
#include "YapBase/YapSubsystem.h"

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

void UBangoYapSubsystem::PushOpenConversation(int64 InstanceId, const FString& ConversationID, FName Name)
{
	TArray<FOpenConversation>& Open = OpenByInstance.FindOrAdd(InstanceId);
	if (!Open.ContainsByPredicate([&ConversationID](const FOpenConversation& Entry) { return Entry.ID == ConversationID; }))
	{
		Open.Add({ ConversationID, Name });
	}
}

void UBangoYapSubsystem::PopOpenConversation(int64 InstanceId, const FString& ConversationID)
{
	if (TArray<FOpenConversation>* Open = OpenByInstance.Find(InstanceId))
	{
		Open->RemoveAll([&ConversationID](const FOpenConversation& Entry) { return Entry.ID == ConversationID; });
		if (Open->Num() == 0)
		{
			OpenByInstance.Remove(InstanceId);
		}
	}
}

bool UBangoYapSubsystem::GetOpenConversation(int64 InstanceId, FString& OutConversationID, FName& OutName) const
{
	const TArray<FOpenConversation>* Open = OpenByInstance.Find(InstanceId);
	if (!Open || Open->Num() == 0)
	{
		return false;
	}

	OutConversationID = Open->Last().ID;
	OutName = Open->Last().Name;
	return true;
}

bool UBangoYapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
