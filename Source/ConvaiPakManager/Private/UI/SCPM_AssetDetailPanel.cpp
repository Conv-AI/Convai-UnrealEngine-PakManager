// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/SCPM_AssetDetailPanel.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CPM_PakManagerSettings.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SComboButton.h"
#include "IContentBrowserSingleton.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UI/CPM_PakManagerStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCPM_AssetDetailPanel"

namespace
{
	const FTextBlockStyle& SecondaryTextStyle()
	{
		return FCPM_PakManagerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CPM.Text.Secondary");
	}

	const FButtonStyle& SecondaryButtonStyle()
	{
		return FCPM_PakManagerStyle::Get().GetWidgetStyle<FButtonStyle>("CPM.Button.Secondary");
	}

	void Notify(const FText& Message, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 4.0f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	TSharedRef<SWidget> Row(const FText& Label, TSharedRef<SWidget> Content)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(120.0f)
				[
					SNew(STextBlock).TextStyle(&SecondaryTextStyle()).Text(Label)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				Content
			];
	}

	FText RelativeTimeText(const FDateTime& Time)
	{
		const FTimespan Age = FDateTime::UtcNow() - Time;
		if (Age < FTimespan::FromMinutes(1))
		{
			return LOCTEXT("JustNow", "just now");
		}
		if (Age < FTimespan::FromHours(1))
		{
			return FText::Format(LOCTEXT("MinutesAgo", "{0} min ago"), FText::AsNumber(FMath::FloorToInt(Age.GetTotalMinutes())));
		}
		if (Age < FTimespan::FromDays(1))
		{
			return FText::Format(LOCTEXT("HoursAgo", "{0} h ago"), FText::AsNumber(FMath::FloorToInt(Age.GetTotalHours())));
		}
		return FText::Format(LOCTEXT("DaysAgo", "{0} d ago"), FText::AsNumber(FMath::FloorToInt(Age.GetTotalDays())));
	}

	/**
	 * What turning off the project upload gives up, asked before it does.
	 *
	 * A dialog rather than a tooltip because the cost is entirely in the future - Convai repackaging
	 * this Asset for an engine version that does not exist yet - so nothing the creator does next
	 * would reveal it. They may still say yes; this only makes it a decision rather than a click.
	 *
	 * Returns true when the creator confirms skipping it.
	 */
	bool ConfirmSkippingRawArchive()
	{
		bool bConfirmed = false;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("SkipArchiveTitle", "Don't upload your project?"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMaximize(false)
			.SupportsMinimize(false);

		Window->SetContent(
			SNew(SBox)
			.Padding(16.0f)
			.MaxDesiredWidth(460.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("SkipArchiveBody",
						"Convai keeps a copy of your project so that when Unreal Engine moves to a new version, "
						"we can repackage your asset for it - you do nothing.\n\nWithout that copy, this asset "
						"stops working on a new engine version until you publish it again yourself.\n\nYour paks "
						"still upload as usual. You can turn this back on at any time."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f).HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FButtonStyle>("CPM.Button.Primary"))
						.Text(LOCTEXT("SkipArchiveKeep", "Keep uploading"))
						.OnClicked_Lambda([Window]
						{
							Window->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&SecondaryButtonStyle())
						.Text(LOCTEXT("SkipArchiveSkip", "Don't upload"))
						.OnClicked_Lambda([Window, &bConfirmed]
						{
							bConfirmed = true;
							Window->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]);

		FSlateApplication::Get().AddModalWindow(Window, nullptr);
		return bConfirmed;
	}

	FText PlatformText(ECPM_Platform Platform)
	{
		switch (Platform)
		{
		case ECPM_Platform::Windows:
			return LOCTEXT("WindowsPlatform", "Windows");
		case ECPM_Platform::Linux:
			return LOCTEXT("LinuxPlatform", "Linux");
		default:
			return LOCTEXT("OtherPlatform", "Other");
		}
	}
}

void SCPM_AssetDetailPanel::Construct(const FArguments& InArgs)
{
	LoadShowAllPlatforms();

	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		bIsScene = Subsystem->GetAssetType() != ECPM_AssetType::Avatar;
		SpawnStatus = Subsystem->GetSpawnPointStatus();
	}

	ChildSlot
	[
		SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([this] { return Asset.IsValid() ? 0 : 1; })
		+ SWidgetSwitcher::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 12.0f, 12.0f, 0.0f)
			[
				BuildProgressPanel()
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SScrollBox)
				.IsEnabled_Lambda([this] { return !IsBusy(); })
				+ SScrollBox::Slot().Padding(12.0f, 8.0f, 12.0f, 0.0f)
				[
					BuildIdentitySection()
				]
				+ SScrollBox::Slot().Padding(12.0f, 8.0f, 12.0f, 0.0f)
				[
					BuildContentSourceSection()
				]
				+ SScrollBox::Slot().Padding(12.0f, 8.0f, 12.0f, 0.0f)
				[
					BuildPreviewSection()
				]
				+ SScrollBox::Slot().Padding(12.0f, 8.0f, 12.0f, 0.0f)
				[
					BuildUploadSection()
				]
				+ SScrollBox::Slot().Padding(12.0f, 8.0f, 12.0f, 12.0f)
				[
					BuildTechnicalSection()
				]
			]
		]
		+ SWidgetSwitcher::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MaxDesiredWidth(420.0f)
			[
				SNew(STextBlock)
				.TextStyle(&SecondaryTextStyle())
				.AutoWrapText(true)
				.Justification(ETextJustify::Center)
				.Text(LOCTEXT("NoChunks", "This project has no Chunks. Add a Primary Asset Label to the content you want to publish."))
			]
		]
	];

	if (bIsScene)
	{
		// Polled: nothing broadcasts when a creator moves or deletes tagged actors in the level.
		RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SCPM_AssetDetailPanel::RefreshSpawnStatus));
	}

	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		// The rows are the Policy's platforms, so the panel cannot draw them until one is read.
		// Asked for here, once, and never from a paint path - the read is over the network.
		Subsystem->OnPolicyChanged.AddSP(this, &SCPM_AssetDetailPanel::OnPolicyChanged);
		Subsystem->RefreshPolicy();
	}
}

