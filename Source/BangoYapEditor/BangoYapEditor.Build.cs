using UnrealBuildTool;

/** BangoYap editor support: dialogue authoring inside the Bango script editor (graph nodes drawn like Yap's). */
public class BangoYapEditor : ModuleRules
{
	public BangoYapEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"Projects",
				"AssetRegistry",
				"GameplayTags",
				"BangoScripts",
				"BangoScriptsScriptEditor",
				"YapBase",
				"YapBaseEditor",
				"BangoYap",
			}
		);
	}
}
