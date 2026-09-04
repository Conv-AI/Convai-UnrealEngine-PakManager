// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/CPM_PublishTypes.h"
#include "UObject/Object.h"

#include "CPM_PublishJobs.generated.h"

class UCPM_PublishRunner;
class UCPM_CreatePakAssetProxy;
class UCPM_UpdatePakAssetProxy;
class UCPM_UploadPakAssetProxy;

namespace ConvaiPakManager::Publish
{
	/**
	 * The form fields a create or an update carries alongside the metadata document.
	 *
	 * None of this is visible in the composed document, and all of it decides what the server files
	 * the Asset under - so it is pure and separate from the Job, where a test can read the values
	 * that actually go on the wire rather than trust the Job that fills them.
	 *
	 * MetaData and Thumbnail are not here: both are read off disk and belong with the Job that knows
	 * the Chunk's paths.
	 */
	CONVAIPAKMANAGER_API void FillPublishFormFields(
		const FString& AssetType,
		const FCPM_PublishPolicy& Policy,
		bool bIncludesRawArchive,
		bool bIsUpdate,
		FCPM_CreatePakAssetParams& OutParams);
}

/**
 * Shared plumbing for the Jobs of a Publish.
 *
 * Holds the runner and the run's shared state, reports exactly once, and turns the two-part cancel
 * into something a subclass cannot get subtly wrong: a Job that reports Failed when it was cancelled
 * tells the creator their publish broke rather than that they stopped it.
 */
UCLASS(Abstract)
class CONVAIPAKMANAGER_API UCPM_PublishJobBase : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UCPM_PublishRunner* InRunner, FCPM_PublishContext* InContext);

	virtual void Execute() {}

	/** Overriding this means still reporting Cancelled from it - the runner waits for that report. */
	virtual void Cancel(bool bForce);

	/** Shown to the creator as this Publish's step, and used as the planned step's name. */
	virtual FString Name() const { return FString(); }

	/** 0 for no deadline, which is the honest answer for a cook and an upload. */
	virtual float TimeoutSeconds() const { return 0.0f; }

	/**
	 * What a creator is told while this Job runs.
	 *
	 * Lives on the Job rather than being mapped from its name by the subsystem: a status derived from
	 * display text breaks silently the first time somebody improves the wording.
	 */
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const { return ECPM_AssetManagerStatus::Max; }

protected:
	void Report(ECPM_PublishResult Result, const FString& Error);
	void ReportProgress(const FString& Step, float Percent);

	UPROPERTY()
	TObjectPtr<UCPM_PublishRunner> Runner;

	/** The run's shared state, owned by the runner. Jobs read what came before and write their own. */
	FCPM_PublishContext* Context = nullptr;

	/** Set once reported, so a late callback from a cancelled request cannot report a second time. */
	bool bReported = false;

	/** Set by Cancel, so work already in flight resolves as Cancelled rather than Failed. */
	bool bCancelled = false;
};

/**
 * Builds one Pak per platform the Publish Policy asks for.
 *
 * ONE Job for every platform rather than one per platform: the packaging runs sequentially anyway,
 * because two UAT invocations against one project would fight over the same intermediate
 * directories.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_PackagePaksJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const override { return ECPM_AssetManagerStatus::Packaging_Begin; }

	virtual FString Name() const override { return TEXT("Packaging"); }

	virtual void Execute() override;
	virtual void Cancel(bool bForce) override;

private:
	void PackageNextPlatform();

	/** Where this platform's Pak belongs, whether it was cooked this run or found already there. */
	FCPM_PakArtifact ArtifactFor(ECPM_Platform Platform) const;

	/** Report, with Live Coding put back first. Every terminal path of this Job goes through it. */
	void ReportAndRestore(ECPM_PublishResult Result, const FString& Error);

	void RestoreLiveCoding();

	UFUNCTION()
	void HandlePackageFinished(const FString& Result, double Runtime);

	TArray<ECPM_Platform> Remaining;
	TArray<FCPM_PakArtifact> Built;
	FCPM_PublishRequest Request;

	/** Set only when this Job was the one that turned Live Coding off, so it restores nothing else. */
	bool bParkedLiveCoding = false;
};

/** Archives the creator's project for its Version. Constructed only when the Policy asks for it. */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_ArchiveRawProjectJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const override { return ECPM_AssetManagerStatus::Archiving_Begin; }

	virtual FString Name() const override { return TEXT("Archiving project"); }

	virtual void Execute() override;

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

	virtual FString Name() const override { return TEXT("Creating asset"); }

	virtual float TimeoutSeconds() const override { return 120.0f; }

	virtual void Execute() override;
	virtual void Cancel(bool bForce) override;

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
 * Sends what it was CONSTRUCTED to send, never whatever it finds in the context: a run asked for no
 * Pak must not start uploading one an earlier run left on disk.
 */
UCLASS()
class CONVAIPAKMANAGER_API UCPM_UploadArtifactsJob : public UCPM_PublishJobBase
{
	GENERATED_BODY()

public:
	virtual ECPM_AssetManagerStatus GetPhaseStatus() const override { return ECPM_AssetManagerStatus::UploadPak_Begin; }

	/**
	 * What this run built and so what to send. Set from the same two decisions the queue was built
	 * from, before the Job runs.
	 *
	 * No timeout on this Job: a Pak is hundreds of megabytes and an upload's honest duration depends
	 * on the creator's connection, not on anything we can predict.
	 */
	void Configure(bool bInExpectPaks, bool bInExpectRawArchive);

	virtual FString Name() const override { return TEXT("Uploading"); }

	virtual void Execute() override;
	virtual void Cancel(bool bForce) override;

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

	/** Read once when the Job starts; the context is not re-read per file. */
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

	virtual FString Name() const override { return TEXT("Recording asset"); }

	virtual float TimeoutSeconds() const override { return 30.0f; }

	virtual void Execute() override;
};