void SCPM_AssetDetailPanel::LoadShowAllPlatforms()
{
	GConfig->GetBool(TEXT("ConvaiPakManager"), TEXT("bShowAllPlatforms"), bShowAllPlatforms, GEditorPerProjectIni);
}

void SCPM_AssetDetailPanel::SetShowAllPlatforms(const bool bShow)
{
	bShowAllPlatforms = bShow;
	GConfig->SetBool(TEXT("ConvaiPakManager"), TEXT("bShowAllPlatforms"), bShow, GEditorPerProjectIni);
	RebuildUploadRows();
}

FCPM_PublishOptions SCPM_AssetDetailPanel::BuildPublishOptions(const bool bReuseExistingPaks) const
{
	if (!Asset.IsValid())
	{
		FCPM_PublishOptions Options;
		Options.bReuseExistingPaks = bReuseExistingPaks;
		return Options;
	}
	return Asset->PublishOptions(PolicyPlatforms(), bReuseExistingPaks);
}

bool SCPM_AssetDetailPanel::HasAnyBuiltPak() const
{
	if (!Asset.IsValid())
	{
		return false;
	}
	return Asset->PakStatuses.ContainsByPredicate(
		[this](const FCPM_PakPlatformStatus& Status)
		{
			// Only a Pak this run would actually send counts: reusing one for a platform the
			// Selection excludes changes nothing about what gets published.
			return Status.bExists && Asset->SelectedPlatforms.Contains(Status.Platform);
		});
}

void SCPM_AssetDetailPanel::OnPolicyChanged()
{
	if (Asset.IsValid())
	{
		Asset->SeedPlatformSelection(PolicyPlatforms());
	}

	// The read typically lands a few hundred ms after the panel opens, and nothing else would
	// repaint: the row set is derived from the Policy, not from anything the view model watches.
	RebuildUploadRows();
}

void SCPM_AssetDetailPanel::SetAssetViewModel(TSharedPtr<FCPM_AssetViewModel> InAsset)
{
	const bool bSameChunk = Asset.IsValid() && InAsset.IsValid() && Asset->ChunkId == InAsset->ChunkId;

	Asset = InAsset;
	EntryPointError = FText::GetEmpty();

	RebuildStageRow();
	BuiltRowSignature.Reset();
	RebuildUploadRows();
	RefreshThumbnailBrush(true);

	// Only a real switch resets what the creator expanded; a tab-foreground refresh keeps it.
	if (!bSameChunk)
	{
		if (UploadArea.IsValid())
		{
			// Paks cannot exist before the first Publish, so the section starts out of the way.
			UploadArea->SetExpanded(Asset.IsValid() && !Asset->AssetId.IsEmpty());
		}
		if (TechnicalArea.IsValid())
		{
			TechnicalArea->SetExpanded(false);
		}
	}
}

