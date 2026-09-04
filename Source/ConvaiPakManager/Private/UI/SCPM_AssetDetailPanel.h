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
		/** The project just gained its first Chunk; the root panel re-reads and repopulates. */
		SLATE_EVENT(FSimpleDelegate, OnChunkCreated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Points the form at another Chunk's view model (or none) and re-derives everything cached. */
	void SetAssetViewModel(TSharedPtr<FCPM_AssetViewModel> InAsset);

	/** Called by the root panel when the shown Chunk's status changed - keeps progress and Paks current. */
	void OnActiveStatusChanged();

	/** For the root panel's More menu, which owns the verbs but not the platform state. */
	bool IsShowingAllPlatforms() const { return bShowAllPlatforms; }
	void ToggleShowAllPlatforms() { SetShowAllPlatforms(!bShowAllPlatforms); }

	/** The options this Chunk's next Publish or Package should run with. */
	FCPM_PublishOptions BuildPublishOptions(bool bReuseExistingPaks) const;

	/** Whether any platform this run would build already has a Pak on disk to reuse. */
	bool HasAnyBuiltPak() const;

private:
	TSharedRef<SWidget> BuildProgressPanel();
	TSharedRef<SWidget> BuildIdentitySection();
	TSharedRef<SWidget> BuildContentSourceSection();
	TSharedRef<SWidget> BuildPreviewSection();
	TSharedRef<SWidget> BuildUploadSection();
	TSharedRef<SWidget> BuildTechnicalSection();

	/** One platform's row: name, state, its inclusion checkbox and its "..." menu. */
	TSharedRef<SWidget> BuildPlatformRow(ECPM_Platform Platform);

	/** The "..." menu on one platform's row. */
	TSharedRef<SWidget> BuildPlatformRowMenu(ECPM_Platform Platform);

	/** The "..." menu on the project source row - the same anatomy as a platform's. */
	TSharedRef<SWidget> BuildSourceRowMenu();

	/**
	 * Platforms the ledger shows: what the Policy asks for, or every platform when the creator asked
	 * to see them all - and always every platform while the Policy is unread or unreadable, so a
	 * failed network read can never hide a Pak the creator has on disk.
	 */
	TArray<ECPM_Platform> VisiblePlatforms() const;

	/** What the Policy asks for, empty unless it has actually been read. */
	TArray<ECPM_Platform> PolicyPlatforms() const;

	/** Whether the Policy asks this project for the Raw Project Archive. Fails open, as platforms do. */
	bool PolicyAsksForProjectSource() const;

	static UConvaiPakEditorSubsystem* GetSubsystem();

	FReply HandleCreateChunk();
	FReply HandleUseSelectedAsset();
	FReply HandleRelocateEntryPoint();
	FReply HandleShowDependencies();

	/**
	 * Offers to copy in whatever an Entry Point inside the plugin still reaches outside it.
	 *
	 * Asked at the pick, not left for the creator to find: the Pak is built from what the plugin
	 * holds, and a level whose meshes live in /Game looks perfectly fine right up to the publish.
	 */
	void OfferToGatherDependencies(const FString& EntryPoint);
	FReply HandleRevealEntryPoint();
	FReply HandleCaptureThumbnail();
	FReply HandleChooseThumbnailImage();
	FReply HandleUseSelectedTexture();
	FReply HandlePreviewThumbnail();
	FReply HandleSetSpawnPoint();
	FReply HandleAddNavMeshBounds();
	FReply HandleCopyAssetId();
	FReply HandleCancelPublish();

	void RefreshThumbnailBrush(bool bForceReload);
	void RebuildStageRow();
	void RebuildUploadRows();
	EActiveTimerReturnType RefreshSpawnStatus(double InCurrentTime, float InDeltaTime);

	/** This Chunk is busy, which locks its form (other Chunks stay editable - design D14). */
	bool IsBusy() const;

	/** Re-seeds the Platform Selection and rebuilds the rows after the cached Policy changed. */
	void OnPolicyChanged();

	/**
	 * Show platforms the Publish Policy does not ask for.
	 *
	 * Per USER, in EditorPerProjectUserSettings rather than project config: the people who want it
	 * want it permanently, and it is a preference about looking, not a statement about the project -
	 * putting it in project config would ship one person's choice to the whole team.
	 */
	bool bShowAllPlatforms = false;

	void LoadShowAllPlatforms();
	void SetShowAllPlatforms(bool bShow);

	FSimpleDelegate OnChunkCreated;

	TSharedPtr<FCPM_AssetViewModel> Asset;

	/**
	 * What the empty state offers and why, re-derived whenever the root panel re-points this form.
	 *
	 * Cached because both answers read the project's files, and the empty state paints every frame.
	 */
	bool bCanCreateChunk = false;
	bool bLegacyLayoutPending = false;

	/** Scene or Avatar is fixed per project, so the tree shape is decided once at construction. */
	bool bIsScene = true;

	/** Backs the gender combo. A member because SComboBox holds OptionsSource as a raw pointer. */
	TArray<TSharedPtr<FString>> GenderOptions;

	/** Why the last Entry Point pick was refused. Empty means no inline error row. */
	FText EntryPointError;

	/**
	 * What the last pick changed on the creator's own asset, or empty when it changed nothing.
	 *
	 * On screen rather than only in the Output Log: a pick edits and saves the creator's blueprint,
	 * and an edit nobody is told about is one nobody knows to undo.
	 */
	FText SetupNotes;

	/**
	 * The package a refused pick named, when the refusal was that it sits outside the Modding Plugin.
	 *
	 * Empty for every other refusal - offering to copy is only honest when copying is the fix.
	 */
	FString OutsidePick;

	/** The only brush on the thumbnail file, so a recapture can release the texture and re-read it. */
	TSharedPtr<FSlateDynamicImageBrush> ThumbnailBrush;
	TSharedPtr<SImage> ThumbnailImage;

	FCPM_SpawnPointStatus SpawnStatus;

	/** Refreshed on the same timer as SpawnStatus: both are facts about the level, not about a Chunk. */
	bool bHasNavMeshBounds = false;

	TSharedPtr<SWrapBox> StageRow;
	/** What the stage row was last built from, so progress ticks do not rebuild widgets. */
	TArray<FString> BuiltStageSteps;

	TSharedPtr<SVerticalBox> UploadRows;

	/**
	 * What the rows were last built from.
	 *
	 * NOT a count: the rendered set varies with the Policy and the show-all toggle while
	 * PakStatuses stays two entries long, so a count would leave a Linux row labelled Windows.
	 */
	FString BuiltRowSignature;

	TSharedPtr<SExpandableArea> UploadArea;
	TSharedPtr<SExpandableArea> TechnicalArea;
};
