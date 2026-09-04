// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakManagerEditorUtils.h"
#include "CPM_Defination.h"
#include "CPM_DependencyCopyAPI.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Editor.h"                  
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "IUATHelperModule.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "EditorViewportClient.h"               
#include "ImageUtils.h"
#include "Slate/SceneViewport.h"
#include "FileUtilities/ZipArchiveWriter.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LevelEditorViewport.h" // For GCurrentLevelEditingViewportClient
#include "ScopedTransaction.h" // For FScopedTransaction
#include "Editor/EditorEngine.h" // For GEditor
#include "Elements/Framework/TypedElementHandle.h" // For FTypedElementHandle
#include "Engine/PrimaryAssetLabel.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Thumbnail/CPM_Thumbnail.h"
#include "EditorAssetLibrary.h"
#include "Utility/CPM_Log.h"

namespace
{
	/**
	 * A PrimaryAssetLabel bakes the set of assets it labels into its own package when it is saved, and
	 * the cook builds its manager graph from that serialized set rather than re-deriving it. Labels are
	 * created before content is copied into their folder, so the set on disk is just the label itself:
	 * every asset the label was meant to claim lands in chunk 0 and the requested pakchunk is never
	 * emitted. Saving re-derives the set in PreSave, so re-saving the labels here is what makes the cook
	 * agree with what the panel shows.
	 *
	 * Unconditional on purpose. Whether the serialized set is stale cannot be read back: loading a label
	 * refreshes the copy the Asset Registry hands out, and the panel's own Chunk discovery loads every
	 * label at start-up, so by the time packaging runs nothing still looks stale.
	 */
	void ResavePrimaryAssetLabels()
	{
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry)
		{
			CPM_LOG(Warning, TEXT("No Asset Registry; packaging without refreshing the Chunk labels."));
			return;
		}

		FARFilter Filter;
		Filter.ClassPaths.Add(UPrimaryAssetLabel::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> Labels;
		AssetRegistry->GetAssets(Filter, Labels);

		int32 Saved = 0;
		for (const FAssetData& Label : Labels)
		{
			UObject* Asset = Label.GetAsset();
			if (Asset && UEditorAssetLibrary::SaveLoadedAsset(Asset, /*bOnlyIfIsDirty=*/false))
			{
				++Saved;
			}
			else
			{
				CPM_LOG(Warning, TEXT("Could not re-save '%s'; its Chunk may collapse into pakchunk0."),
					*Label.PackageName.ToString());
			}
		}

		CPM_LOG(Log, TEXT("Re-saved %d of %d PrimaryAssetLabel(s) before packaging."), Saved, Labels.Num());
	}
}


