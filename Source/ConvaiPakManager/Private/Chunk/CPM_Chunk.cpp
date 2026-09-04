// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Chunk/CPM_Chunk.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/Blueprint.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Factories/DataAssetFactory.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "PluginDescriptor.h"
#include "RestAPI/ConvaiURL.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"
#include "Utility/CPM_Log.h"
#include "Utility/CPM_UtilityLibrary.h"

namespace ConvaiPakManager::Chunk
{
namespace
{
	const TCHAR* EssentialsFolderName = TEXT("ConvaiEssentials");

	/**
	 * One state file, named by its stem so the flat and per-Chunk names cannot drift apart:
	 * `CreateAssetData.json` flat becomes `CreateAssetData_10.json` under a Chunk.
	 *
	 * The two extensions differ only for ModdingMetaData: the Modding Tool has always written JSON
	 * into a `.txt`, and the per-Chunk name says what the contents are.
	 */
	struct FStateFile
	{
		const TCHAR* Stem;
		const TCHAR* LegacyExtension;
		const TCHAR* PerChunkExtension;

		FString LegacyName() const { return FString(Stem) + LegacyExtension; }
		FString PerChunkName(const int32 ChunkId) const
		{
			return FString::Printf(TEXT("%s_%d%s"), Stem, ChunkId, PerChunkExtension);
		}
	};

	/**
	 * The three files a pre-Chunk project keeps flat in ConvaiEssentials.
	 *
	 * CreateAssetData is the one that matters: it holds the AssetID, which exists nowhere else.
	 * The other two are rebuildable from a Publish; that one is not.
	 */
	const FStateFile StateFiles[] = {
		{ TEXT("CreateAssetData"), TEXT(".json"), TEXT(".json") },
		{ TEXT("PakMetaData"),     TEXT(".json"), TEXT(".json") },
		{ TEXT("ModdingMetaData"), TEXT(".txt"),  TEXT(".json") },
	};

	/** Which of the three are still flat in ConvaiEssentials, by legacy name, in StateFiles order. */
	TArray<FString> FlatLegacyFilesIn(const FString& EssentialsDirectory)
	{
		TArray<FString> Present;
		for (const FStateFile& StateFile : StateFiles)
		{
			if (IFileManager::Get().FileExists(*FPaths::Combine(EssentialsDirectory, StateFile.LegacyName())))
			{
				Present.Add(StateFile.LegacyName());
			}
		}
		return Present;
	}

	/** Memoised GetSoleChunkId. Unset means "not yet resolved from a completed Asset Registry scan". */
	TOptional<int32> CachedSoleChunkId;

	FString StateDirectoryIn(const FString& EssentialsDirectory, const int32 ChunkId)
	{
		return FPaths::Combine(EssentialsDirectory, FString::Printf(TEXT("ChunkId_%d"), ChunkId));
	}

	/**
	 * Whether some OTHER backend still holds an Asset for this Chunk.
	 *
	 * An Asset record is the only proof: an environment folder can outlive its records (a delete
	 * leaves the directory), so its existence says nothing.
	 */
	bool PublishedToAnyEnvironmentExcept(
		const FString& EssentialsDirectory, const int32 ChunkId, const FString& EnvironmentSlug)
	{
		const FString StateDirectory = StateDirectoryIn(EssentialsDirectory, ChunkId);

		TArray<FString> Environments;
		IFileManager::Get().FindFiles(
			Environments, *FPaths::Combine(StateDirectory, TEXT("Env_*")),
			/*Files=*/false, /*Directories=*/true);

		const FString Record = FString::Printf(TEXT("CreateAssetData_%d.json"), ChunkId);
		return Environments.ContainsByPredicate(
			[&](const FString& Candidate)
			{
				return !Candidate.Equals(EnvironmentSlug, ESearchCase::IgnoreCase)
					&& IFileManager::Get().FileExists(
						*FPaths::Combine(StateDirectory, Candidate, Record));
			});
	}

	/** A URL reduced to one spelling per backend, plus the part of it a human can read. */
	struct FCanonicalUrl
	{
		/** Scheme, authority and path. This is what is hashed, and it is the whole identity. */
		FString Whole;
		/** Authority and path alone, what the readable half of the slug is made from. */
		FString HostAndPath;
	};

	FCanonicalUrl CanonicaliseUrl(const FString& BaseUrl)
	{
		const FString Trimmed = BaseUrl.TrimStartAndEnd();

		FString Scheme, Rest;
		if (!Trimmed.Split(TEXT("://"), &Scheme, &Rest))
		{
			Rest = Trimmed;
		}

		FString Authority = Rest;
		FString Path;
		int32 FirstSlash = INDEX_NONE;
		if (Rest.FindChar(TEXT('/'), FirstSlash))
		{
			Authority = Rest.Left(FirstSlash);
			Path = Rest.RightChop(FirstSlash);
		}

		// A trailing slash is one address typed two ways - GetFullURL puts one there either way. The
		// rest of the path is the server's to interpret, so it keeps the case the creator wrote.
		while (Path.EndsWith(TEXT("/")))
		{
			Path.LeftChopInline(1);
		}

		FCanonicalUrl Canonical;
		Canonical.HostAndPath = Authority.ToLower() + Path;
		Canonical.Whole = Scheme.IsEmpty()
			? Canonical.HostAndPath
			: Scheme.ToLower() + TEXT("://") + Canonical.HostAndPath;
		return Canonical;
	}
}

TArray<FCPM_Chunk> Discover()
{
	TArray<FCPM_Chunk> Chunks;

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		CPM_LOG(Warning, TEXT("Chunk discovery: no Asset Registry; treating project as having no Chunks."));
		return Chunks;
	}

	// A partial scan yields a partial answer, and a caller cannot tell that from a project that
	// genuinely has fewer Chunks - so say so rather than let it pass as a result.
	if (AssetRegistry->IsLoadingAssets())
	{
		CPM_LOG(Warning, TEXT("Chunk discovery ran while the Asset Registry was still scanning; result may be incomplete."));
	}

