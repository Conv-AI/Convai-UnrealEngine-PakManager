// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/SCPM_PakManagerPanel.h"

#include "ConvaiPakEditorSubsystem.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MessageDialog.h"
#include "UI/CPM_PakManagerStyle.h"
#include "UI/SCPM_AssetDetailPanel.h"
#include "UI/SCPM_AssetListPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCPM_PakManagerPanel"

namespace
{
	/** Below this the sidebar steals too much of the form; the header picker takes over. */
	constexpr float NarrowDockWidth = 560.0f;

	void Notify(const FText& Message, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 4.0f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	FText DisplayNameOf(const TSharedPtr<FCPM_AssetViewModel>& AssetVM)
	{
		if (!AssetVM.IsValid())
		{
			return LOCTEXT("NoAsset", "No asset");
		}
		// Printf, not AsNumber: a Chunk id is an identifier, not a quantity to group digits in.
		return AssetVM->Name.IsEmpty()
			? FText::FromString(FString::Printf(TEXT("Chunk %d"), AssetVM->ChunkId))
			: FText::FromString(AssetVM->Name);
	}
}

void SCPM_PakManagerPanel::Construct(const FArguments& InArgs)
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Project.Refresh(*Subsystem);
		StatusChangedHandle = Subsystem->OnChunkStatusChanged.AddSP(
			this, &SCPM_PakManagerPanel::HandleChunkStatusChanged);
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Canvas"))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildHeader()
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot().SizeRule(SSplitter::SizeToContent)
				[
					SNew(SBox)
					.WidthOverride(260.0f)
					.Visibility_Lambda([this]
					{
						return ShouldShowList() ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						SAssignNew(ListPanel, SCPM_AssetListPanel)
						.Project(&Project)
						.OnAssetSelected(FOnCPMAssetSelected::CreateSP(
							this, &SCPM_PakManagerPanel::RequestSelectAsset))
					]
				]
				+ SSplitter::Slot()
				[
					SAssignNew(DetailPanel, SCPM_AssetDetailPanel)
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildActionBar()
			]
		]
	];

	SyncSelectionWidgets();
}

SCPM_PakManagerPanel::~SCPM_PakManagerPanel()
{
	// Unsubscribed explicitly: the subsystem outlives this widget, and a delegate left bound to a
	// destroyed panel is a crash on the next status change.
	if (StatusChangedHandle.IsValid())
	{
		if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->OnChunkStatusChanged.Remove(StatusChangedHandle);
		}
	}
}

void SCPM_PakManagerPanel::RefreshProject()
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Project.Refresh(*Subsystem);
	}
	if (ListPanel.IsValid())
	{
		ListPanel->RefreshList();
	}
	if (ChunkCombo.IsValid())
	{
		ChunkCombo->RefreshOptions();
	}
	SyncSelectionWidgets();
}

TSharedRef<SWidget> SCPM_PakManagerPanel::BuildHeader()
{
	return SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Canvas"))
		.Padding(FMargin(12.0f, 8.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CPM.Text.Title"))
				.Text(LOCTEXT("PanelTitle", "Convai Pak Manager"))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(ChunkCombo, SComboBox<TSharedPtr<FCPM_AssetViewModel>>)
				.OptionsSource(&Project.Assets)
				.Visibility_Lambda([this]
				{
					// The narrow-dock stand-in for the sidebar; pointless when the sidebar shows.
					return Project.Assets.Num() > 1 && !ShouldShowList()
						? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FCPM_AssetViewModel> Item)
				{
					return SNew(STextBlock).Text(DisplayNameOf(Item));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FCPM_AssetViewModel> Item, ESelectInfo::Type SelectInfo)
				{
					if (SelectInfo != ESelectInfo::Direct && Item.IsValid())
					{
						RequestSelectAsset(Item);
					}
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this] { return DisplayNameOf(Project.Active); })
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CPM.Text.Secondary"))
				.Text_Lambda([this]
				{
					return FText::Format(LOCTEXT("ProjectLabel", "Project: {0} - UE {1}"),
						FText::FromString(Project.ProjectName), FText::FromString(Project.EngineVersion));
				})
			]
		];
}

