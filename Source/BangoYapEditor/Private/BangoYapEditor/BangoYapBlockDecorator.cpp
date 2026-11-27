#include "BangoYapBlockDecorator.h"

#include "SBangoYapBlockHeader.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

const TArray<FString>& FBangoYapBlockDecorator::GetCommandNames()
{
	static const TArray<FString> Names = { TEXT("Speech"), TEXT("Prompt"), TEXT("Conversation") };
	return Names;
}

TOptional<FText> FBangoYapBlockDecorator::GetTitle(const FBangoNodeDecoratorContext& Context) const
{
	const FText Command = FText::FromString(Context.CommandName);
	if (!Context.Layout)
	{
		return Command;
	}

	// "Speech  ·  Radio", "Conversation  ·  Shop": the config or name, when it's given as a string.
	const FBangoYapBlockData Data = FBangoYapBlockData::FromContext(Context);
	if (Data.Name.IsEmpty() || !Data.bNameIsLiteral)
	{
		return Command;
	}
	return FText::Format(LOCTEXT("BlockTitle", "{0}  ·  {1}"), Command, FText::FromString(Data.Name));
}

TOptional<FText> FBangoYapBlockDecorator::GetTooltip(const FBangoNodeDecoratorContext& Context) const
{
	if (Context.CommandName == TEXT("Prompt"))
	{
		return LOCTEXT("BlockToolTip_Prompt", "Shows the player the AddPrompt entries reached in its block, and waits for their choice: the chosen entry is said, then its block runs. Must be in a Conversation.");
	}
	if (Context.CommandName == TEXT("Conversation"))
	{
		return LOCTEXT("BlockToolTip_Conversation", "Opens a conversation for its block: the speech and prompts run in it are the conversation's. Waits here while another conversation is open.");
	}
	return LOCTEXT("BlockToolTip_Speech", "A group of lines, said one after another (run all), or one of them (select one, select random). In a Conversation, part of it; otherwise free speech.");
}

TSharedPtr<SWidget> FBangoYapBlockDecorator::MakeExtraContent(const FBangoNodeDecoratorContext& Context)
{
	return SNew(SBangoYapBlockHeader)
		.Data(FBangoYapBlockData::FromContext(Context));
}

bool FBangoYapBlockDecorator::HidesField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex) const
{
	return FBangoYapBlockData::HeaderCoversField(Context, FieldIndex);
}

#undef LOCTEXT_NAMESPACE
