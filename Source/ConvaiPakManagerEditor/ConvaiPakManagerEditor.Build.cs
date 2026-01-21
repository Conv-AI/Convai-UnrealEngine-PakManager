// Copyright 2022 Convai Inc. All Rights Reserved.

using UnrealBuildTool;

public class ConvaiPakManagerEditor : ModuleRules
{
	public ConvaiPakManagerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"HTTP",
				"Json",
				"JsonUtilities",
				"PakFile",
				"ImageWrapper",
				"AssetRegistry",
				"Convai"
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Slate
				"Slate",
				"SlateCore",
				
				// UMG
				"UMG",
				"UMGEditor",
				
				// Editor
				"UnrealEd",
				"LevelEditor",
				"Blutility",
				"EditorScriptingUtilities",
				"PropertyEditor",
				"DeveloperSettings",
				"EditorSubsystem",
				"ContentBrowser",
				"AssetTools",
				"DesktopPlatform",
				
				// Packaging
				"UATHelper",
				"LiveCoding",
				
				// Rendering
				"RenderCore",
				
				// File utilities
				"FileUtilities",
				"Projects"
			}
		);
		
		// Enable logging
		const bool bEnableLogging = true;
		PublicDefinitions.Add("CONVAI_PAK_MANAGER_LOG=" + (bEnableLogging ? "1" : "0"));
	}
}