TSharedRef<SWidget> SCPM_PakManagerPanel::BuildActionBar()
{
	return SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Panel"))
		.Padding(FMargin(12.0f, 8.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(this, &SCPM_PakManagerPanel::GetActionBarSummary)
				.ColorAndOpacity(this, &SCPM_PakManagerPanel::GetActionBarSummaryColor)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FButtonStyle>("CPM.Button.Secondary"))
				.Text(LOCTEXT("SaveChanges", "Save changes"))
				.IsEnabled_Lambda([this]
				{
					return Project.Active.IsValid() && Project.Active->IsDirty()
						&& !Project.Active->Status.IsBusy();
				})
				.OnClicked(this, &SCPM_PakManagerPanel::HandleSaveClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FButtonStyle>("CPM.Button.Primary"))
				.Text(this, &SCPM_PakManagerPanel::GetPrimaryButtonText)
				.IsEnabled(this, &SCPM_PakManagerPanel::CanClickPrimary)
				.OnClicked(this, &SCPM_PakManagerPanel::HandlePrimaryClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(MoreButton, SComboButton)
				.OnGetMenuContent(this, &SCPM_PakManagerPanel::BuildMoreMenu)
				.ButtonContent()
				[
					SNew(STextBlock).Text(LOCTEXT("More", "More"))
				]
			]
		];
}

TSharedRef<SWidget> SCPM_PakManagerPanel::BuildMoreMenu()
{
	return SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Panel"))
		.Padding(4.0f)
		[
			SNew(SButton)
			.ButtonStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FButtonStyle>("CPM.Button.Danger"))
			.Text(LOCTEXT("DeleteAsset", "Delete asset..."))
			.ToolTipText(LOCTEXT("DeleteAssetTip", "Removes the published Convai asset. Local project files stay untouched."))
			.IsEnabled_Lambda([this]
			{
				return Project.Active.IsValid() && !Project.Active->AssetId.IsEmpty()
					&& !Project.Active->Status.IsBusy();
			})
			.OnClicked(this, &SCPM_PakManagerPanel::HandleDeleteClicked)
		];
}

UConvaiPakEditorSubsystem* SCPM_PakManagerPanel::GetSubsystem()
{
	return GEditor ? GEditor->GetEditorSubsystem<UConvaiPakEditorSubsystem>() : nullptr;
}

void SCPM_PakManagerPanel::RequestSelectAsset(TSharedPtr<FCPM_AssetViewModel> NewSelection)
{
	if (NewSelection == Project.Active)
	{
		return;
	}

	if (Project.Active.IsValid() && Project.Active->IsDirty())
	{
		const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNoCancel,
			FText::Format(LOCTEXT("UnsavedBody", "Save changes to \"{0}\" before switching?"),
				DisplayNameOf(Project.Active)),
			LOCTEXT("UnsavedTitle", "Unsaved changes"));

		if (Choice == EAppReturnType::Cancel || (Choice == EAppReturnType::Yes && !SaveActive(true)))
		{
			// The click already moved the list's highlight; put it back where the edits are.
			SyncSelectionWidgets();
			return;
		}
		if (Choice == EAppReturnType::No)
		{
			Project.Active->Revert();
		}
	}

	Project.Active = NewSelection;
	SyncSelectionWidgets();
}

void SCPM_PakManagerPanel::SyncSelectionWidgets()
{
	if (ListPanel.IsValid())
	{
		ListPanel->SetSelection(Project.Active);
	}
	if (ChunkCombo.IsValid())
	{
		ChunkCombo->SetSelectedItem(Project.Active);
	}
	if (DetailPanel.IsValid())
	{
		DetailPanel->SetAssetViewModel(Project.Active);
	}
}

