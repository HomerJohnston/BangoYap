#pragma once

#include "CoreMinimal.h"
#include "BangoNodeDecorator.h"

/**
 * Draws BangoYap's block commands' nodes (Speech, Prompt, Conversation) with the header of Yap's Flow dialogue node
 * under their fields (SBangoYapBlockHeader): Speech's run mode, config and progression settings, Prompt's config,
 * Conversation's name. The node leaves out the fields the header covers, but for any written as an expression.
 *
 * Stage 5 of the graph nodes (BangoYap Graph Nodes Plan, in the project). With a plain block, each line is still its own
 * node, wired from the block's Body. When the block is a list of entries (stage 7), the node's rows area (Bango
 * v0.144), under the header, the node's whole width, has a row per entry (SBangoYapEntryRow), holding its lines or
 * options (Bango v0.143), but the default's, which is wired as nodes; each option's pin, and the default's, on the
 * node's right edge.
 */
class FBangoYapBlockDecorator : public IBangoNodeDecorator
{
public:
	virtual TOptional<FText> GetTitle(const FBangoNodeDecoratorContext& Context) const override;
	virtual TOptional<FText> GetTooltip(const FBangoNodeDecoratorContext& Context) const override;
	/** The fields the header doesn't cover, then the header (SBangoYapBlockHeader). */
	virtual TSharedPtr<SWidget> MakeFieldsArea(const FBangoNodeDecoratorContext& Context) override;
	virtual bool HidesField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex) const override;
	/** A Speech's or Prompt's entries, a row each (SBangoYapEntryRow::MakeRowsArea), when its block is a list of entries. */
	virtual TSharedPtr<SWidget> MakeRowsArea(const FBangoNodeDecoratorContext& Context) override;
	/** A Speech's and a Prompt's entries hold their lines or options in their rows (SBangoYapEntryStatements), but the default (the bypass, which may hold anything). */
	virtual bool DrawsEntryStatements(const FBangoNodeDecoratorContext& Context, int32 Entry) const override;

	/** The commands drawn this way. */
	static const TArray<FString>& GetCommandNames();
};