	FARFilter Filter;
	Filter.ClassPaths.Add(UPrimaryAssetLabel::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Labels;
	AssetRegistry->GetAssets(Filter, Labels);

	for (const FAssetData& Label : Labels)
	{
		// Loaded rather than read from registry tags: ChunkId lives in PrimaryAssetRules, which is
		// not tagged. Labels are tiny and this is not a per-frame path.
		const UPrimaryAssetLabel* Asset = Cast<UPrimaryAssetLabel>(Label.GetAsset());
		if (!Asset)
		{
			CPM_LOG(Warning, TEXT("Chunk discovery: could not load Primary Asset Label %s; skipping."),
				*Label.PackageName.ToString());
			continue;
		}

		const int32 ChunkId = Asset->Rules.ChunkId;
		if (ChunkId == INDEX_NONE)
		{
			// A label with no chunk is legitimate - it is labelling for cook rules, not packaging.
			continue;
		}

		if (const FCPM_Chunk* Existing = Chunks.FindByPredicate(
			[ChunkId](const FCPM_Chunk& Chunk) { return Chunk.Id == ChunkId; }))
		{
			// Two labels claiming one Chunk means two sets of Source Packages cook into one Pak and
			// publish as one Asset. That is a project misconfiguration the creator has to fix, and it
			// is worth naming both labels because the second is invisible from the first.
			CPM_LOG(Error, TEXT("Chunk %d is claimed by two Primary Asset Labels (%s and %s). Its Pak will hold both."),
				ChunkId, *Existing->LabelPackage.ToString(), *Label.PackageName.ToString());
			continue;
		}

		FCPM_Chunk Chunk;
		Chunk.Id = ChunkId;
		Chunk.LabelPackage = Label.PackageName;
		Chunks.Add(Chunk);
	}

	Chunks.Sort([](const FCPM_Chunk& A, const FCPM_Chunk& B) { return A.Id < B.Id; });

	// Verbose: Discover runs on every panel refresh, and the answer only matters when it is
	// surprising - which is exactly when someone turns this category up.
	CPM_LOG(Verbose, TEXT("Chunk discovery found %d chunk(s) across %d label(s)."), Chunks.Num(), Labels.Num());
	return Chunks;
}

int32 GetSoleChunkId()
{
	if (CachedSoleChunkId.IsSet())
	{
		return CachedSoleChunkId.GetValue();
	}

	const TArray<FCPM_Chunk> Chunks = Discover();
	const int32 Resolved = Chunks.Num() == 1 ? Chunks[0].Id : INDEX_NONE;

	// A mid-scan answer is a guess about a project, not a fact about it. Recompute next time.
	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (AssetRegistry && !AssetRegistry->IsLoadingAssets())
	{
		CachedSoleChunkId = Resolved;
	}

	return Resolved;
}

void InvalidateSoleChunkCache()
{
	CachedSoleChunkId.Reset();
}

bool EnsureLabel(const FString& MountRoot, int32& ChunkId, FString& OutError)
{
	// Checked before anything else: CreateAsset on an unmounted root produces a package that cannot
	// be saved, and the creator would see a save failure rather than the real problem.
	if (!FPackageName::MountPointExists(MountRoot + TEXT("/")))
	{
		OutError = FString::Printf(
			TEXT("%s is not mounted in this project, so a Primary Asset Label cannot be created in it."),
			*MountRoot);
		return false;
	}

	const FString LabelName = FString::Printf(TEXT("PAL_%s"), *FPaths::GetCleanFilename(MountRoot));
	const FString PackagePath = MountRoot / LabelName;

	UPrimaryAssetLabel* Label = nullptr;
	if (FPackageName::DoesPackageExist(PackagePath))
	{
		Label = LoadObject<UPrimaryAssetLabel>(nullptr, *(PackagePath + TEXT(".") + LabelName));
		if (!Label)
		{
			OutError = FString::Printf(
				TEXT("%s exists but is not a Primary Asset Label. Rename or delete it and try again."),
				*PackagePath);
			return false;
		}

		if (Label->Rules.ChunkId != INDEX_NONE)
		{
			// It already declares a Chunk, so Discover lists it and there is nothing to mint.
			// Rewriting its ChunkId here would move published content into a different Pak. The
			// caller asked for an id it cannot have; hand back the one the project really has.
			ChunkId = Label->Rules.ChunkId;
			return true;
		}
	}
	else
	{
		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = UPrimaryAssetLabel::StaticClass();

		Label = Cast<UPrimaryAssetLabel>(IAssetTools::Get().CreateAsset(
			LabelName, MountRoot, UPrimaryAssetLabel::StaticClass(), Factory));
		if (!Label)
		{
			OutError = FString::Printf(TEXT("Could not create %s."), *PackagePath);
			return false;
		}
	}

	// The rules the Modding Tool has always written. bLabelAssetsInMyDirectory is what makes the
	// label gather anything at all, and AlwaysCook is what stops the cooker dropping content the
	// creator's own levels do not reference.
	Label->bLabelAssetsInMyDirectory = true;
	Label->bIsRuntimeLabel = true;
	Label->Rules.Priority = 0;
	Label->Rules.ChunkId = ChunkId;
	Label->Rules.bApplyRecursively = true;
	Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;

	UPackage* Package = Label->GetOutermost();
	Package->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	// No dialog: this runs behind a button the creator pressed to get a Chunk, not to be asked about
	// package saving.
	SaveArgs.SaveFlags = SAVE_NoError;

	if (!UPackage::SavePackage(
		Package, Label,
		*FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()),
		SaveArgs))
	{
		OutError = FString::Printf(
			TEXT("Created %s but could not save it. Check that the file is not read-only."), *PackagePath);
		return false;
	}

	CPM_LOG(Display, TEXT("Primary Asset Label %s now declares chunk %d."), *PackagePath, ChunkId);
	return true;
}

