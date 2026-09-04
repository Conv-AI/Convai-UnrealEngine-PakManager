// Copyright 2025 Convai Inc. All Rights Reserved.

#include "UI/SCPM_PakManagerPanel.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "ConvaiPakEditorSubsystem.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "UI/CPM_PakManagerStyle.h"
#include "UI/SCPM_AssetDetailPanel.h"
#include "UI/SCPM_AssetListPanel.h"
#include "Utility/CPM_UtilityLibrary.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"
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

	/**
	 * The delete confirmation and the one choice it carries.
	 *
	 * Its own window rather than FMessageDialog because a message box has nowhere to put a choice,
	 * and this one must be made in the same breath as the confirmation: deleting the content is a
	 * second, larger destruction - the creator's own authored Source Packages - so it is opt-in,
	 * off every time the dialog opens, and never remembered.
	 *
	 * Blocks until the creator answers, so the outputs are read from the stack of the caller.
	 */
	void ShowDeleteAssetDialog(
		const FText& AssetName, const FString& PluginName, bool& bOutConfirmed, bool& bOutDeleteContent)
	{
		bOutConfirmed = false;
		bOutDeleteContent = false;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("DeleteAssetTitle", "Delete asset"))
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
					.Text(FText::Format(LOCTEXT("DeleteAssetBody",
						"Delete \"{0}\"?\n\nThis permanently removes the asset and all of its versions from Convai. "
						"It cannot be undone.\n\nThis project's record of it - name, description, thumbnail and "
						"entry point - is cleared with it."), AssetName))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
				[
					SNew(SCheckBox)
					// Hidden rather than disabled where no plugin is recorded: there is no folder to
					// name, and an option that cannot say what it deletes should not be offered.
					.Visibility(PluginName.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					.IsChecked(ECheckBoxState::Unchecked)
					.OnCheckStateChanged_Lambda([&bOutDeleteContent](ECheckBoxState State)
					{
						bOutDeleteContent = State == ECheckBoxState::Checked;
					})
					.ToolTipText(LOCTEXT("DeleteContentTip",
						"Deletes the levels, blueprints and other content you added under this plugin. Its asset "
						"label is kept, so the asset stays in this list and can be filled again. Cannot be undone."))
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(FText::Format(LOCTEXT("DeleteContentOption",
							"Also delete the content I added in plugin {0}"), FText::FromString(PluginName)))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f).HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FButtonStyle>("CPM.Button.Secondary"))
						.Text(LOCTEXT("DeleteCancel", "Cancel"))
						.OnClicked_Lambda([Window]
						{
							Window->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCPM_PakManagerStyle::Get().GetWidgetStyle<FButtonStyle>("CPM.Button.Danger"))
						.Text(LOCTEXT("DeleteConfirm", "Delete"))
						.OnClicked_Lambda([Window, &bOutConfirmed]
						{
							bOutConfirmed = true;
							Window->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]);

		FSlateApplication::Get().AddModalWindow(Window, nullptr);
	}
}

void SCPM_PakManagerPanel::Construct(const FArguments& InArgs)
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		StatusChangedHandle = Subsystem->OnChunkStatusChanged.AddSP(
			this, &SCPM_PakManagerPanel::HandleChunkStatusChanged);
		CompatibilityChangedHandle = Subsystem->OnCompatibilityChanged.AddSP(
			this, &SCPM_PakManagerPanel::HandleCompatibilityChanged);
		PolicyChangedHandle = Subsystem->OnPolicyChanged.AddSP(
			this, &SCPM_PakManagerPanel::HandlePolicyChanged);
		Subsystem->RefreshCompatibility();
		// Read once up front as well: the Policy may already be cached from an earlier tab, in which
		// case nothing would broadcast and the banner would stay silent about a real problem.
		HandlePolicyChanged();
	}

	// A tab restored by the saved layout constructs while the registry is still scanning, when no
	// Primary Asset Label is discoverable yet; without this it sits on "no Chunks" until the creator
	// clicks away and back.
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get(); AssetRegistry && AssetRegistry->IsLoadingAssets())
	{
		FilesLoadedHandle = AssetRegistry->OnFilesLoaded().AddSP(this, &SCPM_PakManagerPanel::HandleFilesLoaded);
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
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildLegacyBanner()
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildCompatibilityBanner()
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildToolchainBanner()
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
					.OnChunkCreated(FSimpleDelegate::CreateSP(this, &SCPM_PakManagerPanel::RefreshProject))
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildActionBar()
			]
		]
	];

	RefreshProject();
}

