#pragma once

#include "CoreMinimal.h"
#include "BangoNodeDecorator.h"

/**
 * Draws a line's node (Say, SayM, AddPrompt) like a fragment of Yap's Flow dialogue node: its title is the speaker
 * (and who it's said to) in the speaker's colour, and its fields area (Bango v0.144) is the fields the row doesn't
 * cover, with the row SBangoYapLineRow draws under them.
 *
 * The row edits the speaker, who it's said to, the mood, the texts and the line's settings, and the node leaves those
 * fields out (but for any written as an expression the row can't edit). See BangoYap Graph Nodes Plan, in the project.
 */
class FBangoYapLineDecorator : public IBangoNodeDecorator
{
public:
	virtual TOptional<FText> GetTitle(const FBangoNodeDecoratorContext& Context) const override;
	virtual TOptional<FLinearColor> GetTitleColor(const FBangoNodeDecoratorContext& Context) const override;
	virtual TOptional<FText> GetTooltip(const FBangoNodeDecoratorContext& Context) const override;
	virtual TSharedPtr<SWidget> MakeFieldsArea(const FBangoNodeDecoratorContext& Context) override;
	virtual bool HidesField(const FBangoNodeDecoratorContext& Context, int32 FieldIndex) const override;

	/** The commands drawn this way. */
	static const TArray<FString>& GetCommandNames();
};
