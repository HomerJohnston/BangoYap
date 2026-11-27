#include "BangoYapLineDecorator.h"

#include "BangoYapEditorStyle.h"
#include "BangoYapNodeFields.h"
#include "SBangoYapLineRow.h"
#include "YapBase/YapCharacterAsset.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

using namespace BangoYapEditor;

namespace BangoYapLineDecoratorPrivate
{
	static FText CharacterTitle(const FCharacterRef& Ref)
	{
		if (const UYapCharacterAsset* Asset = FindCharacter(Ref))
		{
			const FText Name = Asset->GetCharacterName();
			return Name.IsEmpty() ? FText::FromName(Asset->GetCharacterID()) : Name;
		}
		return FText::FromString(Ref.ToDisplayString());
	}

	static FText CommandTitle(const FString& CommandName)
	{
		if (CommandName == TEXT("SayM"))
		{
			return LOCTEXT("Title_SayM", "Say (mature)");
		}
		if (CommandName == TEXT("AddPrompt"))
		{
			return LOCTEXT("Title_AddPrompt", "Prompt");
		}
		return LOCTEXT("Title_Say", "Say");
	}
}

const TArray<FString>& FBangoYapLineDecorator::GetCommandNames()
{
	static const TArray<FString> Names = { TEXT("Say"), TEXT("SayM"), TEXT("AddPrompt") };
	return Names;
}

TOptional<FText> FBangoYapLineDecorator::GetTitle(const FBangoNodeDecoratorContext& Context) const
{
	using namespace BangoYapLineDecoratorPrivate;

	if (!Context.Layout)
	{
		return TOptional<FText>();
	}

	const FSpeakerTarget Target = ParseSpeakerTarget(GetFieldText(*Context.Layout, TEXT("Target")));
	const FText Command = CommandTitle(Context.CommandName);

	if (!Target.bValid || !Target.Speaker.IsSet())
	{
		return Command;
	}
	if (Target.DirectedAt.IsSet())
	{
		return FText::Format(LOCTEXT("Title_SpeakerTo", "{0}  \u00B7  {1} \u2192 {2}"), Command, CharacterTitle(Target.Speaker), CharacterTitle(Target.DirectedAt));
	}
	return FText::Format(LOCTEXT("Title_Speaker", "{0}  \u00B7  {1}"), Command, CharacterTitle(Target.Speaker));
}

TOptional<FLinearColor> FBangoYapLineDecorator::GetTitleColor(const FBangoNodeDecoratorContext& Context) const
{
	if (!Context.Layout)
	{
		return TOptional<FLinearColor>();
	}

	const FSpeakerTarget Target = ParseSpeakerTarget(GetFieldText(*Context.Layout, TEXT("Target")));
	if (const UYapCharacterAsset* Asset = FindCharacter(Target.Speaker))
	{
		// The speaker's colour, darkened as a title bar's is.
		FLinearColor Color = BangoYapColor::Desaturate(Asset->GetCharacterColor(), 0.25f) * 0.5f;
		Color.A = 1.0f;
		return Color;
	}
	return BangoYapColor::Gray;
}

TOptional<FText> FBangoYapLineDecorator::GetTooltip(const FBangoNodeDecoratorContext& Context) const
{
	if (Context.CommandName == TEXT("AddPrompt"))
	{
		return LOCTEXT("ToolTip_AddPrompt", "One of the prompt's choices: said by the speaker when the player picks it, then its block runs.");
	}
	if (Context.CommandName == TEXT("SayM"))
	{
		return LOCTEXT("ToolTip_SayM", "A line with mature and child-safe text (hold Shift over it to see the child-safe text).");
	}
	return LOCTEXT("ToolTip_Say", "A line of dialogue. In a Speech block, one of its lines; on its own, as if in Speech(RUN_ALL).");
}

TSharedPtr<SWidget> FBangoYapLineDecorator::MakeExtraContent(const FBangoNodeDecoratorContext& Context)
{
	return SNew(SBangoYapLineRow)
		.Data(FBangoYapLineData::FromContext(Context));
}

bool FBangoYapLineDecorator::HidesField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex) const
{
	return FBangoYapLineData::RowCoversField(Context, FieldIndex);
}

#undef LOCTEXT_NAMESPACE
