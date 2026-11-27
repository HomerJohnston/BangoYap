#pragma once

#include "CoreMinimal.h"
#include "BangoGraphLayout.h"

namespace BangoYapEditor
{
	/** One field to set: its label, and its new value as script text (empty: leave it as it is). */
	struct FFieldChange
	{
		FString Label;
		FString Value;
	};

	/**
	 * Writes a node's fields for BangoYap's node widgets, through the decorator context's SetFieldValue (each one an
	 * undoable change; the node is drawn again on the next frame).
	 *
	 * - Setting a field after optional ones that aren't there fills those with `nil` first (an argument can't be left
	 *   out from the middle); setting one to `nil` removes it if nothing after it is there.
	 * - Several fields at once are set in the order they're written, with this copy of the layout kept in step between
	 *   them, and any `nil` left at the end is removed afterwards (`Say(Text, nil, nil)` becomes `Say(Text)`).
	 * - A refused change shows as a notification ("Couldn't change the node: ...").
	 */
	class FFieldWriter
	{
	public:
		using FSetFieldValue = TFunction<bool(int32 FieldIndex, const FString& Value, FString& OutError)>;

		FFieldWriter() = default;
		FFieldWriter(const FBangoNodeLayout& InLayout, const FSetFieldValue& InSetFieldValue);

		/** False where the node can't be changed (no SetFieldValue). */
		bool CanEdit() const { return (bool)SetFieldValue; }

		bool SetField(const TCHAR* Label, const FString& Value);
		/** Stops at the first change that's refused. */
		bool SetFields(const TArray<FFieldChange>& Changes);

		const FBangoNodeLayout& GetLayout() const { return Layout; }

		static void ReportError(const FString& Error);

	private:
		void RemoveTrailingNils();

		FBangoNodeLayout Layout;
		FSetFieldValue SetFieldValue;
	};
}
