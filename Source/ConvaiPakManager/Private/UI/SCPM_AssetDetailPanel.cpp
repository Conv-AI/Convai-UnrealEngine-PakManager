// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/SCPM_AssetDetailPanel.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CPM_PakManagerSettings.h"
#include "Chunk/CPM_Chunk.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Thumbnail/CPM_Thumbnail.h"
#include "UI/CPM_PakManagerStyle.h"
#include "Utility/CPM_UtilityLibrary.h"
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
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
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

		/** As the asset metadata API spells them. The sibling plugin's EGenderType is UPPERCASE and for
	 *  a different endpoint; do not reuse its string helper here. */
	const TCHAR* GenderValues[] = { TEXT("male"), TEXT("female") };

	/** An unset gender publishes as `male`, so the form says so rather than showing a blank. */
	FText GenderLabel(const FString& Value)
	{
		return Value.Equals(TEXT("female"), ESearchCase::IgnoreCase)
			? LOCTEXT("GenderFemale", "Female")
			: LOCTEXT("GenderMale", "Male");
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
	OnChunkCreated = InArgs._OnChunkCreated;

	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		bIsScene = Subsystem->GetAssetType() != ECPM_AssetType::Avatar;
		SpawnStatus = Subsystem->GetSpawnPointStatus();
		bHasNavMeshBounds = Subsystem->HasNavMeshBoundsVolume();
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
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.TextStyle(&SecondaryTextStyle())
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
					.Text_Lambda([this]
					{
						return bLegacyLayoutPending
							? LOCTEXT("NoChunksLegacy",
								"This project has records from an earlier version of this tool but no Chunk yet. "
								"Create one to recover them.")
							: LOCTEXT("NoChunks",
								"This project has no Chunks yet. Create one to publish this project's content, or "
								"add a Primary Asset Label to the content yourself.");
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 12.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FButtonStyle>("CPM.Button.Primary"))
					.Visibility_Lambda([this]
					{
						return bCanCreateChunk ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Text(LOCTEXT("CreateChunk", "Create chunk"))
					.ToolTipText(LOCTEXT("CreateChunkTip",
						"Creates the Primary Asset Label that gathers this project's modding plugin into one "
						"publishable Chunk. Nothing is uploaded."))
					.OnClicked(this, &SCPM_AssetDetailPanel::HandleCreateChunk)
				]
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
	SetupNotes = FText::GetEmpty();
	OutsidePick.Reset();

	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		bCanCreateChunk = Subsystem->CanAddAnotherChunk();
		bLegacyLayoutPending = Subsystem->HasUnmigratedLegacyLayout();
	}

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
	const TSharedRef<SVerticalBox> Body =
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
							.Text(bIsScene ? LOCTEXT("SceneChip", "Scene") : LOCTEXT("AvatarChip", "Avatar"))
							.ToolTipText(LOCTEXT("AssetTypeChipTip",
								"Decided by the Convai Modding Tool when this project was generated; it cannot be changed here."))
						]
					])
			];

	// A Scene's entity_data has no gender, so the row is absent from the tree rather than hidden in
	// it - the same rule the spawn point follows in the other direction.
	if (!bIsScene)
	{
		GenderOptions.Reset();
		for (const TCHAR* Value : GenderValues)
		{
			GenderOptions.Add(MakeShared<FString>(Value));
		}

		Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
		[
			Row(LOCTEXT("GenderLabel", "Gender"),
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&GenderOptions)
				.ToolTipText(LOCTEXT("GenderTip", "The voice gender this avatar is published with."))
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(GenderLabel(Item.IsValid() ? *Item : FString()));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
				{
					if (SelectInfo != ESelectInfo::Direct && Item.IsValid() && Asset.IsValid())
					{
						Asset->Gender = *Item;
					}
				})
				[
					// Read off the view model rather than the combo's own selection: the form
					// re-points at another Chunk without rebuilding, and a remembered selection
					// would then name the previous one's gender.
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						return GenderLabel(Asset.IsValid() ? Asset->Gender : FString());
					})
				])
		];
	}

	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("IdentitySection", "Identity & metadata"))
		.InitiallyCollapsed(false)
		.Padding(FMargin(12.0f, 8.0f))
		.BodyContent()
		[
			Body
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
						.ButtonStyle(&SecondaryButtonStyle())
						.Text(LOCTEXT("ShowDependencies", "Dependencies..."))
						.ToolTipText(LOCTEXT("ShowDependenciesTip",
							"What this asset drags into its pak, and how much of it lies outside the plugin."))
						.IsEnabled_Lambda([this] { return Asset.IsValid() && !Asset->EntryPoint.IsEmpty(); })
						.OnClicked(this, &SCPM_AssetDetailPanel::HandleShowDependencies)
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
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]
				{
					return EntryPointError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
				})
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this] { return EntryPointError; })
					.ColorAndOpacity(FSlateColor(FPalette::Error))
					.AutoWrapText(true)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&SecondaryButtonStyle())
					.Text(LOCTEXT("RelocateEntryPoint", "Copy into plugin..."))
					.ToolTipText(LOCTEXT("RelocateEntryPointTip",
						"Copies this asset and everything it needs into the modding plugin, then picks the copy."))
					.Visibility_Lambda([this]
					{
						return OutsidePick.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
					})
					.OnClicked(this, &SCPM_AssetDetailPanel::HandleRelocateEntryPoint)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(120.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this] { return SetupNotes; })
				.ColorAndOpacity(FSlateColor(FPalette::GreenPrimary))
				.AutoWrapText(true)
				.Visibility_Lambda([this]
				{
					return SetupNotes.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
				})
			]
		];
}

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildPreviewSection()
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	// The preview mirrors the shape this tool captures at, so the box does not imply a framing the
	// capture will not produce - which flips with the asset type, a Scene being a landscape shot.
	const FIntPoint Shape = ConvaiPakManager::Thumbnail::WrittenShape(
		bIsScene ? ECPM_AssetType::Scene : ECPM_AssetType::Avatar);
	constexpr float PreviewLongSide = 192.0f;
	const float Scale = PreviewLongSide / static_cast<float>(FMath::Max(Shape.X, Shape.Y));
	const float PreviewWidth = Shape.X * Scale;
	const float PreviewHeight = Shape.Y * Scale;

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
				.WidthOverride(PreviewWidth)
				.HeightOverride(PreviewHeight)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						// Fitted, not filled: a picked texture or an imported file keeps its own
						// shape, and the box would otherwise stretch a square one to twice its height.
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFit)
						[
							SAssignNew(ThumbnailImage, SImage)
						]
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
				.ToolTipText(bIsScene
					? LOCTEXT("CaptureThumbnailTip", "Captures the active viewport as this asset's thumbnail.")
					: LOCTEXT("CaptureAvatarThumbnailTip", "Renders the avatar blueprint you picked as this asset's thumbnail."))
				.OnClicked(this, &SCPM_AssetDetailPanel::HandleCaptureThumbnail)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SButton)
				.ButtonStyle(&SecondaryButtonStyle())
				.Text(LOCTEXT("ChooseThumbnailImage", "Choose image..."))
				.ToolTipText(LOCTEXT("ChooseThumbnailImageTip",
					"Use a PNG or JPEG you made yourself; it is stored as this asset's thumbnail."))
				.OnClicked(this, &SCPM_AssetDetailPanel::HandleChooseThumbnailImage)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SButton)
				.ButtonStyle(&SecondaryButtonStyle())
				.Text(LOCTEXT("UseSelectedTexture", "Use selected texture"))
				.ToolTipText(LOCTEXT("UseSelectedTextureTip",
					"Uses the texture selected in the Content Browser as this asset's thumbnail."))
				.OnClicked(this, &SCPM_AssetDetailPanel::HandleUseSelectedTexture)
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
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.TextStyle(&SecondaryTextStyle())
					// Coloured only while it is missing: this is a Precondition, so the publish will
					// refuse over it, and a grey line reads as information rather than as a blocker.
					.ColorAndOpacity_Lambda([this]
					{
						return FSlateColor(bHasNavMeshBounds ? FPalette::TextSecondary : FPalette::Warning);
					})
					.Text_Lambda([this]
					{
						return bHasNavMeshBounds
							? LOCTEXT("NavMeshPresent", "Nav mesh bounds are in this level.")
							: LOCTEXT("NavMeshMissing",
								"No nav mesh bounds in this level. Characters cannot walk without them.");
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&SecondaryButtonStyle())
					.Text(LOCTEXT("AddNavMeshBounds", "Add nav mesh bounds"))
					.ToolTipText(LOCTEXT("AddNavMeshBoundsTip",
						"Places a Nav Mesh Bounds Volume covering what is already in this level. Resize it like any other volume."))
					// Still offered once one exists: a level can want a second volume over a region
					// the first does not reach, and refusing that would be this tool's opinion, not
					// a requirement.
					.OnClicked(this, &SCPM_AssetDetailPanel::HandleAddNavMeshBounds)
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
							Asset.IsValid() && Asset->HasPublishedRawArchive() ? "Icons.Check" : "Icons.Info");
					})
					.ColorAndOpacity_Lambda([this]
					{
						// Never Warning. Nothing uploaded yet is the ordinary state of a draft, and
						// an upload the creator turned off is a choice - alarming about either is
						// what this rework removes.
						return FSlateColor(Asset.IsValid() && Asset->HasPublishedRawArchive()
							? FPalette::GreenPrimary : FPalette::TextSecondary);
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
						// Asked of the Policy actually read, not of the settings - the old check
						// only ever answered for a hand-typed override.
						if (!PolicyAsksForProjectSource())
						{
							return LOCTEXT("RawArchiveNotInPolicy", "Convai does not ask this project for a copy of your project");
						}

						const bool bUploaded = Asset.IsValid() && Asset->HasPublishedRawArchive();

						// With the include control behind the menu, this line is the only thing that
						// says whether the source is in the next publish - so it always says it.
						if (!UCPM_PakManagerSettings::Get().bUploadRawProjectArchive)
						{
							return bUploaded
								? FText::Format(LOCTEXT("RawArchiveExcludedUploaded",
									"Not in the next publish. Uploaded {0}"), RelativeTimeText(Asset->RawArchiveUploadTime))
								: LOCTEXT("RawArchiveExcluded", "Not in the next publish.");
						}

						if (!bUploaded)
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
						// Never Warning. Nothing uploaded yet is the ordinary state of a draft, and
						// an upload the creator turned off is a choice - alarming about either is
						// what this rework removes.
						return FSlateColor(Asset.IsValid() && Asset->HasPublishedRawArchive()
							? FPalette::GreenPrimary : FPalette::TextSecondary);
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					// The same "..." every platform row carries, so the source reads as one more of
					// them rather than the one row with its own controls.
					SNew(SComboButton)
					.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
					.HasDownArrow(false)
					.ToolTipText(LOCTEXT("SourceMoreTip", "More for the project source"))
					.OnGetMenuContent(this, &SCPM_AssetDetailPanel::BuildSourceRowMenu)
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("RowMoreGlyphSource", "..."))
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
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
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
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.TextStyle(&SecondaryTextStyle())
				.AutoWrapText(true)
				.Text(LOCTEXT("EssentialsWarning",
					"This asset's identity lives in this project's ConvaiEssentials folder. Never move or delete it - "
					"without it this asset can no longer be updated or deleted."))
			]
		];
}

