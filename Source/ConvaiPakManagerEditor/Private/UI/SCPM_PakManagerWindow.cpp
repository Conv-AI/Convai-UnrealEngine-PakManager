// Copyright 2022 Convai Inc. All Rights Reserved.

#include "UI/SCPM_PakManagerWindow.h"
#include "SlateWidgets/Widgets/SCPM_KeyValueList.h"
#include "SlateWidgets/Core/SCPM_Button.h"
#include "SlateWidgets/Composite/SCPM_AlertBanner.h"
#include "SlateWidgets/CPM_SlateStyle.h"
#include "Jobs/CPM_VersionCheckJobs.h"
#include "Utility/WorkflowBlueprintLibrary.h"
#include "Core/Workflow.h"
#include "Core/WorkflowManagerSubsystem.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "SCPM_PakManagerWindow"

SCPM_PakManagerWindow::~SCPM_PakManagerWindow()
{
	if (VersionValidationHandle.IsValid())
	{
		UWorkflowBlueprintLibrary::CancelWorkflow(VersionValidationHandle, true);
	}
}

void SCPM_PakManagerWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(CPMStyle::Background())
		.Padding(0)
		[
			SNew(SVerticalBox)

			// Version Validation Alert Banner (hidden by default)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(VersionAlertBanner, SCPM_AlertBanner)
				.AlertType(ECPM_AlertType::Error)
				.Message(FText::GetEmpty())
				.ActionButtonText(LOCTEXT("RetryValidation", "Retry"))
				.OnClose(FSimpleDelegate::CreateSP(this, &SCPM_PakManagerWindow::DismissVersionAlert))
				.OnActionClicked(FSimpleDelegate::CreateSP(this, &SCPM_PakManagerWindow::RunVersionValidation))
				.Visibility(EVisibility::Collapsed)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)
					
					// Header
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(CPMStyle::ContentPadding())
					[
						BuildHeaderSection()
					]
					
					// Asset Info Section (KeyValueList with asset_type dropdown)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(CPMStyle::ContentPadding())
					[
						BuildAssetInfoSection()
					]
					
					// Thumbnail Section
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(CPMStyle::ContentPadding())
					[
						BuildThumbnailSection()
					]
					
					// Action Buttons
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(CPMStyle::ContentPadding())
					[
						BuildActionButtonsSection()
					]
					
					// Status Section
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(CPMStyle::ContentPadding())
					[
						BuildStatusSection()
					]
				]
			]
		]
	];

	RunVersionValidation();
}

TSharedRef<SWidget> SCPM_PakManagerWindow::BuildHeaderSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WindowTitle", "Convai Pak Manager"))
			.Font(CPMStyle::HeaderFont())
			.ColorAndOpacity(CPMStyle::TextColor())
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WindowSubtitle", "Create and manage pak files for your assets"))
			.Font(CPMStyle::CaptionFont())
			.ColorAndOpacity(CPMStyle::HintTextColor())
		];
}

TSharedRef<SWidget> SCPM_PakManagerWindow::BuildAssetInfoSection()
{
	// Load initial pairs from JSON config
	TArray<FCPM_KeyValuePair> InitialPairs = LoadAssetConfigFromJson();
	
	// Populate auto-generated values (project name, UE version, etc.)
	PopulateAutoValues(InitialPairs);

	return SNew(SVerticalBox)
		
		// Section Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AssetInfoHeader", "Asset Information"))
			.Font(CPMStyle::HeaderFont())
			.ColorAndOpacity(CPMStyle::TextColor())
		]
		
		// KeyValue List
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(AssetInfoList, SCPM_KeyValueList)
			.InitialPairs(InitialPairs)
			.KeyHintText(LOCTEXT("KeyHint", "Property"))
			.ValueHintText(LOCTEXT("ValueHint", "Value"))
			.ShowAddButton(true)
			.MaxHeight(400.0f)
			.OnListChanged(this, &SCPM_PakManagerWindow::HandleAssetInfoChanged)
		];
}

