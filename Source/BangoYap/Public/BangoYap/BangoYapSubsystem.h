#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "YapBase/YapTypes.h"
#include "BangoYapSubsystem.generated.h"

struct FBangoGlobalStore;

/** A character's speech focus changed (SetSpeechFocus): who they talk to (None: no one). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBangoYapOnSpeechFocusChanged, FName, CharacterID, FName, OldFocus, FName, NewFocus);

/**
 * BangoYap's bookkeeping for one world: which YapBase conversation each script's Conversation block stands for, and
 * each character's speech focus.
 *
 * A Conversation block has an ID of its own, in its block state, so it's saved with the script. Speech and prompts
 * find the block they're in (FBangoCommandCall::FindOpenBlock), and the YapBase conversation from its ID here; it's
 * opened again if it's gone (after a save is loaded).
 *
 * **Speech focus** is who a character is talking to: a line that only names its speaker is directed at them (taken
 * when the line starts). It's kept in Bango's global variables, under a key scripts can't name, so Save Bango State
 * saves it and Load Bango State brings it back, with nothing more for the game to do. It's cleared when a world starts
 * (before the level's start script) and when it ends. Loading doesn't fire OnSpeechFocusChanged.
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

	/** Sets who the character is talking to (None: no one). False if there's no character, or it's themselves. */
	UFUNCTION(BlueprintCallable, Category = "Yap")
	bool SetSpeechFocus(FName CharacterID, FName FocusID);

	/** Who the character is talking to, or None. */
	UFUNCTION(BlueprintPure, Category = "Yap")
	FName GetSpeechFocus(FName CharacterID) const;

	/** A character's speech focus was set: turn heads, move cameras. */
	UPROPERTY(BlueprintAssignable, Category = "Yap")
	FBangoYapOnSpeechFocusChanged OnSpeechFocusChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Bango's global store (on the game instance), or null outside a game. */
	FBangoGlobalStore* GetGlobalStore() const;

	/** Every character's focus, from the store. */
	TMap<FName, FName> ReadSpeechFocus() const;
	void WriteSpeechFocus(const TMap<FName, FName>& Focus);
	void ClearSpeechFocus();

	TMap<FString, FYapConversationHandle> Conversations;
};
