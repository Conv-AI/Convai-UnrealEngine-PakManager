// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/SCPM_PakManagerPage.h"

#include "ConvaiPakEditorSubsystem.h"
#include "Editor.h"
#include "Misc/EngineVersion.h"
#include "Styling/ConvaiStyle.h"
#include "UI/Widgets/Composites/SFormField.h"
#include "UI/Widgets/SRoundedProgressBar.h"
#include "Utility/CPM_UtilityLibrary.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ConvaiPakManager"

namespace
{
	/** One row of the form. Every labelled field on this panel goes through here. */
	TSharedRef<SWidget> FormRow(const FText& Label, const TSharedRef<SWidget>& Content)
	{
		return SNew(SFormField).Label(Label)[Content];
	}

	TSharedRef<SWidget> ReadOnlyValue(const FText& Value)
	{
		return SNew(STextBlock).Text(Value);
	}
}

void SCPM_PakManagerPage::Construct(const FArguments& InArgs)
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		ChunkIds = Subsystem->GetChunkIds();
		StatusChangedHandle = Subsystem->OnChunkStatusChanged.AddRaw(
			this, &SCPM_PakManagerPage::HandleChunkStatusChanged);
	}

	SelectedChunkId = ChunkIds.IsEmpty() ? INDEX_NONE : ChunkIds[0];

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(16.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[BuildProjectSection()]
			+ SVerticalBox::Slot().AutoHeight()[BuildChunkSection()]
			+ SVerticalBox::Slot().AutoHeight()[BuildAssetSection()]
			+ SVerticalBox::Slot().AutoHeight()[BuildActionSection()]
			+ SVerticalBox::Slot().AutoHeight()[BuildStatusSection()]
		]
	];

	RefreshFromChunk();
}

SCPM_PakManagerPage::~SCPM_PakManagerPage()
{
	// Unsubscribed explicitly: the subsystem outlives this widget, and a raw delegate left bound to a
	// destroyed panel is a crash on the next status change rather than a leak.
	if (StatusChangedHandle.IsValid())
	{
		if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->OnChunkStatusChanged.Remove(StatusChangedHandle);
		}
	}
}

UConvaiPakEditorSubsystem* SCPM_PakManagerPage::GetSubsystem() const
{
	return GEditor ? GEditor->GetEditorSubsystem<UConvaiPakEditorSubsystem>() : nullptr;
}

TSharedRef<SWidget> SCPM_PakManagerPage::BuildProjectSection() const
{
	const FEngineVersion& Engine = FEngineVersion::Current();

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			FormRow(LOCTEXT("ProjectName", "Project"),
				ReadOnlyValue(FText::FromString(UCPM_UtilityLibrary::GetProjectName())))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			FormRow(LOCTEXT("EngineVersion", "Unreal Engine"),
				ReadOnlyValue(FText::FromString(
					FString::Printf(TEXT("%d.%d"), Engine.GetMajor(), Engine.GetMinor()))))
		];
}

TSharedRef<SWidget> SCPM_PakManagerPage::BuildChunkSection()
{
	// Shown only when there is a choice to make. A creator's project has one Chunk, and a picker with
	// one entry is a question with one answer. See docs/adr/0003.
	return SNew(SBox)
		.Visibility(this, &SCPM_PakManagerPage::GetChunkPickerVisibility)
		[
			FormRow(LOCTEXT("Chunk", "Chunk"),
				SNew(STextBlock).Text_Lambda([this]
				{
					return FText::AsNumber(SelectedChunkId);
				}))
		];
}

TSharedRef<SWidget> SCPM_PakManagerPage::BuildAssetSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			FormRow(LOCTEXT("AssetId", "Asset ID"),
				SAssignNew(AssetIdText, STextBlock)
				.Text(LOCTEXT("NotPublished", "not published yet")))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			FormRow(LOCTEXT("AssetName", "Asset Name"),
				SAssignNew(AssetNameBox, SEditableTextBox)
				.HintText(LOCTEXT("AssetNameHint", "Name shown in Convai"))
				.OnTextCommitted(this, &SCPM_PakManagerPage::HandleAssetNameCommitted))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			FormRow(LOCTEXT("AssetDescription", "Description"),
				SAssignNew(AssetDescriptionBox, SEditableTextBox)
				.HintText(LOCTEXT("AssetDescriptionHint", "What this asset is"))
				.OnTextCommitted(this, &SCPM_PakManagerPage::HandleAssetDescriptionCommitted))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			FormRow(LOCTEXT("Thumbnail", "Thumbnail"),
				SNew(SButton)
				.Text(LOCTEXT("CaptureThumbnail", "Capture from viewport"))
				.IsEnabled_Lambda([this] { return !IsBusy(); })
				.OnClicked(this, &SCPM_PakManagerPage::HandleCaptureThumbnailClicked))
		];
}

TSharedRef<SWidget> SCPM_PakManagerPage::BuildActionSection()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 12.0f, 8.0f, 12.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Publish", "Publish"))
			.ToolTipText(LOCTEXT("PublishTooltip",
				"Package this chunk, create or update its asset on Convai, and upload every artefact."))
			.IsEnabled_Lambda([this] { return !IsBusy() && SelectedChunkId != INDEX_NONE; })
			.OnClicked(this, &SCPM_PakManagerPage::HandlePublishClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 12.0f, 8.0f, 12.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Cancel", "Cancel"))
			.Visibility(this, &SCPM_PakManagerPage::GetCancelVisibility)
			.OnClicked(this, &SCPM_PakManagerPage::HandleCancelClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 12.0f, 0.0f, 12.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Delete", "Delete Asset"))
			.IsEnabled_Lambda([this]
			{
				const UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
				return !IsBusy() && Subsystem && !Subsystem->GetAssetId(SelectedChunkId).IsEmpty();
			})
			.OnClicked(this, &SCPM_PakManagerPage::HandleDeleteClicked)
		];
}

