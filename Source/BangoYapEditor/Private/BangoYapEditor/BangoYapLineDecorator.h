#pragma once

#include "CoreMinimal.h"
#include "BangoNodeDecorator.h"

/**
 * Draws a line's node (Say, SayM, AddPrompt) like a fragment of Yap's Flow dialogue node: its title is the speaker
 * (and who it's said to) in the speaker's colour, and under its fields is the row SBangoYapLineRow draws.
 *
 * Stage 1 of the graph nodes (BangoYap Graph Nodes Plan, in the project): the row only shows the line, and the node's
 * fields stay, to edit it. Later stages edit through the row and hide the fields it covers.
 */
class FBangoYapLineDecorator : public IBangoNodeDecorator
{
public:
	virtual TOptional<FText> GetTitle(const FBangoNodeDecoratorContext& Context) const override;
	virtual TOptional<FLinearColor> GetTitleColor(const FBangoNodeDecoratorContext& Context) const override;
	virtual TOptional<FText> GetTooltip(const FBangoNodeDecoratorContext& Context) const override;
	virtual TSharedPtr<SWidget> MakeExtraContent(const FBangoNodeDecoratorContext& Context) override;

	/** The commands drawn this way. */
	static const TArray<FString>& GetCommandNames();
};
