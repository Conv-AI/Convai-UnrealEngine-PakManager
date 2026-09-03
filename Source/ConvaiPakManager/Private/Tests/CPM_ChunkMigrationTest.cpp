// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Chunk/CPM_Chunk.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_AUTOMATION_TESTS

using namespace ConvaiPakManager::Chunk;

namespace
{
	/**
	 * A throwaway ConvaiEssentials directory.
	 *
	 * Migration is tested against a real filesystem rather than an abstraction over one, because
	 * what it must get right IS filesystem behaviour - that a move happened, that a destination was
	 * not clobbered, that a refusal left the originals where they were.
	 */
	struct FScratchEssentials
	{
		FString Path;

		explicit FScratchEssentials(const TCHAR* TestName)
		{
			Path = FPaths::Combine(
				FPaths::ProjectIntermediateDir(), TEXT("CPM_Tests"), TestName, TEXT("ConvaiEssentials"));
			IFileManager::Get().DeleteDirectory(*Path, false, true);
			IFileManager::Get().MakeDirectory(*Path, true);
		}

		~FScratchEssentials()
		{
			IFileManager::Get().DeleteDirectory(*Path, false, true);
		}

		void WriteLegacy(const TCHAR* FileName, const FString& Contents) const
		{
			FFileHelper::SaveStringToFile(Contents, *FPaths::Combine(Path, FileName));
		}

		bool LegacyExists(const TCHAR* FileName) const
		{
			return IFileManager::Get().FileExists(*FPaths::Combine(Path, FileName));
		}

		FString ReadPerChunk(const int32 ChunkId, const FString& FileName) const
		{
			FString Contents;
			FFileHelper::LoadFileToString(
				Contents,
				*FPaths::Combine(Path, FString::Printf(TEXT("ChunkId_%d"), ChunkId), FileName));
			return Contents;
		}

		bool PerChunkExists(const int32 ChunkId, const FString& FileName) const
		{
			return IFileManager::Get().FileExists(
				*FPaths::Combine(Path, FString::Printf(TEXT("ChunkId_%d"), ChunkId), FileName));
		}

		void WritePerChunk(const int32 ChunkId, const FString& FileName, const FString& Contents) const
		{
			const FString Directory = FPaths::Combine(Path, FString::Printf(TEXT("ChunkId_%d"), ChunkId));
			IFileManager::Get().MakeDirectory(*Directory, true);
			FFileHelper::SaveStringToFile(Contents, *FPaths::Combine(Directory, FileName));
		}
	};

	/** A published creator project's CreateAssetData, trimmed to the field that cannot be recovered. */
	const TCHAR* PublishedAssetJson =
		TEXT("{\"transactionID\":\"t-1\",\"assets\":[{\"asset\":{\"asset_id\":\"ea7a8d50-90b8-4485-a746-f53eeb34a843\"}}]}");
}