UConvaiPakEditorSubsystem* SCPM_AssetDetailPanel::GetSubsystem()
{
	return GEditor ? GEditor->GetEditorSubsystem<UConvaiPakEditorSubsystem>() : nullptr;
}

FReply SCPM_AssetDetailPanel::HandleCreateChunk()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return FReply::Handled();
	}

	FString Error;
	if (!Subsystem->CreateChunk(Error))
	{
		Notify(FText::FromString(Error), SNotificationItem::CS_Fail);
		return FReply::Handled();
	}

	Notify(LOCTEXT("ChunkCreated", "Chunk created."), SNotificationItem::CS_Success);
	OnChunkCreated.ExecuteIfBound();
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleUseSelectedAsset()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Asset.IsValid())
	{
		return FReply::Handled();
	}

	SetupNotes = FText::GetEmpty();
	OutsidePick.Reset();

	FString Notes;
	if (Subsystem->PickEntryPointFromSelection(Asset->ChunkId, Notes))
	{
		EntryPointError = FText::GetEmpty();
		SetupNotes = FText::FromString(Notes);
		Asset->LoadFrom(*Subsystem);
		OfferToGatherDependencies(Asset->EntryPoint);
	}
	else
	{
		// The refusal wrote its reason to the status; the previous valid Entry Point survives.
		const FString Message = Subsystem->GetChunkStatus(Asset->ChunkId).Message;
		EntryPointError = Message.IsEmpty()
			? LOCTEXT("EntryPointRefused",
				"The selection does not match this project's Asset Type - a Scene needs a level, an Avatar a blueprint. The previous Entry Point is kept.")
			: FText::FromString(Message);

		// Asked of the subsystem rather than read out of the refusal text: only one of the refusals
		// is fixable by copying, and matching on a sentence is how the offer reaches the others.
		FString Picked;
		Subsystem->GetSelectedAssetPackageName(Picked);
		if (!Picked.IsEmpty() && !Subsystem->IsInsideModdingPlugin(Asset->ChunkId, Picked))
		{
			OutsidePick = Picked;
		}
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleRelocateEntryPoint()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Asset.IsValid() || OutsidePick.IsEmpty())
	{
		return FReply::Handled();
	}

	TArray<FString> Inside;
	TArray<FString> Outside;
	// Refused rather than asked with the empty arrays: the count is the whole substance of the
	// question below, and a confirmed "0 dependencies" is a different copy than the one that runs.
	if (!Subsystem->ListDependencies(Asset->ChunkId, OutsidePick, Inside, Outside))
	{
		Notify(LOCTEXT("DependenciesUnreadable", "Could not read this asset's dependencies."),
			SNotificationItem::CS_Fail);
		return FReply::Handled();
	}

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(Asset->ChunkId, Modding);

	// The count is in the question because the copy is not one asset: a blueprint with a full
	// character on it brings its meshes and materials, and that is what fills the creator's plugin.
	const FText Question = FText::Format(
		LOCTEXT("RelocateQuestion",
			"Copy {0} and its {1} dependencies into /{2}/?\n\nYour original stays where it is; the copy is what publishes."),
		FText::FromString(FPaths::GetCleanFilename(OutsidePick)),
		FText::AsNumber(Inside.Num() + Outside.Num()),
		FText::FromString(Modding.PluginName));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	FString NewPackage;
	FString Notes;
	FString Why;
	if (!Subsystem->RelocateEntryPointIntoPlugin(Asset->ChunkId, OutsidePick, NewPackage, Notes, Why))
	{
		// Notified as well as written into the row: the row is where the refusal that produced this
		// button already sits, so swapping its text is a report a creator can miss entirely.
		EntryPointError = FText::FromString(Why);
		Notify(EntryPointError, SNotificationItem::CS_Fail);
		return FReply::Handled();
	}

	EntryPointError = FText::GetEmpty();
	SetupNotes = FText::FromString(Notes);
	OutsidePick.Reset();
	Asset->LoadFrom(*Subsystem);
	Notify(FText::Format(LOCTEXT("RelocateDone", "Copied into the plugin as {0}, and picked it."),
		FText::FromString(NewPackage)), SNotificationItem::CS_Success);
	return FReply::Handled();
}