TSharedRef<SWidget> SCPM_PakManagerWindow::BuildThumbnailSection()
{
	// Create a simple brush for the placeholder
	ThumbnailBrush = FSlateBrush();
	ThumbnailBrush.TintColor = FSlateColor(CPMStyle::Secondary());

	return SNew(SVerticalBox)
		
		// Section Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ThumbnailHeader", "Thumbnail"))
			.Font(CPMStyle::HeaderFont())
			.ColorAndOpacity(CPMStyle::TextColor())
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			
			// Thumbnail Preview
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("Border"))
				.BorderBackgroundColor(CPMStyle::InputBorder())
				.Padding(2)
				[
					SNew(SBox)
					.WidthOverride(128.0f)
					.HeightOverride(128.0f)
					[
						SAssignNew(ThumbnailImage, SImage)
						.Image(&ThumbnailBrush)
					]
				]
			]
			
			// Thumbnail Actions
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(16, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 8)
				[
					SAssignNew(CaptureThumbnailButton, SCPM_Button)
					.Text(LOCTEXT("CaptureButton", "Capture from Viewport"))
					.ButtonStyle(ECPM_ButtonStyle::Secondary)
					.OnClicked(this, &SCPM_PakManagerWindow::HandleCaptureThumbnailClicked)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ThumbnailHint", "256x256 recommended"))
					.Font(CPMStyle::CaptionFont())
					.ColorAndOpacity(CPMStyle::HintTextColor())
				]
			]
		];
}

TSharedRef<SWidget> SCPM_PakManagerWindow::BuildActionButtonsSection()
{
	return SNew(SVerticalBox)
		
		// Section Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ActionsHeader", "Actions"))
			.Font(CPMStyle::HeaderFont())
			.ColorAndOpacity(CPMStyle::TextColor())
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			
			// Package Button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(PackageButton, SCPM_Button)
				.Text(LOCTEXT("PackageButton", "Package"))
				.ButtonStyle(ECPM_ButtonStyle::Secondary)
				.OnClicked(this, &SCPM_PakManagerWindow::HandlePackageClicked)
			]
			
			// Create Button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(CreateButton, SCPM_Button)
				.Text(LOCTEXT("CreateButton", "Create Asset"))
				.ButtonStyle(ECPM_ButtonStyle::Primary)
				.OnClicked(this, &SCPM_PakManagerWindow::HandleCreateClicked)
			]
		];
}

TSharedRef<SWidget> SCPM_PakManagerWindow::BuildStatusSection()
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(CPMStyle::RowBackground())
		.Padding(CPMStyle::InputPadding())
		[
			SAssignNew(StatusText, STextBlock)
			.Text(LOCTEXT("StatusReady", "Ready"))
			.Font(CPMStyle::BodyFont())
			.ColorAndOpacity(CPMStyle::HintTextColor())
		];
}

// Config Loading
TArray<FCPM_KeyValuePair> SCPM_PakManagerWindow::LoadAssetConfigFromJson()
{
	TArray<FCPM_KeyValuePair> Pairs;
	
	const FString ConfigPath = UCPM_UtilityLibrary::CPM_GetUIDefaultsFilePath(TEXT("AssetConfig.json"));
	
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ConfigPath))
	{
		CPM_LOG(Warning, TEXT("Failed to load AssetConfig.json from: %s"), *ConfigPath);
		return Pairs;
	}
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		CPM_LOG(Error, TEXT("Failed to parse AssetConfig.json"));
		return Pairs;
	}
	
	const TArray<TSharedPtr<FJsonValue>>* PairsArray;
	if (!JsonObject->TryGetArrayField(TEXT("Pairs"), PairsArray))
	{
		CPM_LOG(Error, TEXT("AssetConfig.json missing 'Pairs' array"));
		return Pairs;
	}
	
	for (const TSharedPtr<FJsonValue>& PairValue : *PairsArray)
	{
		const TSharedPtr<FJsonObject>* PairObject;
		if (!PairValue->TryGetObject(PairObject))
		{
			continue;
		}
		
		FCPM_KeyValuePair Pair;
		Pair.Key = (*PairObject)->GetStringField(TEXT("key"));
		Pair.Value = (*PairObject)->GetStringField(TEXT("value"));
		Pair.bKeyReadOnly = (*PairObject)->GetBoolField(TEXT("bKeyReadOnly"));
		Pair.bValueReadOnly = (*PairObject)->GetBoolField(TEXT("bValueReadOnly"));
		Pair.bUseDropdownForValue = (*PairObject)->GetBoolField(TEXT("bUseDropdownForValue"));
		Pair.bCannotRemove = (*PairObject)->GetBoolField(TEXT("bCannotRemove"));
		Pair.bIsHidden = (*PairObject)->GetBoolField(TEXT("bIsHidden"));
		
		// Load value options for dropdowns
		const TArray<TSharedPtr<FJsonValue>>* OptionsArray;
		if ((*PairObject)->TryGetArrayField(TEXT("valueOptions"), OptionsArray))
		{
			for (const TSharedPtr<FJsonValue>& OptionValue : *OptionsArray)
			{
				Pair.ValueOptions.Add(OptionValue->AsString());
			}
		}
		
		Pairs.Add(Pair);
	}
	
	CPM_LOG(Log, TEXT("Loaded %d pairs from AssetConfig.json"), Pairs.Num());
	return Pairs;
}

