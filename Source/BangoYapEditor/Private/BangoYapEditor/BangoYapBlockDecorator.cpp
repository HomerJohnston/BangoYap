#include "BangoYapBlockDecorator.h"

#include "BangoYapNodeFields.h"
#include "SBangoYapBlockHeader.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

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

TSharedPtr<SWidget> FBangoYapBlockDecorator::MakeFieldsArea(const FBangoNodeDecoratorContext& Context)
{
	// The fields the header doesn't cover (HidesField: expressions it can't edit; none shown takes no room), then the header.
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Context.MakeDefaultFieldsArea ? Context.MakeDefaultFieldsArea() : SNullWidget::NullWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f, 4.0f, 2.0f, 2.0f)
		[
			SNew(SBangoYapBlockHeader)
			.Data(FBangoYapBlockData::FromContext(Context))
		];
}

TSharedPtr<SWidget> FBangoYapBlockDecorator::MakeRowsArea(const FBangoNodeDecoratorContext& Context)
{
	const bool bHoldsLines = Context.CommandName == TEXT("Speech") || Context.CommandName == TEXT("Prompt");
	if (!bHoldsLines || !Context.bEntries)
	{
		return nullptr; // a plain block, or a Conversation: drawn as usual
	}
	return SBangoYapEntryRow::MakeRowsArea(Context);
}

bool FBangoYapBlockDecorator::DrawsEntryStatements(const FBangoNodeDecoratorContext& Context, int32 Entry) const
{
	const bool bHoldsLines = Context.CommandName == TEXT("Speech") || Context.CommandName == TEXT("Prompt");
	return bHoldsLines && Entry != Context.DefaultEntry;
}

bool FBangoYapBlockDecorator::HidesField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex) const
{
	return FBangoYapBlockData::HeaderCoversField(Context, FieldIndex);
}

#undef LOCTEXT_NAMESPACE
