// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/CPM_PakManagerViewModels.h"

#include "ConvaiPakEditorSubsystem.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Utility/CPM_UtilityLibrary.h"

#define LOCTEXT_NAMESPACE "CPM_PakManagerViewModels"

void FCPM_AssetViewModel::LoadFrom(UConvaiPakEditorSubsystem& Subsystem)
{
	// Decided before the snapshot moves, or a mid-typing refresh would silently discard the edits.
	const bool bKeepEdits = IsDirty();

	SavedName = Subsystem.GetAssetName(ChunkId);
	SavedDescription = Subsystem.GetAssetDescription(ChunkId);
	if (!bKeepEdits)
	{
		Name = SavedName;
		Description = SavedDescription;
	}

	AssetId = Subsystem.GetAssetId(ChunkId);
	EntryPoint = Subsystem.GetEntryPoint(ChunkId);
	AssetType = Subsystem.GetAssetType();
	ThumbnailPath = Subsystem.GetThumbnailPath(ChunkId);
	bThumbnailExists = !ThumbnailPath.IsEmpty() && FPaths::FileExists(ThumbnailPath);
	PakStatuses = Subsystem.GetPakStatuses(ChunkId);
	RawArchiveUploadTime = Subsystem.GetRawArchiveUploadTime(ChunkId);
	Status = Subsystem.GetChunkStatus(ChunkId);
}

void FCPM_AssetViewModel::SeedPlatformSelection(const TArray<ECPM_Platform>& PolicyPlatforms)
{
	// Only the first read seeds. A creator who ticked Linux for this session must not lose it
	// because the panel re-read the same Policy a minute later.
	if (bPlatformSelectionSeeded)
	{
		return;
	}

	SelectedPlatforms.Reset();
	for (const ECPM_Platform Platform : PolicyPlatforms)
	{
		SelectedPlatforms.Add(Platform);
	}
	bPlatformSelectionSeeded = true;
}

FCPM_PublishOptions FCPM_AssetViewModel::PublishOptions(
	const TArray<ECPM_Platform>& PolicyPlatforms, const bool bReuseExistingPaks) const
{
	FCPM_PublishOptions Options;
	Options.bReuseExistingPaks = bReuseExistingPaks;

	if (!bPlatformSelectionSeeded)
	{
		// Nothing has been read to differ from, so this run follows whatever the Publish resolves.
		return Options;
	}

	bool bDiffers = SelectedPlatforms.Num() != PolicyPlatforms.Num();
	for (const ECPM_Platform Platform : PolicyPlatforms)
	{
		bDiffers |= !SelectedPlatforms.Contains(Platform);
	}

	// Left alone when it matches: the Publish re-reads the Policy for itself, and overriding with a
	// copy this panel read earlier would pin a stale answer onto a run that could see a fresh one.
	if (bDiffers)
	{
		Options.bOverridePlatforms = true;
		Options.Platforms = SelectedPlatforms.Array();
	}

	return Options;
}

bool FCPM_AssetViewModel::IsDirty() const
{
	return Name != SavedName || Description != SavedDescription;
}

bool FCPM_AssetViewModel::Save(UConvaiPakEditorSubsystem& Subsystem)
{
	bool bAccepted = true;
	if (Name != SavedName)
	{
		bAccepted &= Subsystem.SetAssetName(ChunkId, Name);
	}
	if (Description != SavedDescription)
	{
		bAccepted &= Subsystem.SetAssetDescription(ChunkId, Description);
	}

	// Re-read rather than assume: a refused Set* leaves that field dirty against the real record.
	SavedName = Subsystem.GetAssetName(ChunkId);
	SavedDescription = Subsystem.GetAssetDescription(ChunkId);
	return bAccepted;
}

void FCPM_AssetViewModel::Revert()
{
	Name = SavedName;
	Description = SavedDescription;
}

namespace
{
	bool IsFailedStatus(ECPM_AssetManagerStatus Status)
	{
		switch (Status)
		{
		case ECPM_AssetManagerStatus::Packaging_Failed:
		case ECPM_AssetManagerStatus::Create_Failed:
		case ECPM_AssetManagerStatus::Update_Failed:
		case ECPM_AssetManagerStatus::UploadPak_Failed:
		case ECPM_AssetManagerStatus::Delete_Failed:
			return true;
		default:
			return false;
		}
	}

	/** Busy with a Publish or Package. A delete is busy too, but it is one HTTP call and contends with nothing. */
	bool IsPublishing(const FCPM_ChunkStatus& Status)
	{
		return Status.IsBusy() && Status.Status != ECPM_AssetManagerStatus::Delete_Begin;
	}
}

