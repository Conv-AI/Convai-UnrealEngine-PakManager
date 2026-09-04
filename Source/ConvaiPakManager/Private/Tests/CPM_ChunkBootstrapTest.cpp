// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Chunk/CPM_Chunk.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"

#if WITH_AUTOMATION_TESTS

using namespace ConvaiPakManager::Chunk;

namespace
{
	/**
	 * A throwaway ConvaiEssentials directory. Its own rather than the migration test's, because a
	 * unity build puts both files' unnamed namespaces in one translation unit.
	 */
	struct FScratchBootstrapEssentials
	{
		FString Path;

		explicit FScratchBootstrapEssentials(const TCHAR* TestName)
		{
			Path = FPaths::Combine(
				FPaths::ProjectIntermediateDir(), TEXT("CPM_Tests"), TestName, TEXT("ConvaiEssentials"));
			IFileManager::Get().DeleteDirectory(*Path, false, true);
			IFileManager::Get().MakeDirectory(*Path, true);
		}

		~FScratchBootstrapEssentials()
		{
			IFileManager::Get().DeleteDirectory(*Path, false, true);
		}

		void Write(const FString& RelativePath, const FString& Contents) const
		{
			FFileHelper::SaveStringToFile(Contents, *FPaths::Combine(Path, RelativePath));
		}
	};
}