bool EnsureLabelDirectoryScanned(const FString& MountRoot)
{
	UAssetManagerSettings* Settings = GetMutableDefault<UAssetManagerSettings>();

	const FName LabelType = UPrimaryAssetLabel::StaticClass()->GetFName();
	FPrimaryAssetTypeInfo* TypeInfo = Settings->PrimaryAssetTypesToScan.FindByPredicate(
		[&LabelType](const FPrimaryAssetTypeInfo& Candidate) { return Candidate.PrimaryAssetType == LabelType; });

	if (!TypeInfo)
	{
		TypeInfo = &Settings->PrimaryAssetTypesToScan.Add_GetRef(FPrimaryAssetTypeInfo(
			LabelType, UPrimaryAssetLabel::StaticClass(),
			/*bHasBlueprintClasses=*/false, /*bIsEditorOnly=*/false));
	}
	else if (TypeInfo->GetDirectories().ContainsByPredicate(
		[&MountRoot](const FDirectoryPath& Directory) { return Directory.Path.Equals(MountRoot, ESearchCase::IgnoreCase); }))
	{
		// Already scanned. Returning here rather than rewriting the same value is what keeps
		// DefaultGame.ini untouched on every boot of an already-configured project.
		return true;
	}

	TypeInfo->GetDirectories().Add(FDirectoryPath{ MountRoot });

	// The whole section, not UpdateSinglePropertyInConfigFile: that one refuses TArray properties
	// unless ini.UseNewPropertySaving is on - off by default - so nothing would reach disk and the
	// next editor boot, and every build machine, would cook this label into chunk 0. This is what
	// Project Settings itself calls, and it answers whether the file was writable.
	const bool bWritten = Settings->TryUpdateDefaultConfigFile();

	// A label in a directory nothing scans cooks into chunk 0 and emits no pakchunk at all, so this
	// has to take effect in the session that minted it rather than only after a restart.
	if (UAssetManager::IsInitialized())
	{
		UAssetManager::Get().ReinitializeFromConfig();
	}

	if (!bWritten)
	{
		CPM_LOG(Warning,
			TEXT("%s is read-only, so %s could not be recorded as a Primary Asset Label scan directory. ")
			TEXT("Chunks cook correctly for this session only; check the file out and reopen the Pak Manager."),
			*Settings->GetDefaultConfigFilename(), *MountRoot);
		return false;
	}

	CPM_LOG(Display, TEXT("Registered %s as a Primary Asset Label scan directory."), *MountRoot);
	return true;
}

FString GetEssentialsDirectory()
{
	return FPaths::Combine(FPaths::ProjectDir(), EssentialsFolderName);
}

FString GetStateDirectory(const int32 ChunkId)
{
	return StateDirectoryIn(GetEssentialsDirectory(), ChunkId);
}

FString EnvironmentSlug(const FString& BaseUrl)
{
	const FCanonicalUrl Canonical = CanonicaliseUrl(BaseUrl);

	// The whole canonical URL, not just the host: two backends can share a host and differ by
	// scheme, port or path, and one folder they both wrote into is the crossover this exists to stop.
	const FTCHARToUTF8 Utf8(*Canonical.Whole);
	const FString Hash =
		FMD5::HashBytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()).Left(8);

	// ASCII spelled out rather than FChar::IsAlnum: this becomes a directory name on three
	// platforms, and a locale that calls some other letter alphanumeric would put it in one.
	FString Segment;
	Segment.Reserve(Canonical.HostAndPath.Len());
	for (const TCHAR Character : Canonical.HostAndPath)
	{
		const bool bReadable = (Character >= TEXT('0') && Character <= TEXT('9'))
			|| (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('a') && Character <= TEXT('z'))
			|| Character == TEXT('.');
		Segment.AppendChar(bReadable ? Character : TEXT('-'));
	}
	Segment.LeftInline(24);

	return FString::Printf(TEXT("Env_%s_%s"), Segment.IsEmpty() ? TEXT("unknown") : *Segment, *Hash);
}

FString CurrentEnvironmentSlug()
{
	return EnvironmentSlug(UConvaiURL::GetBaseURL(/*bUseBeta=*/false));
}

FString GetEnvironmentDirectory(const int32 ChunkId, const FString& EnvironmentSlug)
{
	return FPaths::Combine(GetStateDirectory(ChunkId), EnvironmentSlug);
}

FString GetCreateAssetDataPath(const int32 ChunkId, const FString& EnvironmentSlug)
{
	return FPaths::Combine(
		GetEnvironmentDirectory(ChunkId, EnvironmentSlug),
		FString::Printf(TEXT("CreateAssetData_%d.json"), ChunkId));
}

FString GetPakMetadataPath(const int32 ChunkId, const FString& EnvironmentSlug)
{
	return FPaths::Combine(
		GetEnvironmentDirectory(ChunkId, EnvironmentSlug),
		FString::Printf(TEXT("PakMetaData_%d.json"), ChunkId));
}

FString ReadAssetId(const int32 ChunkId, const FString& EnvironmentSlug)
{
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *GetCreateAssetDataPath(ChunkId, EnvironmentSlug)))
	{
		return FString();
	}

	FCPM_CreatedAssets Created;
	if (!UCPM_UtilityLibrary::GetCreatedAssetsFromJSON(Contents, Created) || Created.Assets.IsEmpty())
	{
		return FString();
	}

	return Created.Assets[0].Asset.AssetId;
}

bool WriteCreateAssetData(const int32 ChunkId, const FString& EnvironmentSlug, const FString& ResponseString)
{
	// SaveStringToFile creates the environment directory on the way, so the first publish to a
	// backend needs no separate step.
	const FString Path = GetCreateAssetDataPath(ChunkId, EnvironmentSlug);
	if (FFileHelper::SaveStringToFile(ResponseString, *Path))
	{
		return true;
	}

	// Loud, because this is the failure that orphans an Asset: it exists on Convai and the
	// creator's project has no record of its id.
	CPM_LOG(Error, TEXT("Could not write %s. This orphans an Asset that already exists on Convai."), *Path);
	return false;
}

bool WritePakMetadata(const int32 ChunkId, const FString& EnvironmentSlug, const FString& Document)
{
	const FString Path = GetPakMetadataPath(ChunkId, EnvironmentSlug);
	if (FFileHelper::SaveStringToFile(Document, *Path))
	{
		return true;
	}

	CPM_LOG(Error, TEXT("Could not write %s."), *Path);
	return false;
}

