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
				"Slate",
				"SlateCore",
				"UnrealEd",
				"WebBrowser",
				"Convai",
				"ApplicationCore",
				"UMGEditor",
				"Blutility",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UMG",
				"EditorStyle",
				"LevelEditor",
				"Projects",
				"ToolMenus",
				"EditorFramework",
				"HTTPServer",
				"Sockets",
				"Networking",
				"RHI",
				"RenderCore",
				"AssetTools",
				"EditorScriptingUtilities",
				"PropertyEditor",
				"DeveloperSettings",
				"ContentBrowser",
				"ContentBrowserData",
				"TextureEditor",
				"LiveCoding",
				"UATHelper",
				"FileUtilities",
			}
		);

		// Enable logging
		const bool bEnableLogging = true;
		PublicDefinitions.Add("CONVAI_PAK_MANAGER_LOG=" + (bEnableLogging ? "1" : "0"));
	}
}