void SCPM_AssetDetailPanel::OnActiveStatusChanged()
{
	const TArray<FString> Steps = Asset.IsValid() ? Asset->Status.PlannedSteps : TArray<FString>();
	if (Steps != BuiltStageSteps)
	{
		RebuildStageRow();
	}
	RebuildUploadRows();
	RefreshThumbnailBrush(false);
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildProgressPanel()
{
	return SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Panel"))
		.Padding(12.0f)
		.Visibility_Lambda([this] { return IsBusy() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([this]
				{
					if (!Asset.IsValid())
					{
						return FText::GetEmpty();
					}
					const FCPM_ChunkStatus& Status = Asset->Status;
					FText Step;
					if (Status.PlannedSteps.IsValidIndex(Status.CurrentStepIndex))
					{
						Step = FText::FromString(Status.PlannedSteps[Status.CurrentStepIndex]);
					}
					else if (!Status.StepName.IsEmpty())
					{
						Step = FText::FromString(Status.StepName);
					}
					else
					{
						Step = StaticEnum<ECPM_AssetManagerStatus>()->GetDisplayNameTextByValue(static_cast<int64>(Status.Status));
					}
					return FText::Format(LOCTEXT("ProgressLine", "{0} - {1}%"),
						Step, FText::AsNumber(FMath::RoundToInt(Status.Progress * 100.0f)));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
			[
				SNew(SProgressBar)
				.Percent_Lambda([this]
				{
					return Asset.IsValid() ? TOptional<float>(Asset->Status.Progress) : TOptional<float>();
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(StageRow, SWrapBox)
				.UseAllottedSize(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&SecondaryButtonStyle())
					.Text(LOCTEXT("CancelPublish", "Cancel"))
					.Visibility_Lambda([this]
					{
						// A delete has no cancel Command; only a Publish does.
						return Asset.IsValid() && Asset->Status.Status == ECPM_AssetManagerStatus::Delete_Begin
							? EVisibility::Collapsed : EVisibility::Visible;
					})
					.OnClicked(this, &SCPM_AssetDetailPanel::HandleCancelPublish)
				]
			]
		];
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildIdentitySection()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("IdentitySection", "Identity & metadata"))
		.InitiallyCollapsed(false)
		.Padding(FMargin(12.0f, 8.0f))
		.BodyContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
			[
				Row(LOCTEXT("AssetNameLabel", "Asset name"),
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("AssetNameHint", "Name shown in Convai"))
					.Text_Lambda([this]
					{
						return Asset.IsValid() ? FText::FromString(Asset->Name) : FText::GetEmpty();
					})
					.OnTextChanged_Lambda([this](const FText& Text)
					{
						if (Asset.IsValid())
						{
							Asset->Name = Text.ToString();
						}
					}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
			[
				Row(LOCTEXT("DescriptionLabel", "Description"),
					SNew(SBox)
					.MinDesiredHeight(64.0f)
					[
						SNew(SMultiLineEditableTextBox)
						.AutoWrapText(true)
						.HintText(LOCTEXT("DescriptionHint", "What this asset is"))
						.Text_Lambda([this]
						{
							return Asset.IsValid() ? FText::FromString(Asset->Description) : FText::GetEmpty();
						})
						.OnTextChanged_Lambda([this](const FText& Text)
						{
							if (Asset.IsValid())
							{
								Asset->Description = Text.ToString();
							}
						})
					])
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
			[
				Row(LOCTEXT("AssetTypeLabel", "Asset type"),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBorder)
						.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Hover"))
						.Padding(FMargin(8.0f, 3.0f))
						[
							SNew(STextBlock)
							.TextStyle(&SecondaryTextStyle())
							.Text(bIsScene
								? LOCTEXT("SceneChip", "Scene (fixed by project)")
								: LOCTEXT("AvatarChip", "Avatar (fixed by project)"))
						]
					])
			]
		];
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildContentSourceSection()
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("ContentSourceSection", "Content"))
		.InitiallyCollapsed(false)
		.Padding(FMargin(12.0f, 8.0f))
		.BodyContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
			[
				Row(LOCTEXT("EntryPointLabel", "Selected asset"),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]
						{
							return Asset.IsValid() && !Asset->EntryPoint.IsEmpty()
								? FText::FromString(Asset->EntryPoint)
								: LOCTEXT("NoEntryPoint", "Not picked yet");
						})
						.ColorAndOpacity_Lambda([this]
						{
							return FSlateColor(Asset.IsValid() && !Asset->EntryPoint.IsEmpty()
								? FPalette::TextPrimary : FPalette::TextSecondary);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&SecondaryButtonStyle())
						.Text(LOCTEXT("UseSelectedAsset", "Use selected asset"))
						.ToolTipText(LOCTEXT("UseSelectedAssetTip",
							"Records the Content Browser selection as this Chunk's Entry Point - the level for a Scene, the blueprint for an Avatar."))
						.OnClicked(this, &SCPM_AssetDetailPanel::HandleUseSelectedAsset)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FAppStyle::Get(), "SimpleButton")
						.ToolTipText(LOCTEXT("RevealEntryPointTip", "Find the Entry Point in the Content Browser"))
						.IsEnabled_Lambda([this] { return Asset.IsValid() && !Asset->EntryPoint.IsEmpty(); })
						.OnClicked(this, &SCPM_AssetDetailPanel::HandleRevealEntryPoint)
						[
							SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.BrowseContent"))
						]
					])
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(120.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this] { return EntryPointError; })
				.ColorAndOpacity(FSlateColor(FPalette::Error))
				.AutoWrapText(true)
				.Visibility_Lambda([this]
				{
					return EntryPointError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
				})
			]
		];
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildPreviewSection()
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.ThumbnailFrame"))
			.Padding(2.0f)
			[
				SNew(SBox)
				.WidthOverride(192.0f)
				.HeightOverride(108.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(ThumbnailImage, SImage)
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(&SecondaryTextStyle())
						.Text(LOCTEXT("NoThumbnail", "No thumbnail yet"))
						.Visibility_Lambda([this]
						{
							return Asset.IsValid() && Asset->bThumbnailExists
								? EVisibility::Collapsed : EVisibility::Visible;
						})
					]
				]
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SButton)
				.ButtonStyle(&SecondaryButtonStyle())
				.Text(LOCTEXT("CaptureThumbnail", "Capture thumbnail"))
				.ToolTipText(LOCTEXT("CaptureThumbnailTip", "Captures the active viewport as this asset's thumbnail."))
				.OnClicked(this, &SCPM_AssetDetailPanel::HandleCaptureThumbnail)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SButton)
				.ButtonStyle(&SecondaryButtonStyle())
				.Text(LOCTEXT("PreviewThumbnail", "Preview thumbnail"))
				.ToolTipText(LOCTEXT("PreviewThumbnailTip", "Opens the exact image that uploads on the next publish."))
				.IsEnabled_Lambda([this] { return Asset.IsValid() && Asset->bThumbnailExists; })
				.OnClicked(this, &SCPM_AssetDetailPanel::HandlePreviewThumbnail)
			]
		]
	];

	if (bIsScene)
	{
		Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.TextStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CPM.Text.Section"))
				.Text(LOCTEXT("SpawnPointHeader", "Spawn point"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]
				{
					return SpawnStatus.Count > 1 ? EVisibility::Visible : EVisibility::Collapsed;
				})
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
					.ColorAndOpacity(FSlateColor(FPalette::Warning))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor(FPalette::Warning))
					.Text(LOCTEXT("SpawnPointWarning",
						"More than one spawn point is tagged in this level. Delete the extras - moving one of many would silently change which point wins."))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&SecondaryTextStyle())
					.Text_Lambda([this]
					{
						if (SpawnStatus.Count == 0)
						{
							return LOCTEXT("NoSpawnPoint", "No spawn point in this level yet.");
						}
						if (SpawnStatus.Count == 1)
						{
							const FVector Location = SpawnStatus.Transform.GetLocation();
							return FText::Format(LOCTEXT("SpawnPointSummary", "X {0}  Y {1}  Z {2}  -  Yaw {3}"),
								FText::AsNumber(FMath::RoundToInt(Location.X)),
								FText::AsNumber(FMath::RoundToInt(Location.Y)),
								FText::AsNumber(FMath::RoundToInt(Location.Z)),
								FText::AsNumber(FMath::RoundToInt(SpawnStatus.Transform.Rotator().Yaw)));
						}
						return FText::Format(LOCTEXT("SpawnPointCount", "{0} spawn points found."), FText::AsNumber(SpawnStatus.Count));
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&SecondaryButtonStyle())
					.Text_Lambda([this]
					{
						// Adaptive per design D11: place one when none exists, otherwise re-snap it.
						return SpawnStatus.Count == 0
							? LOCTEXT("AddSpawnPoint", "Add spawn point")
							: LOCTEXT("SetSpawnPoint", "Set from viewport");
					})
					.IsEnabled_Lambda([this] { return SpawnStatus.Count <= 1; })
					.OnClicked(this, &SCPM_AssetDetailPanel::HandleSetSpawnPoint)
				]
			]
		];
	}

	return SNew(SExpandableArea)
		.AreaTitle(bIsScene
			? LOCTEXT("PreviewSpawnSection", "Preview & spawn point")
			: LOCTEXT("PreviewSection", "Preview"))
		.InitiallyCollapsed(false)
		.Padding(FMargin(12.0f, 8.0f))
		.BodyContent()
		[
			Body
		];
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildUploadSection()
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	return SAssignNew(UploadArea, SExpandableArea)
		// "Upload", not "Packaging": the project source is one of these rows now, and it is never
		// packaged. Every row here says what Convai holds.
		.AreaTitle(LOCTEXT("UploadSection", "Upload"))
		.InitiallyCollapsed(true)
		.Padding(FMargin(12.0f, 8.0f))
		.BodyContent()
		[
			SNew(SVerticalBox)
			// A sibling of the platform rows, never a child: RebuildUploadRows clears that box.
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(UploadRows, SVerticalBox)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					// The platform rows' label column, so the archive reads as one more of them.
					.WidthOverride(90.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("RawArchiveLabel", "Project source"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SImage)
					.Image_Lambda([this]
					{
						return FAppStyle::Get().GetBrush(
							Asset.IsValid() && Asset->HasPublishedRawArchive() ? "Icons.Check" : "Icons.Warning");
					})
					.ColorAndOpacity_Lambda([this]
					{
						return FSlateColor(Asset.IsValid() && Asset->HasPublishedRawArchive()
							? FPalette::GreenPrimary : FPalette::Warning);
					})
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						// Said before anything about uploads: with the Policy asking for no archive,
						// the checkbox beside this cannot cause one, and a row that only said "not
						// uploaded yet" would leave the creator waiting for a publish to fix it.
						if (UCPM_PakManagerSettings::Get().PolicyExcludesRawArchive())
						{
							return LOCTEXT("RawArchiveNotInPolicy", "Not part of this project's publish policy");
						}
						if (!Asset.IsValid() || !Asset->HasPublishedRawArchive())
						{
							// Not "sent by the next publish": whether one is sent at all is the
							// Publish Policy's to say, and this promises nothing on its behalf.
							return LOCTEXT("RawArchiveMissing", "Not uploaded yet");
						}
						return FText::Format(
							LOCTEXT("RawArchiveUploaded", "Uploaded {0}"), RelativeTimeText(Asset->RawArchiveUploadTime));
					})
					.ColorAndOpacity_Lambda([this]
					{
						return FSlateColor(Asset.IsValid() && Asset->HasPublishedRawArchive()
							? FPalette::GreenPrimary : FPalette::Warning);
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([]
					{
						return UCPM_PakManagerSettings::Get().bUploadRawProjectArchive
							? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([](ECheckBoxState State)
					{
						const bool bUpload = State == ECheckBoxState::Checked;

						// Only turning it OFF is explained, and only before it takes effect: this is
						// the choice that costs the creator something later, and the cost is
						// invisible until an engine version they have not heard of yet ships.
						if (!bUpload && !ConfirmSkippingRawArchive())
						{
							return;
						}

						UCPM_PakManagerSettings* Settings = GetMutableDefault<UCPM_PakManagerSettings>();
						Settings->bUploadRawProjectArchive = bUpload;
						if (!Settings->TryUpdateDefaultConfigFile())
						{
							// The session honours it either way; said out loud because the change
							// silently coming back after a restart is the confusing half.
							Notify(LOCTEXT("UploadArchiveNotSaved",
								"Could not write DefaultGame.ini - this applies to the current session only."),
								SNotificationItem::CS_Fail);
						}
					})
					.ToolTipText(LOCTEXT("UploadArchiveTip",
						"Sends your project alongside the paks, so Convai can repackage this asset for future "
						"Unreal Engine versions without you republishing it. It is the longest step of a publish; "
						"turn it off while iterating. Applies to every asset in this project."))
					[
						SNew(STextBlock).Text(LOCTEXT("UploadArchive", "Include"))
					]
				]
			]
		];
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildTechnicalSection()
{
	return SAssignNew(TechnicalArea, SExpandableArea)
		.AreaTitle(LOCTEXT("TechnicalSection", "Technical details"))
		.InitiallyCollapsed(true)
		.Padding(FMargin(12.0f, 8.0f))
		.Visibility_Lambda([this]
		{
			return Asset.IsValid() && !Asset->AssetId.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.BodyContent()
		[
			Row(LOCTEXT("AssetIdLabel", "Asset ID"),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SEditableTextBox)
					.IsReadOnly(true)
					.Text_Lambda([this]
					{
						return Asset.IsValid() ? FText::FromString(Asset->AssetId) : FText::GetEmpty();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&SecondaryButtonStyle())
					.Text(LOCTEXT("CopyAssetId", "Copy"))
					.OnClicked(this, &SCPM_AssetDetailPanel::HandleCopyAssetId)
				])
		];
}

UConvaiPakEditorSubsystem* SCPM_AssetDetailPanel::GetSubsystem()
{
	return GEditor ? GEditor->GetEditorSubsystem<UConvaiPakEditorSubsystem>() : nullptr;
}

FReply SCPM_AssetDetailPanel::HandleUseSelectedAsset()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Asset.IsValid())
	{
		return FReply::Handled();
	}

	if (Subsystem->PickEntryPointFromSelection(Asset->ChunkId))
	{
		EntryPointError = FText::GetEmpty();
		Asset->LoadFrom(*Subsystem);
	}
	else
	{
		// The refusal wrote its reason to the status; the previous valid Entry Point survives.
		const FString Message = Subsystem->GetChunkStatus(Asset->ChunkId).Message;
		EntryPointError = Message.IsEmpty()
			? LOCTEXT("EntryPointRefused",
				"The selection does not match this project's Asset Type - a Scene needs a level, an Avatar a blueprint. The previous Entry Point is kept.")
			: FText::FromString(Message);
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleRevealEntryPoint()
{
	if (!Asset.IsValid() || Asset->EntryPoint.IsEmpty())
	{
		return FReply::Handled();
	}

	const FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Found;
	AssetRegistry.Get().GetAssetsByPackageName(FName(*Asset->EntryPoint), Found);
	if (Found.Num() > 0)
	{
		FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowser.Get().SyncBrowserToAssets(Found);
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleCaptureThumbnail()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Asset.IsValid())
	{
		return FReply::Handled();
	}

	if (Subsystem->CaptureThumbnail(Asset->ChunkId))
	{
		Asset->LoadFrom(*Subsystem);
		RefreshThumbnailBrush(true);
	}
	else
	{
		Notify(LOCTEXT("CaptureFailed", "Could not capture a thumbnail from the viewport."), SNotificationItem::CS_Fail);
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandlePreviewThumbnail()
{
	if (!Asset.IsValid() || Asset->ThumbnailPath.IsEmpty())
	{
		return FReply::Handled();
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Asset->ThumbnailPath))
	{
		Notify(LOCTEXT("PreviewLoadFailed", "Could not read the thumbnail file."), SNotificationItem::CS_Fail);
		return FReply::Handled();
	}

	FVector2D ImageSize(1280.0, 720.0);
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	const TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (Wrapper.IsValid() && Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
	{
		ImageSize = FVector2D(Wrapper->GetWidth(), Wrapper->GetHeight());
	}

	const double Scale = FMath::Min3(1.0, 1280.0 / ImageSize.X, 720.0 / ImageSize.Y);

	// The window paints the panel's brush through a weak guard rather than owning one of its own: two
	// dynamic brushes on one file share the texture, so a second would keep a recapture from being
	// re-read from disk - and a brush held by this panel could be freed under a window that outlives it.
	TWeakPtr<SCPM_AssetDetailPanel> WeakSelf = SharedThis(this);
	FSlateApplication::Get().AddWindow(
		SNew(SWindow)
		.Title(LOCTEXT("PreviewWindowTitle", "Thumbnail"))
		.ClientSize(ImageSize * Scale)
		.SupportsMaximize(false)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SImage)
				.Image_Lambda([WeakSelf]() -> const FSlateBrush*
				{
					const TSharedPtr<SCPM_AssetDetailPanel> Self = WeakSelf.Pin();
					return Self.IsValid() ? Self->ThumbnailBrush.Get() : nullptr;
				})
			]
		]);

	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleSetSpawnPoint()
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->SetSpawnPointFromViewport();
		SpawnStatus = Subsystem->GetSpawnPointStatus();
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleCopyAssetId()
{
	if (Asset.IsValid() && !Asset->AssetId.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Asset->AssetId);
		Notify(LOCTEXT("AssetIdCopied", "Asset ID copied."), SNotificationItem::CS_Success);
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleCancelPublish()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (Subsystem && Asset.IsValid())
	{
		Subsystem->CancelPublish(Asset->ChunkId);
	}
	return FReply::Handled();
}

void SCPM_AssetDetailPanel::RefreshThumbnailBrush(bool bForceReload)
{
	const FString Path = (Asset.IsValid() && Asset->bThumbnailExists) ? Asset->ThumbnailPath : FString();
	const bool bLoaded = ThumbnailBrush.IsValid();
	if (!bForceReload && bLoaded == !Path.IsEmpty() && (!bLoaded || ThumbnailBrush->GetResourceName() == FName(*Path)))
	{
		return;
	}

	// Destroy first: the destructor releases the renderer resource, so a recapture at the same path
	// is re-read from disk instead of served from the texture cache.
	ThumbnailBrush.Reset();
	if (!Path.IsEmpty())
	{
		ThumbnailBrush = MakeShareable(new FSlateDynamicImageBrush(FName(*Path), FVector2D(192.0, 108.0)));
	}
	if (ThumbnailImage.IsValid())
	{
		ThumbnailImage->SetImage(ThumbnailBrush.Get());
	}
}

void SCPM_AssetDetailPanel::RebuildStageRow()
{
	if (!StageRow.IsValid())
	{
		return;
	}

	StageRow->ClearChildren();
	BuiltStageSteps = Asset.IsValid() ? Asset->Status.PlannedSteps : TArray<FString>();

	using FPalette = FCPM_PakManagerStyle::FPalette;
	for (int32 Index = 0; Index < BuiltStageSteps.Num(); ++Index)
	{
		if (Index > 0)
		{
			StageRow->AddSlot().Padding(6.0f, 0.0f)
			[
				SNew(STextBlock)
				.TextStyle(&SecondaryTextStyle())
				.Text(FText::FromString(TEXT("\u2192")))
			];
		}

		StageRow->AddSlot()
		[
			SNew(STextBlock)
			.Text(FText::FromString(BuiltStageSteps[Index]))
			.ColorAndOpacity_Lambda([this, Index]
			{
				const int32 Current = Asset.IsValid() ? Asset->Status.CurrentStepIndex : INDEX_NONE;
				if (Current != INDEX_NONE && Index < Current)
				{
					return FSlateColor(FPalette::GreenBright);
				}
				if (Index == Current)
				{
					return FSlateColor(FPalette::GreenPrimary);
				}
				return FSlateColor(FPalette::TextSecondary);
			})
			.Font_Lambda([this, Index]
			{
				const int32 Current = Asset.IsValid() ? Asset->Status.CurrentStepIndex : INDEX_NONE;
				return FCoreStyle::GetDefaultFontStyle(Index == Current ? "Bold" : "Regular", 9);
			})
		];
	}
}

TArray<ECPM_Platform> SCPM_AssetDetailPanel::PolicyPlatforms() const
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}

	FCPM_PublishPolicy Policy;
	FDateTime ReadAt;
	ECPM_PolicyReadState State = ECPM_PolicyReadState::Unread;
	if (!Subsystem->GetPublishPolicy(Policy, ReadAt, State))
	{
		return {};
	}
	return Policy.PlatformsToPackage();
}

