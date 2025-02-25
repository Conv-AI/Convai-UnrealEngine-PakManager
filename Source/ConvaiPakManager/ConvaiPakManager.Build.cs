// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ConvaiPakManager : ModuleRules
{
	public ConvaiPakManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"HTTP",
				"PakFile", "ImageWrapper",
				// ... add other public dependencies that you statically link with here ...
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"DesktopPlatform",
				"Json", 
				"Convai",
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
