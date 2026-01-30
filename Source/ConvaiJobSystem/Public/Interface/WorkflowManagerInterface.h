// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Type/JS_Definations.h"
#include "WorkflowManagerInterface.generated.h"

class UWorkflowContext;
class IJobInterface;
class IWorkflowListenerInterface;

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
	virtual FWorkflowStatusInfo GetStatusInfo() = 0;
	virtual UWorkflowContext* GetContext() = 0;
	virtual void OnJobCompleted(const TScriptInterface<IJobInterface>& Job, EJobResult Result, const FString& ErrorMessage = TEXT("")) = 0;
	virtual void ReportJobProgress(const TScriptInterface<IJobInterface>& Job, float Progress) = 0;
	virtual void AddListener(const TScriptInterface<IWorkflowListenerInterface>& Listener) = 0;
	virtual void RemoveListener(const TScriptInterface<IWorkflowListenerInterface>& Listener) = 0;
};