SCPM_PakManagerPanel::~SCPM_PakManagerPanel()
{
	// Unsubscribed explicitly: the subsystem outlives this widget, and a delegate left bound to a
	// destroyed panel is a crash on the next status change.
	if (StatusChangedHandle.IsValid() || CompatibilityChangedHandle.IsValid() || PolicyChangedHandle.IsValid())
	{
		if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->OnChunkStatusChanged.Remove(StatusChangedHandle);
			Subsystem->OnCompatibilityChanged.Remove(CompatibilityChangedHandle);
			Subsystem->OnPolicyChanged.Remove(PolicyChangedHandle);
		}
	}
	if (FilesLoadedHandle.IsValid())
	{
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			AssetRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
		}
	}
}

void SCPM_PakManagerPanel::HandleFilesLoaded()
{
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
	}
	FilesLoadedHandle.Reset();
	RefreshProject();
}

void SCPM_PakManagerPanel::HandleCompatibilityChanged()
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->GetCompatibility(Compatibility);
	}
}

void SCPM_PakManagerPanel::HandlePolicyChanged()
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	FCPM_PublishPolicy Policy;
	FDateTime ReadAt;
	ECPM_PolicyReadState State = ECPM_PolicyReadState::Unread;
	bPolicyPackagesLinux = Subsystem->GetPublishPolicy(Policy, ReadAt, State)
		&& Policy.PlatformsToPackage().Contains(ECPM_Platform::Linux);

	// Only when it could matter. The read stats the disk, and a Windows-only project has no reason
	// to pay for it or to be told the answer.
	LinuxToolchain = bPolicyPackagesLinux
		? ConvaiPakManager::Preconditions::InspectLinuxToolchain()
		: ConvaiPakManager::Preconditions::FLinuxToolchain();
}

void SCPM_PakManagerPanel::RefreshProject()
{
	if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
	{
		// First, because a Chunk discovered or minted since the last refresh is what lets migration
		// attribute a pre-Chunk layout at all - and what is left over is the banner's condition.
		Subsystem->ReconcileChunkState();
		Project.Refresh(*Subsystem);
		bLegacyLayoutPending = Subsystem->HasUnmigratedLegacyLayout();
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

TSharedRef<SWidget> SCPM_PakManagerPanel::BuildLegacyBanner()
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	// A banner rather than a toast: the condition outlives any click, and only moving files off disk
	// clears it.
	return SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Panel"))
		.Padding(FMargin(12.0f, 8.0f))
		.Visibility_Lambda([this]
		{
			return bLegacyLayoutPending ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
				.ColorAndOpacity(FSlateColor(FPalette::Error))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text_Lambda([this]
				{
					return Project.Assets.IsEmpty()
						? LOCTEXT("LegacyNoChunk",
							"This project holds records from an earlier version of this tool, but no Chunk to attach "
							"them to. Create the Chunk to recover them - if an asset was published, publishing before "
							"that would create a second one and lose the first.")
						: LOCTEXT("LegacyUnattributed",
							"This project holds records from an earlier version of this tool that could not be "
							"attributed to a Chunk, so nothing was moved. Publishing is disabled until this is "
							"resolved - see the Output Log for which files.");
				})
			]
		];
}

TSharedRef<SWidget> SCPM_PakManagerPanel::BuildCompatibilityBanner()
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	// Tells, never blocks: the check fails open on a network it could not reach, and a creator held
	// back by it has no way to update the tool from in here - a dead end with no door out.
	return SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Panel"))
		.Padding(FMargin(12.0f, 8.0f))
		.Visibility_Lambda([this]
		{
			// bToolBelowFloor in its own right: the two version pins are read from different files,
			// so a floor can be known in a session where "latest" was not.
			return Compatibility.bToolOutdated || Compatibility.bEngineMismatch || Compatibility.bToolBelowFloor
				? EVisibility::Visible
				: EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
				.ColorAndOpacity(FSlateColor(FPalette::Warning))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FPalette::Warning))
				.Text_Lambda([this]
				{
					TArray<FText> Sentences;
					// The floor replaces the advisory rather than adding to it: telling a creator a
					// newer version exists is noise next to being told this one is refused.
					if (Compatibility.bToolBelowFloor)
					{
						Sentences.Add(FText::Format(LOCTEXT("ToolBelowFloor",
							"Pak Manager {0} is installed and Convai no longer accepts anything below {1}. "
							"Publishing is refused until you update it with the Convai Modding Tool."),
							FText::FromString(Compatibility.InstalledToolVersion),
							FText::FromString(Compatibility.MinimumToolVersion)));
					}
					else if (Compatibility.bToolOutdated)
					{
						Sentences.Add(FText::Format(LOCTEXT("ToolOutdated",
							"Pak Manager {0} is installed and {1} is available. Update it with the Convai Modding "
							"Tool before publishing."),
							FText::FromString(Compatibility.InstalledToolVersion),
							FText::FromString(Compatibility.LatestToolVersion)));
					}
					if (Compatibility.bEngineMismatch)
					{
						Sentences.Add(FText::Format(LOCTEXT("EngineMismatch",
							"This project runs Unreal {0}; Convai targets {1}. Paks built here may not load in "
							"Convai products."),
							FText::FromString(Compatibility.EngineVersion),
							FText::FromString(Compatibility.TargetEngineVersion)));
					}
					return FText::Join(FText::FromString(TEXT("\n")), Sentences);
				})
			]
		];
}

