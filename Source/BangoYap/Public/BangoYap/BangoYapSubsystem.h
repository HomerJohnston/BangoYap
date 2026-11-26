#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "YapBase/YapTypes.h"
#include "BangoYapSubsystem.generated.h"

/**
 * BangoYap's bookkeeping for one world: which YapBase conversation each script's Conversation block stands for.
 *
 * A Conversation block has an ID of its own, in its block state, so it's saved with the script. Speech and prompts
 * find the block they're in (FBangoCommandCall::FindOpenBlock), and the YapBase conversation from its ID here; it's
 * opened again if it's gone (after a save is loaded).
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

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TMap<FString, FYapConversationHandle> Conversations;
};
