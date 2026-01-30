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
	virtual bool IExecuteWorkflow(const FWorkflowRequest& Request) = 0;
	virtual void ICancelWorkflow(bool bForce = false) = 0;
	virtual FWorkflowStatusInfo IGetStatusInfo() = 0;
	virtual UWorkflowContext* IGetContext() = 0;
	virtual void IOnJobCompleted(const FJobCompletionInfo& CompletionInfo) = 0;
	virtual void IReportJobProgress(const FJobProgressInfo& ProgressInfo) = 0;
	virtual void IAddListener(const TScriptInterface<IWorkflowListenerInterface>& Listener) = 0;
	virtual void IRemoveListener(const TScriptInterface<IWorkflowListenerInterface>& Listener) = 0;
};