FString GetModdingMetadataPathIn(const FString& EssentialsDirectory, const int32 ChunkId)
{
	const FString Directory = StateDirectoryIn(EssentialsDirectory, ChunkId);
	const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("ModdingMetaData_%d.json"), ChunkId));
	if (IFileManager::Get().FileExists(*Path))
	{
		return Path;
	}

	// A project generated by a Modding Tool that has not caught up yet still has to open.
	const FString LegacyPath = FPaths::Combine(Directory, FString::Printf(TEXT("ModdingMetaData_%d.txt"), ChunkId));
	if (IFileManager::Get().FileExists(*LegacyPath))
	{
		return LegacyPath;
	}

	// The un-migrated project, which has no Chunk and therefore no per-Chunk path to resolve. Its
	// plugin_name is here and nowhere else, and that is what minting its Chunk needs.
	const FString FlatPath = FPaths::Combine(EssentialsDirectory, StateFiles[2].LegacyName());
	return IFileManager::Get().FileExists(*FlatPath) ? FlatPath : Path;
}

FString GetModdingMetadataPath(const int32 ChunkId)
{
	return GetModdingMetadataPathIn(GetEssentialsDirectory(), ChunkId);
}

FString GetRawArchiveRecordPath(const int32 ChunkId, const FString& EnvironmentSlug)
{
	return FPaths::Combine(
		GetEnvironmentDirectory(ChunkId, EnvironmentSlug),
		FString::Printf(TEXT("RawArchive_%d.txt"), ChunkId));
}

FString GetDraftPath(const int32 ChunkId)
{
	return FPaths::Combine(GetStateDirectory(ChunkId), FString::Printf(TEXT("Draft_%d.json"), ChunkId));
}

void ClearAssetRecordsIn(
	const FString& EssentialsDirectory,
	const int32 ChunkId,
	const FString& EnvironmentSlug,
	TArray<FString>& OutUndeleted)
{
	const FString StateDirectory = StateDirectoryIn(EssentialsDirectory, ChunkId);
	const FString Directory = FPaths::Combine(StateDirectory, EnvironmentSlug);

	const TArray<FString> Records = {
		FPaths::Combine(Directory, FString::Printf(TEXT("CreateAssetData_%d.json"), ChunkId)),
		FPaths::Combine(Directory, FString::Printf(TEXT("PakMetaData_%d.json"), ChunkId)),
		FPaths::Combine(Directory, FString::Printf(TEXT("RawArchive_%d.txt"), ChunkId)),
	};

	for (const FString& Record : Records)
	{
		// EvenReadOnly, because a project under source control keeps these read-only until checked
		// out and the creator has already said to delete them.
		if (!IFileManager::Get().Delete(*Record, /*RequireExists=*/false, /*EvenReadOnly=*/true))
		{
			OutUndeleted.Add(Record);
		}
	}

	if (PublishedToAnyEnvironmentExcept(EssentialsDirectory, ChunkId, EnvironmentSlug))
	{
		return;
	}

	// Nothing is published anywhere any more, so the inputs have nobody left to be inputs for. Left
	// behind they read as an Asset that still exists: the form comes back filled and the creator's
	// own thumbnail sits under a Publish that would mint a new Asset.
	const TArray<FString> Inputs = {
		FPaths::Combine(StateDirectory, FString::Printf(TEXT("Draft_%d.json"), ChunkId)),
		FPaths::Combine(StateDirectory, FString::Printf(TEXT("Thumbnail_%d.png"), ChunkId)),
	};

	for (const FString& Input : Inputs)
	{
		if (!IFileManager::Get().Delete(*Input, /*RequireExists=*/false, /*EvenReadOnly=*/true))
		{
			OutUndeleted.Add(Input);
		}
	}
}

void ClearAssetRecords(const int32 ChunkId, const FString& EnvironmentSlug, TArray<FString>& OutUndeleted)
{
	ClearAssetRecordsIn(GetEssentialsDirectory(), ChunkId, EnvironmentSlug, OutUndeleted);
}

FString GetThumbnailPath(const int32 ChunkId)
{
	return FPaths::Combine(GetStateDirectory(ChunkId), FString::Printf(TEXT("Thumbnail_%d.png"), ChunkId));
}

EMigrationResult MigrateLegacyLayoutIn(
	const FString& EssentialsDirectory,
	const int32 ChunkId,
	TArray<FString>& OutMovedFiles)
{
	IFileManager& FileManager = IFileManager::Get();

	const TArray<FString> Present = FlatLegacyFilesIn(EssentialsDirectory);
	if (Present.IsEmpty())
	{
		return EMigrationResult::NothingToMigrate;
	}

	if (ChunkId == INDEX_NONE)
	{
		return EMigrationResult::Ambiguous;
	}

	const FString StateDirectory = StateDirectoryIn(EssentialsDirectory, ChunkId);
	if (!FileManager.DirectoryExists(*StateDirectory) && !FileManager.MakeDirectory(*StateDirectory, true))
	{
		CPM_LOG(Error, TEXT("Could not create Chunk state directory %s. Nothing was moved."), *StateDirectory);
		return EMigrationResult::Failed;
	}

	bool bAnyFailed = false;
	for (const FStateFile& StateFile : StateFiles)
	{
		const FString LegacyPath = FPaths::Combine(EssentialsDirectory, StateFile.LegacyName());
		if (!FileManager.FileExists(*LegacyPath))
		{
			continue;
		}

		const FString DestinationPath = FPaths::Combine(StateDirectory, StateFile.PerChunkName(ChunkId));

		if (FileManager.FileExists(*DestinationPath))
		{
			// Already migrated. The per-Chunk copy is the live one, so leave both alone rather than
			// overwrite newer state with a stale flat file.
			CPM_LOG(Warning, TEXT("%s already exists; leaving the legacy %s in place untouched."),
				*DestinationPath, *StateFile.LegacyName());
			continue;
		}

		// Move rather than copy-then-delete: a half-finished copy that then loses its original is
		// how the AssetID gets destroyed, and Move either happens or does not.
		if (FileManager.Move(*DestinationPath, *LegacyPath))
		{
			OutMovedFiles.Add(DestinationPath);
		}
		else
		{
			bAnyFailed = true;
			CPM_LOG(Error, TEXT("Could not move %s to %s."), *LegacyPath, *DestinationPath);
		}
	}

	if (bAnyFailed)
	{
		return EMigrationResult::Failed;
	}

	if (!OutMovedFiles.IsEmpty())
	{
		CPM_LOG(Log, TEXT("Migrated %d pre-Chunk file(s) into %s."), OutMovedFiles.Num(), *StateDirectory);
		return EMigrationResult::Migrated;
	}

	return EMigrationResult::NothingToMigrate;
}