void SCPM_AssetDetailPanel::OfferToGatherDependencies(const FString& EntryPoint)
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Asset.IsValid() || EntryPoint.IsEmpty())
	{
		return;
	}

	TArray<FString> Inside;
	TArray<FString> Outside;
	if (!Subsystem->ListDependencies(Asset->ChunkId, EntryPoint, Inside, Outside) || Outside.IsEmpty())
	{
		return;
	}

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(Asset->ChunkId, Modding);

	// Named, not just counted: "8 assets" tells a creator nothing about whether the copy is the
	// right answer, and the first few paths tell them exactly which folder they built out of.
	const int32 NumListed = FMath::Min(Outside.Num(), 8);
	TArray<FString> Listed;
	for (int32 Index = 0; Index < NumListed; ++Index)
	{
		Listed.Add(Outside[Index]);
	}
	if (Outside.Num() > NumListed)
	{
		Listed.Add(FString::Printf(TEXT("...and %d more"), Outside.Num() - NumListed));
	}

	const FText Question = FText::Format(
		LOCTEXT("GatherQuestion",
			"{0} uses {1} assets that are not in /{2}/:\n\n{3}\n\n"
			"Only what is in the plugin is published. Copy them in and point {0} at the copies?\n\n"
			"Your originals stay where they are."),
		FText::FromString(FPaths::GetCleanFilename(EntryPoint)),
		FText::AsNumber(Outside.Num()),
		FText::FromString(Modding.PluginName),
		FText::FromString(FString::Join(Listed, TEXT("\n"))));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
	{
		return;
	}

	int32 Copied = 0;
	FString Why;
	if (!Subsystem->GatherDependenciesIntoPlugin(Asset->ChunkId, EntryPoint, Copied, Why))
	{
		Notify(FText::FromString(Why), SNotificationItem::CS_Fail);
		return;
	}

	Asset->LoadFrom(*Subsystem);
	Notify(FText::Format(LOCTEXT("GatherDone", "Copied {0} packages into /{1}/ and repointed {2}."),
		FText::AsNumber(Copied),
		FText::FromString(Modding.PluginName),
		FText::FromString(FPaths::GetCleanFilename(EntryPoint))), SNotificationItem::CS_Success);
}

