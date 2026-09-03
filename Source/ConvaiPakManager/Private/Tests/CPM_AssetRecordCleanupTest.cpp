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
	/** The two pinned slugs, spelled out so a test cannot agree with a broken slug. */
	const TCHAR* ProductionSlug = TEXT("Env_api.convai.com_29e2cb96");
	const TCHAR* StagingSlug = TEXT("Env_api-stg.convai.com_64b86207");
}

/**
 * A delete says something about ONE backend. Clearing staging's records must leave production's
 * AssetID alone - it still names a live Asset - and must leave the Draft and the thumbnail alone
 * too, because those are the inputs production's next Update is built from rather than records of
 * the Asset just destroyed.
 *
 * Tested against a real filesystem because what it has to get right IS filesystem behaviour: that
 * one environment's three files went, that the neighbouring folder was not touched, and that a
 * record already missing is not reported as a failure.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetRecordCleanupADeleteOnOneEnvironmentLeavesTheOtherAlone,
	"ConvaiPakManager.Chunk.Cleanup.ADeleteOnOneEnvironmentLeavesTheOtherAlone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetRecordCleanupADeleteOnOneEnvironmentLeavesTheOtherAlone::RunTest(const FString&)
{
	const int32 ChunkId = 7;
	const FString Essentials = FPaths::Combine(
		FPaths::ProjectIntermediateDir(), TEXT("CPM_Tests"), TEXT("ADeleteOnOneEnvironment"), TEXT("ConvaiEssentials"));

	IFileManager::Get().DeleteDirectory(*Essentials, false, true);

	const FString Chunk = FPaths::Combine(Essentials, TEXT("ChunkId_7"));
	auto Write = [&Chunk](const FString& RelativePath, const FString& Contents)
	{
		const FString Path = FPaths::Combine(Chunk, RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(Contents, *Path);
	};
	auto Exists = [&Chunk](const FString& RelativePath)
	{
		return IFileManager::Get().FileExists(*FPaths::Combine(Chunk, RelativePath));
	};
	auto Read = [&Chunk](const FString& RelativePath)
	{
		FString Contents;
		FFileHelper::LoadFileToString(Contents, *FPaths::Combine(Chunk, RelativePath));
		return Contents;
	};

	Write(TEXT("Draft_7.json"), TEXT("{\"asset_name\":\"Landing\"}"));
	Write(TEXT("Thumbnail_7.png"), TEXT("not really a png"));
	Write(TEXT("ModdingMetaData_7.json"), TEXT("{\"asset_type\":\"Scene\"}"));

	for (const TCHAR* Slug : { ProductionSlug, StagingSlug })
	{
		Write(FString(Slug) / TEXT("CreateAssetData_7.json"), FString::Printf(TEXT("{\"minted_by\":\"%s\"}"), Slug));
		Write(FString(Slug) / TEXT("PakMetaData_7.json"), FString::Printf(TEXT("{\"echoed_by\":\"%s\"}"), Slug));
		Write(FString(Slug) / TEXT("RawArchive_7.txt"), TEXT("x"));
	}

	TArray<FString> Undeleted;
	ClearAssetRecordsIn(Essentials, ChunkId, StagingSlug, Undeleted);

	TestTrue(TEXT("nothing was left behind"), Undeleted.IsEmpty());

	const FString Staging = FString(StagingSlug) + TEXT("/");
	TestFalse(TEXT("staging's asset record is gone"), Exists(Staging + TEXT("CreateAssetData_7.json")));
	TestFalse(TEXT("staging's metadata cache is gone"), Exists(Staging + TEXT("PakMetaData_7.json")));
	TestFalse(TEXT("staging's archive marker is gone"), Exists(Staging + TEXT("RawArchive_7.txt")));

	const FString Production = FString(ProductionSlug) + TEXT("/");
	TestEqual(TEXT("production's AssetID is untouched"),
		Read(Production + TEXT("CreateAssetData_7.json")),
		FString::Printf(TEXT("{\"minted_by\":\"%s\"}"), ProductionSlug));
	TestEqual(TEXT("so is its metadata cache"),
		Read(Production + TEXT("PakMetaData_7.json")),
		FString::Printf(TEXT("{\"echoed_by\":\"%s\"}"), ProductionSlug));
	TestTrue(TEXT("and its archive marker"), Exists(Production + TEXT("RawArchive_7.txt")));

	TestEqual(TEXT("what the creator typed survives a delete on one backend"),
		Read(TEXT("Draft_7.json")), FString(TEXT("{\"asset_name\":\"Landing\"}")));
	TestTrue(TEXT("and so does their thumbnail"), Exists(TEXT("Thumbnail_7.png")));
	TestTrue(TEXT("what the Modding Tool decided about the project stays"), Exists(TEXT("ModdingMetaData_7.json")));

	// A Chunk deleted before it ever published on this backend has none of these, and that is not a
	// failure.
	Undeleted.Reset();
	ClearAssetRecordsIn(Essentials, ChunkId, StagingSlug, Undeleted);
	TestTrue(TEXT("clearing records that are already gone reports nothing"), Undeleted.IsEmpty());

	IFileManager::Get().DeleteDirectory(*Essentials, false, true);
	return true;
}

/**
 * The other half: once the last backend lets go, the inputs go with it. Kept, they would refill the
 * form for an Asset that no longer exists anywhere, under a Publish that can only mint a new one.
 *
 * ModdingMetaData still stays - it describes the project, not anything published from it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetRecordCleanupTheLastDeleteTakesTheDraftAndThumbnail,
	"ConvaiPakManager.Chunk.Cleanup.TheLastDeleteTakesTheDraftAndThumbnail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetRecordCleanupTheLastDeleteTakesTheDraftAndThumbnail::RunTest(const FString&)
{
	const int32 ChunkId = 7;
	const FString Essentials = FPaths::Combine(
		FPaths::ProjectIntermediateDir(), TEXT("CPM_Tests"), TEXT("TheLastDelete"), TEXT("ConvaiEssentials"));

	IFileManager::Get().DeleteDirectory(*Essentials, false, true);

	const FString Chunk = FPaths::Combine(Essentials, TEXT("ChunkId_7"));
	auto Write = [&Chunk](const FString& RelativePath, const FString& Contents)
	{
		const FString Path = FPaths::Combine(Chunk, RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(Contents, *Path);
	};
	auto Exists = [&Chunk](const FString& RelativePath)
	{
		return IFileManager::Get().FileExists(*FPaths::Combine(Chunk, RelativePath));
	};

	Write(TEXT("Draft_7.json"), TEXT("{\"asset_name\":\"Landing\"}"));
	Write(TEXT("Thumbnail_7.png"), TEXT("not really a png"));
	Write(TEXT("ModdingMetaData_7.json"), TEXT("{\"asset_type\":\"Scene\"}"));

	for (const TCHAR* Slug : { ProductionSlug, StagingSlug })
	{
		Write(FString(Slug) / TEXT("CreateAssetData_7.json"), TEXT("{}"));
		Write(FString(Slug) / TEXT("PakMetaData_7.json"), TEXT("{}"));
		Write(FString(Slug) / TEXT("RawArchive_7.txt"), TEXT("x"));
	}

	TArray<FString> Undeleted;
	ClearAssetRecordsIn(Essentials, ChunkId, StagingSlug, Undeleted);

	TestTrue(TEXT("production still holds, so the Draft stays"), Exists(TEXT("Draft_7.json")));
	TestTrue(TEXT("and the thumbnail with it"), Exists(TEXT("Thumbnail_7.png")));

	// Staging's folder is still on disk with its records gone. Nothing may read that as a backend
	// that still holds this Chunk.
	Undeleted.Reset();
	ClearAssetRecordsIn(Essentials, ChunkId, ProductionSlug, Undeleted);

	TestTrue(TEXT("nothing was left behind"), Undeleted.IsEmpty());
	TestFalse(TEXT("the last delete takes the Draft"), Exists(TEXT("Draft_7.json")));
	TestFalse(TEXT("and the creator's thumbnail"), Exists(TEXT("Thumbnail_7.png")));
	TestTrue(TEXT("what the Modding Tool decided about the project stays"), Exists(TEXT("ModdingMetaData_7.json")));

	// Deleting again finds nothing at all, and that is not a failure.
	Undeleted.Reset();
	ClearAssetRecordsIn(Essentials, ChunkId, ProductionSlug, Undeleted);
	TestTrue(TEXT("a repeated delete reports nothing"), Undeleted.IsEmpty());

	IFileManager::Get().DeleteDirectory(*Essentials, false, true);
	return true;
}

#endif