bool HasUnmigratedLegacyLayoutIn(const FString& EssentialsDirectory)
{
	return !FlatLegacyFilesIn(EssentialsDirectory).IsEmpty();
}

bool HasUnmigratedLegacyLayout()
{
	return HasUnmigratedLegacyLayoutIn(GetEssentialsDirectory());
}

namespace
{
	/** Everything on a metadata document that the creator typed rather than a backend minted. */
	const TCHAR* DraftFields[] = {
		TEXT("asset_name"),
		TEXT("asset_description"),
		TEXT("root_path"),
		TEXT("level_name"),
		TEXT("blueprint_class"),
		TEXT("blueprint_class_path"),
	};

	bool SaveJsonObject(const TSharedRef<FJsonObject>& Root, const FString& Path)
	{
		FString Serialised;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Serialised);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			return false;
		}

		return FFileHelper::SaveStringToFile(Serialised, *Path);
	}

	/**
	 * Gives the Draft the fields the pre-partition PakMetaData was carrying on its behalf.
	 *
	 * The creator's name, description and Entry Point live in the server's document today. That
	 * document is about to move under one backend, so without this the form comes back empty on the
	 * first launch after the upgrade and the creator re-types what they already entered.
	 */
	void SeedDraftFromPakMetadata(const FString& StateDirectory, const int32 ChunkId)
	{
		const FString DraftPath = FPaths::Combine(StateDirectory, FString::Printf(TEXT("Draft_%d.json"), ChunkId));
		if (IFileManager::Get().FileExists(*DraftPath))
		{
			// A Draft that already exists is the creator's own, and outranks anything reconstructed.
			return;
		}

		const FString MetadataPath =
			FPaths::Combine(StateDirectory, FString::Printf(TEXT("PakMetaData_%d.json"), ChunkId));
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *MetadataPath))
		{
			return;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
		TSharedPtr<FJsonObject> Parsed;
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			// Not fatal to the adoption: the document still moves, it just cannot be read from.
			CPM_LOG(Warning, TEXT("%s is not valid JSON, so no Draft could be seeded from it."), *MetadataPath);
			return;
		}

		const TSharedRef<FJsonObject> Draft = MakeShared<FJsonObject>();
		for (const TCHAR* Field : DraftFields)
		{
			FString Value;
			if (Parsed->TryGetStringField(Field, Value))
			{
				Draft->SetStringField(Field, Value);
			}
		}

		SaveJsonObject(Draft, DraftPath);
	}
}

EMigrationResult AdoptLooseRecordsIn(
	const FString& EssentialsDirectory,
	const int32 ChunkId,
	const FString& EnvironmentSlug,
	TArray<FString>& OutMovedFiles)
{
	IFileManager& FileManager = IFileManager::Get();

	const FString StateDirectory = StateDirectoryIn(EssentialsDirectory, ChunkId);
	if (!FileManager.DirectoryExists(*StateDirectory))
	{
		return EMigrationResult::NothingToMigrate;
	}

	const FString EnvironmentDirectory = FPaths::Combine(StateDirectory, EnvironmentSlug);

	struct FAdoption
	{
		FString Source;
		FString Destination;
	};
	TArray<FAdoption> Loose;

	for (const FString& Name : {
		FString::Printf(TEXT("CreateAssetData_%d.json"), ChunkId),
		FString::Printf(TEXT("PakMetaData_%d.json"), ChunkId),
		FString::Printf(TEXT("RawArchive_%d.txt"), ChunkId) })
	{
		const FString Source = FPaths::Combine(StateDirectory, Name);
		if (FileManager.FileExists(*Source))
		{
			Loose.Add({ Source, FPaths::Combine(EnvironmentDirectory, Name) });
		}
	}

	// Stays at Chunk level - it describes the project, which is the same on every backend. This one
	// is only here because the same pass is what renames it to the extension its contents deserve.
	const FString LegacyModdingPath =
		FPaths::Combine(StateDirectory, FString::Printf(TEXT("ModdingMetaData_%d.txt"), ChunkId));
	if (FileManager.FileExists(*LegacyModdingPath))
	{
		Loose.Add({
			LegacyModdingPath,
			FPaths::Combine(StateDirectory, FString::Printf(TEXT("ModdingMetaData_%d.json"), ChunkId)) });
	}

	if (Loose.IsEmpty())
	{
		return EMigrationResult::NothingToMigrate;
	}

	// Before anything moves: once PakMetaData is under the backend folder there is nothing left at
	// Chunk level to read the creator's fields out of.
	SeedDraftFromPakMetadata(StateDirectory, ChunkId);

	if (!FileManager.DirectoryExists(*EnvironmentDirectory) &&
		!FileManager.MakeDirectory(*EnvironmentDirectory, true))
	{
		CPM_LOG(Error, TEXT("Could not create environment directory %s. Nothing was moved."), *EnvironmentDirectory);
		return EMigrationResult::Failed;
	}

	bool bAnyFailed = false;
	for (const FAdoption& Adoption : Loose)
	{
		if (FileManager.FileExists(*Adoption.Destination))
		{
			// This backend has published since. Its record is the live one and the loose file is the
			// older copy, so neither is destroyed and the creator can see both.
			CPM_LOG(Warning, TEXT("%s already exists; leaving the loose %s in place untouched."),
				*Adoption.Destination, *FPaths::GetCleanFilename(Adoption.Source));
			continue;
		}

		// Move rather than copy-then-delete, for the reason MigrateLegacyLayoutIn gives: a
		// half-finished copy that then loses its original is how the AssetID gets destroyed.
		if (FileManager.Move(*Adoption.Destination, *Adoption.Source))
		{
			OutMovedFiles.Add(Adoption.Destination);
		}
		else
		{
			bAnyFailed = true;
			CPM_LOG(Error, TEXT("Could not move %s to %s."), *Adoption.Source, *Adoption.Destination);
		}
	}

	if (bAnyFailed)
	{
		return EMigrationResult::Failed;
	}

	if (!OutMovedFiles.IsEmpty())
	{
		CPM_LOG(Log, TEXT("Adopted %d loose record(s) into %s."), OutMovedFiles.Num(), *EnvironmentDirectory);
		return EMigrationResult::Migrated;
	}

	return EMigrationResult::NothingToMigrate;
}

