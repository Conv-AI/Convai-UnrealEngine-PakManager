// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Chunk/CPM_Chunk.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_AUTOMATION_TESTS

using namespace ConvaiPakManager::Chunk;

namespace
{
	/**
	 * A throwaway ConvaiEssentials directory.
	 *
	 * Adoption is tested against a real filesystem rather than an abstraction over one, because what
	 * it must get right IS filesystem behaviour - that a move happened, that a backend that has
	 * already published was not clobbered, that what the creator authored stayed where it was.
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

		void WriteAt(const FString& RelativePath, const FString& Contents) const
		{
			const FString Absolute = FPaths::Combine(Path, RelativePath);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Absolute), true);
			FFileHelper::SaveStringToFile(Contents, *Absolute);
		}

		FString ReadAt(const FString& RelativePath) const
		{
			FString Contents;
			FFileHelper::LoadFileToString(Contents, *FPaths::Combine(Path, RelativePath));
			return Contents;
		}

		bool ExistsAt(const FString& RelativePath) const
		{
			return IFileManager::Get().FileExists(*FPaths::Combine(Path, RelativePath));
		}

		void DeleteAt(const FString& RelativePath) const
		{
			IFileManager::Get().Delete(*FPaths::Combine(Path, RelativePath), false, true);
		}
	};

	/** A published creator project's CreateAssetData, trimmed to the field that cannot be recovered. */
	const TCHAR* PublishedAssetJson =
		TEXT("{\"transactionID\":\"t-1\",\"assets\":[{\"asset\":{\"asset_id\":\"ea7a8d50-90b8-4485-a746-f53eeb34a843\"}}]}");

	/** The production slug, spelled out rather than computed, so a test cannot agree with a broken slug. */
	const TCHAR* ProductionSlug = TEXT("Env_api.convai.com_29e2cb96");

	TSharedPtr<FJsonObject> ParseObject(const FString& Contents)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
		TSharedPtr<FJsonObject> Parsed;
		FJsonSerializer::Deserialize(Reader, Parsed);
		return Parsed;
	}
}

