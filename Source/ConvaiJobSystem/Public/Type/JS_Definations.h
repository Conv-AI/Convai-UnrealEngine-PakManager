// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JS_Definations.generated.h"

class IJobInterface;
class IWorkflowListenerInterface;

UENUM(BlueprintType)
enum class EJobResult : uint8
{
	Pending		UMETA(DisplayName = "Pending"),
	InProgress	UMETA(DisplayName = "In Progress"),
	Success		UMETA(DisplayName = "Success"),
	Failed		UMETA(DisplayName = "Failed"),
	Cancelled	UMETA(DisplayName = "Cancelled"),
	Timeout		UMETA(DisplayName = "Timeout"),
	Skipped		UMETA(DisplayName = "Skipped")
};

UENUM(BlueprintType)
enum class EWorkflowStatus : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Initialized	UMETA(DisplayName = "Initialized"),
	Running		UMETA(DisplayName = "Running"),
	Cancelling	UMETA(DisplayName = "Cancelling"),
	Completed	UMETA(DisplayName = "Completed"),
	Failed		UMETA(DisplayName = "Failed"),
	Timeout		UMETA(DisplayName = "Timeout"),
	Cancelled	UMETA(DisplayName = "Cancelled")
};

UENUM(BlueprintType)
enum class EWorkflowEventType : uint8
{
	WorkflowStarted		UMETA(DisplayName = "Workflow Started"),
	WorkflowCompleted	UMETA(DisplayName = "Workflow Completed"),
	WorkflowFailed		UMETA(DisplayName = "Workflow Failed"),
	WorkflowCancelled	UMETA(DisplayName = "Workflow Cancelled"),
	WorkflowTimeout		UMETA(DisplayName = "Workflow Timeout"),
	JobStarted			UMETA(DisplayName = "Job Started"),
	JobCompleted		UMETA(DisplayName = "Job Completed"),
	JobFailed			UMETA(DisplayName = "Job Failed"),
	JobTimeout			UMETA(DisplayName = "Job Timeout"),
	JobSkipped			UMETA(DisplayName = "Job Skipped"),
	JobRetrying			UMETA(DisplayName = "Job Retrying"),
	ProgressUpdated		UMETA(DisplayName = "Progress Updated")
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowHandle
{
	GENERATED_BODY()

	FWorkflowHandle() = default;
	explicit FWorkflowHandle(const FGuid& InId) : Id(InId) {}

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	FGuid Id;

	bool IsValid() const { return Id.IsValid(); }

	static FWorkflowHandle Generate() { return FWorkflowHandle(FGuid::NewGuid()); }
	static FWorkflowHandle Invalid() { return FWorkflowHandle(); }

	bool operator==(const FWorkflowHandle& Other) const { return Id == Other.Id; }
	bool operator!=(const FWorkflowHandle& Other) const { return Id != Other.Id; }

	friend uint32 GetTypeHash(const FWorkflowHandle& Handle) { return GetTypeHash(Handle.Id); }
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FJobConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	bool bOverrideWorkflowDefaults = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "bOverrideWorkflowDefaults"))
	float TimeoutSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "bOverrideWorkflowDefaults"))
	int32 MaxRetries = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "bOverrideWorkflowDefaults"))
	float RetryDelaySeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "bOverrideWorkflowDefaults"))
	bool bRetryOnTimeout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "bOverrideWorkflowDefaults"))
	bool bRetryOnFailure = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "bOverrideWorkflowDefaults"))
	bool bContinueWorkflowOnFailure = false;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FJobStatusInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	int32 Index = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	EJobResult Result = EJobResult::Pending;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	TScriptInterface<IJobInterface> JobObject = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	int32 RetryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	FText ProgressText;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	float ElapsedTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	FString ErrorMessage;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowStatusInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	FWorkflowHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	EWorkflowStatus Status = EWorkflowStatus::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	int32 TotalJobs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	float ElapsedTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	FJobStatusInfo CurrentJob;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	float WorkflowTimeoutSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	FJobConfig DefaultJobConfig;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FJobCompletionInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	TScriptInterface<IJobInterface> Job;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	EJobResult Result = EJobResult::Failed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	FString ErrorMessage;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FJobProgressInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	TScriptInterface<IJobInterface> Job;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Progress = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	FText ProgressText;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FJobDefinition
{
	GENERATED_BODY()

	/** The job class to instantiate. Must implement IJobInterface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	TSubclassOf<UObject> JobClass;

	/** Optional per-job configuration override. Will be passed in IInitialize */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	FJobConfig JobConfig;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowRequestOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	FWorkflowConfig WorkflowConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	TArray<TScriptInterface<IWorkflowListenerInterface>> Listeners;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowRequestFromJobs
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	FWorkflowRequestOptions Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	TArray<TScriptInterface<IJobInterface>> Jobs;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowRequestFromJobDefinitions
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	FWorkflowRequestOptions Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	TArray<FJobDefinition> JobDefinitions;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FCreateWorkflowFromJobsParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	FWorkflowRequestFromJobs Request;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	bool bStartImmediately = true;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FCreateWorkflowFromJobDefinitionsParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	FWorkflowRequestFromJobDefinitions Request;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workflow")
	bool bStartImmediately = true;
};