void UConvaiPakManagerEditorUtils::CPM_PackageProject(const FCPM_PackageParam& PackageParam, const FOnUatTaskResultCallack OnPackagingCompleted)
{
	if (!PackageParam.IsValid())
    {
        // Answered rather than dropped. The packaging Job keeps no deadline, so a silent return here
        // leaves it waiting for a callback that never comes, for the rest of the editor session.
        CPM_LOG(Error, TEXT("Refusing to package: platform or output directory is unset."));
        OnPackagingCompleted.ExecuteIfBound(TEXT("Failed"), 0.0);
        return;
    }

    ResavePrimaryAssetLabels();

    const FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
    const FString ProjectName = FPaths::GetBaseFilename(ProjectFilePath);
    const FString UnrealExe = FPlatformProcess::ExecutablePath();

    const FString CommandLine = FString::Printf(
        TEXT(
          "-ScriptsForProject=\"%s\" "
          "Turnkey -command=VerifySdk -platform=%s -UpdateIfNeeded "
          "-EditorIO -EditorIOPort=55342 "
          "-project=\"%s\" "
          "BuildCookRun "
            "-nop4 -utf8output -nocompileeditor -skipbuildeditor "
            "-cook -project=\"%s\" -target=%s "
            "-unrealexe=\"%s\" -platform=%s -installed "
            "-stage -archive -package -build -pak -compressed -prereqs "
            "-archivedirectory=\"%s\" -manifests "
            "-clientconfig=%s -nodebuginfo "
            // The cook is a second editor process, and it reads the same EditorPerProjectUserSettings as
            // the running one. With the Model Context Protocol server set to auto-start, the cook tries to
            // bind a port the editor already holds, logs an error, and any cook error is fatal. Turning it
            // off for the cook alone lets MCP stay on in the editor while packaging still succeeds.
            "-AdditionalCookerOptions=-ini:EditorPerProjectUserSettings:"
              "[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False"
        ),
        *ProjectFilePath,              // -ScriptsForProject
        *PackageParam.GetPlatform(),        // -platform=Win64
        *ProjectFilePath,              // first -project for VerifySdk
        *ProjectFilePath,              // second -project for BuildCookRun
        *ProjectName,                  // -target=Blank_53
        *UnrealExe,                    // -unrealexe="...Editor-Cmd.exe"
        *PackageParam.GetPlatform(),        // -platform=Win64
        *PackageParam.OutputDirectory, // -archivedirectory="D:/UEProjects/..."
        *PackageParam.Configuration    // -clientconfig=Shipping
    );

    // The whole line, at Log. It is the first thing anyone diagnosing a cook asks for, and
    // reconstructing it by hand from nine arguments is how it gets asked for twice.
    CPM_LOG(Log, TEXT("Packaging %s (%s): %s %s"),
        *PackageParam.GetPlatform(), *PackageParam.Configuration, *UnrealExe, *CommandLine);

    IUATHelperModule::Get().CreateUatTask(
        CommandLine,
        FText::FromString(PackageParam.GetPlatform()),              // PlatformDisplayName
        FText::FromString(TEXT("Packaging Project")),               // TaskName
        FText::FromString(TEXT("Packaging")),                       // TaskShortName
        nullptr,                                                    // TaskIcon
        /*OptionalAnalyticsParamArray=*/ nullptr,                   // Analytics params (UE5.3+)
        [=](FString Result, double Runtime)
        {
            AsyncTask(ENamedThreads::GameThread, [=]()
            {
                CPM_LOG(Log, TEXT("Packaging %s finished: %s after %.0fs."),
                    *PackageParam.GetPlatform(), *Result, Runtime);
                OnPackagingCompleted.ExecuteIfBound(Result, Runtime);
            });
        },
        FString()                                                   // ResultLocation
    );
}

bool UConvaiPakManagerEditorUtils::CPM_TakeViewportScreenshot(const FString& FilePath)
{
	if (FilePath.IsEmpty()) return false;
	
	if (!GEditor)
	{
		CPM_LOG(Warning, TEXT("GEditor is null."));
		return false;
	}

	const FViewport* RawViewport = GEditor->GetActiveViewport();
	if (!RawViewport)
	{
		CPM_LOG(Warning, TEXT("No active viewport."));
		return false;
	}

	FEditorViewportClient* EditorViewportClient = static_cast<FEditorViewportClient*>(RawViewport->GetClient());
	if (!EditorViewportClient)
	{
		CPM_LOG(Warning, TEXT("Viewport client is invalid."));
		return false;
	}

	FSceneViewport* SceneViewport = static_cast<FSceneViewport*>(EditorViewportClient->Viewport);
	if (!SceneViewport)
	{
		CPM_LOG(Warning, TEXT("Scene viewport is null."));
		return false;
	}

	// Store current game view mode
	const bool bWasGameView = EditorViewportClient->IsInGameView();

	// Enter game view (hides gizmos and overlays)
	EditorViewportClient->SetGameView(true);

	// Every thumbnail this tool writes is one shape, whichever path made it.
	constexpr uint32 TargetX = static_cast<uint32>(ConvaiPakManager::Thumbnail::WrittenWidth);
	constexpr uint32 TargetY = static_cast<uint32>(ConvaiPakManager::Thumbnail::WrittenHeight);
	SceneViewport->SetFixedViewportSize(TargetX, TargetY);
	SceneViewport->UpdateViewportRHI(false, TargetX, TargetY, EWindowMode::Windowed, PF_Unknown);
	SceneViewport->Invalidate();

	// Force redraw
	SceneViewport->Draw(false);
	FlushRenderingCommands(); // Ensure rendering has completed

	// Read pixels
	TArray<FColor> Bitmap;
	if (!SceneViewport->ReadPixels(Bitmap) || Bitmap.Num() == 0)
	{
		CPM_LOG(Warning, TEXT("Failed to read pixels."));
		return false;
	}
	
	// Restore previous state
	EditorViewportClient->SetGameView(bWasGameView);
	SceneViewport->SetFixedViewportSize(0, 0); // Reset size
	const FIntPoint OriginalSize = SceneViewport->GetSizeXY();
	SceneViewport->UpdateViewportRHI(false, OriginalSize.X, OriginalSize.Y, EWindowMode::Windowed, PF_Unknown);
	
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FilePath));
	TArray<uint8> Compressed;
	FImageUtils::ThumbnailCompressImageArray(TargetX, TargetY, Bitmap, Compressed);
	if (!FFileHelper::SaveArrayToFile(Compressed, *FilePath))
	{
		CPM_LOG(Warning, TEXT("Failed to save screenshot to %s"), *FilePath);
		return false;
	}

	CPM_LOG(Log, TEXT("Clean screenshot saved to: %s"), *FilePath);
	return true;
}