FReply SCPM_AssetDetailPanel::HandleShowDependencies()
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Asset.IsValid() || Asset->EntryPoint.IsEmpty())
	{
		return FReply::Handled();
	}

	TArray<FString> Inside;
	TArray<FString> Outside;
	if (!Subsystem->ListDependencies(Asset->ChunkId, Asset->EntryPoint, Inside, Outside))
	{
		Notify(LOCTEXT("DependenciesUnreadable", "Could not read this asset's dependencies."),
			SNotificationItem::CS_Fail);
		return FReply::Handled();
	}

	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(Asset->ChunkId, Modding);

	// Held by the row generator the list view owns, not by this panel: the window outlives a closed
	// tab, and a list view whose items source has been freed paints freed memory.
	const TSharedRef<TArray<TSharedPtr<FString>>> Items = MakeShared<TArray<TSharedPtr<FString>>>();
	for (const FString& Package : Outside)
	{
		Items->Add(MakeShared<FString>(Package));
	}
	for (const FString& Package : Inside)
	{
		Items->Add(MakeShared<FString>(Package));
	}

	const int32 NumOutside = Outside.Num();
	const FText Header = NumOutside > 0
		? FText::Format(
			LOCTEXT("DependencyHeaderOutside",
				"{0} packages are reachable from {1}. {2} of them are outside /{3}/ and will NOT be in the "
				"Pak - pick the asset again to be offered a copy of them."),
			FText::AsNumber(Items->Num()),
			FText::FromString(FPaths::GetCleanFilename(Asset->EntryPoint)),
			FText::AsNumber(NumOutside),
			FText::FromString(Modding.PluginName))
		: FText::Format(
			LOCTEXT("DependencyHeaderClean",
				"{0} packages are reachable from {1}, and every one of them is in /{2}/, so the Pak has all of them."),
			FText::AsNumber(Items->Num()),
			FText::FromString(FPaths::GetCleanFilename(Asset->EntryPoint)),
			FText::FromString(Modding.PluginName));

	FSlateApplication::Get().AddWindow(
		SNew(SWindow)
		.Title(LOCTEXT("DependencyWindowTitle", "Dependencies"))
		.ClientSize(FVector2D(640.0, 480.0))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f)
			[
				SNew(STextBlock).AutoWrapText(true).Text(Header)
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(12.0f, 0.0f, 12.0f, 12.0f)
			[
				SNew(SListView<TSharedPtr<FString>>)
				.ListItemsSource(&Items.Get())
				.SelectionMode(ESelectionMode::None)
				// Items is captured for its lifetime, not its contents: this reference is the only
				// thing keeping alive what ListItemsSource points at.
				.OnGenerateRow_Lambda([Items, PluginName = Modding.PluginName](TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
				{
					const bool bOutside = !ConvaiPakManager::Chunk::IsUnderModdingPlugin(*Item, PluginName);
					return SNew(STableRow<TSharedPtr<FString>>, Owner)
						[
							SNew(STextBlock)
							.Text(FText::FromString(*Item))
							.ColorAndOpacity(FSlateColor(bOutside ? FPalette::Warning : FPalette::TextPrimary))
						];
				})
			]
		]);

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

	FString Why;
	if (Subsystem->CaptureThumbnail(Asset->ChunkId, Why))
	{
		Asset->LoadFrom(*Subsystem);
		RefreshThumbnailBrush(true);
	}
	else
	{
		Notify(Why.IsEmpty()
			? LOCTEXT("CaptureFailed", "Could not capture a thumbnail.")
			: FText::FromString(Why), SNotificationItem::CS_Fail);
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleChooseThumbnailImage()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Asset.IsValid())
	{
		return FReply::Handled();
	}

	const FString ImagePath = UCPM_UtilityLibrary::OpenFileDialog({ TEXT("png"), TEXT("jpg"), TEXT("jpeg") });
	if (ImagePath.IsEmpty())
	{
		return FReply::Handled();
	}

	FString Why;
	if (Subsystem->SetThumbnailFromFile(Asset->ChunkId, ImagePath, Why))
	{
		Asset->LoadFrom(*Subsystem);
		RefreshThumbnailBrush(true);
	}
	else
	{
		Notify(FText::FromString(Why), SNotificationItem::CS_Fail);
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandleUseSelectedTexture()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Asset.IsValid())
	{
		return FReply::Handled();
	}

	FString Picked;
	Subsystem->GetSelectedAssetPackageName(Picked);
	if (Picked.IsEmpty())
	{
		Notify(LOCTEXT("NoTextureSelected", "Select a texture in the Content Browser first."),
			SNotificationItem::CS_Fail);
		return FReply::Handled();
	}

	FString Why;
	if (Subsystem->SetThumbnailFromTexture(Asset->ChunkId, Picked, Why))
	{
		Asset->LoadFrom(*Subsystem);
		RefreshThumbnailBrush(true);
	}
	else
	{
		Notify(FText::FromString(Why), SNotificationItem::CS_Fail);
	}
	return FReply::Handled();
}

