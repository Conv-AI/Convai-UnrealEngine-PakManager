// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Publish/CPM_Preconditions.h"

#if WITH_AUTOMATION_TESTS

using namespace ConvaiPakManager::Preconditions;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPreconditionsReadsToolchainVersions,
	"ConvaiPakManager.Preconditions.ReadsToolchainVersions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPreconditionsReadsToolchainVersions::RunTest(const FString&)
{
	// Verbatim from Engine/Config/Linux/Linux_SDK.json, so a change to the naming shape fails here.
	TestEqual(TEXT("reads the ordinal a toolchain name opens with"),
		ToolchainOrdinal(TEXT("v26_clang-20.1.8-rockylinux8")), 26);
	TestEqual(TEXT("ordinals are compared as numbers, not as text"),
		ToolchainOrdinal(TEXT("v9_clang-7.0.1-centos7")), 9);

	TestEqual(TEXT("a name with no v prefix has no ordinal"),
		ToolchainOrdinal(TEXT("clang-20.1.8")), INDEX_NONE);
	TestEqual(TEXT("a bare v has no ordinal"), ToolchainOrdinal(TEXT("v")), INDEX_NONE);
	TestEqual(TEXT("an empty name has no ordinal"), ToolchainOrdinal(FString()), INDEX_NONE);

	// The pin the whole check turns on: v9 must not read as newer than v26, which is exactly what
	// the legacy substring comparison did.
	TestTrue(TEXT("v26 sits inside a v26..v26 range"),
		ToolchainWithinRange(TEXT("v26_clang-20.1.8-rockylinux8"), TEXT("v26_clang-20.1.8-rockylinux8"),
			TEXT("v26_clang-20.1.8-rockylinux8")));
	TestFalse(TEXT("v23 is below a v26 floor"),
		ToolchainWithinRange(TEXT("v23_clang-18.1.0-rockylinux8"), TEXT("v26_x"), TEXT("v26_x")));
	TestFalse(TEXT("v9 is below a v26 floor despite sorting after it as text"),
		ToolchainWithinRange(TEXT("v9_clang-7.0.1-centos7"), TEXT("v26_x"), TEXT("v26_x")));
	TestTrue(TEXT("a version inside a wider range is accepted"),
		ToolchainWithinRange(TEXT("v24_x"), TEXT("v23_x"), TEXT("v26_x")));
	TestFalse(TEXT("a version above the ceiling is refused"),
		ToolchainWithinRange(TEXT("v27_x"), TEXT("v23_x"), TEXT("v26_x")));

	// Fails open on the RANGE and closed on the FIND: an engine config this cannot read must not
	// refuse a creator whose toolchain is present, but an unreadable toolchain is UBT's own refusal.
	TestTrue(TEXT("an unreadable range accepts a readable toolchain"),
		ToolchainWithinRange(TEXT("v26_x"), FString(), FString()));
	TestFalse(TEXT("an unreadable toolchain is refused even with no range"),
		ToolchainWithinRange(TEXT("not-a-toolchain"), FString(), FString()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPreconditionsExplainsRefusals,
	"ConvaiPakManager.Preconditions.ExplainsRefusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMPreconditionsExplainsRefusals::RunTest(const FString&)
{
	FLinuxToolchain Usable;
	Usable.bUsable = true;
	TestTrue(TEXT("a usable toolchain refuses nothing"), WhyLinuxCannotPackage(Usable).IsEmpty());

	FLinuxToolchain Absent;
	Absent.ExpectedVersion = TEXT("v26_clang-20.1.8-rockylinux8");
	const FString AbsentWhy = WhyLinuxCannotPackage(Absent);
	TestFalse(TEXT("a missing toolchain is refused"), AbsentWhy.IsEmpty());
	TestTrue(TEXT("the refusal names the version to install"),
		AbsentWhy.Contains(TEXT("v26_clang-20.1.8-rockylinux8")));
	// Load-bearing: packaging inherits this process's environment block, so without the restart the
	// creator's next attempt fails in exactly the same way. See docs/adr/0014.
	TestTrue(TEXT("the refusal says to restart the editor"), AbsentWhy.Contains(TEXT("restart the editor")));

	FLinuxToolchain Stale;
	Stale.ExpectedVersion = TEXT("v26_clang-20.1.8-rockylinux8");
	Stale.FoundAt = TEXT("C:/UnrealToolchains/v23_clang-18.1.0-rockylinux8");
	Stale.FoundVersion = TEXT("v23_clang-18.1.0-rockylinux8");
	const FString StaleWhy = WhyLinuxCannotPackage(Stale);
	TestTrue(TEXT("a stale toolchain's refusal names what was found"),
		StaleWhy.Contains(TEXT("v23_clang-18.1.0-rockylinux8")));
	TestTrue(TEXT("a stale toolchain's refusal names where it was found"),
		StaleWhy.Contains(TEXT("C:/UnrealToolchains/v23_clang-18.1.0-rockylinux8")));

	// An Avatar has no nav mesh requirement; a Scene has both. Every missing thing is named at once.
	TestTrue(TEXT("an Avatar with a spawn point is ready"),
		WhyAssetRecordCannotBeWritten(ECPM_AssetType::Avatar, 1, false).IsEmpty());
	TestTrue(TEXT("a Scene with both is ready"),
		WhyAssetRecordCannotBeWritten(ECPM_AssetType::Scene, 1, true).IsEmpty());

	const FString NoSpawn = WhyAssetRecordCannotBeWritten(ECPM_AssetType::Avatar, 0, false);
	TestTrue(TEXT("a missing spawn point is refused"), NoSpawn.Contains(TEXT("spawn point")));

	const FString SceneNoNav = WhyAssetRecordCannotBeWritten(ECPM_AssetType::Scene, 1, false);
	TestTrue(TEXT("a Scene without nav mesh bounds is refused"),
		SceneNoNav.Contains(TEXT("Nav Mesh Bounds Volume")));

	const FString Both = WhyAssetRecordCannotBeWritten(ECPM_AssetType::Scene, 0, false);
	TestTrue(TEXT("both refusals are reported together, not one at a time"),
		Both.Contains(TEXT("spawn point")) && Both.Contains(TEXT("Nav Mesh Bounds Volume")));

	return true;
}

#endif
