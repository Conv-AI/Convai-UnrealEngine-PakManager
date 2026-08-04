// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/CPM_PublishTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

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
 */
class CONVAIPAKMANAGER_API SCPM_PakManagerPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCPM_PakManagerPage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCPM_PakManagerPage() override;

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

	/** Held so the thumbnail can be swapped without rebuilding the panel. */
	FSlateBrush ThumbnailBrush;
	TSharedPtr<FSlateDynamicImageBrush> ThumbnailImageBrush;

	FDelegateHandle StatusChangedHandle;
};
