// Copyright 2025 Convai Inc. All Rights Reserved.

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Utility/CPM_UtilityLibrary.h"

#if WITH_AUTOMATION_TESTS

/**
 * The predicate behind Use Existing Pak File, on the two files that are not a Pak.
 *
 * Neither case may reach ValidatePakFile: mounting is pointless on a file with no bytes in it, and
 * the error it logs on the way out would read as a failure where "not packaged yet" is the answer.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPakReuseRejectsWhatIsNotAPak,
	"ConvaiPakManager.Publish.PakReuse.RejectsWhatIsNotAPak",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPakReuseRejectsWhatIsNotAPak::RunTest(const FString&)
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CPM_PakReuseTest"));
	const FString NeverBuilt = FPaths::Combine(Directory, TEXT("pakchunk9999-Windows.pak"));
	const FString Empty = FPaths::Combine(Directory, TEXT("pakchunk9998-Windows.pak"));

	IFileManager::Get().Delete(*NeverBuilt);
	TestFalse(TEXT("a Pak that was never built is not usable"), UCPM_UtilityLibrary::IsPakUsable(NeverBuilt));

	TestTrue(TEXT("the empty Pak is written"), FFileHelper::SaveArrayToFile(TArray<uint8>(), *Empty));
	TestEqual(TEXT("and is zero bytes"), IFileManager::Get().FileSize(*Empty), static_cast<int64>(0));
	TestFalse(TEXT("a zero-byte Pak is not usable"), UCPM_UtilityLibrary::IsPakUsable(Empty));

	IFileManager::Get().Delete(*Empty);
	IFileManager::Get().DeleteDirectory(*Directory);
	return true;
}

#endif