FCPM_AssetViewModel::EBadge FCPM_AssetViewModel::Badge() const
{
	if (Status.IsBusy())
	{
		return EBadge::Publishing;
	}
	if (IsFailedStatus(Status.Status))
	{
		return EBadge::NeedsAttention;
	}
	if (!AssetId.IsEmpty())
	{
		return EBadge::Published;
	}
	if (ValidationMessages().IsEmpty())
	{
		return EBadge::ReadyToPublish;
	}
	return EBadge::Draft;
}

TArray<FText> FCPM_AssetViewModel::ValidationMessages() const
{
	TArray<FText> Messages;

	if (Name.TrimStartAndEnd().IsEmpty())
	{
		Messages.Add(LOCTEXT("NameRequired", "Asset name is required."));
	}

	if (EntryPoint.IsEmpty())
	{
		switch (AssetType)
		{
		case ECPM_AssetType::Scene:
			Messages.Add(LOCTEXT("EntryPointRequiredScene", "Pick the level this scene opens."));
			break;
		case ECPM_AssetType::Avatar:
			Messages.Add(LOCTEXT("EntryPointRequiredAvatar", "Pick the blueprint this avatar loads."));
			break;
		default:
			Messages.Add(LOCTEXT("EntryPointRequired", "Pick what this asset loads."));
			break;
		}
	}

	if (!bThumbnailExists)
	{
		Messages.Add(LOCTEXT("ThumbnailRequired", "Capture a thumbnail before creating."));
	}

	return Messages;
}

bool FCPM_AssetViewModel::CanCreateOrPublish() const
{
	return !Status.IsBusy() && ValidationMessages().IsEmpty();
}

FText FCPM_AssetViewModel::BadgeText() const
{
	switch (Badge())
	{
	case EBadge::Publishing:
		return LOCTEXT("BadgePublishing", "Publishing");
	case EBadge::NeedsAttention:
		return LOCTEXT("BadgeNeedsAttention", "Needs attention");
	case EBadge::Published:
		return LOCTEXT("BadgePublished", "Published");
	case EBadge::ReadyToPublish:
		return LOCTEXT("BadgeReadyToPublish", "Ready to publish");
	default:
		return LOCTEXT("BadgeDraft", "Draft");
	}
}

void FCPM_ProjectViewModel::Refresh(UConvaiPakEditorSubsystem& Subsystem)
{
	ProjectName = UCPM_UtilityLibrary::GetProjectName();
	EngineVersion = FEngineVersion::Current().ToString(EVersionComponent::Patch);

	// Surviving Chunks keep their VM instance, and with it any dirty edits and the Active pointer.
	TArray<TSharedPtr<FCPM_AssetViewModel>> Refreshed;
	for (const int32 ChunkId : Subsystem.GetChunkIds())
	{
		TSharedPtr<FCPM_AssetViewModel> Model = FindByChunkId(ChunkId);
		if (!Model.IsValid())
		{
			Model = MakeShared<FCPM_AssetViewModel>();
			Model->ChunkId = ChunkId;
		}
		Model->LoadFrom(Subsystem);
		Refreshed.Add(MoveTemp(Model));
	}
	Assets = MoveTemp(Refreshed);

	if (!Active.IsValid() || !Assets.Contains(Active))
	{
		Active = Assets.Num() > 0 ? Assets[0] : nullptr;
	}
}

bool FCPM_ProjectViewModel::AnyPublishInFlight() const
{
	for (const TSharedPtr<FCPM_AssetViewModel>& Asset : Assets)
	{
		if (Asset.IsValid() && IsPublishing(Asset->Status))
		{
			return true;
		}
	}
	return false;
}

FText FCPM_ProjectViewModel::PublishingAssetName() const
{
	for (const TSharedPtr<FCPM_AssetViewModel>& Asset : Assets)
	{
		if (Asset.IsValid() && IsPublishing(Asset->Status))
		{
			// Printf, not AsNumber: a Chunk id is an identifier, not a quantity to group digits in.
			return Asset->Name.IsEmpty()
				? FText::FromString(FString::Printf(TEXT("Chunk %d"), Asset->ChunkId))
				: FText::FromString(Asset->Name);
		}
	}
	return FText::GetEmpty();
}

TSharedPtr<FCPM_AssetViewModel> FCPM_ProjectViewModel::FindByChunkId(int32 ChunkId) const
{
	for (const TSharedPtr<FCPM_AssetViewModel>& Asset : Assets)
	{
		if (Asset.IsValid() && Asset->ChunkId == ChunkId)
		{
			return Asset;
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
