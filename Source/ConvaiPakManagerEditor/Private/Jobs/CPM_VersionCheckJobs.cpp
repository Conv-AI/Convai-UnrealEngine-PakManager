// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Jobs/CPM_VersionCheckJobs.h"
#include "Proxy/CPM_GithubProxy.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogCPMVersionCheck, Log, All);

//////////////////////////////////////////////////////////////////////////
// UCPM_PakManagerVersionCheckJob
//////////////////////////////////////////////////////////////////////////

static const FString PakManagerRepo	= TEXT("Conv-AI/Convai-UnrealEngine-PakManager");
static const FString PakManagerBranch	= TEXT("rfctr/slate");
static const FString PakManagerFile	= TEXT("ConvaiPakManager.uplugin");

UCPM_PakManagerVersionCheckJob::UCPM_PakManagerVersionCheckJob()
{
	JobConfig.Name = TEXT("Pak Manager Version Check");
	JobConfig.Description = TEXT("Validates local ConvaiPakManager plugin version against the remote repository");
}

void UCPM_PakManagerVersionCheckJob::IPreInitialize_Implementation(const FJobDefinition& Definition)
{
	JobConfig = Definition.JobConfig;
}

void UCPM_PakManagerVersionCheckJob::IInitialize_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow)
{
	CachedWorkflow = Workflow;
}

void UCPM_PakManagerVersionCheckJob::IExecute_Implementation()
{
	bIsCancelled = false;

	UE_LOG(LogCPMVersionCheck, Log, TEXT("PakManagerVersionCheck: Fetching remote plugin descriptor..."));

	ActiveProxy = UCPM_GetGithubRepoFileProxy::GetGithubRepoFileProxy(PakManagerRepo, PakManagerBranch, PakManagerFile);

	if (!ActiveProxy)
	{
		NotifyCompletion(EJobResult::Failed, TEXT("Failed to create GitHub file request"));
		return;
	}

	ActiveProxy->OnSuccess.AddDynamic(this, &UCPM_PakManagerVersionCheckJob::OnFileReceived);
	ActiveProxy->OnFailure.AddDynamic(this, &UCPM_PakManagerVersionCheckJob::OnFileFetchFailed);
	ActiveProxy->Activate();
}

void UCPM_PakManagerVersionCheckJob::ICancel_Implementation(bool bForce)
{
	bIsCancelled = true;
	ActiveProxy = nullptr;

	UE_LOG(LogCPMVersionCheck, Log, TEXT("PakManagerVersionCheck: Cancelled"));
	NotifyCompletion(EJobResult::Cancelled);
}

void UCPM_PakManagerVersionCheckJob::OnFileReceived(const FString& ResponseString)
{
	if (bIsCancelled) return;

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		NotifyCompletion(EJobResult::Failed, TEXT("Failed to parse remote ConvaiPakManager.uplugin JSON"));
		return;
	}

	int32 RemoteVersion = 0;
	if (!JsonObject->TryGetNumberField(TEXT("Version"), RemoteVersion))
	{
		NotifyCompletion(EJobResult::Failed, TEXT("Remote ConvaiPakManager.uplugin does not contain a 'Version' field"));
		return;
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ConvaiPakManager"));
	if (!Plugin.IsValid())
	{
		NotifyCompletion(EJobResult::Failed, TEXT("ConvaiPakManager plugin not found in this project"));
		return;
	}

	const int32 LocalVersion = Plugin->GetDescriptor().Version;

	UE_LOG(LogCPMVersionCheck, Log, TEXT("PakManagerVersionCheck: Local=%d  Remote=%d"), LocalVersion, RemoteVersion);

	if (LocalVersion < RemoteVersion)
	{
		const FString Error = FString::Printf(
			TEXT("ConvaiPakManager is outdated (local: %d, remote: %d). Please update the plugin."),
			LocalVersion, RemoteVersion);
		NotifyCompletion(EJobResult::Failed, Error);
	}
	else
	{
		NotifyCompletion(EJobResult::Success);
	}
}

void UCPM_PakManagerVersionCheckJob::OnFileFetchFailed(const FString& ResponseString)
{
	if (bIsCancelled) return;

	const FString Error = FString::Printf(
		TEXT("Failed to fetch remote ConvaiPakManager.uplugin: %s"), *ResponseString);
	NotifyCompletion(EJobResult::Failed, Error);
}