TSharedRef<SWidget> SCPM_PakManagerPanel::BuildToolchainBanner()
{
	using FPalette = FCPM_PakManagerStyle::FPalette;

	// Error, not Warning: the two banners above tell a creator something, this one names why the
	// publish is going to refuse. Colouring it like the advisory ones would understate it.
	return SNew(SBorder)
		.BorderImage(FCPM_PakManagerStyle::Get().GetBrush("CPM.Panel"))
		.Padding(FMargin(12.0f, 8.0f))
		.Visibility_Lambda([this]
		{
			return bPolicyPackagesLinux && !LinuxToolchain.bUsable ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Error"))
				.ColorAndOpacity(FSlateColor(FPalette::Error))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FPalette::Error))
				// The refusal's own words, so the banner and the failed publish cannot disagree
				// about what is wrong or about what to do next.
				.Text_Lambda([this]
				{
					return FText::FromString(
						ConvaiPakManager::Preconditions::WhyLinuxCannotPackage(LinuxToolchain));
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
	// Two predicates, named once, so nine entries cannot each forget one. Read-only entries below
	// deliberately use neither: reading a string or opening a settings page contends with nothing,
	// and a twenty-minute publish is exactly when someone reaches for them.
	auto IsBusy = [this]
	{
		return Project.Active.IsValid() && (Project.Active->Status.IsBusy() || Project.AnyPublishInFlight());
	};
	auto IsPublished = [this]
	{
		return Project.Active.IsValid() && !Project.Active->AssetId.IsEmpty();
	};

	FMenuBuilder Menu(/*bShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	Menu.BeginSection(NAME_None, LOCTEXT("MenuPublish", "Publish"));
	{
		Menu.AddMenuEntry(
			LOCTEXT("PackageNow", "Package now"),
			LOCTEXT("PackageNowTip", "Builds the paks this publish would build. Nothing is uploaded."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SCPM_PakManagerPanel::HandlePackageNow),
				FCanExecuteAction::CreateLambda([this, IsBusy] { return !IsBusy() && !bLegacyLayoutPending; })));

		Menu.AddMenuEntry(
			LOCTEXT("PublishReusing", "Publish without repackaging"),
			LOCTEXT("PublishReusingTip",
				"Uploads the paks already on this computer instead of building new ones. Use it only when you know "
				"they match your content - nothing downstream can tell an old pak from a fresh one."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SCPM_PakManagerPanel::HandlePublishReusingPaks),
				FCanExecuteAction::CreateLambda([this, IsBusy]
				{
					return !IsBusy() && !bLegacyLayoutPending && DetailPanel.IsValid() && DetailPanel->HasAnyBuiltPak();
				})));

		Menu.AddMenuEntry(
			LOCTEXT("DeleteBuiltPaks", "Clean up the packaged asset"),
			LOCTEXT("DeleteBuiltPaksTip",
				"Deletes what this project has built on this computer, for every platform. Convai keeps the "
				"versions it already holds, and your next publish builds them again."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SCPM_PakManagerPanel::HandleDeleteBuiltPaks),
				FCanExecuteAction::CreateLambda([this, IsBusy]
				{
					return !IsBusy() && Project.Active.IsValid()
						&& Project.Active->PakStatuses.ContainsByPredicate(
							[](const FCPM_PakPlatformStatus& Status) { return Status.bExists; });
				})));
	}
	Menu.EndSection();

	Menu.BeginSection(NAME_None, LOCTEXT("MenuShow", "Show"));
	{
		Menu.AddMenuEntry(
			LOCTEXT("ShowEveryPlatform", "Show every platform"),
			LOCTEXT("ShowEveryPlatformTip",
				"Lists platforms this project's publish policy does not ask for, so you can reach a pak or a "
				"version you already have, or publish one Convai has agreed to host for this project."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]
				{
					if (DetailPanel.IsValid())
					{
						DetailPanel->ToggleShowAllPlatforms();
					}
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]
				{
					return DetailPanel.IsValid() && DetailPanel->IsShowingAllPlatforms();
				})),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);

		Menu.AddMenuEntry(
			LOCTEXT("RereadPolicy", "Re-read the publish policy"),
			LOCTEXT("RereadPolicyTip", "Asks Convai again which platforms this project publishes for."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]
			{
				if (UConvaiPakEditorSubsystem* Subsystem = GetSubsystem())
				{
					Subsystem->RefreshPolicy();
				}
			})));
	}
	Menu.EndSection();

	Menu.BeginSection(NAME_None, LOCTEXT("MenuAsset", "Asset"));
	{
		Menu.AddMenuEntry(
			LOCTEXT("DeleteAsset", "Delete asset..."),
			LOCTEXT("DeleteAssetTip", "Removes the published Convai asset. Local project files stay untouched."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this] { HandleDeleteClicked(); }),
				FCanExecuteAction::CreateLambda([IsBusy, IsPublished] { return IsPublished() && !IsBusy(); })));
	}
	Menu.EndSection();

	return Menu.MakeWidget();
}

void SCPM_PakManagerPanel::HandlePackageNow()
{
	TSharedPtr<FCPM_AssetViewModel> Active = Project.Active;
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Active.IsValid() || !Subsystem || !DetailPanel.IsValid())
	{
		return;
	}

	if (!Subsystem->PackageWithOptions(Active->ChunkId, DetailPanel->BuildPublishOptions(/*bReuseExistingPaks=*/false)))
	{
		NotifyRefusal(Active->ChunkId, LOCTEXT("PackageRefused", "The packaging run was not accepted."));
	}
}

