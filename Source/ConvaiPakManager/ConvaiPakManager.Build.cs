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
				"CONVAIHTTP",
				"PakFile", 
				"ImageWrapper",
				"Convai",
				"HTTP"
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
				"Json", 
				"JsonUtilities",
				// ... add private dependencies that you statically link with here ...	
			}
			);
			
		// Only include DesktopPlatform for editor builds, not for shipping builds
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("DesktopPlatform");
		}
		
		const bool bEnableLogging = true;
		PublicDefinitions.Add("CONVAI_PAK_MANAGER_LOG=" + (bEnableLogging ? "1" : "0"));
	}
}
