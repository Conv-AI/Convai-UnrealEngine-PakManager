// Copyright 2022 Convai Inc. All Rights Reserved.

#include "Services/CPM_WorkflowService.h"
#include "Services/CPM_ConfigService.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"
#include "Proxy/CPM_Proxy.h"
#include "Proxy/CPM_GithubProxy.h"

// Static instance
TSharedPtr<FCPM_WorkflowService> FCPM_WorkflowServiceManager::Instance = nullptr;

TSharedPtr<FCPM_WorkflowService> FCPM_WorkflowServiceManager::Get()
{
	if (!Instance.IsValid())
	{
		Initialize();
	}
	return Instance;
}

void FCPM_WorkflowServiceManager::Initialize()
{
	if (!Instance.IsValid())
	{
		Instance = MakeShared<FCPM_WorkflowService>();
		Instance->Initialize();
		CPM_LOG(Log, TEXT("WorkflowService initialized"));
	}
}

void FCPM_WorkflowServiceManager::Shutdown()
{
	if (Instance.IsValid())
	{
		Instance->Shutdown();
		Instance.Reset();
		CPM_LOG(Log, TEXT("WorkflowService shutdown"));
	}
}

FCPM_WorkflowService::FCPM_WorkflowService()
{
}

FCPM_WorkflowService::~FCPM_WorkflowService()
{
	Shutdown();
}

void FCPM_WorkflowService::Initialize()
{
	CPM_LOG(Log, TEXT("FCPM_WorkflowService::Initialize"));
}

void FCPM_WorkflowService::Shutdown()
{
	CancelCurrentWorkflow();
	CPM_LOG(Log, TEXT("FCPM_WorkflowService::Shutdown"));
}

ECPM_WorkflowState FCPM_WorkflowService::GetCurrentState() const
{
	FScopeLock Lock(&StateMutex);
	return CurrentState;
}

bool FCPM_WorkflowService::IsWorkflowRunning() const
{
	FScopeLock Lock(&StateMutex);
	return CurrentState != ECPM_WorkflowState::Idle && 
	       CurrentState != ECPM_WorkflowState::Completed && 
	       CurrentState != ECPM_WorkflowState::Failed &&
	       CurrentState != ECPM_WorkflowState::Cancelled;
}

void FCPM_WorkflowService::CancelCurrentWorkflow()
{
	if (CurrentTokenSource.IsValid())
	{
		CurrentTokenSource->Cancel();
		
		FScopeLock Lock(&StateMutex);
		CurrentState = ECPM_WorkflowState::Cancelled;
	}
	
	ReportProgress(ECPM_WorkflowState::Cancelled, 0.0f, TEXT("Workflow cancelled"));
}

void FCPM_WorkflowService::ReportProgress(ECPM_WorkflowState State, float Progress, const FString& Message, int32 Step, int32 TotalSteps)
{
	{
		FScopeLock Lock(&StateMutex);
		CurrentState = State;
	}

	FCPM_WorkflowProgress ProgressInfo;
	ProgressInfo.State = State;
	ProgressInfo.Progress = Progress;
	ProgressInfo.Message = Message;
	ProgressInfo.CurrentStepIndex = Step;
	ProgressInfo.TotalSteps = TotalSteps;

	// Broadcast on game thread
	AsyncTask(ENamedThreads::GameThread, [this, ProgressInfo]()
	{
		OnProgressDelegate.Broadcast(ProgressInfo);
	});

	CPM_LOG(Log, TEXT("Workflow Progress: [%d/%d] %s (%.0f%%)"), Step, TotalSteps, *Message, Progress * 100.0f);
}

