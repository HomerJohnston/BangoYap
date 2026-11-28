#pragma once

#include "CoreMinimal.h"
#include "BangoNodeDecorator.h"

/**
 * Draws BangoYap's block commands' nodes (Speech, Prompt, Conversation) with the header of Yap's Flow dialogue node
 * under their fields (SBangoYapBlockHeader): Speech's run mode, config and progression settings, Prompt's config,
 * Conversation's name. The node leaves out the fields the header covers, but for any written as an expression.
 *
 * Stage 5 of the graph nodes (BangoYap Graph Nodes Plan, in the project). With a plain block, each line is still its own
 * node, wired from the block's Body. When the block is a list of entries (stage 7), each entry's row is an
 * SBangoYapEntryRow, and holds its lines or options (Bango v0.143), but the default's, which is wired as nodes.
 */
class FBangoYapBlockDecorator : public IBangoNodeDecorator
{
public:
	virtual TOptional<FText> GetTitle(const FBangoNodeDecoratorContext& Context) const override;
	virtual TOptional<FText> GetTooltip(const FBangoNodeDecoratorContext& Context) const override;
	virtual TSharedPtr<SWidget> MakeExtraContent(const FBangoNodeDecoratorContext& Context) override;
	virtual bool HidesField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex) const override;
	/** A Speech's or Prompt's entry row (SBangoYapEntryRow), when its block is a list of entries. */
	virtual TSharedPtr<SWidget> MakeEntryRowWidget(const FBangoNodeDecoratorContext& Context, int32 Entry) override;
	/** A Speech's and a Prompt's entries hold their lines or options in their rows (SBangoYapEntryStatements), but the default (the bypass, which may hold anything). */
	virtual bool DrawsEntryStatements(const FBangoNodeDecoratorContext& Context, int32 Entry) const override;

	/** The commands drawn this way. */
	static const TArray<FString>& GetCommandNames();
};