void SCPM_PakManagerWindow::PopulateAutoValues(TArray<FCPM_KeyValuePair>& Pairs)
{
	for (FCPM_KeyValuePair& Pair : Pairs)
	{
		if (Pair.Key == TEXT("project_name"))
		{
			Pair.Value = UCPM_UtilityLibrary::GetProjectName();
		}
		else if (Pair.Key == TEXT("current_ue_version"))
		{
			Pair.Value = FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);
		}
	}
}

// Getters
ECPM_AssetType SCPM_PakManagerWindow::GetSelectedAssetType() const
{
	FString AssetTypeStr = GetAssetInfoValue(TEXT("asset_type"));
	if (AssetTypeStr == TEXT("Scene"))
	{
		return ECPM_AssetType::Scene;
	}
	return ECPM_AssetType::Avatar;
}

TArray<FCPM_KeyValuePair> SCPM_PakManagerWindow::GetAssetInfoPairs() const
{
	if (AssetInfoList.IsValid())
	{
		return AssetInfoList->GetAllPairs();
	}
	return TArray<FCPM_KeyValuePair>();
}

FString SCPM_PakManagerWindow::GetAssetInfoValue(const FString& Key) const
{
	if (AssetInfoList.IsValid())
	{
		return AssetInfoList->GetValueForKey(Key);
	}
	return FString();
}

FString SCPM_PakManagerWindow::GetAssetInfoAsJson() const
{
	if (AssetInfoList.IsValid())
	{
		return AssetInfoList->GetPairsAsJsonString();
	}
	return TEXT("{}");
}

// Event Handlers
void SCPM_PakManagerWindow::HandleAssetInfoChanged(const TArray<FCPM_KeyValuePair>& Pairs)
{
	CPM_LOG(Log, TEXT("Asset info changed, %d pairs"), Pairs.Num());
}

FReply SCPM_PakManagerWindow::HandleCaptureThumbnailClicked()
{
	CPM_LOG(Log, TEXT("Capture thumbnail clicked"));
	UpdateStatus(TEXT("Capturing thumbnail... (Not yet implemented)"));
	
	// TODO: Implement viewport capture
	return FReply::Handled();
}

FReply SCPM_PakManagerWindow::HandlePackageClicked()
{
	CPM_LOG(Log, TEXT("Package clicked"));
	
	FString AssetName = GetAssetInfoValue(TEXT("asset_name"));
	
	if (AssetName.IsEmpty())
	{
		UpdateStatus(TEXT("Error: Asset name is required"), true);
		return FReply::Handled();
	}
	
	UpdateStatus(FString::Printf(TEXT("Packaging '%s'..."), *AssetName));
	
	// Log all the asset info
	CPM_LOG(Log, TEXT("Asset Info JSON: %s"), *GetAssetInfoAsJson());
	
	// TODO: Implement packaging logic
	return FReply::Handled();
}

