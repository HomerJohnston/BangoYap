#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * Yap's look, for BangoYap's graph nodes: brushes, icons and fonts copied from Yap's editor style
 * (Plugins/BangoYap/Resources holds Yap's images).
 */
class FBangoYapEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static const ISlateStyle& Get();
	static FName GetStyleSetName();

	static const FSlateBrush* GetBrush(FName Name);

	/** The icon for a mood, by its tag's last part ("Happy"); a question mark for one without an icon; empty: no mood. */
	static const FSlateBrush* GetMoodBrush(const FString& MoodName);

	/** Yap's dialogue text (Normal 10) and title text (Italic 10). */
	static FSlateFontInfo DialogueFont();
	static FSlateFontInfo TitleFont();

	// Brush names
	static const FName Box_SolidWhite;
	static const FName Box_SolidWhite_Rounded;
	static const FName Border_Thick_RoundedSquare;
	static const FName Icon_IDTag;
	static const FName Icon_FilledCircle;
	static const FName Portrait_Missing;
	static const FName Mood_None;
	static const FName Mood_Unknown;

private:
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};

/** Yap's colours (YapEditorColor.h), the ones BangoYap's nodes use. */
namespace BangoYapColor
{
	inline const FLinearColor White(1.0f, 1.0f, 1.0f, 1.0f);
	inline const FLinearColor White_Trans(1.0f, 1.0f, 1.0f, 0.7f);
	inline const FLinearColor White_Glass(1.0f, 1.0f, 1.0f, 0.2f);
	inline const FLinearColor LightGray(0.5f, 0.5f, 0.5f, 1.0f);
	inline const FLinearColor Gray(0.2f, 0.2f, 0.2f, 1.0f);
	inline const FLinearColor DarkGray(0.03f, 0.03f, 0.03f, 1.0f);
	inline const FLinearColor DarkGray_SemiGlass(0.03f, 0.03f, 0.03f, 0.45f);
	inline const FLinearColor DarkGray_Glass(0.03f, 0.03f, 0.03f, 0.2f);
	inline const FLinearColor YellowGray(0.5f, 0.45f, 0.25f, 1.0f);
	inline const FLinearColor LightYellow(0.9f, 0.85f, 0.25f, 1.0f);
	inline const FLinearColor Red(0.95f, 0.04f, 0.06f, 1.0f);
	inline const FLinearColor LightBlue(0.34f, 0.42f, 1.0f, 1.0f);
	inline const FLinearColor Orange(0.78f, 0.38f, 0.05f, 1.0f);

	/** Yap's own desaturate (FLinearColor::Desaturate also fades alpha). */
	inline FLinearColor Desaturate(const FLinearColor& Color, float Amount)
	{
		const float Lum = 0.3f * Color.R + 0.59f * Color.G + 0.11f * Color.B;
		return FLinearColor(FMath::Lerp(Color.R, Lum, Amount), FMath::Lerp(Color.G, Lum, Amount), FMath::Lerp(Color.B, Lum, Amount), Color.A);
	}
}
