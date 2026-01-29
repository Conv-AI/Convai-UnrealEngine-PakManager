// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/ICPM_Service.h"
#include "Infrastructure/CPM_Result.h"
#include "Infrastructure/CPM_AsyncOperation.h"
#include "Infrastructure/CPM_CancellationToken.h"
#include "Utility/CPM_Utils.h"
#include "Types/CPM_WidgetTypes.h"

/**
 * Workflow state for tracking multi-step operations
 */
UENUM(BlueprintType)
enum class ECPM_WorkflowState : uint8
{
	Idle,
	FetchingConfig,
	Packaging,
	CreatingAsset,
	UploadingPak,
	UpdatingAsset,
	Completed,
	Failed,
	Cancelled
};

/**
 * Workflow progress info
 */
struct FCPM_WorkflowProgress
{
	ECPM_WorkflowState State = ECPM_WorkflowState::Idle;
	float Progress = 0.0f;  // 0.0 to 1.0
	FString Message;
	FString CurrentStep;
	int32 CurrentStepIndex = 0;
	int32 TotalSteps = 0;
};

/**
 * Parameters for the Create Asset workflow
 */
struct FCPM_CreateAssetParams
{
	/** All key-value pairs from the UI */
	TArray<FCPM_KeyValuePair> AssetInfo;
	
	/** Asset type (Avatar/Scene) */
	ECPM_AssetType AssetType = ECPM_AssetType::Avatar;
	
	/** Thumbnail texture (optional) */
	TWeakObjectPtr<UTexture2D> Thumbnail;
	
	/** Chunk ID for packaging */
	int32 ChunkId = 100;
	
	/** Get a value by key */
	FString GetValue(const FString& Key) const
	{
		for (const FCPM_KeyValuePair& Pair : AssetInfo)
		{
			if (Pair.Key == Key)
			{
				return Pair.Value;
			}
		}
		return FString();
	}
};

/**
 * Result of the Create Asset workflow
 */
struct FCPM_CreateAssetResult
{
	FString AssetId;
	FString TransactionId;
	TArray<FString> PakFilePaths;
	bool bSuccess = false;
	FString ErrorMessage;
};

/**
 * Workflow Service - Orchestrates complex multi-step operations
 */
class CONVAIPAKMANAGEREDITOR_API FCPM_WorkflowService : public ICPM_Service
{
public:
	FCPM_WorkflowService();
	virtual ~FCPM_WorkflowService();

	//~ ICPM_Service Interface
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual FName GetServiceName() const override { return TEXT("WorkflowService"); }

	/** Progress delegate */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorkflowProgress, const FCPM_WorkflowProgress& /* Progress */);
	FOnWorkflowProgress& OnProgress() { return OnProgressDelegate; }

	//~ Workflow Operations

	/**
	 * Start the Create Asset workflow
	 * Steps: FetchConfig -> Package -> CreateAsset -> UploadPak
	 */
	TSharedPtr<FCPM_AsyncOperation<FCPM_CreateAssetResult>> StartCreateAssetWorkflow(
		const FCPM_CreateAssetParams& Params);

	/**
	 * Start the Package Only workflow
	 * Steps: FetchConfig -> Package
	 */
	TSharedPtr<FCPM_AsyncOperation<TArray<FString>>> StartPackageWorkflow(
		const FCPM_CreateAssetParams& Params);

	/**
	 * Cancel the current workflow
	 */
	void CancelCurrentWorkflow();

	/**
	 * Get current workflow state
	 */
	ECPM_WorkflowState GetCurrentState() const;

	/**
	 * Check if a workflow is running
	 */
	bool IsWorkflowRunning() const;

private:
	// Internal workflow steps
	TCPM_Result<void> FetchGithubConfig(TSharedPtr<FCPM_CancellationToken> Token);
	TCPM_Result<TArray<FString>> ExecutePackaging(const FCPM_CreateAssetParams& Params, TSharedPtr<FCPM_CancellationToken> Token);
	TCPM_Result<FCPM_CreatedAssets> CreateAssetOnBackend(const FCPM_CreateAssetParams& Params, TSharedPtr<FCPM_CancellationToken> Token);
	TCPM_Result<void> UploadPakFiles(const TArray<FString>& PakPaths, const FCPM_CreatedAssets& CreatedAssets, TSharedPtr<FCPM_CancellationToken> Token);

	// Progress reporting
	void ReportProgress(ECPM_WorkflowState State, float Progress, const FString& Message, int32 Step = 0, int32 TotalSteps = 0);

	// State
	ECPM_WorkflowState CurrentState = ECPM_WorkflowState::Idle;
	TSharedPtr<FCPM_CancellationTokenSource> CurrentTokenSource;
	FOnWorkflowProgress OnProgressDelegate;
	mutable FCriticalSection StateMutex;
};

/**
 * Global accessor for the workflow service (simple singleton pattern)
 */
class CONVAIPAKMANAGEREDITOR_API FCPM_WorkflowServiceManager
{
public:
	static TSharedPtr<FCPM_WorkflowService> Get();
	static void Initialize();
	static void Shutdown();

private:
	static TSharedPtr<FCPM_WorkflowService> Instance;
};
