// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UI/CPM_PakManagerViewModels.h"
#include "Stage/CPM_Stage.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	using EBadge = FCPM_AssetViewModel::EBadge;

	/** A Scene draft that passes every Create gate: named, Entry Point picked, thumbnail captured. */
	FCPM_AssetViewModel MakeValidDraft()
	{
		FCPM_AssetViewModel Model;
		Model.ChunkId = 1001;
		Model.SavedName = TEXT("Forest Scene");
		Model.Name = Model.SavedName;
		Model.EntryPoint = TEXT("/Game/Maps/Forest");
		Model.AssetType = ECPM_AssetType::Scene;
		Model.bThumbnailExists = true;
		return Model;
	}

	/** An Avatar draft with the stage box ticked, otherwise as complete as MakeValidDraft. */
	FCPM_AssetViewModel MakeStagedAvatarDraft()
	{
		FCPM_AssetViewModel Model = MakeValidDraft();
		Model.AssetType = ECPM_AssetType::Avatar;
		Model.EntryPoint = TEXT("/Game/Office/BP_Receptionist");
		Model.bStageEnabled = true;
		Model.StageReport.ChunkId = Model.ChunkId;
		Model.StageReport.bLimitsRead = true;
		return Model;
	}

	FCPM_StageIssue StageIssue(const ECPM_StageSeverity Severity)
	{
		FCPM_StageIssue Issue;
		Issue.Severity = Severity;
		Issue.Reason = TEXT("a line the creator reads");
		return Issue;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMViewModelTracksDirtyEditsAndReverts,
	"ConvaiPakManager.UI.ViewModel.TracksDirtyEditsAndReverts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMViewModelTracksDirtyEditsAndReverts::RunTest(const FString&)
{
	FCPM_AssetViewModel Model = MakeValidDraft();
	TestFalse(TEXT("clean when edits match the snapshot"), Model.IsDirty());

	Model.Name = TEXT("Renamed Scene");
	TestTrue(TEXT("a name edit makes it dirty"), Model.IsDirty());

	Model.Revert();
	TestEqual(TEXT("Revert restores the name"), Model.Name, Model.SavedName);
	TestFalse(TEXT("clean again after Revert"), Model.IsDirty());

	Model.Description = TEXT("A quiet forest.");
	TestTrue(TEXT("a description edit alone makes it dirty"), Model.IsDirty());

	Model.Revert();
	TestFalse(TEXT("Revert also drops description edits"), Model.IsDirty());

	// A field the Save path forgets is a field that silently never reaches the Draft.
	Model.Gender = TEXT("female");
	TestTrue(TEXT("a gender pick alone makes it dirty"), Model.IsDirty());

	Model.Revert();
	TestEqual(TEXT("Revert restores the gender"), Model.Gender, Model.SavedGender);
	TestFalse(TEXT("clean again after Revert"), Model.IsDirty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMViewModelDerivesEveryBadge,
	"ConvaiPakManager.UI.ViewModel.DerivesEveryBadge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMViewModelDerivesEveryBadge::RunTest(const FString&)
{
	// Busy outranks everything, including an existing AssetId.
	FCPM_AssetViewModel Model = MakeValidDraft();
	Model.AssetId = TEXT("asset-123");
	Model.Status.Status = ECPM_AssetManagerStatus::UploadPak_Begin;
	TestTrue(TEXT("busy shows Publishing"), Model.Badge() == EBadge::Publishing);

	// A failure outranks Published: the creator must see it before trusting the record.
	Model.Status.Status = ECPM_AssetManagerStatus::Create_Failed;
	TestTrue(TEXT("a failed status shows NeedsAttention"), Model.Badge() == EBadge::NeedsAttention);

	Model.Status.Status = ECPM_AssetManagerStatus::Max;
	TestTrue(TEXT("an AssetId at rest shows Published"), Model.Badge() == EBadge::Published);

	Model.AssetId.Empty();
	TestTrue(TEXT("valid but never published shows ReadyToPublish"), Model.Badge() == EBadge::ReadyToPublish);

	Model.Name.Empty();
	TestTrue(TEXT("an invalid draft shows Draft"), Model.Badge() == EBadge::Draft);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMViewModelGatesCreateOnNameEntryPointAndThumbnail,
	"ConvaiPakManager.UI.ViewModel.GatesCreateOnNameEntryPointAndThumbnail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMViewModelGatesCreateOnNameEntryPointAndThumbnail::RunTest(const FString&)
{
	FCPM_AssetViewModel Model = MakeValidDraft();
	TestEqual(TEXT("a complete draft has no messages"), Model.ValidationMessages().Num(), 0);
	TestTrue(TEXT("and may create"), Model.CanCreateOrPublish());

	// Whitespace is not a name.
	Model.Name = TEXT("   ");
	TestEqual(TEXT("a blank name is one message"), Model.ValidationMessages().Num(), 1);
	TestTrue(TEXT("naming the name"), Model.ValidationMessages()[0].ToString().Contains(TEXT("name")));
	TestFalse(TEXT("and closes the gate"), Model.CanCreateOrPublish());
	Model.Revert();

	Model.EntryPoint.Empty();
	TestTrue(TEXT("a Scene without an Entry Point asks for its level"),
		Model.ValidationMessages()[0].ToString().Contains(TEXT("level")));
	Model.AssetType = ECPM_AssetType::Avatar;
	TestTrue(TEXT("an Avatar asks for its blueprint"),
		Model.ValidationMessages()[0].ToString().Contains(TEXT("blueprint")));
	Model.AssetType = ECPM_AssetType::Scene;
	Model.EntryPoint = TEXT("/Game/Maps/Forest");

	Model.bThumbnailExists = false;
	TestEqual(TEXT("a missing thumbnail is one message"), Model.ValidationMessages().Num(), 1);
	TestTrue(TEXT("asking for a capture"), Model.ValidationMessages()[0].ToString().Contains(TEXT("thumbnail")));

	Model.Name.Empty();
	Model.EntryPoint.Empty();
	TestEqual(TEXT("every failed gate reports"), Model.ValidationMessages().Num(), 3);
	TestFalse(TEXT("and the gate stays closed"), Model.CanCreateOrPublish());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMViewModelRefusesToPublishWhileBusy,
	"ConvaiPakManager.UI.ViewModel.RefusesToPublishWhileBusy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMViewModelRefusesToPublishWhileBusy::RunTest(const FString&)
{
	FCPM_AssetViewModel Model = MakeValidDraft();
	Model.Status.Status = ECPM_AssetManagerStatus::Packaging_Begin;

	TestEqual(TEXT("validation still passes"), Model.ValidationMessages().Num(), 0);
	TestFalse(TEXT("but a busy Chunk may not start another Publish"), Model.CanCreateOrPublish());

	Model.Status.Status = ECPM_AssetManagerStatus::Packaging_Success;
	TestTrue(TEXT("open again once idle"), Model.CanCreateOrPublish());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMProjectViewModelReportsThePublishInFlight,
	"ConvaiPakManager.UI.ViewModel.ReportsThePublishInFlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMProjectViewModelReportsThePublishInFlight::RunTest(const FString&)
{
	FCPM_ProjectViewModel Project;
	TSharedPtr<FCPM_AssetViewModel> First = MakeShared<FCPM_AssetViewModel>(MakeValidDraft());
	TSharedPtr<FCPM_AssetViewModel> Second = MakeShared<FCPM_AssetViewModel>(MakeValidDraft());
	Second->ChunkId = 1002;
	Second->Name = TEXT("Desert Scene");
	Project.Assets = { First, Second };

	TestFalse(TEXT("idle project has nothing in flight"), Project.AnyPublishInFlight());
	TestTrue(TEXT("and no publishing name"), Project.PublishingAssetName().IsEmpty());

	Second->Status.Status = ECPM_AssetManagerStatus::UploadPak_Begin;
	TestTrue(TEXT("one busy Chunk flips the project gate"), Project.AnyPublishInFlight());
	TestEqual(TEXT("the hint names the publishing Asset"),
		Project.PublishingAssetName().ToString(), FString(TEXT("Desert Scene")));

	Second->Name.Empty();
	TestEqual(TEXT("an unnamed Chunk is named by its id"),
		Project.PublishingAssetName().ToString(), FString(TEXT("Chunk 1002")));

	Second->Status.Status = ECPM_AssetManagerStatus::Delete_Begin;
	TestFalse(TEXT("a delete is busy but is not a publish"), Project.AnyPublishInFlight());
	TestTrue(TEXT("and names nothing"), Project.PublishingAssetName().IsEmpty());

	TestEqual(TEXT("FindByChunkId answers the busy Chunk"), Project.FindByChunkId(1002), Second);

	return true;
}

/**
 * The Platform Selection only overrides when it actually differs from the Policy.
 *
 * The row that matters is the untouched one: a creator who changed nothing must publish with EMPTY
 * options, so the Publish resolves the Policy itself. Pinning the copy this panel happened to read
 * would silently publish yesterday's platforms after Convai changed them this morning.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMViewModelOverridesPlatformsOnlyWhenTheyDiffer,
	"ConvaiPakManager.UI.ViewModel.OverridesPlatformsOnlyWhenTheyDiffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMViewModelOverridesPlatformsOnlyWhenTheyDiffer::RunTest(const FString&)
{
	const TArray<ECPM_Platform> WindowsOnly = { ECPM_Platform::Windows };

	FCPM_AssetViewModel Model = MakeValidDraft();
	TestFalse(TEXT("nothing is seeded before a policy is read"), Model.bPlatformSelectionSeeded);

	// Before any policy read, options must stay empty rather than claim "publish no platform".
	const FCPM_PublishOptions Unseeded = Model.PublishOptions(WindowsOnly, false);
	TestFalse(TEXT("an unseeded selection overrides nothing"), Unseeded.bOverridePlatforms);

	Model.SeedPlatformSelection(WindowsOnly);
	TestTrue(TEXT("seeding takes the policy's platforms"), Model.SelectedPlatforms.Contains(ECPM_Platform::Windows));

	const FCPM_PublishOptions Untouched = Model.PublishOptions(WindowsOnly, false);
	TestFalse(TEXT("an untouched selection overrides nothing"), Untouched.bOverridePlatforms);
	TestFalse(TEXT("and does not reuse paks"), Untouched.bReuseExistingPaks);

	// Adding: the enterprise project Convai agreed to host Linux for.
	Model.SelectedPlatforms.Add(ECPM_Platform::Linux);
	const FCPM_PublishOptions Added = Model.PublishOptions(WindowsOnly, false);
	TestTrue(TEXT("adding a platform overrides"), Added.bOverridePlatforms);
	TestEqual(TEXT("carrying both platforms"), Added.Platforms.Num(), 2);

	// Removing: one creator sending Windows alone once Linux is general.
	Model.SelectedPlatforms.Remove(ECPM_Platform::Linux);
	Model.SelectedPlatforms.Remove(ECPM_Platform::Windows);
	const FCPM_PublishOptions Removed = Model.PublishOptions(WindowsOnly, true);
	TestTrue(TEXT("removing every platform overrides"), Removed.bOverridePlatforms);
	TestEqual(TEXT("carrying none"), Removed.Platforms.Num(), 0);
	TestTrue(TEXT("and reuse rides along"), Removed.bReuseExistingPaks);

	// A second policy read must not wipe what the creator chose this session.
	Model.SelectedPlatforms.Add(ECPM_Platform::Linux);
	Model.SeedPlatformSelection(WindowsOnly);
	TestTrue(TEXT("re-seeding keeps the creator's choice"), Model.SelectedPlatforms.Contains(ECPM_Platform::Linux));

	return true;
}

/**
 * Only a scan that found Errors closes Upload. Warnings, an unread Policy and a refusal all leave
 * the gate open and say so on the one line - a creator must never read READY over a stage the
 * Publish is about to refuse, nor be locked out by a warning.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMViewModelGatesOnStageIssues,
	"ConvaiPakManager.UI.ViewModel.GatesOnStageIssues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMViewModelGatesOnStageIssues::RunTest(const FString&)
{
	// Box off: the report is ignored whatever it holds.
	FCPM_AssetViewModel Off = MakeStagedAvatarDraft();
	Off.bStageEnabled = false;
	Off.StageReport.Issues = { StageIssue(ECPM_StageSeverity::Error), StageIssue(ECPM_StageSeverity::Error) };
	TestEqual(TEXT("an unticked box adds no message"), Off.ValidationMessages().Num(), 0);
	TestTrue(TEXT("and leaves the gate open"), Off.CanCreateOrPublish());
	TestTrue(TEXT("and shows no line"), Off.StageStatusLine().IsEmpty());

	// One Error closes the gate and names itself.
	FCPM_AssetViewModel One = MakeStagedAvatarDraft();
	One.StageReport.Issues = { StageIssue(ECPM_StageSeverity::Error) };
	TestEqual(TEXT("one error is one message"), One.ValidationMessages().Num(), 1);
	TestEqual(TEXT("in the singular"), One.ValidationMessages()[0].ToString(), FString(TEXT("1 issue needs attention.")));
	TestFalse(TEXT("and closes the gate"), One.CanCreateOrPublish());
	TestEqual(TEXT("the line says the same"), One.StageStatusLine().ToString(), FString(TEXT("1 issue needs attention.")));

	// Two Errors and a Warning: the plural counts errors alone.
	FCPM_AssetViewModel Two = MakeStagedAvatarDraft();
	Two.StageReport.Issues = { StageIssue(ECPM_StageSeverity::Error), StageIssue(ECPM_StageSeverity::Warning), StageIssue(ECPM_StageSeverity::Error) };
	TestEqual(TEXT("Count sees the errors"), Two.StageReport.Count(ECPM_StageSeverity::Error), 2);
	TestEqual(TEXT("and the warning"), Two.StageReport.Count(ECPM_StageSeverity::Warning), 1);
	TestEqual(TEXT("and no info"), Two.StageReport.Count(ECPM_StageSeverity::Info), 0);
	TestEqual(TEXT("two errors read in the plural"), Two.StageStatusLine().ToString(), FString(TEXT("2 issues need attention.")));
	TestFalse(TEXT("and close the gate"), Two.CanCreateOrPublish());

	// Warnings only: open, and READY says how many.
	FCPM_AssetViewModel Warned = MakeStagedAvatarDraft();
	Warned.StageReport.Issues = { StageIssue(ECPM_StageSeverity::Warning), StageIssue(ECPM_StageSeverity::Info) };
	TestEqual(TEXT("a warning adds no message"), Warned.ValidationMessages().Num(), 0);
	TestTrue(TEXT("and leaves the gate open"), Warned.CanCreateOrPublish());
	TestEqual(TEXT("one warning reads READY with a count"), Warned.StageStatusLine().ToString(), FString(TEXT("READY TO UPLOAD (1 warning)")));
	Warned.StageReport.Issues.Add(StageIssue(ECPM_StageSeverity::Warning));
	TestEqual(TEXT("two warnings read in the plural"), Warned.StageStatusLine().ToString(), FString(TEXT("READY TO UPLOAD (2 warnings)")));
	Warned.StageReport.Issues.Reset();
	TestEqual(TEXT("a clean scan reads READY alone"), Warned.StageStatusLine().ToString(), FString(TEXT("READY TO UPLOAD")));

	// Limits not read: nothing was judged, so nothing gates - and READY is never claimed.
	FCPM_AssetViewModel Unread = MakeStagedAvatarDraft();
	Unread.StageReport.bLimitsRead = false;
	Unread.StageReport.Issues = { StageIssue(ECPM_StageSeverity::Error) };
	TestTrue(TEXT("unread limits leave the gate open"), Unread.CanCreateOrPublish());
	TestEqual(TEXT("and ask for the policy"), Unread.StageStatusLine().ToString(), FString(TEXT("Limits not read - Re-read policy")));
	TestFalse(TEXT("never READY"), Unread.StageStatusLine().ToString().Contains(TEXT("READY")));

	// A refusal outranks everything else on the line.
	FCPM_AssetViewModel Refused = MakeStagedAvatarDraft();
	Refused.StageReport.bLimitsRead = false;
	Refused.StageReport.Refusal = TEXT("open the level that holds the avatar and its stage first");
	TestEqual(TEXT("the refusal is the line"), Refused.StageStatusLine().ToString(), Refused.StageReport.Refusal);
	TestTrue(TEXT("and does not gate"), Refused.CanCreateOrPublish());

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
