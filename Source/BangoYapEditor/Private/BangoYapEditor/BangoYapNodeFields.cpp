#include "BangoYapNodeFields.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BangoGraphLayout.h"
#include "GameplayTagsManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "YapBase/YapCharacterAsset.h"
#include "YapBase/YapMoodTags.h"
#include "YapBase/YapSettings.h"
#include "BangoYap/BangoYapSettings.h"

namespace BangoYapEditorPrivate
{
	/** The project's characters by ID, made again when character assets come, go or change. */
	struct FCharacterCache
	{
		TMap<FName, TWeakObjectPtr<UYapCharacterAsset>> ByID;
		bool bDirty = true;
		bool bBound = false;

		void Bind()
		{
			if (bBound)
			{
				return;
			}
			bBound = true;

			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			Registry.OnAssetAdded().AddLambda([this](const FAssetData&) { bDirty = true; });
			Registry.OnAssetRemoved().AddLambda([this](const FAssetData&) { bDirty = true; });
			Registry.OnAssetRenamed().AddLambda([this](const FAssetData&, const FString&) { bDirty = true; });
			FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([this](UObject* Object, FPropertyChangedEvent&)
			{
				if (Cast<UYapCharacterAsset>(Object))
				{
					bDirty = true;
				}
			});
		}

		void Rebuild()
		{
			Bind();
			bDirty = false;
			ByID.Reset();

			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			TArray<FAssetData> Assets;
			Registry.GetAssetsByClass(UYapCharacterAsset::StaticClass()->GetClassPathName(), Assets, true);
			for (const FAssetData& Asset : Assets)
			{
				if (UYapCharacterAsset* Character = Cast<UYapCharacterAsset>(Asset.GetAsset()))
				{
					ByID.Add(Character->GetCharacterID(), Character);
				}
			}
		}

		UYapCharacterAsset* Find(FName ID)
		{
			if (bDirty)
			{
				Rebuild();
			}
			const TWeakObjectPtr<UYapCharacterAsset>* Found = ByID.Find(ID);
			return Found ? Found->Get() : nullptr;
		}
	};

	static FCharacterCache& Characters()
	{
		static FCharacterCache Cache;
		return Cache;
	}