/**
 * The migration exists for exactly one reason: the AssetID has no second copy anywhere. Read the
 * per-Chunk path without moving the flat file first and a published Chunk looks unpublished, which
 * offers Create where it should offer Update - publishing a duplicate and orphaning the original.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkMigrationPreservesTheAssetId,
	"ConvaiPakManager.Chunk.Migration.PreservesTheAssetId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkMigrationPreservesTheAssetId::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("PreservesTheAssetId"));
	Scratch.WriteLegacy(TEXT("CreateAssetData.json"), PublishedAssetJson);
	Scratch.WriteLegacy(TEXT("PakMetaData.json"), TEXT("{\"asset_name\":\"Dev_CPM_53\"}"));
	Scratch.WriteLegacy(TEXT("ModdingMetaData.txt"), TEXT("{\"asset_type\":\"Scene\"}"));

	TArray<FString> Moved;
	const EMigrationResult Result = MigrateLegacyLayoutIn(Scratch.Path, 10, Moved);

	TestTrue(TEXT("reports that it migrated"), Result == EMigrationResult::Migrated);
	TestEqual(TEXT("moves all three files"), Moved.Num(), 3);

	TestEqual(TEXT("the AssetID survives the move"),
		Scratch.ReadPerChunk(10, TEXT("CreateAssetData_10.json")), FString(PublishedAssetJson));
	TestEqual(TEXT("the pak metadata survives the move"),
		Scratch.ReadPerChunk(10, TEXT("PakMetaData_10.json")), FString(TEXT("{\"asset_name\":\"Dev_CPM_53\"}")));
	// Renamed as it moves: the Modding Tool has always written JSON into that .txt.
	TestEqual(TEXT("the modding metadata survives the move"),
		Scratch.ReadPerChunk(10, TEXT("ModdingMetaData_10.json")), FString(TEXT("{\"asset_type\":\"Scene\"}")));
	TestFalse(TEXT("and no longer claims to be text"),
		Scratch.PerChunkExists(10, TEXT("ModdingMetaData_10.txt")));

	TestFalse(TEXT("the flat CreateAssetData is gone"), Scratch.LegacyExists(TEXT("CreateAssetData.json")));
	TestFalse(TEXT("the flat PakMetaData is gone"), Scratch.LegacyExists(TEXT("PakMetaData.json")));
	TestFalse(TEXT("the flat ModdingMetaData is gone"), Scratch.LegacyExists(TEXT("ModdingMetaData.txt")));

	return true;
}

/**
 * A flat layout predates multi-Chunk support, so it can only have belonged to a project with one
 * Chunk. Attributing it to one of several would bind a published Asset to the wrong Chunk, which is
 * worse than not migrating - so an unresolvable Chunk must leave every original untouched.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkMigrationRefusesWhenTheChunkIsUnknown,
	"ConvaiPakManager.Chunk.Migration.RefusesWhenTheChunkIsUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkMigrationRefusesWhenTheChunkIsUnknown::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("RefusesWhenTheChunkIsUnknown"));
	Scratch.WriteLegacy(TEXT("CreateAssetData.json"), PublishedAssetJson);

	AddExpectedError(TEXT("pre-Chunk ConvaiEssentials layout"), EAutomationExpectedErrorFlags::Contains, 1);

	TArray<FString> Moved;
	const EMigrationResult Result = MigrateLegacyLayoutIn(Scratch.Path, INDEX_NONE, Moved);

	TestTrue(TEXT("reports that it could not attribute the files"), Result == EMigrationResult::Ambiguous);
	TestEqual(TEXT("moves nothing"), Moved.Num(), 0);
	TestTrue(TEXT("the creator's only copy of the AssetID is untouched"),
		Scratch.LegacyExists(TEXT("CreateAssetData.json")));

	return true;
}

/**
 * A destination that already exists means migration has already run and the per-Chunk copy is the
 * live one. Overwriting it would replace current state with whatever stale flat file was left
 * behind - so the newer state wins and the leftover is kept rather than destroyed.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkMigrationNeverOverwritesExistingChunkState,
	"ConvaiPakManager.Chunk.Migration.NeverOverwritesExistingChunkState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkMigrationNeverOverwritesExistingChunkState::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("NeverOverwritesExistingChunkState"));
	Scratch.WriteLegacy(TEXT("CreateAssetData.json"), TEXT("{\"stale\":true}"));
	Scratch.WritePerChunk(10, TEXT("CreateAssetData_10.json"), TEXT("{\"current\":true}"));

	AddExpectedError(TEXT("already exists"), EAutomationExpectedErrorFlags::Contains, 1);

	TArray<FString> Moved;
	MigrateLegacyLayoutIn(Scratch.Path, 10, Moved);

	TestEqual(TEXT("the live per-Chunk state is left alone"),
		Scratch.ReadPerChunk(10, TEXT("CreateAssetData_10.json")), FString(TEXT("{\"current\":true}")));
	TestTrue(TEXT("the stale flat file is kept rather than deleted"),
		Scratch.LegacyExists(TEXT("CreateAssetData.json")));

	return true;
}

/** The ordinary case: every run after the first, and every project generated since. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkMigrationIsSilentWithNothingToMigrate,
	"ConvaiPakManager.Chunk.Migration.IsSilentWithNothingToMigrate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkMigrationIsSilentWithNothingToMigrate::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("IsSilentWithNothingToMigrate"));
	Scratch.WritePerChunk(10, TEXT("CreateAssetData_10.json"), PublishedAssetJson);

	TArray<FString> Moved;
	const EMigrationResult Result = MigrateLegacyLayoutIn(Scratch.Path, 10, Moved);

	TestTrue(TEXT("reports nothing to do"), Result == EMigrationResult::NothingToMigrate);
	TestEqual(TEXT("moves nothing"), Moved.Num(), 0);
	TestEqual(TEXT("leaves the existing state alone"),
		Scratch.ReadPerChunk(10, TEXT("CreateAssetData_10.json")), FString(PublishedAssetJson));

	return true;
}

/**
 * A partly-migrated directory is what an interrupted first run leaves behind. Finishing it must not
 * depend on the files that already moved still being there.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkMigrationCompletesAPartialMigration,
	"ConvaiPakManager.Chunk.Migration.CompletesAPartialMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkMigrationCompletesAPartialMigration::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("CompletesAPartialMigration"));
	Scratch.WritePerChunk(10, TEXT("CreateAssetData_10.json"), PublishedAssetJson);
	Scratch.WriteLegacy(TEXT("PakMetaData.json"), TEXT("{\"asset_name\":\"Dev_CPM_53\"}"));

	TArray<FString> Moved;
	const EMigrationResult Result = MigrateLegacyLayoutIn(Scratch.Path, 10, Moved);

	TestTrue(TEXT("reports that it migrated"), Result == EMigrationResult::Migrated);
	TestEqual(TEXT("moves only what was still flat"), Moved.Num(), 1);
	TestEqual(TEXT("the already-migrated AssetID is untouched"),
		Scratch.ReadPerChunk(10, TEXT("CreateAssetData_10.json")), FString(PublishedAssetJson));
	TestEqual(TEXT("the remaining file lands beside it"),
		Scratch.ReadPerChunk(10, TEXT("PakMetaData_10.json")), FString(TEXT("{\"asset_name\":\"Dev_CPM_53\"}")));

	return true;
}

/**
 * The condition the UI states and gates publishing on. It has to stay true for exactly as long as
 * the creator's AssetID is somewhere nothing reads - including after a migration that could not move
 * everything, where a clean "nothing to migrate" would put a Create button in front of a live Asset.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMChunkMigrationReportsWhatIsStillFlat,
	"ConvaiPakManager.Chunk.Migration.ReportsWhatIsStillFlat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMChunkMigrationReportsWhatIsStillFlat::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("ReportsWhatIsStillFlat"));
	TestFalse(TEXT("an empty ConvaiEssentials has nothing flat"), HasUnmigratedLegacyLayoutIn(Scratch.Path));

	Scratch.WriteLegacy(TEXT("PakMetaData.json"), TEXT("{\"asset_name\":\"Dev_CPM_53\"}"));
	TestTrue(TEXT("a flat PakMetaData is enough"), HasUnmigratedLegacyLayoutIn(Scratch.Path));

	TArray<FString> Moved;
	MigrateLegacyLayoutIn(Scratch.Path, 10, Moved);
	TestFalse(TEXT("and is gone once it has moved"), HasUnmigratedLegacyLayoutIn(Scratch.Path));

	// The Modding Tool's own file, which a project that never published still has.
	Scratch.WriteLegacy(TEXT("ModdingMetaData.txt"), TEXT("{\"plugin_name\":\"A3CLP672QMGL73V5F2KH\"}"));
	TestTrue(TEXT("a flat ModdingMetaData is enough on its own"), HasUnmigratedLegacyLayoutIn(Scratch.Path));

	// Migration leaves a flat file alone when the per-Chunk copy already exists, and this has to keep
	// saying so - that leftover is the case a creator most needs told about.
	Scratch.WritePerChunk(10, TEXT("ModdingMetaData_10.json"), TEXT("{\"plugin_name\":\"A3CLP672QMGL73V5F2KH\"}"));
	AddExpectedError(TEXT("already exists"), EAutomationExpectedErrorFlags::Contains, 1);

	TArray<FString> MovedAgain;
	MigrateLegacyLayoutIn(Scratch.Path, 10, MovedAgain);

	TestEqual(TEXT("nothing moves over the existing copy"), MovedAgain.Num(), 0);
	TestTrue(TEXT("and the leftover is still reported"), HasUnmigratedLegacyLayoutIn(Scratch.Path));

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
