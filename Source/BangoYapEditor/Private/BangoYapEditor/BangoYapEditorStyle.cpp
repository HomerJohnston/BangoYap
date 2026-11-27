#include "BangoYapEditorStyle.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

TSharedPtr<FSlateStyleSet> FBangoYapEditorStyle::StyleInstance = nullptr;

const FName FBangoYapEditorStyle::Box_SolidWhite(TEXT("BangoYap.Box_SolidWhite"));
const FName FBangoYapEditorStyle::Box_SolidWhite_Rounded(TEXT("BangoYap.Box_SolidWhite_Rounded"));
const FName FBangoYapEditorStyle::Border_Thick_RoundedSquare(TEXT("BangoYap.Border_Thick_RoundedSquare"));
const FName FBangoYapEditorStyle::Icon_IDTag(TEXT("BangoYap.Icon_IDTag"));
const FName FBangoYapEditorStyle::Icon_FilledCircle(TEXT("BangoYap.Icon_FilledCircle"));
const FName FBangoYapEditorStyle::Portrait_Missing(TEXT("BangoYap.Portrait_Missing"));
const FName FBangoYapEditorStyle::Mood_None(TEXT("BangoYap.Mood.None"));
const FName FBangoYapEditorStyle::Mood_Unknown(TEXT("BangoYap.Mood.Unknown"));

FName FBangoYapEditorStyle::GetStyleSetName()
{
	static const FName Name(TEXT("BangoYapEditorStyle"));
	return Name;
}

void FBangoYapEditorStyle::Initialize()
{
	if (StyleInstance.IsValid())
	{
		return;
	}

	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BangoYap"));
	const FString Resources = Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Resources") : FString();
	Style->SetContentRoot(Resources);

	// As Yap's style defines them (YapEditorStyle.cpp).
	Style->Set(Box_SolidWhite, new FSlateBoxBrush(Style->RootToContentDir(TEXT("Box_SolidWhite"), TEXT(".png")), FMargin(4.0f / 8.0f)));
	Style->Set(Box_SolidWhite_Rounded, new FSlateBoxBrush(Style->RootToContentDir(TEXT("Box_SolidWhite_Rounded"), TEXT(".png")), FMargin(4.0f / 8.0f)));
	Style->Set(Border_Thick_RoundedSquare, new FSlateBorderBrush(Style->RootToContentDir(TEXT("Border_Thick_Rounded"), TEXT(".png")), FMargin(8.0f / 16.0f)));
	Style->Set(Icon_IDTag, new FSlateBoxBrush(Style->RootToContentDir(TEXT("Icon_IDTag"), TEXT(".png")), FMargin(0.5f, 0.5f, 0.5f, 0.5f)));
	Style->Set(Portrait_Missing, new FSlateImageBrush(Style->RootToContentDir(TEXT("Missing_Portrait"), TEXT(".png")), FVector2f(64.0f, 64.0f)));
	if (const FSlateBrush* Circle = FAppStyle::GetBrush(TEXT("Icons.FilledCircle")))
	{
		Style->Set(Icon_FilledCircle, new FSlateVectorImageBrush(Circle->GetResourceName().ToString(), FVector2f(16.0f, 16.0f)));
	}

	// Mood icons: Resources/MoodIcons/<Mood>.svg (Yap's default mood icons, by their tag's last part).
	Style->Set(Mood_None, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT("Icon_MoodTag_None"), TEXT(".svg")), FVector2f(16.0f, 16.0f)));
	TArray<FString> MoodFiles;
	IFileManager::Get().FindFiles(MoodFiles, *(Resources / TEXT("MoodIcons") / TEXT("*.svg")), true, false);
	for (const FString& File : MoodFiles)
	{
		const FString Mood = FPaths::GetBaseFilename(File);
		Style->Set(FName(*(TEXT("BangoYap.Mood.") + Mood)), new FSlateVectorImageBrush(Resources / TEXT("MoodIcons") / File, FVector2f(16.0f, 16.0f)));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*Style);
	StyleInstance = Style;
}

void FBangoYapEditorStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

const ISlateStyle& FBangoYapEditorStyle::Get()
{
	check(StyleInstance.IsValid());
	return *StyleInstance;
}

const FSlateBrush* FBangoYapEditorStyle::GetBrush(FName Name)
{
	return StyleInstance.IsValid() ? StyleInstance->GetBrush(Name) : FAppStyle::GetNoBrush();
}

const FSlateBrush* FBangoYapEditorStyle::GetMoodBrush(const FString& MoodName)
{
	if (!StyleInstance.IsValid())
	{
		return FAppStyle::GetNoBrush();
	}
	if (MoodName.IsEmpty())
	{
		return StyleInstance->GetBrush(Mood_None);
	}

	const FName Name(*(TEXT("BangoYap.Mood.") + MoodName));
	const FSlateBrush* Brush = StyleInstance->GetOptionalBrush(Name, nullptr, nullptr);
	return Brush ? Brush : StyleInstance->GetBrush(Mood_Unknown);
}

FSlateFontInfo FBangoYapEditorStyle::DialogueFont()
{
	return FCoreStyle::GetDefaultFontStyle("Regular", 10);
}

FSlateFontInfo FBangoYapEditorStyle::TitleFont()
{
	return FCoreStyle::GetDefaultFontStyle("Italic", 10);
}
