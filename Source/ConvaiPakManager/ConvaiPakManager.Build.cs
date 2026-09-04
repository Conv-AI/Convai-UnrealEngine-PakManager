// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * One editor module.
 *
 * There were two, split runtime/editor, but nothing in a packaged product ever depended on the
 * runtime half: both modules were Win64-only while projects package for Linux too, so a Linux build
 * contained neither and its Paks load fine. Everything here exists to serve the editor tool, and the
 * split only ever raised the question of which side a helper belonged on.
 */
public class ConvaiPakManager : ModuleRules
{
	public ConvaiPakManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PakFile",
				"ImageWrapper",
				"AssetRegistry",
				"Convai",
				// UCPM_PakManagerSettings derives from UDeveloperSettings in a public header.
				"DeveloperSettings",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"LevelEditor",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"PropertyEditor",
				"ContentBrowser",
				"AssetTools",
				"DesktopPlatform",
				"UATHelper",
				"LiveCoding",
				"RenderCore",
				// FImage::ChangeFormat, for reading a texture's authored source pixels.
				"ImageCore",
				"FileUtilities",
				// ANavMeshBoundsVolume, which a Scene is refused a publish without.
				"NavigationSystem",
				"ToolMenus",
				"Json",
				"JsonUtilities",
				"Projects",
				// The nomad tab sits in the Window menu's Tools category. See docs/adr/0009.
				"WorkspaceMenuStructure",
				// Clipboard, for the copyable Asset ID.
				"ApplicationCore",
			}
			);

		const bool bEnableLogging = true;
		PublicDefinitions.Add("CONVAI_PAK_MANAGER_LOG=" + (bEnableLogging ? "1" : "0"));
	}
}
