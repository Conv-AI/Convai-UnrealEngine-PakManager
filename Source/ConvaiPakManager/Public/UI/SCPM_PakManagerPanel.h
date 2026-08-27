// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
 * it. Shows state and sends Commands - no publishing logic of its own, no Job System subscription.
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
	TSharedRef<SWidget> BuildActionBar();
	TSharedRef<SWidget> BuildMoreMenu();

	static UConvaiPakEditorSubsystem* GetSubsystem();

	/** Selection change behind the D6 unsaved-edits guard: Save / Discard / Cancel. */
	void RequestSelectAsset(TSharedPtr<FCPM_AssetViewModel> NewSelection);

	/** Re-points the list, the header picker and the detail form at the Active view model. */
	void SyncSelectionWidgets();

	/** Saves the Active Chunk's dirty fields. True when nothing needed saving or the save landed. */
	bool SaveActive(bool bNotifyOnSuccess);

	void HandleChunkStatusChanged(const FCPM_ChunkStatus& Status);

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

	TSharedPtr<SCPM_AssetListPanel> ListPanel;
	TSharedPtr<SCPM_AssetDetailPanel> DetailPanel;
	TSharedPtr<SComboBox<TSharedPtr<FCPM_AssetViewModel>>> ChunkCombo;
	TSharedPtr<SComboButton> MoreButton;

	FDelegateHandle StatusChangedHandle;
};