TSharedRef<SWidget> SCPM_PakManagerPage::BuildStatusSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SCPM_PakManagerPage::GetStatusText)
			.ColorAndOpacity(this, &SCPM_PakManagerPage::GetStatusColor)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
		[
			SNew(SBox)
			.HeightOverride(8.0f)
			.Visibility_Lambda([this] { return IsBusy() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SRoundedProgressBar)
				.Percent_Lambda([this] { return LastStatus.Progress; })
			]
		];
}

void SCPM_PakManagerPage::OnPageActivated()
{
	// Chunks can appear and disappear while the panel is closed - a creator adds a Primary Asset
	// Label, or the Modding Tool regenerates one - so the set is re-read here rather than only at
	// construction, which happens once for the lifetime of the shell.
	if (const UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		ChunkIds = Subsystem->GetChunkIds();
	}

	if (!ChunkIds.Contains(SelectedChunkId))
	{
		SelectedChunkId = ChunkIds.IsEmpty() ? INDEX_NONE : ChunkIds[0];
	}

	RefreshFromChunk();
}

void SCPM_PakManagerPage::RefreshFromChunk()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || SelectedChunkId == INDEX_NONE)
	{
		return;
	}

	LastStatus = Subsystem->GetChunkStatus(SelectedChunkId);

	if (AssetNameBox.IsValid())
	{
		AssetNameBox->SetText(FText::FromString(Subsystem->GetAssetName(SelectedChunkId)));
	}
	if (AssetDescriptionBox.IsValid())
	{
		AssetDescriptionBox->SetText(FText::FromString(Subsystem->GetAssetDescription(SelectedChunkId)));
	}
	if (AssetIdText.IsValid())
	{
		const FString AssetId = Subsystem->GetAssetId(SelectedChunkId);
		AssetIdText->SetText(AssetId.IsEmpty()
			? LOCTEXT("NotPublished", "not published yet")
			: FText::FromString(AssetId));
	}
}

void SCPM_PakManagerPage::HandleChunkStatusChanged(const FCPM_ChunkStatus& Status)
{
	// Every Chunk's status arrives here; only the one on screen changes what is drawn.
	if (Status.ChunkId != SelectedChunkId)
	{
		return;
	}

	LastStatus = Status;

	// A finished publish is the only thing that changes the AssetID, so the fields are re-read then
	// rather than on every progress tick.
	if (!Status.IsBusy())
	{
		RefreshFromChunk();
	}
}

bool SCPM_PakManagerPage::IsBusy() const
{
	return LastStatus.IsBusy();
}

EVisibility SCPM_PakManagerPage::GetCancelVisibility() const
{
	return IsBusy() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SCPM_PakManagerPage::GetChunkPickerVisibility() const
{
	return ChunkIds.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SCPM_PakManagerPage::GetStatusText() const
{
	if (SelectedChunkId == INDEX_NONE)
	{
		return LOCTEXT("NoChunks",
			"This project has no chunks. Add a Primary Asset Label to the content you want to publish.");
	}

	if (!LastStatus.Message.IsEmpty())
	{
		return FText::FromString(LastStatus.Message);
	}

	if (!LastStatus.StepName.IsEmpty())
	{
		return FText::FromString(LastStatus.StepName);
	}

	const UEnum* StatusEnum = StaticEnum<ECPM_AssetManagerStatus>();
	return StatusEnum ? StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(LastStatus.Status)) : FText::GetEmpty();
}

FSlateColor SCPM_PakManagerPage::GetStatusColor() const
{
	switch (LastStatus.Status)
	{
	case ECPM_AssetManagerStatus::Packaging_Failed:
	case ECPM_AssetManagerStatus::Create_Failed:
	case ECPM_AssetManagerStatus::Update_Failed:
	case ECPM_AssetManagerStatus::UploadPak_Failed:
	case ECPM_AssetManagerStatus::Delete_Failed:
		return FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f));
	default:
		return FSlateColor::UseForeground();
	}
}

TOptional<float> SCPM_PakManagerPage::GetProgress() const
{
	return LastStatus.Progress;
}

FReply SCPM_PakManagerPage::HandleCaptureThumbnailClicked()
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->CaptureThumbnail(SelectedChunkId);
	}
	return FReply::Handled();
}

FReply SCPM_PakManagerPage::HandlePublishClicked()
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->Publish(SelectedChunkId);
	}
	return FReply::Handled();
}

FReply SCPM_PakManagerPage::HandleCancelClicked()
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->CancelPublish(SelectedChunkId);
	}
	return FReply::Handled();
}

FReply SCPM_PakManagerPage::HandleDeleteClicked()
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		// Empty version deletes the whole Asset rather than one of its Versions.
		Subsystem->DeleteAsset(SelectedChunkId, FString());
	}
	return FReply::Handled();
}

void SCPM_PakManagerPage::HandleAssetNameCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->SetAssetName(SelectedChunkId, Text.ToString());
	}
}

void SCPM_PakManagerPage::HandleAssetDescriptionCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->SetAssetDescription(SelectedChunkId, Text.ToString());
	}
}

#undef LOCTEXT_NAMESPACE
