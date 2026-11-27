#include "BangoYap/BangoYapSettings.h"

#include "BangoDefines.h"
#include "BangoYap/BangoYapLog.h"

namespace BangoYapSettingsPrivate
{
	static const TCHAR* Owner = TEXT("BangoYap");

	/** What each define stands for, for autocomplete. */
	static FString Describe(EBangoYapRunMode Mode)
	{
		switch (Mode)
		{
			case EBangoYapRunMode::RunAll: return TEXT("Speech run mode: say every line reached, one after another.");
			case EBangoYapRunMode::SelectOne: return TEXT("Speech run mode: say only the first line reached.");
			case EBangoYapRunMode::SelectRandom: return TEXT("Speech run mode: say one of the lines reached, picked at random.");
			default: return FString();
		}
	}

	static FString Describe(EBangoYapTimeMode Mode)
	{
		switch (Mode)
		{
			case EBangoYapTimeMode::TextTime: return TEXT("Time mode: the line lasts as long as its text takes to read.");
			case EBangoYapTimeMode::AudioTime: return TEXT("Time mode: the line lasts as long as its audio.");
			case EBangoYapTimeMode::ManualTime: return TEXT("Time mode: the line lasts its Padding, in seconds.");
			case EBangoYapTimeMode::NoTime: return TEXT("Time mode: the line has no time; in a conversation, it waits for the player. Not allowed in free speech.");
			default: return FString();
		}
	}

	static FString Describe(EBangoYapInterruptible Mode)
	{
		switch (Mode)
		{
			case EBangoYapInterruptible::Interruptible: return TEXT("Interruptible: the player can cut the line short (conversations only).");
			case EBangoYapInterruptible::Forced: return TEXT("Interruptible: the player can't cut the line short (conversations only).");
			default: return FString();
		}
	}

	static FString Describe(EBangoYapAdvance Mode)
	{
		switch (Mode)
		{
			case EBangoYapAdvance::AutoAdvance: return TEXT("Advance: the line moves on by itself when its time is up (conversations only).");
			case EBangoYapAdvance::ManualAdvance: return TEXT("Advance: the line waits for the player to continue (conversations only).");
			default: return FString();
		}
	}

	/** One list's rows, in the settings' number order. A name left empty is no define. */
	template <typename EnumType>
	static void AddRows(const TMap<EnumType, FString>& Map, const TCHAR* Category, TArray<Bango::FBangoDefineText>& OutRows)
	{
		TArray<EnumType> Keys;
		Map.GetKeys(Keys);
		Keys.Sort([](EnumType A, EnumType B) { return (uint8)A < (uint8)B; });

		for (EnumType Key : Keys)
		{
			const FString Name = Map.FindChecked(Key).TrimStartAndEnd();
			if (Name.IsEmpty())
			{
				continue;
			}

			Bango::FBangoDefineText& Row = OutRows.AddDefaulted_GetRef();
			Row.Name = Name;
			Row.TypeName = TEXT("int");
			Row.ValueText = FString::FromInt((int32)(uint8)Key);
			Row.Category = Category;
			Row.Description = Describe(Key);
		}
	}
}

UBangoYapSettings::UBangoYapSettings()
{
	SpeechProgression =
	{
		{ EBangoYapRunMode::RunAll, TEXT("RUN_ALL") },
		{ EBangoYapRunMode::SelectOne, TEXT("SELECT_ONE") },
		{ EBangoYapRunMode::SelectRandom, TEXT("SELECT_RANDOM") },
	};

	SpeechTime =
	{
		{ EBangoYapTimeMode::TextTime, TEXT("TEXT_TIME") },
		{ EBangoYapTimeMode::AudioTime, TEXT("AUDIO_TIME") },
		{ EBangoYapTimeMode::ManualTime, TEXT("MANUAL_TIME") },
		{ EBangoYapTimeMode::NoTime, TEXT("NO_TIME") },
	};

	Interruption =
	{
		{ EBangoYapInterruptible::Interruptible, TEXT("INTERRUPTIBLE") },
		{ EBangoYapInterruptible::Forced, TEXT("FORCED") },
	};

	Advancing =
	{
		{ EBangoYapAdvance::AutoAdvance, TEXT("AUTO_ADVANCE") },
		{ EBangoYapAdvance::ManualAdvance, TEXT("MANUAL_ADVANCE") },
	};
}

void UBangoYapSettings::RegisterScriptDefines() const
{
	BangoYap::RegisterDefines();
}

#if WITH_EDITOR
void UBangoYapSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UBangoYapSettings, TimeBarMaxSeconds))
	{
		return; // not a define
	}
	BangoYap::RegisterDefines();
}
#endif

void UBangoYapSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);
	BangoYap::RegisterDefines();
}

void BangoYap::RegisterDefines()
{
	using namespace BangoYapSettingsPrivate;

	const UBangoYapSettings* Settings = GetDefault<UBangoYapSettings>();

	TArray<Bango::FBangoDefineText> Rows;
	AddRows(Settings->SpeechProgression, TEXT("Yap: Speech Progression"), Rows);
	AddRows(Settings->SpeechTime, TEXT("Yap: Speech Time"), Rows);
	AddRows(Settings->Interruption, TEXT("Yap: Interruption"), Rows);
	AddRows(Settings->Advancing, TEXT("Yap: Advancing"), Rows);

	TArray<FString> Errors;
	Bango::RegisterDefines(Owner, Rows, &Errors);

	for (const FString& Error : Errors)
	{
		UE_LOG(LogBangoYap, Warning, TEXT("Script define left out (Project Settings > Plugins > Bango Yap > Script Defines): %s"), *Error);
	}

	UE_LOG(LogBangoYap, Log, TEXT("Registered %d script defines with Bango."), Rows.Num());
}

void BangoYap::UnregisterDefines()
{
	Bango::UnregisterDefines(BangoYapSettingsPrivate::Owner);
}
