// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JS_Definations.generated.h"

class IJobInterface;

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
	float ElapsedTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	FString ErrorMessage;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowStatusInfo
{
	GENERATED_BODY()

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