	static bool IsIdentifier(const FString& Text)
	{
		if (Text.IsEmpty() || !(FChar::IsAlpha(Text[0]) || Text[0] == TEXT('_')))
		{
			return false;
		}
		for (const TCHAR Char : Text)
		{
			if (!(FChar::IsAlnum(Char) || Char == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}

	/** Splits an array literal's inside at its top-level commas (not in strings or brackets). */
	static TArray<FString> SplitTopLevel(const FString& Inside)
	{
		TArray<FString> Parts;
		FString Current;
		int32 Depth = 0;
		bool bInString = false;
		for (int32 i = 0; i < Inside.Len(); ++i)
		{
			const TCHAR Char = Inside[i];
			if (bInString)
			{
				Current.AppendChar(Char);
				if (Char == TEXT('\\') && i + 1 < Inside.Len())
				{
					Current.AppendChar(Inside[++i]);
				}
				else if (Char == TEXT('"'))
				{
					bInString = false;
				}
				continue;
			}
			if (Char == TEXT('"'))
			{
				bInString = true;
			}
			else if (Char == TEXT('[') || Char == TEXT('('))
			{
				++Depth;
			}
			else if (Char == TEXT(']') || Char == TEXT(')'))
			{
				--Depth;
			}
			else if (Char == TEXT(',') && Depth == 0)
			{
				Parts.Add(Current.TrimStartAndEnd());
				Current.Reset();
				continue;
			}
			Current.AppendChar(Char);
		}
		if (!Current.TrimStartAndEnd().IsEmpty() || Parts.Num() > 0)
		{
			Parts.Add(Current.TrimStartAndEnd());
		}
		return Parts;
	}
}

FString BangoYapEditor::FCharacterRef::ToDisplayString() const
{
	switch (Kind)
	{
		case ECharacterRefKind::Entity: return TEXT("@") + Value;
		default: return Value;
	}
}

bool BangoYapEditor::ParseStringLiteral(const FString& Text, FString& OutValue)
{
	const FString Trimmed = Text.TrimStartAndEnd();
	if (Trimmed.Len() < 2 || Trimmed[0] != TEXT('"') || Trimmed[Trimmed.Len() - 1] != TEXT('"'))
	{
		return false;
	}

	OutValue.Reset();
	for (int32 i = 1; i < Trimmed.Len() - 1; ++i)
	{
		TCHAR Char = Trimmed[i];
		if (Char == TEXT('\\') && i + 1 < Trimmed.Len() - 1)
		{
			const TCHAR Next = Trimmed[++i];
			switch (Next)
			{
				case TEXT('n'): Char = TEXT('\n'); break;
				case TEXT('t'): Char = TEXT('\t'); break;
				default: Char = Next; break;
			}
		}
		else if (Char == TEXT('"'))
		{
			return false; // a quote inside: not one literal
		}
		OutValue.AppendChar(Char);
	}
	return true;
}

BangoYapEditor::FCharacterRef BangoYapEditor::ParseCharacterRef(const FString& Text)
{
	FCharacterRef Ref;
	const FString Trimmed = Text.TrimStartAndEnd();

	if (Trimmed.IsEmpty() || Trimmed == TEXT("nil"))
	{
		return Ref;
	}

	FString String;
	if (ParseStringLiteral(Trimmed, String))
	{
		Ref.Kind = String.IsEmpty() ? ECharacterRefKind::None : ECharacterRefKind::ID;
		Ref.Value = String;
		return Ref;
	}

	if (Trimmed[0] == TEXT('@') && BangoYapEditorPrivate::IsIdentifier(Trimmed.RightChop(1)))
	{
		Ref.Kind = ECharacterRefKind::Entity;
		Ref.Value = Trimmed.RightChop(1);
		return Ref;
	}

	Ref.Kind = BangoYapEditorPrivate::IsIdentifier(Trimmed) ? ECharacterRefKind::Variable : ECharacterRefKind::Other;
	Ref.Value = Trimmed;
	return Ref;
}

BangoYapEditor::FSpeakerTarget BangoYapEditor::ParseSpeakerTarget(const FString& Text)
{
	FSpeakerTarget Target;
	const FString Trimmed = Text.TrimStartAndEnd();

	if (Trimmed.StartsWith(TEXT("[")) && Trimmed.EndsWith(TEXT("]")))
	{
		Target.bArray = true;
		const TArray<FString> Parts = BangoYapEditorPrivate::SplitTopLevel(Trimmed.Mid(1, Trimmed.Len() - 2));
		if (Parts.Num() < 1 || Parts.Num() > 2)
		{
			Target.bValid = false;
			return Target;
		}
		Target.Speaker = ParseCharacterRef(Parts[0]);
		if (Parts.Num() == 2)
		{
			Target.DirectedAt = ParseCharacterRef(Parts[1]);
		}
		return Target;
	}

	Target.Speaker = ParseCharacterRef(Trimmed);
	return Target;
}

int32 BangoYapEditor::FindField(const FBangoNodeLayout& Layout, const TCHAR* Label)
{
	for (int32 i = 0; i < Layout.Fields.Num(); ++i)
	{
		if (Layout.Fields[i].Label == Label)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString BangoYapEditor::GetFieldText(const FBangoNodeLayout& Layout, const TCHAR* Label)
{
	const int32 Index = FindField(Layout, Label);
	if (Index == INDEX_NONE || !Layout.Fields[Index].bPresent)
	{
		return FString();
	}
	return Layout.Fields[Index].Text.TrimStartAndEnd();
}

BangoYapEditor::FTextValue BangoYapEditor::ReadTextField(const FBangoNodeLayout& Layout, const TCHAR* Label)
{
	FTextValue Value;
	const FString Text = GetFieldText(Layout, Label);
	if (Text.IsEmpty() || Text == TEXT("nil"))
	{
		return Value;
	}

	Value.bPresent = true;
	FBangoTextLiteral Literal;
	if (Bango::ParseTextLiteral(Text, Literal))
	{
		Value.bLiteral = true;
		Value.bInvariant = Literal.bInvariant;
		Value.Source = Literal.Source;
		Value.Key = Literal.bInvariant ? FString() : Literal.Key;
		Value.Literal = Literal;
	}
	else
	{
		Value.Source = Text;
	}
	return Value;
}

UYapCharacterAsset* BangoYapEditor::FindCharacter(FName CharacterID)
{
	return CharacterID.IsNone() ? nullptr : BangoYapEditorPrivate::Characters().Find(CharacterID);
}

UYapCharacterAsset* BangoYapEditor::FindCharacter(const FCharacterRef& Ref)
{
	return Ref.Kind == ECharacterRefKind::ID ? FindCharacter(FName(*Ref.Value)) : nullptr;
}

FGameplayTag BangoYapEditor::FindMoodTag(const FString& Mood)
{
	if (Mood.IsEmpty())
	{
		return FGameplayTag();
	}

	for (const FGameplayTag& Tag : Yap::GetAllMoodTags())
	{
		const FString Name = Tag.GetTagName().ToString();
		if (Name == Mood || Name.EndsWith(TEXT(".") + Mood))
		{
			return Tag;
		}
	}
	return FGameplayTag();
}

// ================================================================================================ Writing

FString BangoYapEditor::MakeStringLiteral(const FString& Value)
{
	FString Out = TEXT("\"");
	for (const TCHAR Char : Value)
	{
		switch (Char)
		{
			case TEXT('"'): Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default: Out.AppendChar(Char); break;
		}
	}
	Out += TEXT("\"");
	return Out;
}

FString BangoYapEditor::CharacterRefToScript(const FCharacterRef& Ref)
{
	switch (Ref.Kind)
	{
		case ECharacterRefKind::None: return TEXT("nil");
		case ECharacterRefKind::ID: return MakeStringLiteral(Ref.Value);
		case ECharacterRefKind::Entity: return TEXT("@") + Ref.Value;
		default: return Ref.Value;
	}
}

FString BangoYapEditor::MakeTargetScript(const FSpeakerTarget& Target, bool* OutDropped)
{
	if (OutDropped)
	{
		*OutDropped = false;
	}

	if (!Target.bArray)
	{
		return CharacterRefToScript(Target.Speaker);
	}

	FString Speaker = CharacterRefToScript(Target.Speaker);
	if (!Target.DirectedAt.IsSet())
	{
		return FString::Printf(TEXT("[%s]"), *Speaker);
	}

	// An array holds one type: an ID with an ID, an entity with an entity. Variables and expressions are trusted.
	const auto IsLiteralKind = [](ECharacterRefKind Kind) { return Kind == ECharacterRefKind::ID || Kind == ECharacterRefKind::Entity; };
	if (IsLiteralKind(Target.Speaker.Kind) && IsLiteralKind(Target.DirectedAt.Kind) && Target.Speaker.Kind != Target.DirectedAt.Kind)
	{
		if (OutDropped)
		{
			*OutDropped = true;
		}
		return FString::Printf(TEXT("[%s]"), *Speaker);
	}

	return FString::Printf(TEXT("[%s, %s]"), *Speaker, *CharacterRefToScript(Target.DirectedAt));
}

bool BangoYapEditor::IsTargetEditableInRow(const FSpeakerTarget& Target)
{
	if (!Target.bValid)
	{
		return false;
	}
	const auto Editable = [](const FCharacterRef& Ref)
	{
		return Ref.Kind == ECharacterRefKind::None || Ref.Kind == ECharacterRefKind::ID || Ref.Kind == ECharacterRefKind::Entity;
	};
	return Editable(Target.Speaker) && Editable(Target.DirectedAt);
}

FString BangoYapEditor::GetClearValue(const FBangoNodeLayout& Layout, int32 FieldIndex)
{
	for (int32 i = FieldIndex + 1; i < Layout.Fields.Num(); ++i)
	{
		if (Layout.Fields[i].bPresent && Layout.Fields[i].bHasValue)
		{
			return TEXT("nil");
		}
	}
	return FString();
}

TArray<UYapCharacterAsset*> BangoYapEditor::GetAllCharacters()
{
	BangoYapEditorPrivate::FCharacterCache& Cache = BangoYapEditorPrivate::Characters();
	if (Cache.bDirty)
	{
		Cache.Rebuild();
	}

	TArray<UYapCharacterAsset*> Out;
	for (const TPair<FName, TWeakObjectPtr<UYapCharacterAsset>>& Pair : Cache.ByID)
	{
		if (UYapCharacterAsset* Character = Pair.Value.Get())
		{
			Out.Add(Character);
		}
	}
	Out.Sort([](const UYapCharacterAsset& A, const UYapCharacterAsset& B)
	{
		return A.GetCharacterName().ToString() < B.GetCharacterName().ToString();
	});
	return Out;
}

FString BangoYapEditor::MakeLineID()
{
	const FString Format = GetDefault<UYapSettings>()->LineIDFormat.IsEmpty() ? FString(TEXT("***###")) : GetDefault<UYapSettings>()->LineIDFormat;
	static const TCHAR* Letters = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	static const TCHAR* LettersAndDigits = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

	FString ID;
	for (const TCHAR Char : Format)
	{
		switch (Char)
		{
			case TEXT('*'): ID.AppendChar(Letters[FMath::RandRange(0, 25)]); break;
			case TEXT('?'): ID.AppendChar(LettersAndDigits[FMath::RandRange(0, 35)]); break;
			case TEXT('#'): ID.AppendChar(TEXT('0') + FMath::RandRange(0, 9)); break;
			default: ID.AppendChar(Char); break;
		}
	}
	return ID;
}

FString BangoYapEditor::MakeChildSafeKey(const FString& LineID)
{
	return LineID.IsEmpty() ? FString() : LineID + GetDefault<UYapSettings>()->ChildSafeKeySuffix;
}

// ------------------------------------------------------------------------------------------------ Line settings

namespace BangoYapEditorPrivate
{
	/** The project's define names for a setting's values (UBangoYapSettings), by value. */
	static TMap<int32, FString> GetDefineNames(BangoYapEditor::ELineSetting Setting)
	{
		using namespace BangoYapEditor;
		const UBangoYapSettings* Settings = GetDefault<UBangoYapSettings>();
		TMap<int32, FString> Out;

		auto Add = [&Out](const auto& Map)
		{
			for (const auto& Pair : Map)
			{
				const FString Name = Pair.Value.TrimStartAndEnd();
				if (!Name.IsEmpty())
				{
					Out.Add((int32)Pair.Key, Name);
				}
			}
		};

		switch (Setting)
		{
			case ELineSetting::TimeMode: Add(Settings->SpeechTime); break;
			case ELineSetting::Interruptible: Add(Settings->Interruption); break;
			case ELineSetting::Advance: Add(Settings->Advancing); break;
			case ELineSetting::RunMode: Add(Settings->SpeechProgression); break;
		}
		return Out;
	}
}

const TCHAR* BangoYapEditor::GetSettingLabel(ELineSetting Setting)
{
	switch (Setting)
	{
		case ELineSetting::TimeMode: return TEXT("TimeMode");
		case ELineSetting::Interruptible: return TEXT("Interruptible");
		case ELineSetting::RunMode: return TEXT("RunMode");
		default: return TEXT("Advance");
	}
}

int32 BangoYapEditor::GetSettingMax(ELineSetting Setting)
{
	switch (Setting)
	{
		case ELineSetting::TimeMode: return 4;
		case ELineSetting::RunMode: return 3;
		default: return 2;
	}
}

BangoYapEditor::FSettingValue BangoYapEditor::ReadSetting(const FBangoNodeLayout& Layout, ELineSetting Setting)
{
	FSettingValue Out;
	Out.Text = GetFieldText(Layout, GetSettingLabel(Setting));

	if (Out.Text.IsEmpty() || Out.Text == TEXT("nil"))
	{
		return Out;
	}

	int32 Number = 0;
	if (Out.Text.IsNumeric() && LexTryParseString(Number, *Out.Text) && Number >= 1 && Number <= GetSettingMax(Setting))
	{
		Out.Kind = FSettingValue::EKind::Value;
		Out.Value = Number;
		return Out;
	}

	for (const TPair<int32, FString>& Pair : BangoYapEditorPrivate::GetDefineNames(Setting))
	{
		if (Pair.Value == Out.Text)
		{
			Out.Kind = FSettingValue::EKind::Value;
			Out.Value = Pair.Key;
			return Out;
		}
	}

	Out.Kind = FSettingValue::EKind::Expression;
	return Out;
}

FString BangoYapEditor::GetSettingDefineName(ELineSetting Setting, int32 Value)
{
	// Kept in a local: Find points into the map, which must outlive the copy made from it.
	const TMap<int32, FString> Names = BangoYapEditorPrivate::GetDefineNames(Setting);
	const FString* Name = Names.Find(Value);
	return Name ? *Name : FString();
}

FString BangoYapEditor::MakeSettingScript(ELineSetting Setting, int32 Value)
{
	if (Value <= 0)
	{
		return TEXT("nil");
	}
	const FString Name = GetSettingDefineName(Setting, Value);
	return Name.IsEmpty() ? FString::FromInt(Value) : Name;
}

BangoYapEditor::FPaddingValue BangoYapEditor::ReadPadding(const FBangoNodeLayout& Layout)
{
	FPaddingValue Out;
	Out.Text = GetFieldText(Layout, TEXT("Padding"));

	if (Out.Text.IsEmpty() || Out.Text == TEXT("nil"))
	{
		return Out;
	}

	Out.bSet = true;

	// A number, maybe negative (written `-0.25`, which scripts read as a minus and a number).
	FString Number = Out.Text.Replace(TEXT(" "), TEXT(""));
	double Value = 0.0;
	if (Number.IsNumeric() && LexTryParseString(Value, *Number))
	{
		Out.Value = (float)Value;
	}
	else
	{
		Out.bExpression = true;
	}
	return Out;
}

FString BangoYapEditor::MakeSecondsScript(float Seconds)
{
	// Two decimals at most (as Yap's boxes show), with the point kept: "2.0", "0.25", "-0.5".
	const double Rounded = FMath::RoundToDouble(Seconds * 100.0) / 100.0;
	return FString::SanitizeFloat(Rounded, 1);
}
