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
