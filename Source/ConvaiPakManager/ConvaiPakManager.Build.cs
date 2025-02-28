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
				"PakFile", 
				"ImageWrapper",
				"Convai",
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
				"JsonUtilities",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		const bool bEnableLogging = true;
		PublicDefinitions.Add("CONVAI_PAK_MANAGER_LOG=" + (bEnableLogging ? "1" : "0"));
	}
}
