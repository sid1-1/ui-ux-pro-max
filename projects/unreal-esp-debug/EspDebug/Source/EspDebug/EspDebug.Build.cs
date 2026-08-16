// Copyright (c) 2026. Development-only debug overlay module.

using UnrealBuildTool;

public class EspDebug : ModuleRules
{
	public EspDebug(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
