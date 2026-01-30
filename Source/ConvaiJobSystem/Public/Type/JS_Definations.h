// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JS_Definations.generated.h"

UENUM(BlueprintType)
enum class EJobResult : uint8
{
	Success		UMETA(DisplayName = "Success"),
	Failed		UMETA(DisplayName = "Failed"),
	Cancelled	UMETA(DisplayName = "Cancelled"),
	Timeout		UMETA(DisplayName = "Timeout")
};

UENUM(BlueprintType)
enum class EWorkflowStatus : uint8
{
	Idle					UMETA(DisplayName = "Idle"),
	Running					UMETA(DisplayName = "Running"),
	CancellationRequested	UMETA(DisplayName = "Cancellation Requested"),
	Completed				UMETA(DisplayName = "Completed"),
	Failed					UMETA(DisplayName = "Failed"),
	Timeout					UMETA(DisplayName = "Timeout"),
	Cancelled				UMETA(DisplayName = "Cancelled")
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FJobConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	bool bUseWorkflowConfig = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "!bUseWorkflowConfig"))
	float TimeoutSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "!bUseWorkflowConfig"))
	int32 MaxRetries = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "!bUseWorkflowConfig"))
	float RetryDelaySeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "!bUseWorkflowConfig"))
	bool bRetryOnTimeout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "!bUseWorkflowConfig"))
	bool bRetryOnFailure = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (EditCondition = "!bUseWorkflowConfig"))
	bool bContinueWorkflowOnFailure = false;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FJobStatusEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	int32 JobIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	EJobResult Result = EJobResult::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	TObjectPtr<UObject> Job = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	int32 RetryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Job")
	FString ErrorMessage;
};

USTRUCT(BlueprintType)
struct CONVAIJOBSYSTEM_API FWorkflowStatusEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	EWorkflowStatus Status = EWorkflowStatus::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Workflow")
	float ElapsedTime = 0.0f;
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
