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
				"UMG",
				"UMGEditor",
				"ConvaiPakManager",
				"ConvaiJobSystem",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
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
                "UATHelper", 
                "LiveCoding",
                "RenderCore",
                "FileUtilities",
                "AssetRegistry",
                "Json",
                "ToolMenus",
                // The SDK's editor module: its widget kit, styling and shell. See docs/adr/0006 and 0007.
                "ConvaiEditor",
                "Projects"  // For IPluginManager
			}
			);
		
	}
}