bool UConvaiPakManagerEditorUtils::CPM_CreateZip(const FString& ZipFilePath, const TArray<FString>& Files, const TArray<FString>& Directories)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* FileHandle = PlatformFile.OpenWrite(*ZipFilePath);
	if (!FileHandle)
	{
		CPM_LOG(Error, TEXT("Could not create the project archive at %s."), *ZipFilePath);
		return false;
	}

	FZipArchiveWriter ZipWriter(FileHandle);
	const FString ProjectDir = FPaths::ProjectDir();

	// Helper function to safely create relative path and add to zip
	auto SafeAddFileToZip = [&](const FString& FilePath) -> bool
	{
		// Validate file exists
		if (!PlatformFile.FileExists(*FilePath))
		{
			CPM_LOG(Warning, TEXT("File not found: %s"), *FilePath);
			return false;
		}

		// Create relative path
		FString RelativePath = FilePath;
		FPaths::MakePathRelativeTo(RelativePath, *ProjectDir);
		
		// Normalize path separators for zip compatibility
		RelativePath = RelativePath.Replace(TEXT("\\"), TEXT("/"));
		
		// Remove any leading slashes
		RelativePath = RelativePath.TrimStartAndEnd();
		while (RelativePath.StartsWith(TEXT("/")))
		{
			RelativePath = RelativePath.RightChop(1);
		}
		
		// Validate the relative path
		if (RelativePath.IsEmpty() || RelativePath.Contains(TEXT("..")))
		{
			CPM_LOG(Warning, TEXT("Invalid relative path for file: %s -> %s"), *FilePath, *RelativePath);
			return false;
		}

		// Load file data
		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
		{
			CPM_LOG(Error, TEXT("Failed to read file: %s"), *FilePath);
			return false;
		}

		// Validate file data
		if (FileData.Num() == 0)
		{
			CPM_LOG(Warning, TEXT("Empty file: %s"), *FilePath);
			return false;
		}

		// Add to zip
		ZipWriter.AddFile(RelativePath, FileData, PlatformFile.GetTimeStamp(*FilePath));
		return true;
	};

	int32 Added = 0;
	int32 Skipped = 0;

	// Process directories
	for (const FString& Directory : Directories)
	{
		if (!PlatformFile.DirectoryExists(*Directory))
		{
			CPM_LOG(Warning, TEXT("Not archiving %s: no such directory."), *Directory);
			continue;
		}

		TArray<FString> L_Files;
		PlatformFile.FindFilesRecursively(L_Files, *Directory, nullptr);

		for (const FString& FilePath : L_Files)
		{
			SafeAddFileToZip(FilePath) ? ++Added : ++Skipped;
		}
	}

	// Process individual files
	for (const FString& FilePath : Files)
	{
		SafeAddFileToZip(FilePath) ? ++Added : ++Skipped;
	}

	// The per-file results used to be discarded, so an archive in which every file failed reported
	// Success and was uploaded. An empty one is a failure whatever the reason for it.
	if (Added == 0)
	{
		CPM_LOG(Error, TEXT("The project archive at %s would have held nothing; %d file(s) were skipped."),
			*ZipFilePath, Skipped);
		return false;
	}

	CPM_LOG(Log, TEXT("Archived %d file(s) into %s%s."), Added, *ZipFilePath,
		Skipped > 0 ? *FString::Printf(TEXT(" (%d skipped)"), Skipped) : TEXT(""));
	return true;
}

