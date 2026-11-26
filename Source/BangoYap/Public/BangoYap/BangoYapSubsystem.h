#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "YapBase/YapTypes.h"
#include "BangoYapSubsystem.generated.h"

/**
 * BangoYap's bookkeeping for one world: which YapBase conversation each script's Conversation block stands for, and
 * which Conversation blocks each running script has open.
 *
 * A Conversation block has an ID of its own (in its block state, so it's saved with the script); the YapBase
 * conversation is found from it, and opened again if it's gone (after a save is loaded).
 *
 * Which blocks a script has open is kept here by script instance, for now: BangoScripts doesn't yet let a command find
 * an open block it's inside through function calls (BangoYap - requests for BangoScripts, request 1). Until it does,
 * this is lost when a save is loaded, so speech in a Conversation block that's open when the game was saved runs as
 * free speech after loading (a line or prompt that was playing carries on in its conversation).
 */
UCLASS()
class BANGOYAP_API UBangoYapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBangoYapSubsystem* Get(const UObject* WorldContext);

	/** The YapBase conversation for a Conversation block's ID, opened (or queued) if there isn't one. */
	FYapConversationHandle EnsureConversation(const FString& ConversationID, FName Name);

	/** The YapBase conversation for a Conversation block's ID, or an unset handle. */
	FYapConversationHandle FindConversation(const FString& ConversationID) const;

	/** The block is done with its conversation. */
	void ForgetConversation(const FString& ConversationID);

	/** A script instance has opened a Conversation block (again: nothing changes). */
	void PushOpenConversation(int64 InstanceId, const FString& ConversationID, FName Name);

	/** A script instance has left a Conversation block. */
	void PopOpenConversation(int64 InstanceId, const FString& ConversationID);

	/** The innermost Conversation block the instance has open. False if it has none. */
	bool GetOpenConversation(int64 InstanceId, FString& OutConversationID, FName& OutName) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FOpenConversation
	{
		FString ID;
		FName Name;
	};

	TMap<FString, FYapConversationHandle> Conversations;
	TMap<int64, TArray<FOpenConversation>> OpenByInstance;
};
