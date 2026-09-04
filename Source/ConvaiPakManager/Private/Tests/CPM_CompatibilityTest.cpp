// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Publish/CPM_Compatibility.h"

#if WITH_AUTOMATION_TESTS

using namespace ConvaiPakManager::Compatibility;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMCompatibilityParsesVersionSources,
	"ConvaiPakManager.Compatibility.ParsesVersionSources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMCompatibilityParsesVersionSources::RunTest(const FString&)
{
	// Both sources verbatim, so a rename on either side fails here rather than silently reading empty.
	TestEqual(TEXT("reads the .uplugin's VersionName"),
		ParsePluginVersionName(TEXT(R"({"FileVersion":3,"Version":2311,"VersionName":"2.3.11"})")),
		FString(TEXT("2.3.11")));
	TestEqual(TEXT("reads the Modding Tool's target engine"),
		ParseTargetEngineVersion(
			TEXT(R"({"current-ue-version":"5.8","target-ue-version":"5.8","modding-tool-version":"3.0.6"})")),
		FString(TEXT("5.8")));

	TestTrue(TEXT("a .uplugin that is not JSON reads empty"),
		ParsePluginVersionName(TEXT("<html>404</html>")).IsEmpty());
	TestTrue(TEXT("a .uplugin with no VersionName reads empty"),
		ParsePluginVersionName(TEXT(R"({"FileVersion":3})")).IsEmpty());
	TestTrue(TEXT("a Version.json that is not JSON reads empty"),
		ParseTargetEngineVersion(TEXT("not json at all")).IsEmpty());
	TestTrue(TEXT("a Version.json with no target reads empty"),
		ParseTargetEngineVersion(TEXT(R"({"current-ue-version":"5.8"})")).IsEmpty());

	// The floor Convai does not publish yet. Empty is the ordinary answer today, and IsNewerVersion
	// says false for it, so the refusal it feeds stays dormant until the field appears.
	TestEqual(TEXT("reads the published floor when there is one"),
		ParseMinimumToolVersion(TEXT(R"({"target-ue-version":"5.8","min-pak-manager-version":"2.3.0"})")),
		FString(TEXT("2.3.0")));
	TestTrue(TEXT("today's Version.json declares no floor"),
		ParseMinimumToolVersion(
			TEXT(R"({"current-ue-version":"5.8","target-ue-version":"5.8","modding-tool-version":"3.0.6"})")).IsEmpty());
	TestFalse(TEXT("an absent floor refuses nobody"), IsNewerVersion(TEXT("2.3.11"), FString()));
	TestTrue(TEXT("an install below the floor is caught"), IsNewerVersion(TEXT("2.2.9"), TEXT("2.3.0")));
	TestFalse(TEXT("an install at the floor is accepted"), IsNewerVersion(TEXT("2.3.0"), TEXT("2.3.0")));

	TestFalse(TEXT("this install names its own version"), InstalledToolVersion().IsEmpty());

	return true;
}

/**
 * The comparison is numeric, not lexical: 2.10 is newer than 2.9, and a string compare says the
 * opposite. Anything unreadable fails open on both sides - see the header.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMCompatibilityComparesVersions,
	"ConvaiPakManager.Compatibility.ComparesVersions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMCompatibilityComparesVersions::RunTest(const FString&)
{
	TestTrue(TEXT("a later minor is newer"), IsNewerVersion(TEXT("2.3.11"), TEXT("2.4.0")));
	TestFalse(TEXT("the same version is not newer"), IsNewerVersion(TEXT("2.3.11"), TEXT("2.3.11")));
	TestFalse(TEXT("2.9.0 is not newer than 2.10.0"), IsNewerVersion(TEXT("2.10.0"), TEXT("2.9.0")));
	TestFalse(TEXT("an unread latest never nags"), IsNewerVersion(TEXT("2.3.11"), TEXT("")));
	TestFalse(TEXT("an unreadable install never nags"), IsNewerVersion(TEXT("garbage"), TEXT("2.4.0")));

	TestTrue(TEXT("a patch of the target engine matches"), EngineMatchesTarget(TEXT("5.8.1"), TEXT("5.8")));
	TestFalse(TEXT("the previous minor does not"), EngineMatchesTarget(TEXT("5.7.2"), TEXT("5.8")));
	TestTrue(TEXT("an unknown target matches everything"), EngineMatchesTarget(TEXT("5.8.0"), TEXT("")));
	TestFalse(TEXT("a later major does not"), EngineMatchesTarget(TEXT("6.0.0"), TEXT("5.8")));

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