EMigrationResult AdoptLooseRecords(
	const int32 ChunkId,
	const FString& EnvironmentSlug,
	TArray<FString>& OutMovedFiles)
{
	return AdoptLooseRecordsIn(GetEssentialsDirectory(), ChunkId, EnvironmentSlug, OutMovedFiles);
}

EMigrationResult ReconcileStateLayoutIn(
	const FString& EssentialsDirectory,
	const TArray<FCPM_Chunk>& Chunks,
	const FString& ProductionSlug,
	TArray<FString>& OutMovedFiles)
{
	// A flat layout predates multi-Chunk support, so anything but one Chunk is unattributable and
	// MigrateLegacyLayoutIn refuses on INDEX_NONE.
	const EMigrationResult Result = MigrateLegacyLayoutIn(
		EssentialsDirectory, Chunks.Num() == 1 ? Chunks[0].Id : INDEX_NONE, OutMovedFiles);

	// Second, because its input is what the first pass moved.
	for (const FCPM_Chunk& Chunk : Chunks)
	{
		TArray<FString> Adopted;
		if (AdoptLooseRecordsIn(EssentialsDirectory, Chunk.Id, ProductionSlug, Adopted) == EMigrationResult::Migrated)
		{
			CPM_LOG(Display, TEXT("Adopted %d loose record(s) of chunk %d into %s: %s"),
				Adopted.Num(), Chunk.Id, *ProductionSlug, *FString::Join(Adopted, TEXT(", ")));
		}
	}

	return Result;
}

namespace
{
	/**
	 * Whether this session has already said the layout cannot be attributed.
	 *
	 * The panel's banner is the creator-facing report; this line is for a headless run and for the
	 * log a creator sends in. Reconciling happens on every panel refresh and every tab activation, so
	 * repeating it per call buries the log in the common case - a Modding Tool project that was
	 * generated, never published, and never given a Chunk.
	 *
	 * Lives out here rather than in ReconcileStateLayoutIn because the automation suite can run twice
	 * in one editor session, and the tests rely on that variant being silent.
	 */
	bool bUnattributedLayoutReported = false;
}

void ReconcileStateLayout()
{
	// Refuses to run at all against a partial scan. This MOVES the flat CreateAssetData.json - the
	// only copy of the project's AssetID - under whichever Chunk Discover can see, and mid-scan a
	// two-label project reads as a one-label one, so the record would be filed under a Chunk it does
	// not belong to. Every caller runs this again once the registry finishes.
	if (const IAssetRegistry* AssetRegistry = IAssetRegistry::Get(); AssetRegistry && AssetRegistry->IsLoadingAssets())
	{
		return;
	}

	// The label set may have changed since the last run, and anything cached before now could have
	// been answered mid-scan or before a label existed at all.
	InvalidateSoleChunkCache();

	const TArray<FCPM_Chunk> Chunks = Discover();

	// Everything still loose is production's by definition - nothing that could reach another
	// backend has shipped. The URL comes from the SETTINGS rather than UConvaiURL::GetBaseURL,
	// which honours -ConvaiProdURL= on the command line: one CI launch against staging would file a
	// production record under staging permanently, where nothing would ever look for it again. It is
	// also the setting the legacy tool resolved its own URL through, so a project whose CustomProdURL
	// named staging published there and its loose records belong under that slug.
	//
	// Read through GConfig because UConvaiSettings lives at the SDK module's root, outside its
	// Public folder, so its header is not includable from here. The default is the SDK's own, from
	// UConvaiURL::GetBaseURL.
	FString ProdUrl;
	GConfig->GetString(TEXT("/Script/Convai.ConvaiSettings"), TEXT("CustomProdURL"), ProdUrl, GEngineIni);
	ProdUrl.TrimStartAndEndInline();
	if (ProdUrl.IsEmpty())
	{
		ProdUrl = TEXT("https://api.convai.com");
	}

	TArray<FString> MovedFiles;
	switch (ReconcileStateLayoutIn(GetEssentialsDirectory(), Chunks, EnvironmentSlug(ProdUrl), MovedFiles))
	{
	case EMigrationResult::Migrated:
		CPM_LOG(Display, TEXT("Moved %d file(s) into this project's per-Chunk state directory."), MovedFiles.Num());
		break;

	case EMigrationResult::Ambiguous:
		if (!bUnattributedLayoutReported)
		{
			bUnattributedLayoutReported = true;
			CPM_LOG(Warning,
				TEXT("Found a pre-Chunk ConvaiEssentials layout (%s) but this project does not have exactly one ")
				TEXT("Chunk, so it cannot be attributed and nothing was moved. The Pak Manager panel shows this ")
				TEXT("as a banner."),
				*FString::Join(FlatLegacyFilesIn(GetEssentialsDirectory()), TEXT(", ")));
		}
		break;

	case EMigrationResult::Failed:
		// Already logged which file and why. Nothing was moved, so the creator's own copy of their
		// AssetID is still where it was, and HasUnmigratedLegacyLayout is what puts it in front of them.
		break;

	case EMigrationResult::NothingToMigrate:
		break;
	}

	// Writes DefaultGame.ini, so it stays out of the seam. Not only the labels this tool minted: a
	// Modding Plugin generated before the Modding Tool wrote the scan entry cooks into chunk 0 and
	// produces no pakchunk, and the creator's only symptom is a Publish that ships nothing.
	for (const FCPM_Chunk& Chunk : Chunks)
	{
		const FString LabelPackage = Chunk.LabelPackage.ToString();
		const int32 SecondSlash = LabelPackage.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
		if (SecondSlash != INDEX_NONE)
		{
			EnsureLabelDirectoryScanned(LabelPackage.Left(SecondSlash));
		}
	}
}

namespace
{
	/** The leaf of a package path: "/PLUGIN/Maps/Landing" -> "Landing". Already-short names pass through. */
	FString LeafOf(const FString& PackagePath)
	{
		int32 LastSlash = INDEX_NONE;
		return PackagePath.FindLastChar(TEXT('/'), LastSlash)
			? PackagePath.RightChop(LastSlash + 1)
			: PackagePath;
	}