TSharedPtr<FCPM_AsyncOperation<FCPM_CreateAssetResult>> FCPM_WorkflowService::StartCreateAssetWorkflow(
	const FCPM_CreateAssetParams& Params)
{
	// Create cancellation token source
	CurrentTokenSource = MakeShared<FCPM_CancellationTokenSource>();
	auto Token = CurrentTokenSource->GetToken();

	// Create the async operation
	auto Operation = MakeShared<FCPM_AsyncOperation<FCPM_CreateAssetResult>>(
		[this, Params](TSharedPtr<FCPM_CancellationToken> CancelToken, TSharedPtr<ICPM_ProgressReporter> Progress) -> TCPM_Result<FCPM_CreateAssetResult>
		{
			FCPM_CreateAssetResult Result;
			const int32 TotalSteps = 4;

			// Step 1: Fetch GitHub config
			ReportProgress(ECPM_WorkflowState::FetchingConfig, 0.1f, TEXT("Fetching configuration..."), 1, TotalSteps);
			auto ConfigResult = FetchGithubConfig(CancelToken);
			if (ConfigResult.IsFailure())
			{
				Result.bSuccess = false;
				Result.ErrorMessage = ConfigResult.GetError();
				return TCPM_Result<FCPM_CreateAssetResult>::Failure(ConfigResult.GetError());
			}

			if (CancelToken->IsCancellationRequested())
			{
				return TCPM_Result<FCPM_CreateAssetResult>::Failure(TEXT("Cancelled"));
			}

			// Step 2: Package
			ReportProgress(ECPM_WorkflowState::Packaging, 0.3f, TEXT("Packaging project..."), 2, TotalSteps);
			auto PackageResult = ExecutePackaging(Params, CancelToken);
			if (PackageResult.IsFailure())
			{
				Result.bSuccess = false;
				Result.ErrorMessage = PackageResult.GetError();
				return TCPM_Result<FCPM_CreateAssetResult>::Failure(PackageResult.GetError());
			}
			Result.PakFilePaths = PackageResult.GetValue();

			if (CancelToken->IsCancellationRequested())
			{
				return TCPM_Result<FCPM_CreateAssetResult>::Failure(TEXT("Cancelled"));
			}

			// Step 3: Create Asset
			ReportProgress(ECPM_WorkflowState::CreatingAsset, 0.6f, TEXT("Creating asset on server..."), 3, TotalSteps);
			auto CreateResult = CreateAssetOnBackend(Params, CancelToken);
			if (CreateResult.IsFailure())
			{
				Result.bSuccess = false;
				Result.ErrorMessage = CreateResult.GetError();
				return TCPM_Result<FCPM_CreateAssetResult>::Failure(CreateResult.GetError());
			}
			auto CreatedAssets = CreateResult.GetValue();
			Result.TransactionId = CreatedAssets.TransactionID;
			if (CreatedAssets.Assets.Num() > 0)
			{
				Result.AssetId = CreatedAssets.Assets[0].Asset.AssetId;
			}

			if (CancelToken->IsCancellationRequested())
			{
				return TCPM_Result<FCPM_CreateAssetResult>::Failure(TEXT("Cancelled"));
			}

			// Step 4: Upload Pak
			ReportProgress(ECPM_WorkflowState::UploadingPak, 0.8f, TEXT("Uploading pak files..."), 4, TotalSteps);
			auto UploadResult = UploadPakFiles(Result.PakFilePaths, CreatedAssets, CancelToken);
			if (UploadResult.IsFailure())
			{
				Result.bSuccess = false;
				Result.ErrorMessage = UploadResult.GetError();
				return TCPM_Result<FCPM_CreateAssetResult>::Failure(UploadResult.GetError());
			}

			// Complete
			ReportProgress(ECPM_WorkflowState::Completed, 1.0f, TEXT("Asset created successfully!"), TotalSteps, TotalSteps);
			Result.bSuccess = true;
			return TCPM_Result<FCPM_CreateAssetResult>::Success(Result);
		},
		Token
	);

	Operation->Start();
	return Operation;
}

TSharedPtr<FCPM_AsyncOperation<TArray<FString>>> FCPM_WorkflowService::StartPackageWorkflow(
	const FCPM_CreateAssetParams& Params)
{
	// Create cancellation token source
	CurrentTokenSource = MakeShared<FCPM_CancellationTokenSource>();
	auto Token = CurrentTokenSource->GetToken();

	auto Operation = MakeShared<FCPM_AsyncOperation<TArray<FString>>>(
		[this, Params](TSharedPtr<FCPM_CancellationToken> CancelToken, TSharedPtr<ICPM_ProgressReporter> Progress) -> TCPM_Result<TArray<FString>>
		{
			const int32 TotalSteps = 2;

			// Step 1: Fetch GitHub config
			ReportProgress(ECPM_WorkflowState::FetchingConfig, 0.2f, TEXT("Fetching configuration..."), 1, TotalSteps);
			auto ConfigResult = FetchGithubConfig(CancelToken);
			if (ConfigResult.IsFailure())
			{
				return TCPM_Result<TArray<FString>>::Failure(ConfigResult.GetError());
			}

			if (CancelToken->IsCancellationRequested())
			{
				return TCPM_Result<TArray<FString>>::Failure(TEXT("Cancelled"));
			}

			// Step 2: Package
			ReportProgress(ECPM_WorkflowState::Packaging, 0.5f, TEXT("Packaging project..."), 2, TotalSteps);
			auto PackageResult = ExecutePackaging(Params, CancelToken);
			if (PackageResult.IsFailure())
			{
				return TCPM_Result<TArray<FString>>::Failure(PackageResult.GetError());
			}

			// Complete
			ReportProgress(ECPM_WorkflowState::Completed, 1.0f, TEXT("Packaging complete!"), TotalSteps, TotalSteps);
			return TCPM_Result<TArray<FString>>::Success(PackageResult.GetValue());
		},
		Token
	);

	Operation->Start();
	return Operation;
}

