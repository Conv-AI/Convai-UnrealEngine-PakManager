// Copyright 2025 Convai Inc. All Rights Reserved.

#include "ConvaiPakManagerEditorUtils.h"
#include "FileUtilities/ZipArchiveReader.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Publish/CPM_PublishTypes.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMSourcePolicyPreservesLegacyDefaults,
	"ConvaiPakManager.Publish.SourcePolicy.PreservesLegacyDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMSourcePolicyPreservesLegacyDefaults::RunTest(const FString&)
{
	const FCPM_PublishOptions Options;
	for (const bool bPackageOnly : { false, true })
	{
		for (const bool bPolicy : { false, true })
		{
			for (const bool bSetting : { false, true })
			{
				bool bArchive = true;
				FString Error = TEXT("previous error");
				TestTrue(TEXT("default source choice is accepted"),
					Options.ResolveSourceArchive(bPackageOnly, bPolicy, bSetting, bArchive, Error));
				TestEqual(*FString::Printf(TEXT("package-only=%d policy=%d setting=%d"), bPackageOnly, bPolicy, bSetting),
					bArchive, !bPackageOnly && bPolicy && bSetting);
				TestTrue(TEXT("success clears an earlier error"), Error.IsEmpty());
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMSourcePolicyExplicitChoiceRespectsPolicy,
	"ConvaiPakManager.Publish.SourcePolicy.ExplicitChoiceRespectsPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMSourcePolicyExplicitChoiceRespectsPolicy::RunTest(const FString&)
{
	FCPM_PublishOptions Options;
	Options.bOverridePlatforms = true;
	Options.Platforms = { ECPM_Platform::Windows };
	bool bArchive = true;
	FString Error;
	for (const bool bSetting : { false, true })
	{
		Options.SourceChoice = ECPM_SourceChoice::Omit;
		TestTrue(TEXT("explicit omission is accepted"),
			Options.ResolveSourceArchive(false, true, bSetting, bArchive, Error));
		TestFalse(TEXT("omission does not queue the archive"), bArchive);

		Options.SourceChoice = ECPM_SourceChoice::IncludeFresh;
		TestTrue(TEXT("fresh source follows this run's choice when policy permits"),
			Options.ResolveSourceArchive(false, true, bSetting, bArchive, Error));
		TestTrue(TEXT("fresh source queues the archive even when the project default is off"), bArchive);

		TestFalse(TEXT("a platform override cannot authorize source denied by policy"),
			Options.ResolveSourceArchive(false, false, bSetting, bArchive, Error));
		TestFalse(TEXT("denial leaves no archive to queue"), bArchive);
		TestTrue(TEXT("denial identifies the resolved policy"), Error.Contains(TEXT("resolved publish policy")));
	}

	TestEqual(TEXT("source choice preserves the platform selection"), Options.Platforms.Num(), 1);
	TestTrue(TEXT("Windows stays selected"), Options.Platforms.Contains(ECPM_Platform::Windows));
	for (const ECPM_SourceChoice Choice : { ECPM_SourceChoice::Omit, ECPM_SourceChoice::IncludeFresh })
	{
		Options.SourceChoice = Choice;
		TestTrue(TEXT("package-only remains independent of source choice"),
			Options.ResolveSourceArchive(true, false, true, bArchive, Error));
		TestFalse(TEXT("package-only never queues an archive"), bArchive);
	}

	Options.SourceChoice = static_cast<ECPM_SourceChoice>(255);
	TestFalse(TEXT("unknown source choices are refused"),
		Options.ResolveSourceArchive(false, true, true, bArchive, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMSourcePolicyArchiveReplacesPreviousBytes,
	"ConvaiPakManager.Publish.SourcePolicy.ArchiveReplacesPreviousBytes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMSourcePolicyArchiveReplacesPreviousBytes::RunTest(const FString&)
{
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("Automation/ConvaiPakManager/SourcePolicy"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (!TestTrue(TEXT("creates an owned fixture directory"), IFileManager::Get().MakeDirectory(*Directory, true)))
	{
		return false;
	}
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Directory, false, true); };

	const FString Source = FPaths::Combine(Directory, TEXT("source.txt"));
	const FString Removed = FPaths::Combine(Directory, TEXT("removed.txt"));
	const FString Zip = FPaths::Combine(Directory, TEXT("source.zip"));
	const FString Marker = FPaths::Combine(Directory, TEXT("RawArchive_1.txt"));
	const TArray<uint8> OldBytes = { 1, 2, 3 };
	const TArray<uint8> NewBytes = { 4, 5, 6, 7 };
	if (!TestTrue(TEXT("writes source"), FFileHelper::SaveArrayToFile(OldBytes, *Source)) ||
		!TestTrue(TEXT("writes the old dependency"), FFileHelper::SaveArrayToFile(OldBytes, *Removed)) ||
		!TestTrue(TEXT("builds the previous archive"), UConvaiPakManagerEditorUtils::CPM_CreateZip(Zip, { Source, Removed }, {})) ||
		!TestTrue(TEXT("records a previous source upload"), FFileHelper::SaveStringToFile(TEXT("previous upload"), *Marker)))
	{
		return false;
	}
	const FDateTime MarkerTime = IFileManager::Get().GetTimeStamp(*Marker);
	TestTrue(TEXT("edits the source"), FFileHelper::SaveArrayToFile(NewBytes, *Source));

	FCPM_PublishOptions Options;
	Options.SourceChoice = ECPM_SourceChoice::IncludeFresh;
	bool bArchive = false;
	FString Error;
	if (!TestTrue(TEXT("fresh source is queued despite an existing archive and marker"),
		Options.ResolveSourceArchive(false, true, false, bArchive, Error)) ||
		!TestTrue(TEXT("fresh source requires the archive"), bArchive) ||
		!TestTrue(TEXT("rebuilds with the current input list"), UConvaiPakManagerEditorUtils::CPM_CreateZip(Zip, { Source }, {})))
	{
		return false;
	}

	{
		FZipArchiveReader Reader(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Zip));
		if (!TestTrue(TEXT("the rebuilt archive can be read"), Reader.IsValid()))
		{
			return false;
		}
		const TArray<FString> Names = Reader.GetFileNames();
		TestEqual(TEXT("removed inputs do not survive in the previous zip"), Names.Num(), 1);
		if (Names.Num() == 1)
		{
			TestTrue(TEXT("the current source is present"), Names[0].EndsWith(TEXT("/source.txt")));
			TArray<uint8> Actual;
			TestTrue(TEXT("reads the current source bytes"), Reader.TryReadFile(Names[0], Actual));
			TestTrue(TEXT("archive contains this run's source bytes"), Actual == NewBytes);
		}
	}
	TestTrue(TEXT("archiving alone never advances the upload timestamp"), IFileManager::Get().GetTimeStamp(*Marker) == MarkerTime);
	return true;
}

#endif
