#include "BangoYapLineTiming.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "YapBase/YapAssets.h"
#include "YapBase/YapBroker.h"
#include "YapBase/YapSettings.h"
#include "YapBase/YapSpeechConfig.h"
#include "YapBase/YapTypes.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

namespace BangoYapLineTimingPrivate
{
	/** The project's broker (its class's default object), which works out text time and finds audio. */
	static const UYapBroker* GetBroker()
	{
		const TSoftClassPtr<UYapBroker>& BrokerClass = GetDefault<UYapSettings>()->BrokerClass;
		if (!BrokerClass.IsNull())
		{
			if (const UClass* Class = BrokerClass.LoadSynchronous())
			{
				return GetDefault<UYapBroker>(const_cast<UClass*>(Class));
			}
		}
		return GetDefault<UYapBroker>();
	}

	static FText Seconds(float Value)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;
		return FText::AsNumber(Value, &Options);
	}
}

const UYapSpeechConfig* BangoYapEditor::GetDefaultSpeechConfig()
{
	const FName Name = GetDefault<UYapSettings>()->DefaultSpeechConfig;
	if (!Name.IsNone())
	{
		if (const UYapSpeechConfig* Config = Cast<UYapSpeechConfig>(Yap::FindNamedAsset(UYapSpeechConfig::GetPrimaryAssetType(), UYapSpeechConfig::StaticClass(), Name)))
		{
			return Config;
		}
	}
	return GetDefault<UYapSpeechConfig>();
}

FName BangoYapEditor::GetDefaultSpeechConfigName()
{
	return GetDefault<UYapSettings>()->DefaultSpeechConfig;
}

TArray<const UYapSpeechConfig*> BangoYapEditor::GetAllSpeechConfigs()
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UYapSpeechConfig::StaticClass()->GetClassPathName(), Assets, true);

	TArray<const UYapSpeechConfig*> Out;
	for (const FAssetData& Asset : Assets)
	{
		if (const UYapSpeechConfig* Config = Cast<UYapSpeechConfig>(Asset.GetAsset()))
		{
			Out.Add(Config);
		}
	}
	Out.Sort([](const UYapSpeechConfig& A, const UYapSpeechConfig& B)
	{
		return A.GetConfigName().LexicalLess(B.GetConfigName());
	});
	return Out;
}

const UYapSpeechConfig* BangoYapEditor::FindSpeechConfig(FName ConfigName)
{
	for (const UYapSpeechConfig* Config : GetAllSpeechConfigs())
	{
		if (Config->GetConfigName() == ConfigName)
		{
			return Config;
		}
	}
	return nullptr;
}

UObject* BangoYapEditor::FindLineAudio(const FString& AudioID, bool bChildSafe)
{
	const UYapSpeechConfig* Config = GetDefaultSpeechConfig();
	if (AudioID.IsEmpty() || Config->bDisableAudio)
	{
		return nullptr;
	}
	return BangoYapLineTimingPrivate::GetBroker()->FindLineAudio(FName(*AudioID), bChildSafe ? EYapMaturitySetting::ChildSafe : EYapMaturitySetting::Mature, Config);
}

