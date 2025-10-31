// Fill out your copyright notice in the Description page of Project Settings.


#include "ConvaiPakManagerEditorUtils.h"
#include "CPM_Defination.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Editor.h"                  
#include "ILiveCodingModule.h"
#include "LevelEditor.h"              
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "PlayInEditorDataTypes.h"    
#include "IUATHelperModule.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "Settings/ContentBrowserSettings.h"
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
#include "Elements/Interfaces/TypedElementWorldInterface.h" // For ITypedElementWorldInterface
#include "Elements/Framework/TypedElementHandle.h" // For FTypedElementHandle

void UConvaiPakManagerEditorUtils::CPM_MarkAssetDirty(UObject* Asset)
{
	if (!Asset)
	{
		UE_LOG(LogTemp, Warning, TEXT("MarkAssetDirty: Asset is null."));
		return;
	}

	UPackage* Package = Asset->GetOutermost();
	if (Package)
	{
		Package->SetDirtyFlag(true);
		FAssetRegistryModule::AssetCreated(Asset);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MarkAssetDirty: Could not get package for asset '%s'."), *Asset->GetName());
	}
}

#if WITH_EDITOR
static TSharedPtr<IAssetViewport> GetActiveAssetViewport()
{
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		return LevelEditorModule.GetFirstActiveViewport();
	}
	return nullptr;
}
#endif

void UConvaiPakManagerEditorUtils::CPM_TogglePlayMode()
{
#if WITH_EDITOR
	
	if (!GEditor->PlayWorld)
	{
		FRequestPlaySessionParams PlayParams;
		if (const TSharedPtr<IAssetViewport> ActiveViewport = GetActiveAssetViewport(); ActiveViewport.IsValid())
		{
			const TWeakPtr<IAssetViewport> WeakViewport(ActiveViewport);
			PlayParams.DestinationSlateViewport = WeakViewport;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No valid active viewport found. Launching PIE in a new editor window."));
		}
        
		GEditor->RequestPlaySession(PlayParams);
	}
	else
	{
		GEditor->RequestEndPlayMap();
	}
#endif
}

void UConvaiPakManagerEditorUtils::CPM_PackageProject(const FCPM_PackageParam& PackageParam, const FOnUatTaskResultCallack OnPackagingCompleted)
{
	if (!PackageParam.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PackageParam is not valid"));
        return;
    }

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
            "-clientconfig=%s -nodebuginfo"
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
                OnPackagingCompleted.ExecuteIfBound(Result, Runtime);
            });
        },
        FString()                                                   // ResultLocation
    );
}

void UConvaiPakManagerEditorUtils::CPM_ToggleLiveCoding(const bool Enable)
{
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		LiveCoding->EnableByDefault(Enable);

		if (LiveCoding->IsEnabledByDefault() && !LiveCoding->IsEnabledForSession())
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::FromString(TEXT("NoEnableLiveCodingAfterHotReloadLive Coding cannot be enabled while hot-reloaded modules are active. Please close the editor and build from your IDE before restarting.")));
		}
	}
}

void UConvaiPakManagerEditorUtils::CPM_ShowPluginContent(const bool bEnable)
{
	GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders(bEnable);
	GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders(bEnable);
	
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
	GetMutableDefault<UContentBrowserSettings>()->SaveConfig();
}

void UConvaiPakManagerEditorUtils::CPM_SetEngineScalability(const ECPM_CustomScalabilityLevel Level)
{
	using namespace Scalability;

	FQualityLevels NewLevels;

	switch (Level)
	{
	case ECPM_CustomScalabilityLevel::Low:
		// Low maps to absolute index 0 :contentReference[oaicite:2]{index=2}&#8203;:contentReference[oaicite:3]{index=3}
		NewLevels.SetFromSingleQualityLevel(0);
		break;

	case ECPM_CustomScalabilityLevel::Medium:
		// Medium maps to relative index 3 (i.e. Max–3 => 1) :contentReference[oaicite:4]{index=4}&#8203;:contentReference[oaicite:5]{index=5}
		NewLevels.SetFromSingleQualityLevelRelativeToMax(3);
		break;

	case ECPM_CustomScalabilityLevel::High:
		NewLevels.SetFromSingleQualityLevelRelativeToMax(2);
		break;

	case ECPM_CustomScalabilityLevel::Epic:
		NewLevels.SetFromSingleQualityLevelRelativeToMax(1);
		break;

	case ECPM_CustomScalabilityLevel::Cinematic:
		NewLevels.SetFromSingleQualityLevelRelativeToMax(0);
		break;
	default:
		return;
	}

	SetQualityLevels(NewLevels);
	SaveState(GEditorSettingsIni);
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}
}

