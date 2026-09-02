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
	FString ScratchChunkDirectory(const TCHAR* TestName, const int32 ChunkId)
	{
		return FPaths::Combine(
			FPaths::ProjectIntermediateDir(), TEXT("CPM_Tests"), TestName, TEXT("ConvaiEssentials"),
			FString::Printf(TEXT("ChunkId_%d"), ChunkId));
	}
}

/**
 * What a deleted Asset takes with it, and the one file it must not.
 *
 * Tested against a real filesystem because what it has to get right IS filesystem behaviour: that
 * the four records are gone, that ModdingMetaData - which describes the project rather than the
 * Asset, and which nothing regenerates - is still there, and that a record already missing is not
 * reported as a failure.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMAssetRecordCleanupClearsTheAssetAndKeepsTheProject,
	"ConvaiPakManager.Chunk.Cleanup.ClearsTheAssetAndKeepsTheProject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMAssetRecordCleanupClearsTheAssetAndKeepsTheProject::RunTest(const FString&)
{
	const int32 ChunkId = 7;
	const FString Directory = ScratchChunkDirectory(TEXT("ClearsTheAssetRecords"), ChunkId);
	const FString Essentials = FPaths::GetPath(Directory);

	IFileManager::Get().DeleteDirectory(*Essentials, false, true);
	IFileManager::Get().MakeDirectory(*Directory, true);

	auto Write = [&Directory](const TCHAR* FileName)
	{
		FFileHelper::SaveStringToFile(TEXT("x"), *FPaths::Combine(Directory, FileName));
	};
	auto Exists = [&Directory](const TCHAR* FileName)
	{
		return IFileManager::Get().FileExists(*FPaths::Combine(Directory, FileName));
	};

	Write(TEXT("CreateAssetData_7.json"));
	Write(TEXT("PakMetaData_7.json"));
	Write(TEXT("Thumbnail_7.png"));
	Write(TEXT("RawArchive_7.txt"));
	Write(TEXT("ModdingMetaData_7.txt"));

	TArray<FString> Undeleted;
	ClearAssetRecordsIn(Essentials, ChunkId, Undeleted);

	TestTrue(TEXT("nothing was left behind"), Undeleted.IsEmpty());
	TestFalse(TEXT("the asset record is gone"), Exists(TEXT("CreateAssetData_7.json")));
	TestFalse(TEXT("the metadata document is gone"), Exists(TEXT("PakMetaData_7.json")));
	TestFalse(TEXT("the thumbnail is gone"), Exists(TEXT("Thumbnail_7.png")));
	TestFalse(TEXT("the archive record is gone"), Exists(TEXT("RawArchive_7.txt")));
	TestTrue(TEXT("what the Modding Tool decided about the project stays"), Exists(TEXT("ModdingMetaData_7.txt")));

	// A Chunk deleted before it ever published has none of these, and that is not a failure.
	Undeleted.Reset();
	ClearAssetRecordsIn(Essentials, ChunkId, Undeleted);
	TestTrue(TEXT("clearing records that are already gone reports nothing"), Undeleted.IsEmpty());

	IFileManager::Get().DeleteDirectory(*Essentials, false, true);
	return true;
}

#endif
