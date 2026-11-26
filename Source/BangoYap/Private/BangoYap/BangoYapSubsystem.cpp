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

bool UBangoYapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