bool SCPM_PakManagerPanel::SaveActive(bool bNotifyOnSuccess)
{
	TSharedPtr<FCPM_AssetViewModel> Active = Project.Active;
	if (!Active.IsValid() || !Active->IsDirty())
	{
		return true;
	}

	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Active->Save(*Subsystem))
	{
		Notify(LOCTEXT("SaveFailed", "Could not save the changes."), SNotificationItem::CS_Fail);
		return false;
	}

	if (bNotifyOnSuccess)
	{
		// Local-first record (design D19): a published Asset hears about the edit on the next Publish.
		Notify(Active->AssetId.IsEmpty()
			? LOCTEXT("Saved", "Changes saved.")
			: LOCTEXT("SavedLocal", "Saved locally - uploads on next publish."),
			SNotificationItem::CS_Success);
	}
	return true;
}

void SCPM_PakManagerPanel::HandleChunkStatusChanged(const FCPM_ChunkStatus& Status)
{
	TSharedPtr<FCPM_AssetViewModel> AssetVM = Project.FindByChunkId(Status.ChunkId);
	if (!AssetVM.IsValid())
	{
		return;
	}

	const bool bWasBusy = AssetVM->Status.IsBusy();
	AssetVM->Status = Status;

	bool bTerminal = false;
	FText Message;
	SNotificationItem::ECompletionState State = SNotificationItem::CS_None;
	switch (Status.Status)
	{
	case ECPM_AssetManagerStatus::UploadPak_Success:
		bTerminal = true;
		State = SNotificationItem::CS_Success;
		Message = FText::Format(LOCTEXT("PublishSucceeded", "Published \"{0}\"."), DisplayNameOf(AssetVM));
		break;

	case ECPM_AssetManagerStatus::Delete_Success:
		bTerminal = true;
		State = SNotificationItem::CS_Success;
		Message = FText::Format(LOCTEXT("DeleteSucceeded", "Deleted \"{0}\". The local files were kept."), DisplayNameOf(AssetVM));
		break;

	case ECPM_AssetManagerStatus::Publish_Cancelled:
		bTerminal = true;
		Message = LOCTEXT("PublishCancelled", "Publish cancelled.");
		break;

	case ECPM_AssetManagerStatus::Packaging_Failed:
	case ECPM_AssetManagerStatus::Create_Failed:
	case ECPM_AssetManagerStatus::Update_Failed:
	case ECPM_AssetManagerStatus::UploadPak_Failed:
	case ECPM_AssetManagerStatus::Delete_Failed:
		bTerminal = true;
		State = SNotificationItem::CS_Fail;
		Message = Status.Message.IsEmpty()
			? StaticEnum<ECPM_AssetManagerStatus>()->GetDisplayNameTextByValue(static_cast<int64>(Status.Status))
			: FText::FromString(Status.Message);
		break;

	default:
		break;
	}

	if (bTerminal)
	{
		// A finished Publish or delete is what changes AssetId, Paks and history - re-read them now
		// rather than on every progress tick. LoadFrom keeps in-flight edits.
		if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
		{
			AssetVM->LoadFrom(*Subsystem);
		}

		// A refusal that never went busy already answered at the click site (or inline, for the
		// Entry Point pick); toasting it again would say everything twice.
		if (bWasBusy)
		{
			Notify(Message, State);
		}
	}

	if (AssetVM == Project.Active && DetailPanel.IsValid())
	{
		DetailPanel->OnActiveStatusChanged();
	}
}

FReply SCPM_PakManagerPanel::HandleSaveClicked()
{
	SaveActive(true);
	return FReply::Handled();
}

FReply SCPM_PakManagerPanel::HandlePrimaryClicked()
{
	TSharedPtr<FCPM_AssetViewModel> Active = Project.Active;
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Active.IsValid() || !Subsystem)
	{
		return FReply::Handled();
	}

	// Create/Publish auto-saves dirty fields first (design D6); a failed save stops the publish.
	if (!SaveActive(false))
	{
		return FReply::Handled();
	}

	if (!Subsystem->Publish(Active->ChunkId))
	{
		const FString Why = Subsystem->GetChunkStatus(Active->ChunkId).Message;
		Notify(Why.IsEmpty()
			? LOCTEXT("PublishRefused", "The publish was not accepted.")
			: FText::FromString(Why),
			SNotificationItem::CS_Fail);
	}
	return FReply::Handled();
}