TArray<ECPM_Platform> SCPM_AssetDetailPanel::VisiblePlatforms() const
{
	const TArray<ECPM_Platform> Every = { ECPM_Platform::Windows, ECPM_Platform::Linux };

	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return Every;
	}

	FCPM_PublishPolicy Policy;
	FDateTime ReadAt;
	ECPM_PolicyReadState State = ECPM_PolicyReadState::Unread;
	Subsystem->GetPublishPolicy(Policy, ReadAt, State);

	// Fail OPEN. A platform must never disappear because a GitHub read timed out - the creator may
	// have a Pak on disk, or a Version on Convai, that this is the only route to.
	if (State != ECPM_PolicyReadState::Read || bShowAllPlatforms)
	{
		return Every;
	}

	return Policy.PlatformsToPackage();
}

void SCPM_AssetDetailPanel::RebuildUploadRows()
{
	if (!UploadRows.IsValid())
	{
		return;
	}

	const TArray<ECPM_Platform> Platforms = VisiblePlatforms();

	// Keyed on what is actually rendered, not on a count: PakStatuses stays two entries long while
	// the visible set changes with the Policy and the toggle, so a count would leave a stale row
	// reading another platform's data under this platform's label.
	FString Signature;
	for (const ECPM_Platform Platform : Platforms)
	{
		Signature += PlatformText(Platform).ToString() + TEXT(",");
	}
	if (Signature == BuiltRowSignature)
	{
		return;
	}
	BuiltRowSignature = Signature;
	UploadRows->ClearChildren();

	if (Platforms.IsEmpty())
	{
		UploadRows->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(STextBlock)
			.TextStyle(&SecondaryTextStyle())
			.AutoWrapText(true)
			.Text(LOCTEXT("NoPlatforms", "This project's publish policy asks for no platforms."))
		];
		return;
	}

	for (const ECPM_Platform Platform : Platforms)
	{
		UploadRows->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			BuildPlatformRow(Platform)
		];
	}
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildPlatformRow(const ECPM_Platform Platform)
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	// Captures the PLATFORM, never an index into PakStatuses: the rendered set is a subset in a
	// different order, so an index would read the wrong platform's state.
	auto Pak = [this, Platform]() -> const FCPM_PakPlatformStatus*
	{
		if (!Asset.IsValid())
		{
			return nullptr;
		}
		return Asset->PakStatuses.FindByPredicate(
			[Platform](const FCPM_PakPlatformStatus& Status) { return Status.Platform == Platform; });
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(90.0f)
			[
				SNew(STextBlock).Text(PlatformText(Platform))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SImage)
			.Image_Lambda([Pak]
			{
				const FCPM_PakPlatformStatus* Status = Pak();
				return FAppStyle::Get().GetBrush(Status && Status->bExists ? "Icons.Check" : "Icons.Info");
			})
			.ColorAndOpacity_Lambda([Pak]
			{
				const FCPM_PakPlatformStatus* Status = Pak();
				// Never Warning for a missing Pak. A platform with nothing built yet is the ordinary
				// state before a publish, and alarming about it is exactly what this rework removes.
				return FSlateColor(Status && Status->bExists ? FPalette::GreenPrimary : FPalette::TextSecondary);
			})
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(&SecondaryTextStyle())
			.AutoWrapText(true)
			.Text_Lambda([this, Pak, Platform]
			{
				const FCPM_PakPlatformStatus* Status = Pak();
				if (Status && Status->bExists)
				{
					return FText::Format(
						LOCTEXT("PakBuilt", "Built pak on disk, made {0}"), RelativeTimeText(Status->LastPackagedTime));
				}

				// Says what happens next rather than naming an absence, and only promises a build
				// when this run would actually make one.
				const bool bSelected = Asset.IsValid() && Asset->SelectedPlatforms.Contains(Platform);
				return bSelected
					? LOCTEXT("PakWillBuild", "No pak on this computer. The next publish builds one.")
					: LOCTEXT("PakNotIncluded", "No pak on this computer.");
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this, Platform]
			{
				return Asset.IsValid() && Asset->SelectedPlatforms.Contains(Platform)
					? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.IsEnabled_Lambda([this] { return !IsBusy(); })
			.OnCheckStateChanged_Lambda([this, Platform](ECheckBoxState State)
			{
				if (!Asset.IsValid())
				{
					return;
				}
				if (State == ECheckBoxState::Checked)
				{
					Asset->SelectedPlatforms.Add(Platform);
				}
				else
				{
					Asset->SelectedPlatforms.Remove(Platform);
				}
			})
			.ToolTipText_Lambda([this, Platform]
			{
				// The tooltip carries the whole Platform Selection rule, because this checkbox is
				// the only place a creator meets it.
				return PolicyPlatforms().Contains(Platform)
					? LOCTEXT("PlatformIncludedTip",
						"Build and upload this platform on the next publish. Convai asks this project for it; "
						"unticking sends the others alone.")
					: LOCTEXT("PlatformForcedTip",
						"Build and upload this platform on the next publish, even though Convai does not ask this "
						"project for it. For a project Convai has agreed to host this platform for.");
			})
			[
				SNew(STextBlock).Text(LOCTEXT("IncludePlatform", "Include"))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SComboButton)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.HasDownArrow(false)
			.ToolTipText(FText::Format(LOCTEXT("PlatformMoreTip", "More for {0}"), PlatformText(Platform)))
			.OnGetMenuContent_Lambda([this, Platform] { return BuildPlatformRowMenu(Platform); })
			.ButtonContent()
			[
				SNew(STextBlock).Text(LOCTEXT("RowMoreGlyph", "..."))
			]
		];
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildPlatformRowMenu(const ECPM_Platform Platform)
{
	const int32 ChunkId = Asset.IsValid() ? Asset->ChunkId : INDEX_NONE;
	const FText PlatformName = PlatformText(Platform);

	const FCPM_PakPlatformStatus* Status = Asset.IsValid()
		? Asset->PakStatuses.FindByPredicate(
			[Platform](const FCPM_PakPlatformStatus& S) { return S.Platform == Platform; })
		: nullptr;
	const bool bHasPak = Status && Status->bExists;
	const FString PakPath = Status ? Status->PakPath : FString();
	const bool bPublished = Asset.IsValid() && !Asset->AssetId.IsEmpty();

	FMenuBuilder Menu(/*bShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	Menu.BeginSection(NAME_None, LOCTEXT("OnThisComputer", "On this computer"));
	{
		// This is where the standalone Reveal button went.
		Menu.AddMenuEntry(
			LOCTEXT("ShowPakInExplorer", "Show the built pak in Explorer"),
			bHasPak
				? LOCTEXT("ShowPakTip", "Opens the folder holding this platform's pak.")
				: FText::Format(LOCTEXT("NoPakTip", "No {0} pak on this computer."), PlatformName),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([PakPath] { FPlatformProcess::ExploreFolder(*FPaths::GetPath(PakPath)); }),
				// Deliberately NOT gated on busy: opening a folder contends with nothing, and a
				// twenty-minute publish is exactly when someone wants to look.
				FCanExecuteAction::CreateLambda([bHasPak] { return bHasPak; })));

		Menu.AddMenuEntry(
			LOCTEXT("DeleteBuiltPak", "Delete the built pak"),
			LOCTEXT("DeleteBuiltPakTip",
				"Deletes this pak on this computer. Convai keeps the version it already holds, and your next "
				"publish builds a new one."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, ChunkId, Platform, PlatformName]
				{
					const FText Question = FText::Format(
						LOCTEXT("DeleteBuiltPakAsk",
							"Delete the {0} pak on this computer?\n\nConvai keeps the version it already holds, "
							"and your next publish builds a new one."),
						PlatformName);
					if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
					{
						return;
					}
					if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
					{
						const bool bDeleted = Subsystem->DeleteBuiltPak(ChunkId, Platform);
						Notify(
							bDeleted
								? FText::Format(LOCTEXT("DeletedPak", "Deleted the {0} pak."), PlatformName)
								: FText::Format(LOCTEXT("DeletePakFailed",
									"Could not delete the {0} pak. It may be open in another program."), PlatformName),
							bDeleted ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
					}
				}),
				FCanExecuteAction::CreateLambda([this, bHasPak] { return bHasPak && !IsBusy(); })));
	}
	Menu.EndSection();

	Menu.BeginSection(NAME_None, LOCTEXT("OnConvai", "On Convai"));
	{
		Menu.AddMenuEntry(
			FText::Format(LOCTEXT("DeleteVersion", "Delete the {0} version..."), PlatformName),
			LOCTEXT("DeleteVersionTip", "Removes only this platform's version. The asset and its other versions stay."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, ChunkId, Platform, PlatformName]
				{
					const FText Question = FText::Format(
						LOCTEXT("DeleteVersionAsk",
							"Delete the {0} version of \"{1}\" from Convai?\n\nThe asset stays, with its other "
							"versions. Anything loading it on {0} stops working until you publish again.\n\n"
							"This cannot be undone. If Convai does not hold a {0} version, nothing changes."),
						PlatformName, FText::FromString(Asset.IsValid() ? Asset->Name : FString()));
					if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
					{
						return;
					}
					if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
					{
						Subsystem->DeleteVersion(ChunkId, Platform);
					}
				}),
				// Not gated on a local record of the upload: a fresh clone or a second machine must
				// still be able to remove something Convai holds.
				FCanExecuteAction::CreateLambda([this, bPublished] { return bPublished && !IsBusy(); })));
	}
	Menu.EndSection();

	return Menu.MakeWidget();
}

EActiveTimerReturnType SCPM_AssetDetailPanel::RefreshSpawnStatus(double InCurrentTime, float InDeltaTime)
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		SpawnStatus = Subsystem->GetSpawnPointStatus();
	}
	return EActiveTimerReturnType::Continue;
}

bool SCPM_AssetDetailPanel::IsBusy() const
{
	return Asset.IsValid() && Asset->Status.IsBusy();
}

#undef LOCTEXT_NAMESPACE