void UConvaiPakManagerEditorUtils::CPM_CreateZipAsync(const FString& ZipFilePath, const TArray<FString>& Files,
	const TArray<FString>& Directories, const FOnUatTaskResultCallack OnZippingCompleted)
{
	Async(EAsyncExecution::Thread, [=]()
	{
		const double StartTime = FPlatformTime::Seconds();
		const bool bSuccess = CPM_CreateZip(ZipFilePath, Files, Directories);
		const FString ResultMessage = bSuccess ? TEXT("Success") : TEXT("Failed");
		const double Runtime = FPlatformTime::Seconds() - StartTime;

		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			OnZippingCompleted.ExecuteIfBound(ResultMessage, Runtime);
		});
	});
}

AActor* UConvaiPakManagerEditorUtils::SpawnAndSnapActorToView(UClass* ActorClass)
{
    // --- Validation ---
    if (!ActorClass)
    {
        CPM_LOG(Warning, TEXT("SpawnAndSnapActorToView: ActorClass is null."));
        return nullptr;
    }
    if (!GEditor || !GCurrentLevelEditingViewportClient)
    {
        CPM_LOG(Warning, TEXT("SpawnAndSnapActorToView: GEditor or GCurrentLevelEditingViewportClient is not available."));
        return nullptr;
    }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        CPM_LOG(Warning, TEXT("SpawnAndSnapActorToView: Cannot get Editor World."));
        return nullptr;
    }

    // --- Constants ---
    static const FName EditorSpawnTag(TEXT("editorspawn"));

    // --- View Transform ---
    const FVector NewLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
    const FQuat   NewRotation = GCurrentLevelEditingViewportClient->GetViewRotation().Quaternion();
    const FTransform ViewTransform(NewRotation, NewLocation);

    // --- Look for an existing 'editorspawn' actor ---
    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It;
        if (IsValid(A) && !A->IsPendingKillPending() && A->ActorHasTag(EditorSpawnTag))
        {
            TargetActor = A;
            break; // Use the first one found
        }
    }

    // --- Transaction & dirtied scope ---
    FScopedTransaction Transaction(TargetActor
        ? NSLOCTEXT("UnrealEd", "MoveEditorSpawnActorToView", "Move 'editorspawn' Actor to View")
        : NSLOCTEXT("UnrealEd", "SpawnAndSnapActor", "Spawn and Snap Actor to View"));
    FScopedLevelDirtied LevelDirtyCallback;

    // --- If found: just move that actor ---
    if (TargetActor)
    {
        TargetActor->SetFlags(RF_Transactional);

        // Ensure a movable scene root so transform panel & movement work
        if (!TargetActor->GetRootComponent())
        {
            USceneComponent* SceneRoot = NewObject<USceneComponent>(TargetActor, USceneComponent::StaticClass(), TEXT("DefaultSceneRoot"));
            SceneRoot->SetMobility(EComponentMobility::Movable);
            TargetActor->SetRootComponent(SceneRoot);
            SceneRoot->RegisterComponent();
        }
        else if (TargetActor->GetRootComponent()->Mobility != EComponentMobility::Movable)
        {
            TargetActor->GetRootComponent()->SetMobility(EComponentMobility::Movable);
        }

        // Prevent construction scripts while we move things
        FEditorScriptExecutionGuard ScriptGuard;

        TargetActor->SetActorTransform(ViewTransform, /*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);

        LevelDirtyCallback.Request();

        // Editor state
        GEditor->SetPivot(ViewTransform.GetLocation(), false, true);
        GEditor->SelectNone(/*bNoteSelectionChange=*/false, /*bDeselectBSPSurfs=*/true);
        GEditor->SelectActor(TargetActor, /*bSelected=*/true, /*bNotify=*/true);
        GEditor->RedrawLevelEditingViewports();

        return TargetActor;
    }

    // --- Otherwise: spawn new actor, add 'editorspawn' tag, ensure movable root ---
    AActor* SpawnedActor = World->SpawnActorDeferred<AActor>(ActorClass, ViewTransform, /*Owner=*/nullptr, /*Instigator=*/nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!SpawnedActor)
    {
        CPM_LOG(Error, TEXT("SpawnAndSnapActorToView: Failed to spawn actor of class %s"), *ActorClass->GetName());
        Transaction.Cancel();
        return nullptr;
    }

    SpawnedActor->SetFlags(RF_Transactional);

    // Tag it
    SpawnedActor->Tags.AddUnique(EditorSpawnTag);

    // Guarantee a movable scene root
    if (!SpawnedActor->GetRootComponent())
    {
        USceneComponent* SceneRoot = NewObject<USceneComponent>(SpawnedActor, USceneComponent::StaticClass(), TEXT("DefaultSceneRoot"));
        SceneRoot->SetMobility(EComponentMobility::Movable);
        SpawnedActor->SetRootComponent(SceneRoot);
        SceneRoot->RegisterComponent();
    }
    else if (SpawnedActor->GetRootComponent()->Mobility != EComponentMobility::Movable)
    {
        SpawnedActor->GetRootComponent()->SetMobility(EComponentMobility::Movable);
    }

    // Finish spawn & place
    SpawnedActor->FinishSpawning(ViewTransform, /*bIsDefaultTransform=*/true);

    {
        FEditorScriptExecutionGuard ScriptGuard;
        SpawnedActor->SetActorTransform(ViewTransform, /*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);
    }

    LevelDirtyCallback.Request();

    // Editor state
    GEditor->SetPivot(ViewTransform.GetLocation(), false, true);
    GEditor->SelectNone(/*bNoteSelectionChange=*/false, /*bDeselectBSPSurfs=*/true);
    GEditor->SelectActor(SpawnedActor, /*bSelected=*/true, /*bNotify=*/true);
    GEditor->RedrawLevelEditingViewports();

    return SpawnedActor;
}

