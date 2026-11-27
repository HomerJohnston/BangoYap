#pragma once

#include "CoreMinimal.h"
#include "BangoYapNodeFields.h"

class UYapSpeechConfig;

/**
 * A line's time as YapBase will work it out when it runs (UYapSubsystem::RunLine), worked out in the editor for the
 * time bar under a line's node: the speech time (text time from the broker's word count and words per minute, audio
 * time from the broker's audio for the line, or the manual time) and the padding after it.
 *
 * A line's node can't see its Speech, so the project's default speech config is used (Project Settings > Plugins >
 * Yap > Default Speech Config); a line in a Speech with another config may come out differently. Parts written as
 * expressions are guessed (Estimate).
 */
struct FBangoYapLineTiming
{
	/** False for no time (it plays until advanced): no bar. */
	bool bTimed = true;

	float SpeechTime = 0.0f;
	/** After the speech (at least -SpeechTime); 0 for manual time, whose time is the padding. */
	float Padding = 0.0f;

	/** The time mode used (EYapTimeMode: 1 text, 2 audio, 3 manual, 4 none). */
	int32 TimeMode = 0;
	/** The line doesn't set its time mode (it's the config's). */
	bool bDefaultTimeMode = true;
	/** The line sets its padding. */
	bool bPaddingSet = false;
	/** Audio time, but the broker found no audio for the line, so it's text time (as when it runs). */
	bool bAudioMissing = false;
	/** Something it depends on is written as an expression: this is a guess. */
	bool bEstimate = false;

	/** Words counted, for text time. */
	int32 Words = 0;
	/** The config used, for the tooltip. */
	FName ConfigName;
	/** Manual time with no padding: it won't run ("needs its time"). */
	bool bManualMissingTime = false;

	/** "Text time: 2.35 s (12 words) + 0.25 s padding (the config's)", and so on, for the tooltip. */
	FText Describe() const;
};

namespace BangoYapEditor
{
	/** The project's default speech config (an all-defaults one if it can't be found). */
	const UYapSpeechConfig* GetDefaultSpeechConfig();

	/** The project's default speech config's name (Project Settings > Plugins > Yap > Default Speech Config). */
	FName GetDefaultSpeechConfigName();

	/** Every speech config in the project, by their Config Name. */
	TArray<const UYapSpeechConfig*> GetAllSpeechConfigs();

	/** The speech config scripts call this (its Config Name, or its asset's name), or null. */
	const UYapSpeechConfig* FindSpeechConfig(FName ConfigName);

	/**
	 * The line's audio, as the project's broker finds it when the line runs (UYapBroker::FindLineAudio, by its audio
	 * ID: the line ID, or for child-safe audio the child-safe text's key), with the default speech config. Null if
	 * there's none, or the config turns audio off.
	 */
	UObject* FindLineAudio(const FString& AudioID, bool bChildSafe);

	/**
	 * The timing of a line with this text (its literal's source; its key is the line ID, for audio), time mode and
	 * padding, for mature or child-safe players.
	 */
	FBangoYapLineTiming ComputeLineTiming(const FTextValue& Text, const FSettingValue& TimeMode, const FPaddingValue& Padding, bool bChildSafe);
}