	/** Sets Field to Fallback only if the document does not already carry a non-empty value for it. */
	void DefaultString(const TSharedRef<FJsonObject>& Root, const TCHAR* Field, const FString& Fallback)
	{
		FString Existing;
		if (!Root->TryGetStringField(Field, Existing) || Existing.IsEmpty())
		{
			Root->SetStringField(Field, Fallback);
		}
	}

	TSharedRef<FJsonObject> ObjectFieldOrEmpty(const TSharedRef<FJsonObject>& Root, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Existing = nullptr;
		return Root->TryGetObjectField(Field, Existing) && Existing && Existing->IsValid()
			? (*Existing).ToSharedRef()
			: MakeShared<FJsonObject>();
	}
}

void FillRequiredMetadataFields(
	const TSharedRef<FJsonObject>& Root,
	const FString& ProjectName,
	const FString& PluginName,
	const FString& AssetType,
	const TMap<ECPM_Platform, int64>& ArtifactSizes)
{
	const FString Type = AssetType.ToLower();

	// The Draft overlay is flat, and gender belongs inside entity_data. Taken off the top level here
	// rather than written there in the first place, because the Draft has one writer for every field
	// and this document is the only thing that knows the shape the API wants.
	FString DraftedGender;
	Root->TryGetStringField(TEXT("gender"), DraftedGender);
	Root->RemoveField(TEXT("gender"));

	Root->SetStringField(TEXT("project_name"), ProjectName);
	Root->SetStringField(TEXT("plugin_name"), PluginName);
	Root->SetStringField(TEXT("asset_type"), Type);

	// Relative to the packaged binary, and into the plugin the Modding Tool made - NOT the project's
	// own Content, which is where every field of this document was pointing when create-asset began
	// failing.
	Root->SetStringField(TEXT("content_path"),
		FString::Printf(TEXT("../../../%s/Plugins/%s/Content/"), *ProjectName, *PluginName));

	// Only a fallback: the mount point a package actually lives under is known exactly when the
	// creator picks the Entry Point, and an internal project can mount its content elsewhere.
	if (!PluginName.IsEmpty())
	{
		DefaultString(Root, TEXT("root_path"), FString::Printf(TEXT("/%s/"), *PluginName));
	}

	// Only the artefacts this run built. A size the run did not measure is left as the server echoed
	// it, because the Version it describes is still on the Asset - a Windows-only run does not mean
	// the Linux Pak stopped existing.
	for (const TPair<ECPM_Platform, int64>& Size : ArtifactSizes)
	{
		Root->SetNumberField(
			UEnum::GetDisplayValueAsText(Size.Key).ToString() + TEXT("_PakSize"),
			static_cast<double>(Size.Value));
	}

	DefaultString(Root, TEXT("asset_name"), ProjectName);
	DefaultString(Root, TEXT("asset_description"), FString());
	DefaultString(Root, TEXT("level_name"), FString());
	DefaultString(Root, TEXT("blueprint_class_path"), FString());

	// "None" rather than "": the shape a published Asset carries for the class it does not have, and
	// what a product's class resolution reads as "there is no blueprint here".
	DefaultString(Root, TEXT("blueprint_class"), TEXT("None"));

	FString AssetName;
	Root->TryGetStringField(TEXT("asset_name"), AssetName);

	// entity_data is merged, never rebuilt: it is the half of the schema the Pak Manager models
	// least, and the server puts things in it that nothing here would know to put back.
	const TSharedRef<FJsonObject> Entity = ObjectFieldOrEmpty(Root, TEXT("entity_data"));
	if (Type == TEXT("avatar"))
	{
		FString ClassPath;
		Root->TryGetStringField(TEXT("blueprint_class_path"), ClassPath);
		if (ClassPath.IsEmpty())
		{
			DefaultString(Entity, TEXT("avatar_name"), AssetName);
		}
		else
		{
			Entity->SetStringField(TEXT("avatar_name"), LeafOf(ClassPath));
		}
		// Set, not defaulted, when the creator chose one: the server's echo is the last thing it was
		// told, and a creator changing the dropdown on a published Avatar must not lose to it.
		if (!DraftedGender.IsEmpty())
		{
			Entity->SetStringField(TEXT("gender"), DraftedGender);
		}
		DefaultString(Entity, TEXT("gender"), TEXT("male"));
		if (!Entity->HasField(TEXT("avatar_config")))
		{
			Entity->SetObjectField(TEXT("avatar_config"), MakeShared<FJsonObject>());
		}
	}
	else
	{
		FString LevelName;
		Root->TryGetStringField(TEXT("level_name"), LevelName);
		if (LevelName.IsEmpty())
		{
			DefaultString(Entity, TEXT("scene_name"), AssetName);
		}
		else
		{
			Entity->SetStringField(TEXT("scene_name"), LeafOf(LevelName));
		}
		DefaultString(Entity, TEXT("scene_description"), TEXT("Pak scene"));
		if (!Entity->HasField(TEXT("scene_metadata")))
		{
			Entity->SetObjectField(TEXT("scene_metadata"), MakeShared<FJsonObject>());
		}
	}
	Root->SetObjectField(TEXT("entity_data"), Entity);
}

bool IsUnderModdingPlugin(const FString& PackageName, const FString& PluginName)
{
	return PluginName.IsEmpty()
		|| PackageName.StartsWith(TEXT("/") + PluginName + TEXT("/"), ESearchCase::IgnoreCase);
}

bool DeclareConvaiDependency(FPluginDescriptor& Descriptor, const FString& ConvaiPluginName)
{
	FPluginReferenceDescriptor* Existing = Descriptor.Plugins.FindByPredicate(
		[&ConvaiPluginName](const FPluginReferenceDescriptor& Reference)
		{
			return Reference.Name.Equals(ConvaiPluginName, ESearchCase::IgnoreCase);
		});

	if (Existing)
	{
		// A disabled entry is still an entry, and the domain database reads bEnabled, not presence.
		if (Existing->bEnabled)
		{
			return false;
		}
		Existing->bEnabled = true;
		return true;
	}

	Descriptor.Plugins.Emplace(ConvaiPluginName, true);
	return true;
}

