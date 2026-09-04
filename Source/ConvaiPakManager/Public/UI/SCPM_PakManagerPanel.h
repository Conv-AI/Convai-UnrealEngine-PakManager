// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/CPM_Compatibility.h"
#include "Publish/CPM_Preconditions.h"
#include "UI/CPM_PakManagerViewModels.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SCPM_AssetDetailPanel;
class SCPM_AssetListPanel;
class SComboButton;
template <typename OptionType> class SComboBox;
class UConvaiPakEditorSubsystem;

/**
 * The Pak Manager tab's root: header, asset list / detail split, sticky bottom action bar.
 *
 * Owns the project view model and the subsystem subscription; the child panels borrow views into
 * it. Shows state and sends Commands - no publishing logic of its own, no subscription to the queue.
 * See docs/adr/0001, 0008 and 0009, and .scratch/slate-ui-rebuild/design.md.
 */
class CONVAIPAKMANAGER_API SCPM_PakManagerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_PakManagerPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCPM_PakManagerPanel() override;

	/**
	 * Rediscovers Chunks and re-reads every view model. The module calls this when the tab is
	 * foregrounded - Chunks and their records can change while the tab is hidden.
	 */
	void RefreshProject();

private:
	TSharedRef<SWidget> BuildHeader();

	/** The pre-Chunk-records warning, shown for as long as the condition holds. */
	TSharedRef<SWidget> BuildLegacyBanner();

	/** The outdated-tool / wrong-engine warning. Says so and gates nothing - see the comment on it. */
	TSharedRef<SWidget> BuildCompatibilityBanner();

	/**
	 * The missing-Linux-toolchain warning. Unlike the one above, the publish DOES refuse over this.
	 *
	 * It is here so the refusal is not the creator's first news of it: the condition is knowable the
	 * moment the panel opens, and finding out at the click is the dead end this panel avoids.
	 */
	TSharedRef<SWidget> BuildToolchainBanner();

	TSharedRef<SWidget> BuildActionBar();
	TSharedRef<SWidget> BuildMoreMenu();

	/** More menu verbs. Each carries the active Chunk's Platform Selection. */
	void HandlePackageNow();
	void HandlePublishReusingPaks();
	void HandleDeleteBuiltPaks();

	/** Says why the subsystem refused, falling back when it recorded no message. */
	void NotifyRefusal(int32 ChunkId, const FText& Fallback);

	static UConvaiPakEditorSubsystem* GetSubsystem();

	/** Selection change behind the D6 unsaved-edits guard: Save / Discard / Cancel. */
	void RequestSelectAsset(TSharedPtr<FCPM_AssetViewModel> NewSelection);

	/** Re-points the list, the header picker and the detail form at the Active view model. */
	void SyncSelectionWidgets();

	/** Saves the Active Chunk's dirty fields. True when nothing needed saving or the save landed. */
	bool SaveActive(bool bNotifyOnSuccess);

	void HandleChunkStatusChanged(const FCPM_ChunkStatus& Status);

	/** The Asset Registry finished its scan, so Chunks that were invisible at Construct are findable. */
	void HandleFilesLoaded();

	void HandleCompatibilityChanged();

	/** Re-reads the toolchain and whether the Policy wants Linux. Both only change with the Policy. */
	void HandlePolicyChanged();

	FReply HandleSaveClicked();
	FReply HandlePrimaryClicked();
	FReply HandleDeleteClicked();

	/** A Publish runs on some other Chunk - the one-publish-at-a-time project gate (design D14). */
	bool IsOtherChunkPublishing() const;

	/** Multi-Chunk and wide enough for the sidebar; narrow docks get the header picker instead. */
	bool ShouldShowList() const;

	FText GetPrimaryButtonText() const;
	bool CanClickPrimary() const;
	FText GetActionBarSummary() const;
	FSlateColor GetActionBarSummaryColor() const;

	FCPM_ProjectViewModel Project;

	/**
	 * Records from a pre-Chunk layout that migration could not attribute, cached per refresh.
	 *
	 * Answering it reads the ConvaiEssentials directory, so it must never be asked from a paint path.
	 */
	bool bLegacyLayoutPending = false;

	/** What the version check last answered, cached because the banner reads it on every paint. */
	FCPM_CompatibilityStatus Compatibility;

	/** Cached for the same reason: reading it stats the disk, which a paint path must not do. */
	ConvaiPakManager::Preconditions::FLinuxToolchain LinuxToolchain;

	/** Whether the cached Policy asks for Linux at all. Without it the banner nags Windows-only projects. */
	bool bPolicyPackagesLinux = false;

	TSharedPtr<SCPM_AssetListPanel> ListPanel;
	TSharedPtr<SCPM_AssetDetailPanel> DetailPanel;
	TSharedPtr<SComboBox<TSharedPtr<FCPM_AssetViewModel>>> ChunkCombo;
	TSharedPtr<SComboButton> MoreButton;

	FDelegateHandle StatusChangedHandle;
	FDelegateHandle FilesLoadedHandle;
	FDelegateHandle CompatibilityChangedHandle;
	FDelegateHandle PolicyChangedHandle;
};