bool UConvaiPakManagerEditorUtils::GetPackageDependencies(const FName& PackageName, const TArray<FString>& FilterPaths, 
	TSet<FName>& AllDependencies, TSet<FString>& ExternalObjectsPaths, TSet<FName>& ExcludedDependencies)
{
	if (PackageName.IsNone())
	{
		return false; // invalid input
	}

	// Derive the mount point safely (preferred over manual splitting)
	const FName MountPoint = FPackageName::GetPackageMountPoint(PackageName.ToString());
	if (MountPoint.IsNone())
	{
		return false; // malformed package path
	}
	const FString Root = TEXT("/") + MountPoint.ToString() + TEXT("/"); // e.g. "/ConvaiPluginContent/"

	// We don't want to miss any violations, so never early-stop recursion.
	auto NeverExclude = [FilterPaths](FName Dep) -> bool
	{
		const FString S = Dep.ToString();
		for (const auto& It : FilterPaths)
		{
			if (!S.IsEmpty() && S.StartsWith(It))
			{
				return true;
			}
		}
		return false;
	};
	
	// Make sure outputs are clean before we fill them
	// (comment out the Resets if you want to accumulate across multiple calls)
	AllDependencies.Reset();
	ExternalObjectsPaths.Reset();
	
	RecursiveGetDependencies(
		PackageName,
		AllDependencies,
		ExternalObjectsPaths,
		ExcludedDependencies,
		NeverExclude
	);
	
	bool bAllInsideSameRoot = true;
	for (const FName& Dep : AllDependencies)
	{
		const FString DepStr = Dep.ToString();
		if (!DepStr.StartsWith(Root))
		{
			bAllInsideSameRoot = false;
			break;
		}
	}

	return bAllInsideSameRoot;
}

