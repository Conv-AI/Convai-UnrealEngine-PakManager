// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UI/CPM_PakManagerViewModels.h"

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

#endif  // WITH_AUTOMATION_TESTS