bool UConvaiPakManagerEditorUtils::CPM_TakeViewportScreenshot(const FString& FilePath)
{
	if (FilePath.IsEmpty()) return false;
	
	if (!GEditor)
	{
		UE_LOG(LogTemp, Warning, TEXT("GEditor is null."));
		return false;
	}

	const FViewport* RawViewport = GEditor->GetActiveViewport();
	if (!RawViewport)
	{
		UE_LOG(LogTemp, Warning, TEXT("No active viewport."));
		return false;
	}

	FEditorViewportClient* EditorViewportClient = static_cast<FEditorViewportClient*>(RawViewport->GetClient());
	if (!EditorViewportClient)
	{
		UE_LOG(LogTemp, Warning, TEXT("Viewport client is invalid."));
		return false;
	}

	FSceneViewport* SceneViewport = static_cast<FSceneViewport*>(EditorViewportClient->Viewport);
	if (!SceneViewport)
	{
		UE_LOG(LogTemp, Warning, TEXT("Scene viewport is null."));
		return false;
	}

	// Store current game view mode
	const bool bWasGameView = EditorViewportClient->IsInGameView();

	// Enter game view (hides gizmos and overlays)
	EditorViewportClient->SetGameView(true);

	// Resize viewport to 1920x1080
	constexpr uint32 TargetX = 1920;
	constexpr uint32 TargetY = 1080;
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
		UE_LOG(LogTemp, Warning, TEXT("Failed to read pixels."));
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
		UE_LOG(LogTemp, Warning, TEXT("Failed to save screenshot to %s"), *FilePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Clean screenshot saved to: %s"), *FilePath);
	return true;
}

bool UConvaiPakManagerEditorUtils::CPM_CreateZip(const FString& ZipFilePath, const TArray<FString>& Files, const TArray<FString>& Directories)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* FileHandle = PlatformFile.OpenWrite(*ZipFilePath);
	if (!FileHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create zip file: %s"), *ZipFilePath);
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
			UE_LOG(LogTemp, Warning, TEXT("File not found: %s"), *FilePath);
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
			UE_LOG(LogTemp, Warning, TEXT("Invalid relative path for file: %s -> %s"), *FilePath, *RelativePath);
			return false;
		}

		// Load file data
		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to read file: %s"), *FilePath);
			return false;
		}

		// Validate file data
		if (FileData.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Empty file: %s"), *FilePath);
			return false;
		}

		// Add to zip
		ZipWriter.AddFile(RelativePath, FileData, PlatformFile.GetTimeStamp(*FilePath));
		return true;
	};

	// Process directories
	for (const FString& Directory : Directories)
	{
		if (!PlatformFile.DirectoryExists(*Directory))
		{
			UE_LOG(LogTemp, Warning, TEXT("Directory not found: %s"), *Directory);
			continue;
		}

		TArray<FString> L_Files;
		PlatformFile.FindFilesRecursively(L_Files, *Directory, nullptr);

		for (const FString& FilePath : L_Files)
		{
			SafeAddFileToZip(FilePath);
		}
	}

	// Process individual files
	for (const FString& FilePath : Files)
	{
		SafeAddFileToZip(FilePath);
	}
	
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
        UE_LOG(LogTemp, Warning, TEXT("SpawnAndSnapActorToView: ActorClass is null."));
        return nullptr;
    }
    if (!GEditor || !GCurrentLevelEditingViewportClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnAndSnapActorToView: GEditor or GCurrentLevelEditingViewportClient is not available."));
        return nullptr;
    }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnAndSnapActorToView: Cannot get Editor World."));
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
        UE_LOG(LogTemp, Error, TEXT("SpawnAndSnapActorToView: Failed to spawn actor of class %s"), *ActorClass->GetName());
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