void SCPM_PakManagerPanel::HandlePublishReusingPaks()
{
	TSharedPtr<FCPM_AssetViewModel> Active = Project.Active;
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Active.IsValid() || !Subsystem || !DetailPanel.IsValid())
	{
		return;
	}

	// Same auto-save as the primary button: this is a publish, and a publish sends what is saved.
	if (!SaveActive(false))
	{
		return;
	}

	if (!Subsystem->PublishWithOptions(Active->ChunkId, DetailPanel->BuildPublishOptions(/*bReuseExistingPaks=*/true)))
	{
		NotifyRefusal(Active->ChunkId, LOCTEXT("PublishRefused2", "The publish was not accepted."));
	}
}

void SCPM_PakManagerPanel::HandleDeleteBuiltPaks()
{
	TSharedPtr<FCPM_AssetViewModel> Active = Project.Active;
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	if (!Active.IsValid() || !Subsystem)
	{
		return;
	}

	const FText Question = LOCTEXT("DeleteBuiltPaksAsk",
		"Clean up everything this project has built on this computer?\n\nConvai keeps the versions it already "
		"holds, and your next publish builds them again.");
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
	{
		return;
	}

	const int32 Deleted = Subsystem->DeleteBuiltPaks(Active->ChunkId);
	Notify(FText::Format(LOCTEXT("DeletedPaks", "Cleaned up the packaged asset - {0} file(s) removed."), FText::AsNumber(Deleted)),
		SNotificationItem::CS_Success);
}

