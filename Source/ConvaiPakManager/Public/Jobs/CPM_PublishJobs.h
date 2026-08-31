// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interface/JobInterface.h"
#include "Publish/CPM_PublishTypes.h"
#include "Type/JS_Definations.h"
#include "UObject/Object.h"

#include "CPM_PublishJobs.generated.h"

class UCPM_CreatePakAssetProxy;
class UCPM_UpdatePakAssetProxy;
class UCPM_UploadPakAssetProxy;

/**
 * Shared plumbing for the Jobs of a Publish.
 *
 * Holds the workflow, reports exactly once, and turns the two-part cancel into something a subclass
 * cannot get subtly wrong: a Job that reports Failed when it was cancelled is retried by a workflow
 * that is trying to stop, re-issuing the request the creator just cancelled.
 */
UCLASS(Abstract)
class CONVAIPAKMANAGER_API UCPM_PublishJobBase : public UObject, public IJobInterface
{
	GENERATED_BODY()

public:
	virtual void IInitialize_Implementation(const TScriptInterface<IWorkflowInterface>& Workflow) override;
	virtual void ICancel_Implementation(bool bForce) override;

	/**
	 * What a creator is told while this Job runs.
	 *
	 * Lives on the Job rather than being mapped from its name by the subsystem: a status derived from
	 * display text breaks silently the first time somebody improves the wording.
	 */
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const { return ECPM_AssetManagerStatus::Max; }

protected:
	/** Reads the caller's request off the context. Every Job needs it; none of them owns it. */
	bool TryGetRequest(FCPM_PublishRequest& OutRequest) const;

	void Report(EJobResult Result, const FString& Error, TArray<FInstancedStruct>&& Outputs = {});
	void ReportProgress(const FString& Step, float Percent);

	UPROPERTY()
	TScriptInterface<IWorkflowInterface> CachedWorkflow;

	/** Set once reported, so a late callback from a cancelled request cannot report a second time. */
	bool bReported = false;

	/** Set by ICancel, so work already in flight resolves as Cancelled rather than Failed. */
	bool bCancelled = false;
};

/**
 * Builds one Pak per platform the Publish Policy asks for.
 *
 * ONE Job for every platform rather than one per platform, for two reasons: the packaging runs
 * sequentially anyway (two UAT invocations against one project would fight over the same
 * intermediate directories), and the Workflow Context is keyed by type - two Jobs each producing a
 * single FCPM_PakArtifact would have the second overwrite the first.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_PackagePaksJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const override { return ECPM_AssetManagerStatus::Packaging_Begin; }

	virtual void IExecute_Implementation() override;
	virtual FJobConfig IGetJobConfig_Implementation() const override;
	virtual FJobIOSpec IDeclareIO_Implementation() const override;

private:
	void PackageNextPlatform();

	/** Where this platform's Pak belongs, whether it was cooked this run or found already there. */
	FCPM_PakArtifact ArtifactFor(ECPM_Platform Platform) const;

	UFUNCTION()
	void HandlePackageFinished(const FString& Result, double Runtime);

	TArray<ECPM_Platform> Remaining;
	TArray<FCPM_PakArtifact> Built;
	FCPM_PublishRequest Request;
};

/** Archives the creator's project for the `raw` Version. Constructed only when the Policy asks for it. */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_ArchiveRawProjectJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const override { return ECPM_AssetManagerStatus::Archiving_Begin; }

	virtual void IExecute_Implementation() override;
	virtual FJobConfig IGetJobConfig_Implementation() const override;
	virtual FJobIOSpec IDeclareIO_Implementation() const override;

private:
	UFUNCTION()
	void HandleArchiveFinished(const FString& Result, double Runtime);

	FString ZipPath;
};

/**
 * Creates the Asset on Convai, or updates the one this Chunk already has, and takes back the URLs
 * its artefacts are to be PUT to.
 *
 * Which of the two it does is decided by whether the Chunk already records an AssetID - the same
 * fact that decides whether the creator is shown Create or Update.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_CreateAssetJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const override { return ECPM_AssetManagerStatus::Create_Begin; }

	virtual void IExecute_Implementation() override;
	virtual void ICancel_Implementation(bool bForce) override;
	virtual FJobConfig IGetJobConfig_Implementation() const override;
	virtual FJobIOSpec IDeclareIO_Implementation() const override;

private:
	UFUNCTION()
	void HandleCreated(const FCPM_CreatedAssets& Response);

	UFUNCTION()
	void HandleCreateFailed(const FCPM_CreatedAssets& Response);

	UFUNCTION()
	void HandleUpdated(const FString& MintedUrl);

	UFUNCTION()
	void HandleUpdateFailed(const FString& ResponseString);

	UPROPERTY()
	TObjectPtr<UCPM_CreatePakAssetProxy> CreateProxy;

	UPROPERTY()
	TObjectPtr<UCPM_UpdatePakAssetProxy> UpdateProxy;

	FString ExistingAssetId;

	/** The Version this request named. The server answers with the URL for it and no key naming it. */
	FString RequestedVersion;
};

/**
 * PUTs every built artefact to the URL minted for its Version.
 *
 * Declares what it needs from how it was constructed, never from what it finds in the context: a
 * queue built from a Policy with no Linux must not be rejected for a Linux Pak nobody asked for.
 * See docs/adr/0009.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_UploadArtifactsJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const override { return ECPM_AssetManagerStatus::UploadPak_Begin; }

	/** Must be called before the Job joins a queue - IDeclareIO is asked once, at queue build. */
	void Configure(bool bInExpectPaks, bool bInExpectRawArchive);

	virtual void IExecute_Implementation() override;
	virtual void ICancel_Implementation(bool bForce) override;
	virtual FJobConfig IGetJobConfig_Implementation() const override;
	virtual FJobIOSpec IDeclareIO_Implementation() const override;

private:
	/** One artefact still to send, paired with the Version it belongs to. */
	struct FPendingUpload
	{
		FString VersionSlot;
		FString FilePath;
	};

	void UploadNext();

	/** Asks the server for the URL of the Version at the head of the queue. See UploadNext. */
	void MintUrlForNext();

	UFUNCTION()
	void HandleMinted(const FString& MintedUrl);

	UFUNCTION()
	void HandleMintFailed(const FString& ResponseString);

	UFUNCTION()
	void HandleUploadProgress(float Progress);

	UFUNCTION()
	void HandleUploadSucceeded(float Progress);

	UFUNCTION()
	void HandleUploadFailed(float Progress);

	UPROPERTY()
	TObjectPtr<UCPM_UploadPakAssetProxy> UploadProxy;

	UPROPERTY()
	TObjectPtr<UCPM_UpdatePakAssetProxy> MintProxy;

	/** Read once in IExecute; the context is not re-read per file. */
	FCPM_PublishedAsset PublishedForUpload;

	TArray<FPendingUpload> Pending;
	int32 TotalUploads = 0;
	bool bExpectPaks = true;
	bool bExpectRawArchive = false;
};

/**
 * Writes the Chunk's record of the Asset it was published as.
 *
 * Last, and deliberately so: this file holds the only copy of the AssetID anywhere in the creator's
 * world, and writing it before the artefacts land would claim a Publish that had not happened.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_PersistChunkStateJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const override { return ECPM_AssetManagerStatus::Update_Begin; }

	virtual void IExecute_Implementation() override;
	virtual FJobConfig IGetJobConfig_Implementation() const override;
	virtual FJobIOSpec IDeclareIO_Implementation() const override;
};
