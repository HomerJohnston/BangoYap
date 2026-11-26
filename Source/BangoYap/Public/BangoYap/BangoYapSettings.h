#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BangoYapSettings.generated.h"

/** Speech's run mode (its first argument). The numbers are what scripts pass. */
UENUM()
enum class EBangoYapRunMode : uint8
{
	RunAll = 1			UMETA(DisplayName = "Run all"),
	SelectOne = 2		UMETA(DisplayName = "Select one"),
	SelectRandom = 3	UMETA(DisplayName = "Select random"),
};

/** A line's time mode (Say's TimeMode). */
UENUM()
enum class EBangoYapTimeMode : uint8
{
	TextTime = 1		UMETA(DisplayName = "Text time"),
	AudioTime = 2		UMETA(DisplayName = "Audio time"),
	ManualTime = 3		UMETA(DisplayName = "Manual time"),
	NoTime = 4			UMETA(DisplayName = "No time"),
};

/** Whether a line can be cut short (Speech's and Say's Interruptible). */
UENUM()
enum class EBangoYapInterruptible : uint8
{
	Interruptible = 1	UMETA(DisplayName = "Interruptible"),
	Forced = 2			UMETA(DisplayName = "Forced"),
};

/** Whether a line moves on by itself (Speech's and Say's Advance). */
UENUM()
enum class EBangoYapAdvance : uint8
{
	AutoAdvance = 1		UMETA(DisplayName = "Auto advance"),
	ManualAdvance = 2	UMETA(DisplayName = "Manual advance"),
};

/**
 * Project Settings > Plugins > Bango Yap. Saved in DefaultGame.ini.
 *
 * **Script Defines** are the names BangoYap gives scripts for its numbered settings (Bango's defines: spec section 4),
 * so scripts write `Speech(SELECT_RANDOM)` instead of `Speech(3)`. Each list pairs a setting (left) with the define's
 * name (right). Rename one if another plugin or the project already uses that name (`YAP_RUN_ALL`); remove an entry, or
 * leave its name empty, for no define. Scripts that used an old name aren't changed: do this when setting up a project.
 *
 * They're registered with Bango when BangoYap starts, and again whenever they're changed here (or with Register Script
 * Defines), so there's no restart. A name Bango can't take (not SHOUTING_SNAKE_CASE, or already a define) is left out,
 * and the output log says why.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Bango Yap"))
class BANGOYAP_API UBangoYapSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBangoYapSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Speech's run mode: Speech(RUN_ALL) { ... } */
	UPROPERTY(Config, EditAnywhere, Category = "Script Defines|Speech Progression", meta = (ForceInlineRow))
	TMap<EBangoYapRunMode, FString> SpeechProgression;

	/** A line's time mode: Say(Text, Mood, TEXT_TIME) */
	UPROPERTY(Config, EditAnywhere, Category = "Script Defines|Speech Time Settings", meta = (ForceInlineRow))
	TMap<EBangoYapTimeMode, FString> SpeechTime;

	/** Whether a line in a conversation can be cut short: Speech(RUN_ALL, nil, FORCED) */
	UPROPERTY(Config, EditAnywhere, Category = "Script Defines|Interruption", meta = (ForceInlineRow))
	TMap<EBangoYapInterruptible, FString> Interruption;

	/** Whether a line in a conversation moves on by itself: Speech(RUN_ALL, nil, nil, MANUAL_ADVANCE) */
	UPROPERTY(Config, EditAnywhere, Category = "Script Defines|Advancing", meta = (ForceInlineRow))
	TMap<EBangoYapAdvance, FString> Advancing;

	/** Registers the defines above with Bango again (it's done for you when they're changed here). */
	UFUNCTION(CallInEditor, Category = "Script Defines", meta = (DisplayName = "Register Script Defines"))
	void RegisterScriptDefines() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
};

namespace BangoYap
{
	/** Registers BangoYap's script defines (UBangoYapSettings) with Bango, in place of any it had. */
	void RegisterDefines();

	/** Takes them away (the module is shutting down). */
	void UnregisterDefines();
}