void SCPM_PakManagerPanel::NotifyRefusal(const int32 ChunkId, const FText& Fallback)
{
	UConvaiPakEditorSubsystem* Subsystem = GetSubsystem();
	const FString Why = Subsystem ? Subsystem->GetChunkStatus(ChunkId).Message : FString();
	Notify(Why.IsEmpty() ? Fallback : FText::FromString(Why), SNotificationItem::CS_Fail);
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

	case ECPM_AssetManagerStatus::Packaging_Success:
		// Terminal only for a package-only run; a Publish passes straight through this on its way
		// to UploadPak_Success. Without this case the run finishes silently and the Upload rows go
		// on saying no pak is on this computer, having never re-read them.
		bTerminal = true;
		State = SNotificationItem::CS_Success;
		Message = LOCTEXT("PackagingSucceeded", "Packaging finished. Nothing was uploaded.");
		break;

	case ECPM_AssetManagerStatus::Delete_Success:
		bTerminal = true;
		State = SNotificationItem::CS_Success;
		Message = FText::Format(LOCTEXT("DeleteSucceeded", "Deleted \"{0}\"."), DisplayNameOf(AssetVM));
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

	// Carries this Chunk's Platform Selection. When it matches the Policy the options are empty and
	// this behaves exactly as Publish(ChunkId) always did.
	const FCPM_PublishOptions Options = DetailPanel.IsValid()
		? DetailPanel->BuildPublishOptions(/*bReuseExistingPaks=*/false)
		: FCPM_PublishOptions();

	if (!Subsystem->PublishWithOptions(Active->ChunkId, Options))
	{
		NotifyRefusal(Active->ChunkId, LOCTEXT("PublishRefused", "The publish was not accepted."));
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

	// Its own window rather than FMessageDialog, which has no room for a choice: the second, opt-in
	// destruction here is of the creator's own authored content, and it has to be visible and off
	// by default in the same breath as the confirmation.
	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(Active->ChunkId, Modding);

	bool bConfirmed = false;
	bool bDeleteContent = false;
	ShowDeleteAssetDialog(DisplayNameOf(Active), Modding.PluginName, bConfirmed, bDeleteContent);
	if (!bConfirmed)
	{
		return FReply::Handled();
	}

	// Empty Version deletes the whole Asset rather than one of its Versions.
	if (!Subsystem->DeleteAsset(Active->ChunkId, FString(), bDeleteContent))
	{
		const FString Why = Subsystem->GetChunkStatus(Active->ChunkId).Message;
		Notify(Why.IsEmpty()
			? LOCTEXT("DeleteRefused", "The delete was not accepted.")
			: FText::FromString(Why),
			SNotificationItem::CS_Fail);
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
		&& !IsOtherChunkPublishing()
		&& !bLegacyLayoutPending;
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

	// Before the failure branch: this is why the button is dead, whatever the last command left behind.
	if (bLegacyLayoutPending)
	{
		return LOCTEXT("LegacyBlocksPublish",
			"Publishing is disabled: records from an earlier version of this tool could not be attributed to a Chunk.");
	}

	// A failure outlives its toast: the reason stays here until the next command moves the status.
	if (Active->Badge() == FCPM_AssetViewModel::EBadge::NeedsAttention)
	{
		return Active->Status.Message.IsEmpty()
			? StaticEnum<ECPM_AssetManagerStatus>()->GetDisplayNameTextByValue(static_cast<int64>(Active->Status.Status))
			: FText::FromString(Active->Status.Message);
	}

	const TArray<FText> Messages = Active->ValidationMessages();
	return Messages.IsEmpty() ? FText::GetEmpty() : FText::Join(LOCTEXT("ValidationDelim", " "), Messages);
}

FSlateColor SCPM_PakManagerPanel::GetActionBarSummaryColor() const
{
	using FPalette = FCPM_PakManagerStyle::FPalette;
	const TSharedPtr<FCPM_AssetViewModel>& Active = Project.Active;
	if (!Active.IsValid() || Active->Status.IsBusy() || IsOtherChunkPublishing())
	{
		return FSlateColor(FPalette::TextSecondary);
	}
	if (bLegacyLayoutPending || Active->Badge() == FCPM_AssetViewModel::EBadge::NeedsAttention)
	{
		return FSlateColor(FPalette::Error);
	}
	return FSlateColor(Active->ValidationMessages().IsEmpty() ? FPalette::TextSecondary : FPalette::Warning);
}

#undef LOCTEXT_NAMESPACE