bool EnsureConvaiDependency(const FString& PluginName, FString& OutError)
{
	if (PluginName.IsEmpty())
	{
		return true;
	}

	IPluginManager& PluginManager = IPluginManager::Get();
	const TSharedPtr<IPlugin> ModdingPlugin = PluginManager.FindPlugin(PluginName);
	if (!ModdingPlugin)
	{
		OutError = FString::Printf(TEXT("no plugin named %s is mounted"), *PluginName);
		return false;
	}

	// Asked for rather than hardcoded, and asked for the way the domain database asks: it resolves a
	// declared dependency with FindEnabledPlugin, so a name taken from a merely discovered plugin
	// would be written into the descriptor and grant nothing.
	const TSharedPtr<IPlugin> Convai = PluginManager.FindEnabledPlugin(TEXT("ConvAI"));
	if (!Convai)
	{
		OutError = TEXT("the Convai plugin is not enabled in this project");
		return false;
	}
	if (Convai == ModdingPlugin)
	{
		return true;
	}

	FPluginDescriptor Descriptor = ModdingPlugin->GetDescriptor();
	if (!DeclareConvaiDependency(Descriptor, Convai->GetName()))
	{
		return true;
	}

	FText FailReason;
	if (!ModdingPlugin->UpdateDescriptor(Descriptor, FailReason))
	{
		OutError = FString::Printf(TEXT("%s could not be written: %s"),
			*ModdingPlugin->GetDescriptorFileName(), *FailReason.ToString());
		return false;
	}

	CPM_LOG(Display, TEXT("Declared %s as a dependency of the %s plugin, so its content may reference Convai's."),
		*Convai->GetName(), *PluginName);
	return true;
}

bool EntryPointSuitsAssetType(const FTopLevelAssetPath& AssetClass, const FString& PackageName,
	const FString& AssetType, FString& OutWhy)
{
	if (AssetType.Equals(TEXT("Scene"), ESearchCase::IgnoreCase))
	{
		if (AssetClass != UWorld::StaticClass()->GetClassPathName())
		{
			OutWhy = FString::Printf(TEXT("a scene's entry point must be a level, and %s is not"), *PackageName);
			return false;
		}
		return true;
	}

	if (AssetClass != UBlueprint::StaticClass()->GetClassPathName())
	{
		OutWhy = FString::Printf(TEXT("an avatar's entry point must be a blueprint, and %s is not"), *PackageName);
		return false;
	}
	return true;
}

FString ResolveLevelPackage(const FString& LevelName, const FString& RootPath)
{
	if (LevelName.IsEmpty() || LevelName.StartsWith(TEXT("/")))
	{
		return LevelName;
	}

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	FString SearchRoot = RootPath;
	SearchRoot.RemoveFromEnd(TEXT("/"));
	if (!SearchRoot.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*SearchRoot));
	}

	TArray<FAssetData> Levels;
	if (const IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->GetAssets(Filter, Levels);
	}
	for (const FAssetData& Level : Levels)
	{
		if (Level.AssetName.ToString() == LevelName)
		{
			return Level.PackageName.ToString();
		}
	}

	// No longer on disk. The short name still says which level was picked.
	return LevelName;
}

bool ComposePakMetadataAt(
	const FString& MetadataPath,
	const FString& DraftPath,
	const FString& ProjectName,
	const FString& PluginName,
	const FString& AssetType,
	const TMap<ECPM_Platform, int64>& ArtifactSizes)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	FString Contents;
	if (FFileHelper::LoadFileToString(Contents, *MetadataPath))
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
		TSharedPtr<FJsonObject> Parsed;
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			// Refused rather than started fresh: the server puts things in this document that
			// nothing here would know to put back, so an unparseable one is left exactly as it is.
			CPM_LOG(Error, TEXT("Refusing to compose %s: it is not valid JSON."), *MetadataPath);
			return false;
		}
		Root = Parsed;
	}

	// The Draft wins every field it names. What a creator typed is the record of the fields they
	// type; the server owns the rest, and is read back rather than assumed. See docs/adr/0013.
	FString DraftContents;
	if (FFileHelper::LoadFileToString(DraftContents, *DraftPath))
	{
		const TSharedRef<TJsonReader<>> DraftReader = TJsonReaderFactory<>::Create(DraftContents);
		TSharedPtr<FJsonObject> Draft;
		if (FJsonSerializer::Deserialize(DraftReader, Draft) && Draft.IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Draft->Values)
			{
				FString Value;
				if (Field.Value.IsValid() && Field.Value->TryGetString(Value))
				{
					Root->SetStringField(Field.Key, Value);
				}
			}
		}
		else
		{
			CPM_LOG(Error, TEXT("Refusing to compose %s: the Draft at %s is not valid JSON."),
				*MetadataPath, *DraftPath);
			return false;
		}
	}

	// Upgraded here rather than left to the creator to re-pick: a document written by an older Pak
	// Manager holds the leaf alone, and the API wants the path.
	FString LevelName, RootPath;
	Root->TryGetStringField(TEXT("level_name"), LevelName);
	Root->TryGetStringField(TEXT("root_path"), RootPath);
	if (!LevelName.IsEmpty() && !LevelName.StartsWith(TEXT("/")))
	{
		Root->SetStringField(TEXT("level_name"), ResolveLevelPackage(LevelName, RootPath));
	}

	FillRequiredMetadataFields(Root.ToSharedRef(), ProjectName, PluginName, AssetType, ArtifactSizes);

	return SaveJsonObject(Root.ToSharedRef(), MetadataPath);
}

bool ComposePakMetadata(
	const int32 ChunkId, const FString& EnvironmentSlug, const TMap<ECPM_Platform, int64>& ArtifactSizes)
{
	FCPM_ModdingMetadata Modding;
	UCPM_UtilityLibrary::GetModdingMetadataForChunk(ChunkId, Modding);

	return ComposePakMetadataAt(
		GetPakMetadataPath(ChunkId, EnvironmentSlug),
		GetDraftPath(ChunkId),
		UCPM_UtilityLibrary::GetProjectName(),
		Modding.PluginName,
		Modding.AssetType,
		ArtifactSizes);
}
}