void UConvaiPakManagerEditorUtils::RecursiveGetDependencies(const FName& PackageName, TSet<FName>& AllDependencies, TSet<FString>& OutExternalObjectsPaths, TSet<FName>& ExcludedDependencies, const TFunction<bool (FName)>& ShouldExcludeFromDependenciesSearch)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FName> Dependencies;
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.GetDependencies(PackageName, Dependencies);
	
	for (TArray<FName>::TConstIterator DependsIt = Dependencies.CreateConstIterator(); DependsIt; ++DependsIt)
	{
		FString DependencyName = (*DependsIt).ToString();

		const bool bIsScriptPackage = DependencyName.StartsWith(TEXT("/Script"));

		// The asset registry can give some reference to some deleted assets. We don't want to migrate these.
		const bool bAssetExist = AssetRegistry.GetAssetPackageDataCopy(*DependsIt).IsSet();

		if (!bIsScriptPackage && bAssetExist)
		{
			uint32 DependsHash = GetTypeHash(*DependsIt);
			if (!AllDependencies.ContainsByHash(DependsHash, *DependsIt) && !ExcludedDependencies.ContainsByHash(DependsHash, *DependsIt))
			{
				// Early stop the dependency search
				if (ShouldExcludeFromDependenciesSearch(*DependsIt))
				{
					ExcludedDependencies.AddByHash(DependsHash, *DependsIt);
					continue;
				}

				AllDependencies.AddByHash(DependsHash, *DependsIt);

				RecursiveGetDependencies(*DependsIt, AllDependencies, OutExternalObjectsPaths, ExcludedDependencies, ShouldExcludeFromDependenciesSearch);
			}
		}
	}

	// Handle Specific External Objects use case (only used for the Migrate path for now)
	// todo: revisit how to handle those in a more generic way. Should the FExternalActorAssetDependencyGatherer handle the external objects reference also?
	TArray<FAssetData> Assets;

	// The migration only work on the saved version of the assets so no need to scan the for the in memory only assets. This also greatly improve the performance of the migration when a lot of assets are loaded in the editor.
	constexpr bool bOnlyIncludeOnDiskAssets = true;
	if (AssetRegistryModule.Get().GetAssetsByPackageName(PackageName, Assets, bOnlyIncludeOnDiskAssets))
	{
		for (const FAssetData& AssetData : Assets)
		{
			if (AssetData.GetClass() && AssetData.GetClass()->IsChildOf<UWorld>())
			{
				TArray<FString> ExternalObjectsPaths = ULevel::GetExternalObjectsPaths(PackageName.ToString());
				for (const FString& ExternalObjectsPath : ExternalObjectsPaths)
				{
					if (!ExternalObjectsPath.IsEmpty() && !OutExternalObjectsPaths.Contains(ExternalObjectsPath))
					{
						OutExternalObjectsPaths.Add(ExternalObjectsPath);
						AssetRegistryModule.Get().ScanPathsSynchronous({ ExternalObjectsPath }, /*bForceRescan*/true, /*bIgnoreDenyListScanFilters*/true);

						TArray<FAssetData> ExternalObjectAssets;
						AssetRegistryModule.Get().GetAssetsByPath(FName(*ExternalObjectsPath), ExternalObjectAssets, /*bRecursive*/true, bOnlyIncludeOnDiskAssets);

						for (const FAssetData& ExternalObjectAsset : ExternalObjectAssets)
						{
							// We don't expose the early dependency search exit to the external objects/actors since to the users there are same the outer package that own these objects
							AllDependencies.Add(ExternalObjectAsset.PackageName);
							RecursiveGetDependencies(ExternalObjectAsset.PackageName, AllDependencies, OutExternalObjectsPaths, ExcludedDependencies, ShouldExcludeFromDependenciesSearch);
						}
					}
				}
			}
		}
	}
}