FReply SCPM_PakManagerWindow::HandleCreateClicked()
{
	CPM_LOG(Log, TEXT("Create clicked"));
	
	FString AssetName = GetAssetInfoValue(TEXT("asset_name"));
	if (AssetName.IsEmpty())
	{
		UpdateStatus(TEXT("Error: Asset name is required"), true);
		return FReply::Handled();
	}
	
	UpdateStatus(FString::Printf(TEXT("Creating asset '%s'..."), *AssetName));
	
	// Log all the asset info
	CPM_LOG(Log, TEXT("Asset Info JSON: %s"), *GetAssetInfoAsJson());
	
	// TODO: Implement create asset logic
	return FReply::Handled();
}

void SCPM_PakManagerWindow::UpdateStatus(const FString& Message, bool bIsError)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(bIsError ? CPMStyle::Danger() : CPMStyle::HintTextColor());
	}
}

// Version Validation
void SCPM_PakManagerWindow::RunVersionValidation()
{
	if (VersionValidationHandle.IsValid())
	{
		UWorkflowBlueprintLibrary::CancelWorkflow(VersionValidationHandle, true);
	}

	DismissVersionAlert();
	UpdateStatus(TEXT("Validating versions..."));

	auto* PakVersionJob = NewObject<UCPM_PakManagerVersionCheckJob>();
	auto* UEVersionJob = NewObject<UCPM_ModdingToolUEVersionCheckJob>();

	TArray<TScriptInterface<IJobInterface>> Jobs;
	Jobs.Add(TScriptInterface<IJobInterface>(PakVersionJob));
	Jobs.Add(TScriptInterface<IJobInterface>(UEVersionJob));

	FWorkflowRequestFromJobs Request;
	Request.Jobs = Jobs;

	FCreateWorkflowFromJobsParams Params;
	Params.Request = Request;
	Params.bStartImmediately = false;

	VersionValidationHandle = UWorkflowBlueprintLibrary::CreateWorkflowFromJobs(Params);

	if (!VersionValidationHandle.IsValid())
	{
		UpdateStatus(TEXT("Failed to create version validation workflow"), true);
		return;
	}

	UWorkflowManagerSubsystem* Manager = GEngine->GetEngineSubsystem<UWorkflowManagerSubsystem>();
	TScriptInterface<IWorkflowInterface> WorkflowInterface = Manager->IGetWorkflow(VersionValidationHandle);
	if (UWorkflow* Workflow = Cast<UWorkflow>(WorkflowInterface.GetObject()))
	{
		TWeakPtr<SCPM_PakManagerWindow> WeakWindow(SharedThis(this));
		Workflow->SetEventCallback(FWorkflowEventCallback::CreateLambda(
			[WeakWindow, Manager](EWorkflowEventType EventType, const FWorkflowStatusInfo& StatusInfo)
			{
				Manager->HandleWorkflowEvent(EventType, StatusInfo);

				if (TSharedPtr<SCPM_PakManagerWindow> PinnedWindow = WeakWindow.Pin())
				{
					PinnedWindow->OnVersionValidationEvent(EventType, StatusInfo);
				}
			}));

		Workflow->IStartWorkflow();
	}
}

void SCPM_PakManagerWindow::DismissVersionAlert()
{
	if (VersionAlertBanner.IsValid())
	{
		VersionAlertBanner->Hide();
	}
}

void SCPM_PakManagerWindow::OnVersionValidationEvent(EWorkflowEventType EventType, const FWorkflowStatusInfo& StatusInfo)
{
	switch (EventType)
	{
	case EWorkflowEventType::WorkflowCompleted:
		DismissVersionAlert();
		UpdateStatus(TEXT("Ready"));
		break;

	case EWorkflowEventType::WorkflowFailed:
		if (VersionAlertBanner.IsValid())
		{
			VersionAlertBanner->SetMessage(FText::FromString(StatusInfo.ErrorMessage));
			VersionAlertBanner->Show();
		}
		UpdateStatus(TEXT("Version validation failed"), true);
		break;

	case EWorkflowEventType::JobStarted:
		UpdateStatus(FString::Printf(TEXT("Checking: %s..."), *StatusInfo.CurrentJob.Name));
		break;

	default:
		break;
	}
}

#undef LOCTEXT_NAMESPACE