// Workflow Steps Implementation

TCPM_Result<void> FCPM_WorkflowService::FetchGithubConfig(TSharedPtr<FCPM_CancellationToken> Token)
{
	CPM_LOG(Log, TEXT("FetchGithubConfig: Fetching configuration..."));
	
	auto ConfigService = FCPM_ConfigServiceManager::Get();
	if (!ConfigService.IsValid())
	{
		return TCPM_Result<void>::Failure(TEXT("Config service not available"));
	}
	
	// Check cancellation
	if (Token.IsValid() && Token->IsCancellationRequested())
	{
		return TCPM_Result<void>::Failure(TEXT("Cancelled"));
	}
	
	// Start the async fetch
	auto Operation = ConfigService->FetchPackagingConfig();
	
	// Wait for completion (blocking - we're already on a background thread)
	auto Result = Operation->GetResult();
	
	if (Result.IsFailure())
	{
		CPM_LOG(Error, TEXT("Failed to fetch config: %s"), *Result.GetError());
		return TCPM_Result<void>::Failure(Result.GetError());
	}
	
	auto Config = Result.GetValue();
	
	// Log the config details
	TArray<FString> Platforms = Config.GetPackagingPlatforms();
	CPM_LOG(Log, TEXT("FetchGithubConfig: Success - Platforms: [%s], RawUpload: %s, FromFallback: %s"), 
		*FString::Join(Platforms, TEXT(", ")),
		Config.bRawProjectUpload ? TEXT("Yes") : TEXT("No"),
		Config.bIsFromFallback ? TEXT("Yes") : TEXT("No"));
	
	return TCPM_Result<void>::Success();
}

TCPM_Result<TArray<FString>> FCPM_WorkflowService::ExecutePackaging(
	const FCPM_CreateAssetParams& Params, 
	TSharedPtr<FCPM_CancellationToken> Token)
{
	// TODO: Implement actual packaging using UAT
	// For now, return placeholder
	CPM_LOG(Log, TEXT("ExecutePackaging: Placeholder - would run UAT packaging"));
	
	// Simulate packaging time
	FPlatformProcess::Sleep(1.0f);
	
	if (Token->IsCancellationRequested())
	{
		return TCPM_Result<TArray<FString>>::Failure(TEXT("Cancelled"));
	}
	
	TArray<FString> PakPaths;
	PakPaths.Add(TEXT("Placeholder_Win64.pak"));
	PakPaths.Add(TEXT("Placeholder_Linux.pak"));
	
	return TCPM_Result<TArray<FString>>::Success(PakPaths);
}

TCPM_Result<FCPM_CreatedAssets> FCPM_WorkflowService::CreateAssetOnBackend(
	const FCPM_CreateAssetParams& Params, 
	TSharedPtr<FCPM_CancellationToken> Token)
{
	// TODO: Implement actual API call using existing proxy
	CPM_LOG(Log, TEXT("CreateAssetOnBackend: Placeholder - would call API"));
	
	// Simulate API call
	FPlatformProcess::Sleep(0.5f);
	
	if (Token->IsCancellationRequested())
	{
		return TCPM_Result<FCPM_CreatedAssets>::Failure(TEXT("Cancelled"));
	}
	
	FCPM_CreatedAssets CreatedAssets;
	CreatedAssets.TransactionID = TEXT("placeholder_transaction_id");
	
	FCPM_Asset Asset;
	Asset.Asset.AssetId = TEXT("placeholder_asset_id");
	CreatedAssets.Assets.Add(Asset);
	
	return TCPM_Result<FCPM_CreatedAssets>::Success(CreatedAssets);
}

TCPM_Result<void> FCPM_WorkflowService::UploadPakFiles(
	const TArray<FString>& PakPaths, 
	const FCPM_CreatedAssets& CreatedAssets, 
	TSharedPtr<FCPM_CancellationToken> Token)
{
	// TODO: Implement actual upload using existing proxy
	CPM_LOG(Log, TEXT("UploadPakFiles: Placeholder - would upload %d files"), PakPaths.Num());
	
	// Simulate upload
	FPlatformProcess::Sleep(1.0f);
	
	if (Token->IsCancellationRequested())
	{
		return TCPM_Result<void>::Failure(TEXT("Cancelled"));
	}
	
	return TCPM_Result<void>::Success();
}