FBangoYapLineTiming BangoYapEditor::ComputeLineTiming(const FTextValue& Text, const FSettingValue& TimeMode, const FPaddingValue& Padding, bool bChildSafe)
{
	using namespace BangoYapLineTimingPrivate;

	FBangoYapLineTiming Timing;

	const UYapSpeechConfig* Config = GetDefaultSpeechConfig();
	const UYapBroker* Broker = GetBroker();
	Timing.ConfigName = Config->HasAnyFlags(RF_ClassDefaultObject) ? NAME_None : Config->GetConfigName();

	// The time mode: the line's, or the config's (an expression: the config's, as a guess).
	EYapTimeMode Mode = Config->GetDefaultTimeMode();
	if (TimeMode.Kind == FSettingValue::EKind::Value)
	{
		Mode = static_cast<EYapTimeMode>(TimeMode.Value);
		Timing.bDefaultTimeMode = false;
	}
	else if (TimeMode.Kind == FSettingValue::EKind::Expression)
	{
		Timing.bEstimate = true;
		Timing.bDefaultTimeMode = false;
	}

	Timing.bPaddingSet = Padding.bSet;
	if (Padding.bExpression)
	{
		Timing.bEstimate = true;
	}
	if (Text.bPresent && !Text.bLiteral)
	{
		Timing.bEstimate = true;
	}

	// As UYapSubsystem::RunLine.
	const FText LineText = FText::FromString(Text.Source);

	if (Mode == EYapTimeMode::Audio)
	{
		const UObject* Audio = Text.bLiteral ? FindLineAudio(Text.Key, bChildSafe) : nullptr;
		if (Audio)
		{
			Timing.SpeechTime = FMath::Max(Broker->GetAudioDuration(Audio), Config->MinimumAudioTime);
		}
		else
		{
			Timing.bAudioMissing = true;
			Mode = EYapTimeMode::Text;
		}
	}

	switch (Mode)
	{
		case EYapTimeMode::Text:
			Timing.Words = Broker->CalculateWordCount(LineText);
			Timing.SpeechTime = FMath::Max(Broker->CalculateTextTime(Timing.Words, Text.Source.Len(), Config), Config->MinimumTextTime);
			break;

		case EYapTimeMode::Manual:
			Timing.bManualMissingTime = !Padding.bSet;
			Timing.SpeechTime = FMath::Max(Padding.Value, 0.0f);
			break;

		case EYapTimeMode::None:
			Timing.bTimed = false;
			break;

		default:
			break;
	}

	Timing.TimeMode = static_cast<int32>(Mode);

	if (Timing.bTimed)
	{
		Timing.SpeechTime = FMath::Max(Timing.SpeechTime, Config->MinimumSpeakingTime);
	}

	if (Mode == EYapTimeMode::Manual || Mode == EYapTimeMode::None)
	{
		Timing.Padding = 0.0f;
	}
	else
	{
		Timing.Padding = FMath::Max((Padding.bSet && !Padding.bExpression) ? Padding.Value : Config->DefaultPadding, -Timing.SpeechTime);
	}

	return Timing;
}

FText FBangoYapLineTiming::Describe() const
{
	using namespace BangoYapLineTimingPrivate;

	if (!bTimed)
	{
		return LOCTEXT("Timing_None", "No time: plays until it's advanced.");
	}

	FText Speech;
	switch (static_cast<EYapTimeMode>(TimeMode))
	{
		case EYapTimeMode::Audio:
			Speech = FText::Format(LOCTEXT("Timing_Audio", "Audio time: {0} s"), Seconds(SpeechTime));
			break;
		case EYapTimeMode::Manual:
			Speech = bManualMissingTime
				? LOCTEXT("Timing_ManualMissing", "Specified time, but no time is given (set it in the text editor): it won't run.")
				: FText::Format(LOCTEXT("Timing_Manual", "Specified time: {0} s"), Seconds(SpeechTime));
			break;
		default:
			Speech = bAudioMissing
				? FText::Format(LOCTEXT("Timing_TextNoAudio", "Text time: {0} s ({1} words; audio time, but the broker found no audio for it)"), Seconds(SpeechTime), Words)
				: FText::Format(LOCTEXT("Timing_Text", "Text time: {0} s ({1} words)"), Seconds(SpeechTime), Words);
			break;
	}

	TArray<FText> Lines;
	Lines.Add(Speech);

	if (static_cast<EYapTimeMode>(TimeMode) != EYapTimeMode::Manual)
	{
		const FText Sign = Padding < 0.0f ? LOCTEXT("Timing_Minus", "-") : LOCTEXT("Timing_Plus", "+");
		Lines.Add(FText::Format(bPaddingSet
			? LOCTEXT("Timing_PaddingSet", "{0} {1} s padding")
			: LOCTEXT("Timing_PaddingDefault", "{0} {1} s padding (the config's)"), Sign, Seconds(FMath::Abs(Padding))));
	}

	if (bDefaultTimeMode)
	{
		Lines.Add(LOCTEXT("Timing_DefaultMode", "(the config's time mode)"));
	}
	if (bEstimate)
	{
		Lines.Add(LOCTEXT("Timing_Estimate", "A guess: part of it is written as an expression."));
	}
	Lines.Add(ConfigName.IsNone()
		? LOCTEXT("Timing_NoConfig", "With every speech config setting at its default (the project's default config wasn't found).")
		: FText::Format(LOCTEXT("Timing_Config", "With the speech config '{0}' (the project's default: a Speech with another config may differ)."), FText::FromName(ConfigName)));

	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

#undef LOCTEXT_NAMESPACE