/**
 * These five names are the contract. A record filed under one of them is read back only by a Pak
 * Manager pointed at the same backend, so changing how the slug is derived silently strands every
 * AssetID already on disk - the table is what makes that a test failure rather than a support ticket.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentSlugPinsTheTable,
	"ConvaiPakManager.Environment.Slug.PinsTheTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentSlugPinsTheTable::RunTest(const FString&)
{
	TestEqual(TEXT("production"),
		EnvironmentSlug(TEXT("https://api.convai.com/")), FString(TEXT("Env_api.convai.com_29e2cb96")));
	TestEqual(TEXT("preview"),
		EnvironmentSlug(TEXT("https://api-preview.convai.com/")), FString(TEXT("Env_api-preview.convai.com_a37055b4")));
	TestEqual(TEXT("staging"),
		EnvironmentSlug(TEXT("https://api-stg.convai.com/")), FString(TEXT("Env_api-stg.convai.com_64b86207")));
	TestEqual(TEXT("a gateway with a path"),
		EnvironmentSlug(TEXT("https://gateway.example.com/convai")), FString(TEXT("Env_gateway.example.com-conv_b18ab04b")));
	TestEqual(TEXT("a local backend"),
		EnvironmentSlug(TEXT("http://localhost:8000")), FString(TEXT("Env_localhost-8000_70490311")));

	return true;
}

/**
 * Scheme and host are case-insensitive and a trailing slash is one the URL builder adds anyway, so
 * three spellings of one address that folded into three folders would show a creator three Assets
 * where they have one. A path is the server's to interpret, so /Convai and /convai stay apart.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentSlugFoldsHostCaseAndTrailingSlashNotPathCase,
	"ConvaiPakManager.Environment.Slug.FoldsHostCaseAndTrailingSlashNotPathCase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentSlugFoldsHostCaseAndTrailingSlashNotPathCase::RunTest(const FString&)
{
	const FString Canonical = EnvironmentSlug(TEXT("https://api.convai.com/"));

	TestEqual(TEXT("shouting the scheme and host changes nothing"),
		EnvironmentSlug(TEXT("HTTPS://API.CONVAI.COM/")), Canonical);
	TestEqual(TEXT("nor does leaving the trailing slash off"),
		EnvironmentSlug(TEXT("https://api.convai.com")), Canonical);

	TestNotEqual(TEXT("but a path that differs by case is a different backend"),
		EnvironmentSlug(TEXT("https://gateway.example.com/Convai")),
		EnvironmentSlug(TEXT("https://gateway.example.com/convai")));

	return true;
}

/**
 * The URL comes from a text box a creator can type anything into. Whatever they type, the slug has
 * to be a directory name that can be created - a publish that failed because the override was
 * malformed would leave an Asset on the server with nowhere to record it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentSlugSurvivesGarbage,
	"ConvaiPakManager.Environment.Slug.SurvivesGarbage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentSlugSurvivesGarbage::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("SlugSurvivesGarbage"));

	const TCHAR* Overrides[] = {
		TEXT(""),
		TEXT("://"),
		TEXT("not a url!!"),
		TEXT("  https://x.y/  "),
		TEXT("C:\\bad\\path?q=1#f"),
	};

	for (const TCHAR* Override : Overrides)
	{
		const FString Slug = EnvironmentSlug(Override);
		const FString Context = FString::Printf(TEXT("'%s' -> '%s'"), Override, *Slug);

		if (!TestTrue(*(Context + TEXT(" is a slug")), Slug.StartsWith(TEXT("Env_")) && Slug.Len() >= 4 + 1 + 1 + 8))
		{
			continue;
		}

		const FString Segment = Slug.Mid(4, Slug.Len() - 4 - 9);
		const FString Hash = Slug.Right(8);

		TestTrue(*(Context + TEXT(" separates the hash")), Slug[Slug.Len() - 9] == TEXT('_'));
		TestTrue(*(Context + TEXT(" has a readable segment")), Segment.Len() >= 1 && Segment.Len() <= 24);
		for (const TCHAR Character : Segment)
		{
			TestTrue(*(Context + TEXT(" keeps the segment to directory-safe characters")),
				(Character >= TEXT('0') && Character <= TEXT('9'))
					|| (Character >= TEXT('A') && Character <= TEXT('Z'))
					|| (Character >= TEXT('a') && Character <= TEXT('z'))
					|| Character == TEXT('.') || Character == TEXT('-'));
		}
		for (const TCHAR Character : Hash)
		{
			TestTrue(*(Context + TEXT(" ends in lower-case hex")),
				(Character >= TEXT('0') && Character <= TEXT('9')) || (Character >= TEXT('a') && Character <= TEXT('f')));
		}

		TestTrue(*(Context + TEXT(" can be made into a directory")),
			IFileManager::Get().MakeDirectory(*FPaths::Combine(Scratch.Path, Slug), true));
	}

	return true;
}

/**
 * The whole point of the layout: what a backend minted goes under that backend, what the creator
 * authored does not. A Draft or a Thumbnail that landed under an environment would vanish from the
 * form the moment the URL changed, and ModdingMetaData describes the project, not a publish.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentPathsPartitionedRecordsSitUnderTheEnvironment,
	"ConvaiPakManager.Environment.Paths.PartitionedRecordsSitUnderTheEnvironment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentPathsPartitionedRecordsSitUnderTheEnvironment::RunTest(const FString&)
{
	const FString Slug = ProductionSlug;
	const FString EnvironmentDirectory = GetEnvironmentDirectory(10, Slug);

	TestTrue(TEXT("the environment sits under the Chunk"),
		EnvironmentDirectory.StartsWith(GetStateDirectory(10)) && EnvironmentDirectory.EndsWith(Slug));

	TestTrue(TEXT("the AssetID records under the environment"),
		GetCreateAssetDataPath(10, Slug).StartsWith(EnvironmentDirectory)
			&& GetCreateAssetDataPath(10, Slug).EndsWith(TEXT("CreateAssetData_10.json")));
	TestTrue(TEXT("the server's document records under the environment"),
		GetPakMetadataPath(10, Slug).StartsWith(EnvironmentDirectory)
			&& GetPakMetadataPath(10, Slug).EndsWith(TEXT("PakMetaData_10.json")));
	TestTrue(TEXT("the archive marker records under the environment"),
		GetRawArchiveRecordPath(10, Slug).StartsWith(EnvironmentDirectory)
			&& GetRawArchiveRecordPath(10, Slug).EndsWith(TEXT("RawArchive_10.txt")));

	TestTrue(TEXT("the Draft stays at Chunk level"),
		GetDraftPath(10).StartsWith(GetStateDirectory(10)) && !GetDraftPath(10).Contains(Slug));
	TestTrue(TEXT("the Thumbnail stays at Chunk level"),
		GetThumbnailPath(10).StartsWith(GetStateDirectory(10)) && !GetThumbnailPath(10).Contains(Slug));
	TestTrue(TEXT("the Modding Tool's record stays at Chunk level"),
		GetModdingMetadataPath(10).StartsWith(GetStateDirectory(10)) && !GetModdingMetadataPath(10).Contains(Slug));

	return true;
}

/**
 * The Modding Tool writes this file, not the Pak Manager, so the two cannot change extension on the
 * same day. A project generated by a tool that has not caught up still has to open.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentModdingMetadataIsReadUnderEitherExtension,
	"ConvaiPakManager.Environment.ModdingMetadata.IsReadUnderEitherExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentModdingMetadataIsReadUnderEitherExtension::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("ModdingMetadataEitherExtension"));
	const FString JsonPath = FPaths::Combine(Scratch.Path, TEXT("ChunkId_10"), TEXT("ModdingMetaData_10.json"));
	const FString LegacyPath = FPaths::Combine(Scratch.Path, TEXT("ChunkId_10"), TEXT("ModdingMetaData_10.txt"));

	Scratch.WriteAt(TEXT("ChunkId_10/ModdingMetaData_10.txt"), TEXT("{\"asset_type\":\"Scene\"}"));
	TestEqual(TEXT("a project the Modding Tool has not caught up on reads the .txt"),
		GetModdingMetadataPathIn(Scratch.Path, 10), LegacyPath);

	Scratch.WriteAt(TEXT("ChunkId_10/ModdingMetaData_10.json"), TEXT("{\"asset_type\":\"Scene\"}"));
	TestEqual(TEXT("once both exist the .json wins"),
		GetModdingMetadataPathIn(Scratch.Path, 10), JsonPath);

	Scratch.DeleteAt(TEXT("ChunkId_10/ModdingMetaData_10.json"));
	Scratch.DeleteAt(TEXT("ChunkId_10/ModdingMetaData_10.txt"));
	TestEqual(TEXT("with neither on disk it names the one that should be written"),
		GetModdingMetadataPathIn(Scratch.Path, 10), JsonPath);

	return true;
}

/**
 * Every record on disk today was minted by production, because nothing that could reach another
 * backend has shipped. Adoption is what stops the first launch after the upgrade reporting an
 * unpublished Chunk and creating a duplicate Asset beside the one already there.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentAdoptionMovesLooseRecordsIntoTheEnvironment,
	"ConvaiPakManager.Environment.Adoption.MovesLooseRecordsIntoTheEnvironment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentAdoptionMovesLooseRecordsIntoTheEnvironment::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("AdoptionMovesLooseRecords"));
	Scratch.WriteAt(TEXT("ChunkId_10/CreateAssetData_10.json"), PublishedAssetJson);
	Scratch.WriteAt(TEXT("ChunkId_10/PakMetaData_10.json"), TEXT(
		"{\"asset_name\":\"Landing\",\"asset_description\":\"A landing pad\",\"root_path\":\"/MyPlugin/\","
		"\"level_name\":\"/MyPlugin/Maps/Landing\",\"blueprint_class\":\"None\",\"blueprint_class_path\":\"\","
		"\"entity_id\":\"server-minted\"}"));
	Scratch.WriteAt(TEXT("ChunkId_10/RawArchive_10.txt"), TEXT("This chunk's Convai asset holds a raw project archive."));
	Scratch.WriteAt(TEXT("ChunkId_10/ModdingMetaData_10.txt"), TEXT("{\"asset_type\":\"Scene\"}"));
	Scratch.WriteAt(TEXT("ChunkId_10/Thumbnail_10.png"), TEXT("not really a png"));

	TArray<FString> Moved;
	const EMigrationResult Result = AdoptLooseRecordsIn(Scratch.Path, 10, ProductionSlug, Moved);

	TestTrue(TEXT("reports that it adopted"), Result == EMigrationResult::Migrated);
	TestEqual(TEXT("moves the three records and the renamed Modding Tool file"), Moved.Num(), 4);

	const FString Environment = FString(TEXT("ChunkId_10/")) + ProductionSlug + TEXT("/");
	TestEqual(TEXT("the AssetID survives the move byte for byte"),
		Scratch.ReadAt(Environment + TEXT("CreateAssetData_10.json")), FString(PublishedAssetJson));
	TestFalse(TEXT("and is no longer where another backend would read it"),
		Scratch.ExistsAt(TEXT("ChunkId_10/CreateAssetData_10.json")));

	TestTrue(TEXT("the server's document lands under the environment"),
		Scratch.ExistsAt(Environment + TEXT("PakMetaData_10.json")));
	TestFalse(TEXT("and leaves Chunk level"), Scratch.ExistsAt(TEXT("ChunkId_10/PakMetaData_10.json")));
	TestTrue(TEXT("so does the archive marker"), Scratch.ExistsAt(Environment + TEXT("RawArchive_10.txt")));
	TestFalse(TEXT("and it too leaves Chunk level"), Scratch.ExistsAt(TEXT("ChunkId_10/RawArchive_10.txt")));

	const TSharedPtr<FJsonObject> Draft = ParseObject(Scratch.ReadAt(TEXT("ChunkId_10/Draft_10.json")));
	if (TestTrue(TEXT("a Draft was seeded at Chunk level"), Draft.IsValid()))
	{
		TestEqual(TEXT("carrying only what the creator typed"), Draft->Values.Num(), 6);
		TestEqual(TEXT("including the name"), Draft->GetStringField(TEXT("asset_name")), FString(TEXT("Landing")));
		TestEqual(TEXT("the description"),
			Draft->GetStringField(TEXT("asset_description")), FString(TEXT("A landing pad")));
		TestEqual(TEXT("and the Entry Point"),
			Draft->GetStringField(TEXT("level_name")), FString(TEXT("/MyPlugin/Maps/Landing")));
		TestFalse(TEXT("and nothing the server minted"), Draft->HasField(TEXT("entity_id")));
	}

	TestEqual(TEXT("the Modding Tool's record is renamed in place"),
		Scratch.ReadAt(TEXT("ChunkId_10/ModdingMetaData_10.json")), FString(TEXT("{\"asset_type\":\"Scene\"}")));
	TestFalse(TEXT("under its real extension"), Scratch.ExistsAt(TEXT("ChunkId_10/ModdingMetaData_10.txt")));

	TestTrue(TEXT("the creator's thumbnail is the same on every backend and does not move"),
		Scratch.ExistsAt(TEXT("ChunkId_10/Thumbnail_10.png")));

	return true;
}

/**
 * A destination that already exists is a backend that has published. Overwriting its AssetID with a
 * loose one would point Update at the wrong Asset on the wrong server - so the live record wins and
 * the loose file is kept rather than destroyed, where a creator can still see it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentAdoptionNeverOverwritesAPublishedEnvironment,
	"ConvaiPakManager.Environment.Adoption.NeverOverwritesAPublishedEnvironment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentAdoptionNeverOverwritesAPublishedEnvironment::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("AdoptionNeverOverwrites"));
	const FString Environment = FString(TEXT("ChunkId_10/")) + ProductionSlug + TEXT("/");
	Scratch.WriteAt(Environment + TEXT("CreateAssetData_10.json"), TEXT("{\"current\":true}"));
	Scratch.WriteAt(TEXT("ChunkId_10/CreateAssetData_10.json"), TEXT("{\"stale\":true}"));

	AddExpectedError(TEXT("already exists"), EAutomationExpectedErrorFlags::Contains, 1);

	TArray<FString> Moved;
	AdoptLooseRecordsIn(Scratch.Path, 10, ProductionSlug, Moved);

	TestEqual(TEXT("the published AssetID is left alone"),
		Scratch.ReadAt(Environment + TEXT("CreateAssetData_10.json")), FString(TEXT("{\"current\":true}")));
	TestTrue(TEXT("the loose file is kept rather than deleted"),
		Scratch.ExistsAt(TEXT("ChunkId_10/CreateAssetData_10.json")));
	TestEqual(TEXT("and nothing is reported as moved"), Moved.Num(), 0);

	return true;
}

/**
 * Seeding exists to stop the upgrade emptying the creator's form, not to rewrite it. A Draft that is
 * already there is what they last typed and outranks anything reconstructed from a copy of it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentAdoptionKeepsAnExistingDraft,
	"ConvaiPakManager.Environment.Adoption.KeepsAnExistingDraft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentAdoptionKeepsAnExistingDraft::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("AdoptionKeepsAnExistingDraft"));
	Scratch.WriteAt(TEXT("ChunkId_10/Draft_10.json"), TEXT("{\"asset_name\":\"Edited\"}"));
	Scratch.WriteAt(TEXT("ChunkId_10/PakMetaData_10.json"), TEXT("{\"asset_name\":\"Old\"}"));

	TArray<FString> Moved;
	const EMigrationResult Result = AdoptLooseRecordsIn(Scratch.Path, 10, ProductionSlug, Moved);

	TestTrue(TEXT("reports that it adopted"), Result == EMigrationResult::Migrated);
	TestEqual(TEXT("the creator's Draft is untouched"),
		Scratch.ReadAt(TEXT("ChunkId_10/Draft_10.json")), FString(TEXT("{\"asset_name\":\"Edited\"}")));

	return true;
}

/** The ordinary case: every launch after the first, and every project published since. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMEnvironmentAdoptionIsSilentWithNothingLoose,
	"ConvaiPakManager.Environment.Adoption.IsSilentWithNothingLoose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMEnvironmentAdoptionIsSilentWithNothingLoose::RunTest(const FString&)
{
	const FScratchEssentials Scratch(TEXT("AdoptionIsSilentWithNothingLoose"));
	const FString Environment = FString(TEXT("ChunkId_10/")) + ProductionSlug + TEXT("/");
	Scratch.WriteAt(Environment + TEXT("CreateAssetData_10.json"), PublishedAssetJson);
	Scratch.WriteAt(TEXT("ChunkId_10/Draft_10.json"), TEXT("{\"asset_name\":\"Landing\"}"));
	Scratch.WriteAt(TEXT("ChunkId_10/Thumbnail_10.png"), TEXT("not really a png"));
	Scratch.WriteAt(TEXT("ChunkId_10/ModdingMetaData_10.json"), TEXT("{\"asset_type\":\"Scene\"}"));

	TArray<FString> Moved;
	const EMigrationResult Result = AdoptLooseRecordsIn(Scratch.Path, 10, ProductionSlug, Moved);

	TestTrue(TEXT("reports nothing to do"), Result == EMigrationResult::NothingToMigrate);
	TestEqual(TEXT("moves nothing"), Moved.Num(), 0);
	TestEqual(TEXT("the published AssetID is where it was"),
		Scratch.ReadAt(Environment + TEXT("CreateAssetData_10.json")), FString(PublishedAssetJson));
	TestEqual(TEXT("and so is the Draft"),
		Scratch.ReadAt(TEXT("ChunkId_10/Draft_10.json")), FString(TEXT("{\"asset_name\":\"Landing\"}")));

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
