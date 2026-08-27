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
	Status = Subsystem.GetChunkStatus(ChunkId);
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
			Messages.Add(LOCTEXT("EntryPointRequiredScene", "Set an Entry point: the level this Scene opens."));
			break;
		case ECPM_AssetType::Avatar:
			Messages.Add(LOCTEXT("EntryPointRequiredAvatar", "Set an Entry point: the blueprint this Avatar loads."));
			break;
		default:
			Messages.Add(LOCTEXT("EntryPointRequired", "Set an Entry point."));
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
		if (Asset.IsValid() && Asset->Status.IsBusy())
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
		if (Asset.IsValid() && Asset->Status.IsBusy())
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
