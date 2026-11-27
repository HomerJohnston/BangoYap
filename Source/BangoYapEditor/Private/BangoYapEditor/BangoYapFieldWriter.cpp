#include "BangoYapFieldWriter.h"

#include "BangoYapNodeFields.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "BangoYapEditor"

using namespace BangoYapEditor;

FFieldWriter::FFieldWriter(const FBangoNodeLayout& InLayout, const FSetFieldValue& InSetFieldValue)
	: Layout(InLayout)
	, SetFieldValue(InSetFieldValue)
{
}

void FFieldWriter::ReportError(const FString& Error)
{
	FNotificationInfo Info(FText::Format(LOCTEXT("SetFieldError", "Couldn't change the node: {0}"), FText::FromString(Error)));
	Info.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

bool FFieldWriter::SetField(const TCHAR* Label, const FString& Value)
{
	if (!CanEdit())
	{
		return false;
	}

	const int32 Index = FindField(Layout, Label);
	if (Index == INDEX_NONE)
	{
		ReportError(FString::Printf(TEXT("it has no %s."), Label));
		return false;
	}

	FString Error;

	// An argument can't be left out from the middle: fill the optional ones before it that aren't there with nil.
	if (!Value.IsEmpty() && Value != TEXT("nil"))
	{
		for (int32 i = 0; i < Index; ++i)
		{
			FBangoNodeField& Before = Layout.Fields[i];
			if (Before.bOptional && !Before.bPresent && Before.bHasValue)
			{
				if (!SetFieldValue(i, TEXT("nil"), Error))
				{
					ReportError(Error);
					return false;
				}
				Before.bPresent = true;
				Before.Text = TEXT("nil");
			}
		}
	}

	const FString Final = (Value == TEXT("nil") && Layout.Fields[Index].bOptional) ? GetClearValue(Layout, Index) : Value;
	if (Final.IsEmpty() && !Layout.Fields[Index].bPresent)
	{
		return true; // already not there
	}

	if (!SetFieldValue(Index, Final, Error))
	{
		ReportError(Error);
		return false;
	}

	// The node is drawn again from the script later; until then, keep this copy of its fields in step, for any
	// further changes made in the same go.
	Layout.Fields[Index].bPresent = !Final.IsEmpty();
	Layout.Fields[Index].Text = Final;
	return true;
}

bool FFieldWriter::SetFields(const TArray<FFieldChange>& Changes)
{
	if (!CanEdit())
	{
		return false;
	}

	// In the order they're written, so filling the ones before a field with nil never undoes a change.
	TArray<FFieldChange> Ordered;
	for (const FFieldChange& Change : Changes)
	{
		if (!Change.Value.IsEmpty())
		{
			Ordered.Add(Change);
		}
	}
	Ordered.StableSort([this](const FFieldChange& A, const FFieldChange& B)
	{
		return FindField(Layout, *A.Label) < FindField(Layout, *B.Label);
	});

	for (const FFieldChange& Change : Ordered)
	{
		if (!SetField(*Change.Label, Change.Value))
		{
			return false;
		}
	}

	RemoveTrailingNils();
	return true;
}

void FFieldWriter::RemoveTrailingNils()
{
	FString Error;
	for (int32 i = Layout.Fields.Num() - 1; i >= 0; --i)
	{
		FBangoNodeField& Field = Layout.Fields[i];
		if (!Field.bPresent)
		{
			continue;
		}
		if (!Field.bOptional || Field.bLocked || Field.Text.TrimStartAndEnd() != TEXT("nil"))
		{
			return;
		}
		// The last one there, and nil: leave it out.
		if (!SetFieldValue(i, FString(), Error))
		{
			return; // not worth an error: the script is still right
		}
		Field.bPresent = false;
		Field.Text.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
