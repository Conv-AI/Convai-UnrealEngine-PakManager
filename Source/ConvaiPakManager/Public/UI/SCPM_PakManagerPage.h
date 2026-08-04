// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/CPM_PublishTypes.h"
#include "UI/Pages/SBasePage.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SEditableTextBox;
class SRoundedProgressBar;
class STextBlock;
class UConvaiPakEditorSubsystem;

/**
 * The Pak Manager panel.
 *
 * Shows state and sends Commands. It holds no publishing logic of its own - no Workflow Handle, no
 * Job System subscription, no knowledge of what publishing involves. Everything it displays comes
 * from a Chunk's status, which is the seam that makes the same Commands drivable by a script or a
 * test with no widget in the loop. See docs/adr/0001 and docs/adr/0008.
 *
 * Built from the Convai SDK's widget kit rather than a private one, so it looks like the rest of the
 * SDK and so roughly two thousand lines of reimplemented form controls do not exist. See docs/adr/0006.
 *
 * MUST derive from SBasePage. The shell's navigation casts every page it is handed to SBasePage* and
 * calls a virtual on the result, so a page that is merely some other widget reads a wrong vtable and
 * takes the editor down with it. Deriving is the contract, and the type check that follows the cast
 * cannot catch a violation of it - it is a virtual call, so it needs the vtable it means to verify.
 */
class CONVAIPAKMANAGER_API SCPM_PakManagerPage : public SBasePage
{
public:
	SLATE_BEGIN_ARGS(SCPM_PakManagerPage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCPM_PakManagerPage() override;

	/** Re-reads the panel whenever the creator navigates to it, so it never shows stale state. */
	virtual void OnPageActivated() override;

private:
	TSharedRef<SWidget> BuildProjectSection() const;
	TSharedRef<SWidget> BuildChunkSection();
	TSharedRef<SWidget> BuildAssetSection();
	TSharedRef<SWidget> BuildActionSection();
	TSharedRef<SWidget> BuildStatusSection();

	UConvaiPakEditorSubsystem* GetSubsystem() const;

	/** Re-reads everything this panel shows for the selected Chunk. */
	void RefreshFromChunk();

	void HandleChunkStatusChanged(const FCPM_ChunkStatus& Status);

	FReply HandlePickAssetClicked();
	FReply HandleAddSpawnPointClicked();
	FReply HandleCaptureThumbnailClicked();
	FReply HandlePublishClicked();
	FReply HandleCancelClicked();
	FReply HandleDeleteClicked();

	void HandleAssetNameCommitted(const FText& Text, ETextCommit::Type CommitType);
	void HandleAssetDescriptionCommitted(const FText& Text, ETextCommit::Type CommitType);

	/** True while a Publish is running, which is what disables the buttons that would start another. */
	bool IsBusy() const;
	EVisibility GetCancelVisibility() const;
	EVisibility GetChunkPickerVisibility() const;

	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	TOptional<float> GetProgress() const;

	/** The Chunk on screen. INDEX_NONE when the project has none. */
	int32 SelectedChunkId = INDEX_NONE;

	TArray<int32> ChunkIds;

	FCPM_ChunkStatus LastStatus;

	TSharedPtr<SEditableTextBox> AssetNameBox;
	TSharedPtr<SEditableTextBox> AssetDescriptionBox;
	TSharedPtr<STextBlock> AssetIdText;
	TSharedPtr<STextBlock> EntryPointText;
	TSharedPtr<STextBlock> AssetTypeText;

	/** Held so the thumbnail can be swapped without rebuilding the panel. */
	FSlateBrush ThumbnailBrush;
	TSharedPtr<FSlateDynamicImageBrush> ThumbnailImageBrush;

	FDelegateHandle StatusChangedHandle;
};
