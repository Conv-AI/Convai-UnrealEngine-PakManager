// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Type/JS_Definations.h"
#include "WorkflowManagerInterface.generated.h"

class UWorkflowContext;
class IJobInterface;

UINTERFACE(BlueprintType, MinimalAPI)
class UWorkflowManagerInterface : public UInterface
{
	GENERATED_BODY()
};

class CONVAIJOBSYSTEM_API IWorkflowManagerInterface
{
	GENERATED_BODY()

public:
	virtual bool ExecuteWorkflow(const FWorkflowConfig& Config, const TArray<TScriptInterface<IJobInterface>>& Jobs) = 0;
	virtual void CancelWorkflow(bool bForce = false) = 0;
	virtual EWorkflowStatus GetStatus() const = 0;
	virtual bool IsRunning() const = 0;
	virtual float GetProgress() const = 0;
	virtual UWorkflowContext* GetContext() const = 0;
	virtual void OnJobCompleted(UObject* Job, EJobResult Result, const FString& ErrorMessage = TEXT("")) = 0;
	virtual bool IsCancellationRequested() const = 0;
};