/**
 * The un-migrated project has no Chunk, so there is no per-Chunk path for it to resolve - and its
 * plugin_name, which is what minting its Chunk needs, is only in the flat file. Without this the
 * whole bootstrap resolves `ChunkId_-1/ModdingMetaData_-1.json`, reads nothing, and the project
 * reports no Asset Type on top of having no Chunk.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkBootstrapReadsTheFlatModdingMetadata,
	"ConvaiPakManager.Chunk.Bootstrap.ReadsTheFlatModdingMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkBootstrapReadsTheFlatModdingMetadata::RunTest(const FString&)
{
	const FScratchBootstrapEssentials Scratch(TEXT("ReadsTheFlatModdingMetadata"));

	const FString PerChunkPath = FPaths::Combine(Scratch.Path, TEXT("ChunkId_10"), TEXT("ModdingMetaData_10.json"));
	TestEqual(TEXT("an empty project still resolves the per-Chunk path"),
		GetModdingMetadataPathIn(Scratch.Path, 10), PerChunkPath);

	const FString FlatPath = FPaths::Combine(Scratch.Path, TEXT("ModdingMetaData.txt"));
	Scratch.Write(TEXT("ModdingMetaData.txt"), TEXT("{\"plugin_name\":\"A3CLP672QMGL73V5F2KH\"}"));

	TestEqual(TEXT("the flat file answers for a named Chunk"),
		GetModdingMetadataPathIn(Scratch.Path, 10), FlatPath);
	// The call an un-migrated project actually makes: it has no Chunk to name.
	TestEqual(TEXT("and for a project that has no Chunk at all"),
		GetModdingMetadataPathIn(Scratch.Path, INDEX_NONE), FlatPath);

	// Once migrated, the per-Chunk copy is the live one and the flat file is a leftover.
	Scratch.Write(FPaths::Combine(TEXT("ChunkId_10"), TEXT("ModdingMetaData_10.json")), TEXT("{}"));
	TestEqual(TEXT("the per-Chunk copy outranks the flat one"),
		GetModdingMetadataPathIn(Scratch.Path, 10), PerChunkPath);

	return true;
}

/**
 * The project the Modding Tool generates today: it writes `ChunkId_10/ModdingMetaData_10.json` and
 * no Primary Asset Label, so the project has no Chunk to ask with AND no flat file to fall back to.
 * Without this it resolves `ChunkId_-1/ModdingMetaData_-1.json`, reads nothing, and reports a
 * project with no modding plugin - which is the one thing minting its Chunk needs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkBootstrapReadsTheDefaultChunksModdingMetadata,
	"ConvaiPakManager.Chunk.Bootstrap.ReadsTheDefaultChunksModdingMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkBootstrapReadsTheDefaultChunksModdingMetadata::RunTest(const FString&)
{
	const FScratchBootstrapEssentials Scratch(TEXT("ReadsTheDefaultChunksModdingMetadata"));

	const FString DefaultDirectory = FString::Printf(TEXT("ChunkId_%d"), DefaultChunkId);
	const FString DefaultName = FString::Printf(TEXT("ModdingMetaData_%d.json"), DefaultChunkId);
	Scratch.Write(FPaths::Combine(DefaultDirectory, DefaultName), TEXT("{\"plugin_name\":\"P\"}"));

	TestEqual(TEXT("a project with no Chunk reads the default Chunk's metadata"),
		GetModdingMetadataPathIn(Scratch.Path, INDEX_NONE),
		FPaths::Combine(Scratch.Path, DefaultDirectory, DefaultName));

	// A named Chunk is a fact about the project, so it is never answered with another Chunk's file.
	TestEqual(TEXT("a named Chunk still resolves its own path"),
		GetModdingMetadataPathIn(Scratch.Path, 11),
		FPaths::Combine(Scratch.Path, TEXT("ChunkId_11"), TEXT("ModdingMetaData_11.json")));

	// The un-migrated project keeps its flat file, which is the older layout and the more specific
	// answer: a default-Chunk copy beside it would have been written by a tool that also wrote a
	// label, and then there would be a Chunk to ask with.
	Scratch.Write(TEXT("ModdingMetaData.txt"), TEXT("{\"plugin_name\":\"Q\"}"));
	TestEqual(TEXT("the flat file still outranks the default Chunk's copy"),
		GetModdingMetadataPathIn(Scratch.Path, INDEX_NONE),
		FPaths::Combine(Scratch.Path, TEXT("ModdingMetaData.txt")));

	return true;
}

/**
 * An Entry Point outside the Modding Plugin is not in what the label gathers, so it cooks into no
 * Pak and the published Asset opens nothing - and nothing between here and a Convai product would
 * notice. Contains() rather than StartsWith() is how the legacy check let a creator's own
 * `/Game/<plugin>_old/` copy through.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkBootstrapEntryPointMustLiveInThePlugin,
	"ConvaiPakManager.Chunk.Bootstrap.EntryPointMustLiveInThePlugin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkBootstrapEntryPointMustLiveInThePlugin::RunTest(const FString&)
{
	TestTrue(TEXT("a package under the plugin passes"), IsUnderModdingPlugin(TEXT("/P/Maps/A"), TEXT("P")));
	// Mount points are case-insensitive, and the metadata's spelling need not match the creator's.
	TestTrue(TEXT("case does not decide it"), IsUnderModdingPlugin(TEXT("/p/maps/a"), TEXT("P")));

	TestFalse(TEXT("a copy kept beside the plugin is refused"),
		IsUnderModdingPlugin(TEXT("/Game/P_old/A"), TEXT("P")));
	TestFalse(TEXT("a mount root that merely starts with the name is refused"),
		IsUnderModdingPlugin(TEXT("/GameP/A"), TEXT("P")));
	TestFalse(TEXT("the mount root itself is not a package in the plugin"),
		IsUnderModdingPlugin(TEXT("/P"), TEXT("P")));
	TestFalse(TEXT("another plugin is refused"), IsUnderModdingPlugin(TEXT("/P/A"), TEXT("Q")));

	// An internal project labels its own /Game content and records no plugin to be inside of.
	TestTrue(TEXT("a project with no Modding Plugin is not restricted"),
		IsUnderModdingPlugin(TEXT("/Game/Anything"), FString()));

	return true;
}

/**
 * The same refusal gates recording an Entry Point and copying one into the plugin, because the copy
 * drags a whole dependency closure under the mount and leaves it there: a level picked from /Game
 * in an Avatar project is outside the plugin AND the wrong kind, and only the second one can stop
 * the copy before it starts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkBootstrapEntryPointMustMatchTheAssetType,
	"ConvaiPakManager.Chunk.Bootstrap.EntryPointMustMatchTheAssetType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkBootstrapEntryPointMustMatchTheAssetType::RunTest(const FString&)
{
	const FTopLevelAssetPath Level = UWorld::StaticClass()->GetClassPathName();
	const FTopLevelAssetPath Blueprint = UBlueprint::StaticClass()->GetClassPathName();

	FString Why;
	TestTrue(TEXT("a scene takes a level"), EntryPointSuitsAssetType(Level, TEXT("/P/Maps/A"), TEXT("Scene"), Why));
	TestTrue(TEXT("an avatar takes a blueprint"),
		EntryPointSuitsAssetType(Blueprint, TEXT("/P/BP_A"), TEXT("Avatar"), Why));
	TestTrue(TEXT("a passing asset leaves OutWhy alone"), Why.IsEmpty());

	TestFalse(TEXT("a scene refuses a blueprint"),
		EntryPointSuitsAssetType(Blueprint, TEXT("/P/BP_A"), TEXT("Scene"), Why));
	TestTrue(TEXT("and says why"), Why.Contains(TEXT("must be a level")));

	// The case the relocate path exists for: outside the plugin is checked first, so this asset
	// reaches the copy offer and only the kind can turn it back.
	TestFalse(TEXT("an avatar refuses a level"),
		EntryPointSuitsAssetType(Level, TEXT("/Game/Maps/A"), TEXT("Avatar"), Why));
	TestTrue(TEXT("and names the asset"), Why.Contains(TEXT("/Game/Maps/A")));

	// The metadata records "Scene" or "Avatar"; anything else a legacy project wrote is an avatar,
	// which is what a Chunk is when nothing says otherwise.
	TestFalse(TEXT("an unrecognised asset type is read as an avatar"),
		EntryPointSuitsAssetType(Level, TEXT("/P/Maps/A"), FString(), Why));

	return true;
}

/**
 * The Modding Tool generates a descriptor that declares no dependencies, so content under the
 * plugin may not reference Convai's - which every Entry Point ends up doing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkBootstrapDeclaresConvaiOnce,
	"ConvaiPakManager.Chunk.Bootstrap.DeclaresConvaiOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkBootstrapDeclaresConvaiOnce::RunTest(const FString&)
{
	FPluginDescriptor Generated;
	TestTrue(TEXT("a generated descriptor gains the dependency"), DeclareConvaiDependency(Generated, TEXT("Convai")));
	if (!TestEqual(TEXT("exactly one"), Generated.Plugins.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("named Convai"), Generated.Plugins[0].Name, FString(TEXT("Convai")));
	TestTrue(TEXT("and enabled, which is what the domain database reads"), Generated.Plugins[0].bEnabled);

	TestFalse(TEXT("a second pass changes nothing"), DeclareConvaiDependency(Generated, TEXT("Convai")));
	TestEqual(TEXT("and adds no duplicate"), Generated.Plugins.Num(), 1);

	// The `.uplugin` the SDK ships spells its own name "ConvAI" while its content root reads
	// "/Convai/"; a descriptor naming either already declares the dependency.
	FPluginDescriptor OtherCase;
	OtherCase.Plugins.Emplace(TEXT("ConvAI"), true);
	TestFalse(TEXT("case does not make a second entry"), DeclareConvaiDependency(OtherCase, TEXT("Convai")));
	TestEqual(TEXT("so the descriptor still holds one"), OtherCase.Plugins.Num(), 1);

	FPluginDescriptor Disabled;
	Disabled.Plugins.Emplace(TEXT("Convai"), false);
	TestTrue(TEXT("a disabled entry is enabled rather than duplicated"),
		DeclareConvaiDependency(Disabled, TEXT("Convai")));
	TestEqual(TEXT("still one entry"), Disabled.Plugins.Num(), 1);
	TestTrue(TEXT("now enabled"), Disabled.Plugins[0].bEnabled);

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
