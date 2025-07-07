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
				"Core", "PakFile", "ImageWrapper", "Convai", "AssetRegistry" }
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject", "Engine", "Slate", "SlateCore", "Json", "JsonUtilities", "Projects" }
			);
			
		
		
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("DesktopPlatform");
		}
		
		const bool bEnableLogging = true;
		PublicDefinitions.Add("CONVAI_PAK_MANAGER_LOG=" + (bEnableLogging ? "1" : "0"));
	}
}
