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
				"Core", "PakFile", "ImageWrapper", "Convai", }
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject", "Engine", "Slate", "SlateCore", "Json", "JsonUtilities", }
			);
			
		/* Use ConvaiHTTP plugin*/
		const bool bEnableConvaiHTTP = true;
		PublicDefinitions.AddRange(new string[] { "USE_CONVAI_HTTP=0" + (bEnableConvaiHTTP ? "1" : "0")});
		if (bEnableConvaiHTTP)
		{
			PublicDependencyModuleNames.AddRange(new string[] { "CONVAIHTTP", "HTTP" });
		}
		else
		{
			PublicDependencyModuleNames.AddRange(new string[] { "HTTP" });
		}
		
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("DesktopPlatform");
		}
		
		const bool bEnableLogging = true;
		PublicDefinitions.Add("CONVAI_PAK_MANAGER_LOG=" + (bEnableLogging ? "1" : "0"));
	}
}