FReply SCPM_AssetDetailPanel::HandlePreviewThumbnail()
{
	if (!Asset.IsValid() || Asset->ThumbnailPath.IsEmpty())
	{
		return FReply::Handled();
	}

	if (!ThumbnailBrush.IsValid())
	{
		Notify(LOCTEXT("PreviewLoadFailed", "Could not read the thumbnail file."), SNotificationItem::CS_Fail);
		return FReply::Handled();
	}

	const FVector2D ImageSize = ThumbnailBrush->GetImageSize();
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

FReply SCPM_AssetDetailPanel::HandleAddNavMeshBounds()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return FReply::Handled();
	}

	const bool bPlaced = Subsystem->AddNavMeshBoundsVolume() != nullptr;
	bHasNavMeshBounds = Subsystem->HasNavMeshBoundsVolume();
	Notify(bPlaced
		? LOCTEXT("NavMeshAdded", "Placed a Nav Mesh Bounds Volume over the level. Resize it to cover where characters should walk.")
		: LOCTEXT("NavMeshNotAdded", "Could not place a Nav Mesh Bounds Volume in this level."),
		bPlaced ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
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
	int32 Width = 0;
	int32 Height = 0;
	TArray<FColor> Pixels;
	// Sized from the file rather than from the box: a brush that claims a shape the image does not
	// have draws it stretched wherever it is painted, and the preview window opens at that lie too.
	if (!Path.IsEmpty() && ConvaiPakManager::Thumbnail::DecodeImageFile(Path, Width, Height, Pixels))
	{
		ThumbnailBrush = MakeShareable(new FSlateDynamicImageBrush(FName(*Path), FVector2D(Width, Height)));
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

bool SCPM_AssetDetailPanel::PolicyAsksForProjectSource() const
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return true;
	}

	FCPM_PublishPolicy Policy;
	FDateTime ReadAt;
	ECPM_PolicyReadState State = ECPM_PolicyReadState::Unread;

	// Unread or unreadable answers YES, matching how platforms fail open: telling a creator Convai
	// wants no copy of their project, because a GitHub read timed out, is the worse lie.
	return Subsystem->GetPublishPolicy(Policy, ReadAt, State) ? Policy.bUploadRawProject : true;
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
				const bool bBuilt = Status && Status->bExists;
				const bool bSelected = Asset.IsValid() && Asset->SelectedPlatforms.Contains(Platform);

				// With the include control behind the menu, this line is the ONLY thing that says
				// whether the platform is in the next publish - so it always says it.
				if (!bSelected)
				{
					return bBuilt
						? FText::Format(LOCTEXT("PakBuiltExcluded", "Not in the next publish. Built pak on disk, made {0}"),
							RelativeTimeText(Status->LastPackagedTime))
						: LOCTEXT("PakExcluded", "Not in the next publish.");
				}

				return bBuilt
					? FText::Format(LOCTEXT("PakBuilt", "Built pak on disk, made {0}"),
						RelativeTimeText(Status->LastPackagedTime))
					: LOCTEXT("PakWillBuild", "No pak on this computer. The next publish builds one.");
			})
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