FReply SCPM_PakManagerPanel::HandleDeleteClicked()
{
	if (MoreButton.IsValid())
	{
		MoreButton->SetIsOpen(false);
	}

	TSharedPtr<FCPM_AssetViewModel> Active = Project.Active;
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Active.IsValid() || !Subsystem)
	{
		return FReply::Handled();
	}

	const FText Body = FText::Format(LOCTEXT("DeleteAssetBody",
		"Delete \"{0}\"?\n\nThis removes the published Convai asset and its versions. The local project files and source package remain unchanged."),
		DisplayNameOf(Active));

	// Default answer is No: Enter on a destructive dialog must not destroy.
	if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No, Body, LOCTEXT("DeleteAssetTitle", "Delete asset")) == EAppReturnType::Yes)
	{
		// Empty Version deletes the whole Asset rather than one of its Versions.
		if (!Subsystem->DeleteAsset(Active->ChunkId, FString()))
		{
			const FString Why = Subsystem->GetChunkStatus(Active->ChunkId).Message;
			Notify(Why.IsEmpty()
				? LOCTEXT("DeleteRefused", "The delete was not accepted.")
				: FText::FromString(Why),
				SNotificationItem::CS_Fail);
		}
	}
	return FReply::Handled();
}

bool SCPM_PakManagerPanel::IsOtherChunkPublishing() const
{
	if (!Project.AnyPublishInFlight())
	{
		return false;
	}
	return !(Project.Active.IsValid() && Project.Active->Status.IsBusy());
}

bool SCPM_PakManagerPanel::ShouldShowList() const
{
	return Project.Assets.Num() > 1 && GetTickSpaceGeometry().GetLocalSize().X >= NarrowDockWidth;
}

FText SCPM_PakManagerPanel::GetPrimaryButtonText() const
{
	const TSharedPtr<FCPM_AssetViewModel>& Active = Project.Active;
	if (Active.IsValid() && Active->Status.IsBusy())
	{
		// A delete is busy too, and the button must not claim a publish that is not happening.
		return Active->Status.Status == ECPM_AssetManagerStatus::Delete_Begin
			? LOCTEXT("Deleting", "Deleting...")
			: LOCTEXT("Publishing", "Publishing...");
	}
	// Create is the first Publish (design D16); only the label switches.
	return !Active.IsValid() || Active->AssetId.IsEmpty()
		? LOCTEXT("CreateAsset", "Create asset")
		: LOCTEXT("PublishUpdate", "Publish update");
}

bool SCPM_PakManagerPanel::CanClickPrimary() const
{
	return Project.Active.IsValid()
		&& Project.Active->CanCreateOrPublish()
		&& !IsOtherChunkPublishing();
}

FText SCPM_PakManagerPanel::GetActionBarSummary() const
{
	const TSharedPtr<FCPM_AssetViewModel>& Active = Project.Active;
	if (!Active.IsValid())
	{
		return FText::GetEmpty();
	}

	if (Active->Status.IsBusy())
	{
		return FText::Format(Active->Status.Status == ECPM_AssetManagerStatus::Delete_Begin
			? LOCTEXT("ActiveDeletingHint", "Deleting {0}...")
			: LOCTEXT("ActiveBusyHint", "Publishing {0}..."), DisplayNameOf(Active));
	}
	if (IsOtherChunkPublishing())
	{
		return FText::Format(LOCTEXT("OtherBusyHint", "Publishing {0}... other assets can publish when it finishes."),
			Project.PublishingAssetName());
	}

	const TArray<FText> Messages = Active->ValidationMessages();
	return Messages.IsEmpty() ? FText::GetEmpty() : FText::Join(LOCTEXT("ValidationDelim", " "), Messages);
}

FSlateColor SCPM_PakManagerPanel::GetActionBarSummaryColor() const
{
	using FPalette = FCPM_PakManagerStyle::FPalette;
	const TSharedPtr<FCPM_AssetViewModel>& Active = Project.Active;
	const bool bValidationShown = Active.IsValid() && !Active->Status.IsBusy()
		&& !IsOtherChunkPublishing() && !Active->ValidationMessages().IsEmpty();
	return FSlateColor(bValidationShown ? FPalette::Warning : FPalette::TextSecondary);
}

#undef LOCTEXT_NAMESPACE
