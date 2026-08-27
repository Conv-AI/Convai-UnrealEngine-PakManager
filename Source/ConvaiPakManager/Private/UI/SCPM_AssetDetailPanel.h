// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "Brushes/SlateDynamicImageBrush.h"
#include "ConvaiPakEditorSubsystem.h"
#include "CoreMinimal.h"
#include "UI/CPM_PakManagerViewModels.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SExpandableArea;
class SImage;
class SVerticalBox;
class SWrapBox;

/**
 * The form: one Chunk's identity, Entry Point, thumbnail, spawn point, Paks and Asset ID, plus the
 * inline publish-progress panel that replaces the working form while its Chunk is busy.
 *
 * Reads and writes only the view model it is pointed at; the root panel decides which one that is
 * and relays status changes. The spawn subsection is built only for Scene projects - for an Avatar
 * it must not exist in the widget tree at all (design D11, acceptance item).
 */
class SCPM_AssetDetailPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_AssetDetailPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Points the form at another Chunk's view model (or none) and re-derives everything cached. */
	void SetAssetViewModel(TSharedPtr<FCPM_AssetViewModel> InAsset);

	/** Called by the root panel when the shown Chunk's status changed - keeps progress and Paks current. */
	void OnActiveStatusChanged();

private:
	TSharedRef<SWidget> BuildProgressPanel();
	TSharedRef<SWidget> BuildIdentitySection();
	TSharedRef<SWidget> BuildContentSourceSection();
	TSharedRef<SWidget> BuildPreviewSection();
	TSharedRef<SWidget> BuildPackagingSection();
	TSharedRef<SWidget> BuildTechnicalSection();

	static UConvaiPakEditorSubsystem* GetSubsystem();

	FReply HandleUseSelectedAsset();
	FReply HandleRevealEntryPoint();
	FReply HandleCaptureThumbnail();
	FReply HandlePreviewThumbnail();
	FReply HandleSetSpawnPoint();
	FReply HandleCopyAssetId();
	FReply HandleCancelPublish();

	void RefreshThumbnailBrush(bool bForceReload);
	void RebuildStageRow();
	void RebuildPackagingRows();
	EActiveTimerReturnType RefreshSpawnStatus(double InCurrentTime, float InDeltaTime);

	/** This Chunk is busy, which locks its form (other Chunks stay editable - design D14). */
	bool IsBusy() const;

	TSharedPtr<FCPM_AssetViewModel> Asset;

	/** Scene or Avatar is fixed per project, so the tree shape is decided once at construction. */
	bool bIsScene = true;

	/** Why the last Entry Point pick was refused. Empty means no inline error row. */
	FText EntryPointError;

	/** The only brush on the thumbnail file, so a recapture can release the texture and re-read it. */
	TSharedPtr<FSlateDynamicImageBrush> ThumbnailBrush;
	TSharedPtr<SImage> ThumbnailImage;

	FCPM_SpawnPointStatus SpawnStatus;

	TSharedPtr<SWrapBox> StageRow;
	/** What the stage row was last built from, so progress ticks do not rebuild widgets. */
	TArray<FString> BuiltStageSteps;

	TSharedPtr<SVerticalBox> PackagingRows;
	int32 BuiltPakRowCount = INDEX_NONE;

	TSharedPtr<SExpandableArea> PackagingArea;
	TSharedPtr<SExpandableArea> TechnicalArea;
};