TSharedRef<SWidget> SCPM_AssetDetailPanel::BuildSourceRowMenu()
{
	const int32 ChunkId = Asset.IsValid() ? Asset->ChunkId : INDEX_NONE;
	const bool bPublished = Asset.IsValid() && !Asset->AssetId.IsEmpty();

	FMenuBuilder Menu(/*bShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	Menu.BeginSection(NAME_None, LOCTEXT("NextPublishSource", "Next publish"));
	{
		Menu.AddMenuEntry(
			LOCTEXT("IncludeSource", "Include in the next publish"),
			LOCTEXT("UploadArchiveTip",
				"Sends your project alongside the paks, so Convai can repackage this asset for future "
				"Unreal Engine versions without you republishing it. It is the longest step of a publish; "
				"turn it off while iterating. Applies to every asset in this project."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([]
				{
					const bool bUpload = !UCPM_PakManagerSettings::Get().bUploadRawProjectArchive;

					// Only turning it OFF is explained, and only before it takes effect: this is the
					// choice that costs the creator something later, and the cost is invisible until
					// an engine version they have not heard of yet ships.
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
				}),
				// Not gated on the Policy: unticking is always allowed, and when the Policy asks for
				// no archive the row says so and the setting cannot add one anyway.
				FCanExecuteAction::CreateLambda([this] { return !IsBusy(); }),
				FIsActionChecked::CreateLambda([]
				{
					return UCPM_PakManagerSettings::Get().bUploadRawProjectArchive;
				})),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	}
	Menu.EndSection();

	Menu.BeginSection(NAME_None, LOCTEXT("OnConvaiSource", "On Convai"));
	{
		Menu.AddMenuEntry(
			LOCTEXT("DeleteSourceVersion", "Delete the project source Convai holds..."),
			LOCTEXT("DeleteSourceVersionTip",
				"Removes the copy of your project Convai holds. The asset and its builds stay, but Convai can no "
				"longer move this asset to a future Unreal version on its own."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, ChunkId]
				{
					const FText Question = FText::Format(
						LOCTEXT("DeleteSourceAsk",
							"Delete the copy of your project that Convai holds for \"{0}\"?\n\nThe asset and its "
							"builds stay. Convai will no longer be able to move this asset to a future Unreal "
							"version on its own - you would publish it again yourself.\n\nThis cannot be undone."),
						FText::FromString(Asset.IsValid() ? Asset->Name : FString()));
					if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
					{
						return;
					}
					if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
					{
						Subsystem->DeleteVersion(ChunkId, ECPM_Platform::Raw);
					}
				}),
				FCanExecuteAction::CreateLambda([this, bPublished] { return bPublished && !IsBusy(); })));
	}
	Menu.EndSection();

	return Menu.MakeWidget();
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
	const bool bPublished = Asset.IsValid() && !Asset->AssetId.IsEmpty();

	FMenuBuilder Menu(/*bShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	Menu.BeginSection(NAME_None, LOCTEXT("NextPublish", "Next publish"));
	{
		const bool bInPolicy = PolicyPlatforms().Contains(Platform);

		Menu.AddMenuEntry(
			LOCTEXT("IncludePlatform", "Include in the next publish"),
			// The tooltip carries the whole Platform Selection rule, because this is the only place
			// a creator meets it - and it reads differently depending on which way it departs from
			// what Convai asks for.
			bInPolicy
				? LOCTEXT("PlatformIncludedTip",
					"Build and upload this platform on the next publish. Convai asks this project for it; "
					"turning it off sends the others alone.")
				: LOCTEXT("PlatformForcedTip",
					"Build and upload this platform on the next publish, even though Convai does not ask this "
					"project for it. For a project Convai has agreed to host this platform for."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, Platform]
				{
					if (!Asset.IsValid())
					{
						return;
					}
					if (Asset->SelectedPlatforms.Contains(Platform))
					{
						Asset->SelectedPlatforms.Remove(Platform);
					}
					else
					{
						Asset->SelectedPlatforms.Add(Platform);
					}
				}),
				FCanExecuteAction::CreateLambda([this] { return !IsBusy(); }),
				FIsActionChecked::CreateLambda([this, Platform]
				{
					return Asset.IsValid() && Asset->SelectedPlatforms.Contains(Platform);
				})),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	}
	Menu.EndSection();

	Menu.BeginSection(NAME_None, LOCTEXT("OnThisComputer", "On this computer"));
	{
		Menu.AddMenuEntry(
			FText::Format(LOCTEXT("DeleteBuiltPak", "Clean up the {0} package"), PlatformName),
			LOCTEXT("DeleteBuiltPakTip",
				"Deletes what this project built for this platform on this computer. Convai keeps the version it "
				"already holds, and your next publish builds it again."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, ChunkId, Platform, PlatformName]
				{
					const FText Question = FText::Format(
						LOCTEXT("DeleteBuiltPakAsk",
							"Clean up what this project built for {0} on this computer?\n\nConvai keeps the version "
							"it already holds, and your next publish builds it again."),
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
								? FText::Format(LOCTEXT("DeletedPak", "Cleaned up the {0} package."), PlatformName)
								: FText::Format(LOCTEXT("DeletePakFailed",
									"Could not clean up the {0} package. It may be open in another program."), PlatformName),
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
		// Same timer, because a creator who deletes either one in the level is told by the same row
		// rather than by the publish that refuses a minute later.
		bHasNavMeshBounds = Subsystem->HasNavMeshBoundsVolume();
	}
	return EActiveTimerReturnType::Continue;
}

bool SCPM_AssetDetailPanel::IsBusy() const
{
	return Asset.IsValid() && Asset->Status.IsBusy();
}

#undef LOCTEXT_NAMESPACE