void UCPM_PakManagerVersionCheckJob::NotifyCompletion(EJobResult Result, const FString& ErrorMessage)
{
	ActiveProxy = nullptr;

	if (CachedWorkflow.GetInterface())
	{
		FJobCompletionInfo Completion;
		Completion.Job = this;
		Completion.Result = Result;
		Completion.ErrorMessage = ErrorMessage;
		CachedWorkflow->IOnJobCompleted(Completion);
	}
}

//////////////////////////////////////////////////////////////////////////
// UCPM_ModdingToolUEVersionCheckJob
//////////////////////////////////////////////////////////////////////////

static const FString ModdingToolRepo	= TEXT("Conv-AI/Convai-UnrealEngine-ModdingTool");
static const FString ModdingToolBranch	= TEXT("main");
static const FString ModdingToolFile	= TEXT("Version.json");

UCPM_ModdingToolUEVersionCheckJob::UCPM_ModdingToolUEVersionCheckJob()
{
	JobConfig.Name = TEXT("Modding Tool UE Version Check");
	JobConfig.Description = TEXT("Validates current Unreal Engine version against the Modding Tool target version");
}

void UCPM_ModdingToolUEVersionCheckJob::IPreInitialize_Implementation(const FJobDefinition& Definition)
{
	JobConfig = Definition.JobConfig;
}

void UCPM_ModdingToolUEVersionCheckJob::IInitialize_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow)
{
	CachedWorkflow = Workflow;
}

void UCPM_ModdingToolUEVersionCheckJob::IExecute_Implementation()
{
	bIsCancelled = false;

	UE_LOG(LogCPMVersionCheck, Log, TEXT("UEVersionCheck: Fetching remote Version.json..."));

	ActiveProxy = UCPM_GetGithubRepoFileProxy::GetGithubRepoFileProxy(ModdingToolRepo, ModdingToolBranch, ModdingToolFile);

	if (!ActiveProxy)
	{
		NotifyCompletion(EJobResult::Failed, TEXT("Failed to create GitHub file request"));
		return;
	}

	ActiveProxy->OnSuccess.AddDynamic(this, &UCPM_ModdingToolUEVersionCheckJob::OnFileReceived);
	ActiveProxy->OnFailure.AddDynamic(this, &UCPM_ModdingToolUEVersionCheckJob::OnFileFetchFailed);
	ActiveProxy->Activate();
}

void UCPM_ModdingToolUEVersionCheckJob::ICancel_Implementation(bool bForce)
{
	bIsCancelled = true;
	ActiveProxy = nullptr;

	UE_LOG(LogCPMVersionCheck, Log, TEXT("UEVersionCheck: Cancelled"));
	NotifyCompletion(EJobResult::Cancelled);
}

void UCPM_ModdingToolUEVersionCheckJob::OnFileReceived(const FString& ResponseString)
{
	if (bIsCancelled) return;

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		NotifyCompletion(EJobResult::Failed, TEXT("Failed to parse remote Version.json"));
		return;
	}

	FString TargetUEVersion;
	if (!JsonObject->TryGetStringField(TEXT("target-ue-version"), TargetUEVersion))
	{
		NotifyCompletion(EJobResult::Failed, TEXT("Remote Version.json does not contain a 'target-ue-version' field"));
		return;
	}

	const FString CurrentUEVersion = FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);

	UE_LOG(LogCPMVersionCheck, Log, TEXT("UEVersionCheck: Current=%s  Target=%s"), *CurrentUEVersion, *TargetUEVersion);

	if (!CurrentUEVersion.Equals(TargetUEVersion, ESearchCase::IgnoreCase))
	{
		const FString Error = FString::Printf(
			TEXT("Unreal Engine version mismatch. Current: %s, Required: %s. The Modding Tool requires UE %s."),
			*CurrentUEVersion, *TargetUEVersion, *TargetUEVersion);
		NotifyCompletion(EJobResult::Failed, Error);
	}
	else
	{
		NotifyCompletion(EJobResult::Success);
	}
}

void UCPM_ModdingToolUEVersionCheckJob::OnFileFetchFailed(const FString& ResponseString)
{
	if (bIsCancelled) return;

	const FString Error = FString::Printf(
		TEXT("Failed to fetch remote Version.json: %s"), *ResponseString);
	NotifyCompletion(EJobResult::Failed, Error);
}

void UCPM_ModdingToolUEVersionCheckJob::NotifyCompletion(EJobResult Result, const FString& ErrorMessage)
{
	ActiveProxy = nullptr;

	if (CachedWorkflow.GetInterface())
	{
		FJobCompletionInfo Completion;
		Completion.Job = this;
		Completion.Result = Result;
		Completion.ErrorMessage = ErrorMessage;
		CachedWorkflow->IOnJobCompleted(Completion);
	}
}
