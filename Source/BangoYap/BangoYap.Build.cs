using UnrealBuildTool;

/** The BangoYap runtime: Bango commands that hand dialogue to YapBase and wait on it. */
public class BangoYap : ModuleRules
{
	public BangoYap(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"BangoScripts",
				"YapBase",
			}
		);
	}
}
