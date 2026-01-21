
---

# Convai Pak Manager: Slate Migration & Architecture Plan

## Executive Summary

Based on my review of your codebase, you have:
- **Existing Runtime module** with proxy classes (`CPM_Proxy`, `CPM_GithubProxy`) using UObject-based async patterns
- **Existing Editor module** with minimal setup (mostly empty module implementation)
- **Context reference folder** containing a sophisticated MVVM + DI architecture from ConvaiEditor
- **Existing Slate widgets** in Runtime (`SCPM_EditableTextBox`, `SCPM_Button`, etc.) that can be reused

The migration path I recommend leverages the proven patterns from your Context reference while adapting them to the Pak Manager's specific needs.

---

## Key Decision: Editor-Only Module

### Why Remove the Runtime Module?

The Pak Manager is **purely an editor-time tool**. All functionality is editor-only:

| Feature | When It Runs |
|---------|--------------|
| Package assets into .pak | Editor only |
| Upload/update to backend | Editor only |
| Capture thumbnails | Editor only |
| GitHub config fetch | Editor only |
| UAT invocation | Editor only |

**Benefits of consolidating to a single Editor module:**

1. **Smaller shipped builds** - No dead code in packaged games
2. **Simpler architecture** - One module to maintain
3. **Clearer intent** - Explicitly an editor tool
4. **No accidental runtime dependencies** - Can't accidentally reference from game code
5. **Faster iteration** - Single module compile

---

## Part 1: Recommended Architecture

### 1.1 Single Editor Module Structure

```
┌─────────────────────────────────────────────────────────────────────┐
│  ConvaiPakManagerEditor (Editor Module) - SINGLE MODULE             │
│                                                                      │
│  ├── Types/                   - Data structures (FCPM_*, enums)     │
│  ├── Proxy/                   - HTTP proxy classes (moved from RT)  │
│  ├── Utility/                 - Utilities, logging                  │
│  │                                                                   │
│  ├── Infrastructure/          - DI container, Result types          │
│  ├── Async/                   - Async utilities (from Context)      │
│  │                                                                   │
│  ├── Services/                - Business logic, workflow            │
│  │   ├── PakWorkflowService   - Create/Update/Package orchestration │
│  │   ├── ConfigFetchService   - GitHub config fetching + caching    │
│  │   ├── PackagingService     - UAT packaging coordination          │
│  │   └── ValidationService    - Input validation                    │
│  │                                                                   │
│  ├── MVVM/                                                           │
│  │   ├── ViewModelBase        - Base class (from Context)           │
│  │   └── AssetUploaderVM      - Main ViewModel                       │
│  │                                                                   │
│  └── UI/                                                             │
│      ├── Shell/               - Main window (SWindow)               │
│      ├── Pages/               - Page content widgets                │
│      └── Widgets/             - Composite UI components             │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 Layered Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│  UI Layer (Slate Widgets)                                            │
│    └─ SAssetUploaderWindow (main shell)                             │
│    └─ SPakManagerForm (input fields + buttons)                       │
│    └─ SThumbnailPicker (pick/capture thumbnail)                      │
│    └─ Binds to ViewModel properties via TAttribute                   │
├─────────────────────────────────────────────────────────────────────┤
│  ViewModel Layer                                                      │
│    └─ FAssetUploaderViewModel                                        │
│    └─ Manages loading/error states                                   │
│    └─ Calls services, handles callbacks                              │
│    └─ Uses WeakPtr for async safety                                  │
├─────────────────────────────────────────────────────────────────────┤
│  Service Layer                                                        │
│    └─ FPakWorkflowService - Orchestrates multi-step workflows        │
│    └─ FConfigFetchService - GitHub config with caching               │
│    └─ FPackagingService   - UAT invocation + monitoring              │
│    └─ Composes resilience patterns (retry, circuit breaker)          │
├─────────────────────────────────────────────────────────────────────┤
│  Infrastructure Layer                                                 │
│    └─ TResult<T>          - Explicit error handling                  │
│    └─ FAsyncOperation<T>  - Async wrapper with cancellation          │
│    └─ FCancellationToken  - Cooperative cancellation                 │
│    └─ IConvaiDIContainer  - Dependency injection                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Part 2: Widget Structure

### 2.1 Main Window Options

**Option A: Standalone SWindow (Recommended for your use case)**

```cpp
// SCPM_AssetUploaderShell.h
class SCPM_AssetUploaderShell : public SWindow
{
public:
    SLATE_BEGIN_ARGS(SCPM_AssetUploaderShell)
        : _InitialWidth(800)
        , _InitialHeight(700)
    {}
    SLATE_ARGUMENT(int32, InitialWidth)
    SLATE_ARGUMENT(int32, InitialHeight)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetViewModel(TSharedPtr<FAssetUploaderViewModel> InViewModel);
    
private:
    TSharedPtr<FAssetUploaderViewModel> ViewModel;
    TSharedPtr<SCPM_AssetUploaderContent> ContentWidget;
};
```

**Option B: SDockTab (if you want editor integration)**

```cpp
// Register tab in module startup
void FConvaiPakManagerEditorModule::RegisterTab()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        "ConvaiPakManager",
        FOnSpawnTab::CreateRaw(this, &FConvaiPakManagerEditorModule::SpawnTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Pak Manager"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);
}
```

**Recommendation**: Use **Option A (SWindow)** because:
- Better control over window lifecycle
- Easier to manage scope-based services
- More similar to your current EUW approach
- Cleaner separation from editor tabs

### 2.2 Main Content Widget

```cpp
// SCPM_AssetUploaderContent.h
class SCPM_AssetUploaderContent : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCPM_AssetUploaderContent) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FAssetUploaderViewModel> InViewModel);
    
private:
    // UI Construction
    TSharedRef<SWidget> BuildAssetTypeSection();
    TSharedRef<SWidget> BuildMetadataSection();
    TSharedRef<SWidget> BuildThumbnailSection();
    TSharedRef<SWidget> BuildAssetPathSection();
    TSharedRef<SWidget> BuildActionButtons();
    TSharedRef<SWidget> BuildStatusSection();
    
    // Event handlers
    void OnAssetTypeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void OnPackageClicked();
    void OnCreateClicked();
    void OnUpdateClicked();
    void OnCreateAndUpdateClicked();
    void OnCleanClicked();
    
    // ViewModel binding
    void RefreshUI();
    void OnLoadingStateChanged(bool bIsLoading, const FText& Message);
    
    TSharedPtr<FAssetUploaderViewModel> ViewModel;
    
    // Widget references for updates
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<SProgressBar> ProgressBar;
};
```

### 2.3 Toolbar/Menu Integration

```cpp
// In ConvaiPakManagerEditor.cpp - StartupModule()
void FConvaiPakManagerEditorModule::StartupModule()
{
    // Initialize DI container and services first
    InitializeCoreArchitecture();
    RegisterServices();
    
    // Register menu extension
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(
                this, &FConvaiPakManagerEditorModule::RegisterMenus));
    }
}

void FConvaiPakManagerEditorModule::RegisterMenus()
{
    // Add to Window menu
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    FToolMenuSection& Section = Menu->AddSection("ConvaiPakManager", 
        LOCTEXT("ConvaiPakManagerSection", "Convai"));
    
    Section.AddMenuEntry(
        "OpenPakManager",
        LOCTEXT("OpenPakManagerLabel", "Pak Manager"),
        LOCTEXT("OpenPakManagerTooltip", "Open Convai Pak Manager"),
        FSlateIcon(), // Add your icon here
        FUIAction(FExecuteAction::CreateRaw(this, &FConvaiPakManagerEditorModule::OpenPakManagerWindow)));
    
    // Add toolbar button
    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
    FToolMenuSection& ToolbarSection = ToolbarMenu->FindOrAddSection("ConvaiPakManager");
    
    ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
        "OpenPakManager",
        FUIAction(FExecuteAction::CreateRaw(this, &FConvaiPakManagerEditorModule::OpenPakManagerWindow)),
        LOCTEXT("ToolbarLabel", "Pak Manager"),
        LOCTEXT("ToolbarTooltip", "Open Convai Pak Manager"),
        FSlateIcon())); // Add your icon
}

void FConvaiPakManagerEditorModule::OpenPakManagerWindow()
{
    if (PakManagerWindow.IsValid())
    {
        PakManagerWindow->BringToFront();
        return;
    }
    
    // Create scope for window-lifetime services
    WindowScope = FConvaiDIContainerManager::CreateScope("PakManagerWindow");
    FConvaiDIContainerManager::PushScope(WindowScope);
    
    // Create ViewModel
    auto ViewModel = MakeShared<FAssetUploaderViewModel>();
    ViewModel->Initialize();
    
    // Create window
    PakManagerWindow = SNew(SCPM_AssetUploaderShell)
        .Title(LOCTEXT("WindowTitle", "Convai Pak Manager"));
    
    PakManagerWindow->SetViewModel(ViewModel);
    PakManagerWindow->SetOnWindowClosed(
        FOnWindowClosed::CreateRaw(this, &FConvaiPakManagerEditorModule::OnWindowClosed));
    
    FSlateApplication::Get().AddWindow(PakManagerWindow.ToSharedRef());
}
```

---

## Part 3: Workflow Architecture

### 3.1 State Machine for Create/Update Workflows

```cpp
// PakWorkflowTypes.h
UENUM()
enum class EPakWorkflowState : uint8
{
    Idle,
    
    // Config fetch phase
    FetchingConfig,
    ConfigReady,
    ConfigFailed,
    
    // Packaging phase
    Packaging,
    PackagingComplete,
    PackagingFailed,
    
    // Create asset phase
    CreatingAsset,
    AssetCreated,
    CreateFailed,
    
    // Upload phase
    Uploading,
    UploadProgress,
    UploadComplete,
    UploadFailed,
    
    // Update phase  
    UpdatingAsset,
    UpdateComplete,
    UpdateFailed,
    
    // Final states
    WorkflowComplete,
    WorkflowCancelled,
    WorkflowError
};

USTRUCT()
struct FPakWorkflowContext
{
    GENERATED_BODY()
    
    // Input parameters
    ECPM_AssetType AssetType;
    FString SceneName;
    FString AssetName;
    FString AssetDescription;
    FString AssetPath;
    UTexture2D* Thumbnail;
    FString Gender; // For avatars
    TMap<FString, FString> AdditionalMetadata;
    
    // Workflow state
    EPakWorkflowState CurrentState = EPakWorkflowState::Idle;
    FString LastError;
    float UploadProgress = 0.0f;
    
    // Results from previous steps
    FString ConfigJson;
    TArray<FString> GeneratedPakPaths;
    FString CreatedAssetId;
    TArray<FString> UploadUrls;
};
```

### 3.2 Workflow Service Interface

```cpp
// IPakWorkflowService.h
class IPakWorkflowService : public IConvaiService
{
public:
    static FName StaticType() { return TEXT("IPakWorkflowService"); }
    
    // Workflow execution
    virtual void ExecuteCreateWorkflow(
        const FPakWorkflowContext& Context,
        TSharedPtr<FCancellationToken> CancellationToken = nullptr) = 0;
        
    virtual void ExecuteUpdateWorkflow(
        const FString& AssetId,
        const FPakWorkflowContext& Context,
        TSharedPtr<FCancellationToken> CancellationToken = nullptr) = 0;
        
    virtual void ExecuteCreateAndUpdateWorkflow(
        const FPakWorkflowContext& Context,
        TSharedPtr<FCancellationToken> CancellationToken = nullptr) = 0;
    
    virtual void CancelCurrentWorkflow() = 0;
    virtual bool IsWorkflowRunning() const = 0;
    virtual EPakWorkflowState GetCurrentState() const = 0;
    
    // Events
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStateChanged, EPakWorkflowState, const FPakWorkflowContext&);
    virtual FOnStateChanged& OnStateChanged() = 0;
    
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnProgressUpdated, float);
    virtual FOnProgressUpdated& OnProgressUpdated() = 0;
    
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorkflowComplete, bool, const FString&);
    virtual FOnWorkflowComplete& OnWorkflowComplete() = 0;
};
```

### 3.3 Workflow Service Implementation Pattern

```cpp
// FPakWorkflowService.cpp (simplified)
void FPakWorkflowService::ExecuteCreateAndUpdateWorkflow(
    const FPakWorkflowContext& InContext,
    TSharedPtr<FCancellationToken> CancellationToken)
{
    Context = InContext;
    Token = CancellationToken ? CancellationToken : MakeShared<FCancellationToken>();
    
    TransitionTo(EPakWorkflowState::FetchingConfig);
    
    // Start the workflow chain
    FetchConfigAsync()
        .Then<void>([this](const FString& Config) { return PackageAsync(Config); })
        .Then<FCPM_CreatedAssets>([this]() { return CreateAssetAsync(); })
        .Then<void>([this](const FCPM_CreatedAssets& Assets) { return UploadPaksAsync(Assets); })
        .OnComplete([this](const TResult<void>& Result)
        {
            if (Result.IsSuccess())
            {
                TransitionTo(EPakWorkflowState::WorkflowComplete);
                OnWorkflowCompleteDelegate.Broadcast(true, TEXT(""));
            }
            else
            {
                TransitionTo(EPakWorkflowState::WorkflowError);
                OnWorkflowCompleteDelegate.Broadcast(false, Result.GetError());
            }
        });
}

void FPakWorkflowService::TransitionTo(EPakWorkflowState NewState)
{
    Context.CurrentState = NewState;
    OnStateChangedDelegate.Broadcast(NewState, Context);
}
```

### 3.4 Async HTTP Pattern (Bridging Existing Proxies)

Your existing proxy classes use UObject delegates. Here's how to bridge them to the new async pattern:

```cpp
// AsyncProxyBridge.h
template<typename TProxy, typename TResult>
class TAsyncProxyBridge : public TSharedFromThis<TAsyncProxyBridge<TProxy, TResult>>
{
public:
    using FResultCallback = TFunction<void(const TConvaiResult<TResult>&)>;
    
    static void Execute(
        TProxy* Proxy,
        TFunction<void(TProxy*)> BindSuccess,
        TFunction<void(TProxy*)> BindFailure,
        FResultCallback OnComplete)
    {
        // Keep proxy alive
        Proxy->AddToRoot();
        
        TWeakPtr<TAsyncProxyBridge> WeakThis = MakeShared<TAsyncProxyBridge>()->AsShared();
        
        // Bind success
        BindSuccess(Proxy);
        
        // Bind failure  
        BindFailure(Proxy);
        
        // Activate the proxy
        Proxy->Activate();
    }
};

// Usage example:
TSharedPtr<FAsyncOperation<FCPM_CreatedAssets>> FPakWorkflowService::CreateAssetAsync()
{
    return MakeShared<FAsyncOperation<FCPM_CreatedAssets>>(
        [this](TSharedPtr<FCancellationToken> Token, TSharedPtr<IAsyncProgressReporter> Progress) 
            -> TConvaiResult<FCPM_CreatedAssets>
        {
            // Use promise pattern to bridge
            TSharedPtr<TPromise<TConvaiResult<FCPM_CreatedAssets>>> Promise = 
                MakeShared<TPromise<TConvaiResult<FCPM_CreatedAssets>>>();
            
            auto Proxy = UCPM_CreatePakAssetProxy::CreatePakAssetProxy(BuildParams());
            
            Proxy->OnSuccess.AddLambda([Promise](const FCPM_CreatedAssets& Assets, const FString&)
            {
                Promise->SetValue(TConvaiResult<FCPM_CreatedAssets>::Success(Assets));
            });
            
            Proxy->OnFailure.AddLambda([Promise](const FCPM_CreatedAssets&, const FString& Error)
            {
                Promise->SetValue(TConvaiResult<FCPM_CreatedAssets>::Failure(Error));
            });
            
            Proxy->Activate();
            
            return Promise->GetFuture().Get(); // Blocking wait
        },
        Token);
}
```

---

## Part 4: ViewModel Pattern

### 4.1 Asset Uploader ViewModel

```cpp
// FAssetUploaderViewModel.h
class FAssetUploaderViewModel : public FViewModelBase
{
public:
    static FName StaticType() { return TEXT("FAssetUploaderViewModel"); }
    
    virtual void Initialize() override;
    virtual void Shutdown() override;
    
    // Observable properties
    TObservableProperty<ECPM_AssetType> AssetType;
    TObservableProperty<FString> SceneName;
    TObservableProperty<FString> AssetName;
    TObservableProperty<FString> AssetDescription;
    TObservableProperty<FString> AssetPath;
    TObservableProperty<FString> Gender;
    TObservableProperty<UTexture2D*> Thumbnail;
    
    // State properties
    TObservableProperty<EPakWorkflowState> WorkflowState;
    TObservableProperty<float> UploadProgress;
    TObservableProperty<FString> StatusMessage;
    TObservableProperty<bool> CanExecute;
    
    // Commands
    void ExecutePackage();
    void ExecuteCreate();
    void ExecuteUpdate();
    void ExecuteCreateAndUpdate();
    void ExecuteClean();
    void CancelWorkflow();
    
    // Validation
    bool ValidateInputs(FString& OutError) const;
    bool IsAvatarType() const { return AssetType.Get() == ECPM_AssetType::Avatar; }
    
private:
    void OnWorkflowStateChanged(EPakWorkflowState State, const FPakWorkflowContext& Context);
    void OnWorkflowProgress(float Progress);
    void OnWorkflowComplete(bool bSuccess, const FString& Error);
    
    TSharedPtr<IPakWorkflowService> WorkflowService;
    TSharedPtr<FCancellationTokenSource> CurrentTokenSource;
    
    FDelegateHandle StateChangedHandle;
    FDelegateHandle ProgressHandle;
    FDelegateHandle CompleteHandle;
};
```

### 4.2 ViewModel Implementation Pattern

```cpp
// FAssetUploaderViewModel.cpp
void FAssetUploaderViewModel::Initialize()
{
    FViewModelBase::Initialize();
    
    // Resolve services
    auto& Container = FConvaiDIContainerManager::Get();
    Container.Resolve<IPakWorkflowService>()
        .LogOnFailure(LogConvaiPakManager, TEXT("Failed to resolve workflow service"))
        .Tap([this](TSharedPtr<IPakWorkflowService> Service)
        {
            WorkflowService = Service;
            
            // Subscribe to workflow events using WeakPtr pattern
            TWeakPtr<FAssetUploaderViewModel> WeakThis = SharedThis(this);
            
            StateChangedHandle = Service->OnStateChanged().AddLambda(
                [WeakThis](EPakWorkflowState State, const FPakWorkflowContext& Context)
                {
                    if (auto This = WeakThis.Pin())
                    {
                        // Marshal to game thread
                        AsyncTask(ENamedThreads::GameThread, [This, State, Context]()
                        {
                            This->OnWorkflowStateChanged(State, Context);
                        });
                    }
                });
            
            TrackDelegate(Service->OnStateChanged(), StateChangedHandle);
        });
    
    // Set initial state
    CanExecute.Set(true);
    StatusMessage.Set(TEXT("Ready"));
}

void FAssetUploaderViewModel::ExecuteCreateAndUpdate()
{
    FString ValidationError;
    if (!ValidateInputs(ValidationError))
    {
        StatusMessage.Set(ValidationError);
        return;
    }
    
    StartLoading(LOCTEXT("CreatingAndUpdating", "Creating and uploading asset..."));
    CanExecute.Set(false);
    
    // Build context from observable properties
    FPakWorkflowContext Context;
    Context.AssetType = AssetType.Get();
    Context.SceneName = SceneName.Get();
    Context.AssetName = AssetName.Get();
    Context.AssetDescription = AssetDescription.Get();
    Context.AssetPath = AssetPath.Get();
    Context.Thumbnail = Thumbnail.Get();
    
    if (IsAvatarType())
    {
        Context.Gender = Gender.Get();
    }
    
    // Create cancellation token
    CurrentTokenSource = MakeShared<FCancellationTokenSource>();
    
    // Execute workflow
    WorkflowService->ExecuteCreateAndUpdateWorkflow(Context, CurrentTokenSource->GetToken());
}

void FAssetUploaderViewModel::OnWorkflowStateChanged(EPakWorkflowState State, const FPakWorkflowContext& Context)
{
    WorkflowState.Set(State);
    
    // Update status message based on state
    switch (State)
    {
        case EPakWorkflowState::FetchingConfig:
            StatusMessage.Set(TEXT("Fetching configuration..."));
            break;
        case EPakWorkflowState::Packaging:
            StatusMessage.Set(TEXT("Packaging assets..."));
            break;
        case EPakWorkflowState::CreatingAsset:
            StatusMessage.Set(TEXT("Creating asset record..."));
            break;
        case EPakWorkflowState::Uploading:
            StatusMessage.Set(FString::Printf(TEXT("Uploading... %.0f%%"), Context.UploadProgress * 100.f));
            break;
        // ... other states
    }
    
    BroadcastInvalidated();
}

void FAssetUploaderViewModel::OnWorkflowComplete(bool bSuccess, const FString& Error)
{
    StopLoading();
    CanExecute.Set(true);
    
    if (bSuccess)
    {
        StatusMessage.Set(TEXT("Workflow completed successfully!"));
    }
    else
    {
        StatusMessage.Set(FString::Printf(TEXT("Error: %s"), *Error));
    }
    
    BroadcastInvalidated();
}
```

---

## Part 5: Common Pitfalls & Solutions

### 5.1 Lifetime Issues

| Problem | Solution |
|---------|----------|
| Widget destroyed but async callback fires | Always use `TWeakPtr<MyWidget> WeakThis = SharedThis(this)` and check `.Pin()` in callbacks |
| Service outlives module | Use scoped services tied to window lifetime; clean up in `OnWindowClosed` |
| Circular references between ViewModel and Service | ViewModel holds `TSharedPtr` to service; service fires delegates (no back-reference) |
| UObject proxies getting GC'd | Call `AddToRoot()` before async, `RemoveFromRoot()` in completion callback |

### 5.2 GC vs Shared Refs

```cpp
// WRONG - UObject can be GC'd during async
auto Proxy = UCPM_CreatePakAssetProxy::CreatePakAssetProxy(Params);
Proxy->Activate(); // Might crash if GC runs

// CORRECT - Prevent GC during async
auto Proxy = UCPM_CreatePakAssetProxy::CreatePakAssetProxy(Params);
Proxy->AddToRoot(); // Prevent GC
Proxy->OnSuccess.AddLambda([Proxy](...)
{
    Proxy->RemoveFromRoot(); // Allow GC after completion
});
Proxy->OnFailure.AddLambda([Proxy](...)
{
    Proxy->RemoveFromRoot();
});
Proxy->Activate();
```

### 5.3 Editor Module Build.cs Configuration

Since we're now a single Editor-only module, no conditional checks are needed:

```csharp
// ConvaiPakManagerEditor.Build.cs
public class ConvaiPakManagerEditor : ModuleRules
{
    public ConvaiPakManagerEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "HTTP",
            "Json",
            "JsonUtilities",
            "PakFile",
            "ImageWrapper",
            "AssetRegistry",
            "Convai"  // Main Convai plugin dependency
        });
        
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Slate
            "Slate",
            "SlateCore",
            
            // Editor
            "UnrealEd",
            "LevelEditor",
            "ToolMenus",
            "EditorSubsystem",
            "ContentBrowser",
            "AssetTools",
            "PropertyEditor",
            "DesktopPlatform",
            
            // UAT/Packaging
            "UATHelper",
            "LiveCoding",
            
            // Rendering (for thumbnail capture)
            "RenderCore",
            
            // Projects (for plugin paths)
            "Projects"
        });
    }
}
```

**Note:** Remove the old `ConvaiPakManager` Runtime module from `.uplugin`:

```json
{
    "Modules": [
        {
            "Name": "ConvaiPakManagerEditor",
            "Type": "Editor",
            "LoadingPhase": "PostEngineInit",
            "PlatformAllowList": ["Win64"]
        }
        // Remove ConvaiPakManager Runtime module entry
    ]
}
```

### 5.4 Threading Rules

```cpp
// WRONG - Updating Slate from background thread
Proxy->OnSuccess.AddLambda([this](...)
{
    MyTextBlock->SetText(FText::FromString("Done")); // CRASH
});

// CORRECT - Marshal to game thread
Proxy->OnSuccess.AddLambda([WeakThis](...)
{
    AsyncTask(ENamedThreads::GameThread, [WeakThis]()
    {
        if (auto This = WeakThis.Pin())
        {
            This->MyTextBlock->SetText(FText::FromString("Done"));
        }
    });
});
```

---

## Part 6: Step-by-Step Migration Plan

### Phase 0: Consolidate to Single Editor Module

**0.1 Move Runtime code to Editor module:**

```
Source/ConvaiPakManager/Public/Types/         → Source/ConvaiPakManagerEditor/Public/Types/
Source/ConvaiPakManager/Public/Proxy/         → Source/ConvaiPakManagerEditor/Public/Proxy/
Source/ConvaiPakManager/Public/Utility/       → Source/ConvaiPakManagerEditor/Public/Utility/
Source/ConvaiPakManager/Private/Proxy/        → Source/ConvaiPakManagerEditor/Private/Proxy/
Source/ConvaiPakManager/Private/Utility/      → Source/ConvaiPakManagerEditor/Private/Utility/
Source/ConvaiPakManager/Public/SlateWidgets/  → Source/ConvaiPakManagerEditor/Public/UI/Widgets/Core/
Source/ConvaiPakManager/Private/SlateWidgets/ → Source/ConvaiPakManagerEditor/Private/UI/Widgets/Core/
```

**0.2 Update API export macro:**

In all moved files, replace:
```cpp
// From:
class CONVAIPAKMANAGER_API SomeClass
// To:
class CONVAIPAKMANAGEREDITOR_API SomeClass
```

**0.3 Update include paths in moved files:**

```cpp
// From:
#include "Proxy/CPM_Proxy.h"
// To:
#include "ConvaiPakManagerEditor/Public/Proxy/CPM_Proxy.h"
// Or just:
#include "Proxy/CPM_Proxy.h"  // If Public is in include path
```

**0.4 Update Build.cs with all dependencies:**

See Section 5.3 for the complete Build.cs configuration.

**0.5 Update .uplugin - Remove Runtime module:**

```json
{
    "FileVersion": 3,
    "Version": 240,
    "VersionName": "2.4.0",
    "FriendlyName": "ConvaiPakManager",
    "Modules": [
        {
            "Name": "ConvaiPakManagerEditor",
            "Type": "Editor",
            "LoadingPhase": "PostEngineInit",
            "PlatformAllowList": ["Win64"]
        }
    ],
    "Plugins": [
        {"Name": "EditorScriptingUtilities", "Enabled": true},
        {"Name": "JsonBlueprintUtilities", "Enabled": true},
        {"Name": "ConvAI", "Enabled": true}
    ]
}
```

**0.6 Delete old Runtime module folder:**

After verifying the Editor module compiles and works:
```
Delete: Source/ConvaiPakManager/  (entire folder)
```

**0.7 Verify compilation:**
- Compile the plugin
- Ensure all proxy classes and utilities work
- Test existing EUW still functions (it should, just calling Editor module now)

---

### Phase 1: Infrastructure Setup (Foundation)

**1.1 Copy infrastructure from Context folder:**
```
Context/ConvaiEditor/Public/Services/ConvaiDIContainer.h   → Source/ConvaiPakManagerEditor/Public/Infrastructure/
Context/ConvaiEditor/Private/Services/ConvaiDIContainer.cpp → Source/ConvaiPakManagerEditor/Private/Infrastructure/
Context/ConvaiEditor/Public/Services/ServiceScope.h        → Source/ConvaiPakManagerEditor/Public/Infrastructure/
Context/ConvaiEditor/Private/Services/ServiceScope.cpp     → Source/ConvaiPakManagerEditor/Private/Infrastructure/
Context/ConvaiEditor/Public/Async/*                        → Source/ConvaiPakManagerEditor/Public/Async/
Context/ConvaiEditor/Private/Async/*                       → Source/ConvaiPakManagerEditor/Private/Async/
Context/ConvaiEditor/Public/MVVM/ViewModel.h               → Source/ConvaiPakManagerEditor/Public/MVVM/
Context/ConvaiEditor/Private/MVVM/ViewModel.cpp            → Source/ConvaiPakManagerEditor/Private/MVVM/
```

**1.2 Adapt to your namespace:**
- Rename `CONVAIEDITOR_API` → `CONVAIPAKMANAGEREDITOR_API`
- Rename `LogConvaiEditor` → `LogConvaiPakManagerEditor`
- Update include paths
- Create log category in module header

**1.3 Update module header with log category:**

```cpp
// ConvaiPakManagerEditor.h
#pragma once
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogConvaiPakManagerEditor, Log, All);

class FConvaiPakManagerEditorModule : public IModuleInterface
{
    // ...
};
```

### Phase 2: Service Layer

**2.1 Create service interfaces:**
- `IPakWorkflowService` - Workflow orchestration
- `IConfigFetchService` - GitHub config with caching
- `IPackagingService` - UAT packaging wrapper

**2.2 Implement services:**
- Bridge existing proxy classes to new async pattern
- Add retry/circuit breaker for HTTP calls
- Implement config caching

### Phase 3: ViewModel Layer

**3.1 Create FAssetUploaderViewModel:**
- Observable properties for all inputs
- Command methods for buttons
- Validation logic
- Service subscriptions

### Phase 4: UI Layer

**4.1 Create main window shell:**
- `SCPM_AssetUploaderShell` (SWindow subclass)

**4.2 Create content widget:**
- `SCPM_AssetUploaderContent` (main form)
- Reuse existing `SCPM_EditableTextBox`, `SCPM_ComboBox`, etc.

**4.3 Create specialized widgets:**
- `SCPM_ThumbnailPicker` - Pick texture or capture
- `SCPM_AssetPathPicker` - Content browser integration
- `SCPM_WorkflowProgress` - State visualization

### Phase 5: Integration

**5.1 Module startup:**
- Initialize DI container
- Register services
- Register menu/toolbar extensions

**5.2 Window management:**
- Handle window lifecycle
- Scope-based service cleanup

### Phase 6: Testing & Polish

**6.1 Test workflows:**
- Create flow
- Update flow
- Create + Update flow
- Cancellation
- Error recovery

**6.2 Deprecate old EUW:**
- Keep functional until Slate UI is stable
- Add toggle to switch between UIs (optional)

---

## Part 7: Final Directory Structure

After migration, the complete Editor module structure:

```
Source/ConvaiPakManagerEditor/
├── ConvaiPakManagerEditor.Build.cs       (updated with all dependencies)
│
├── Public/
│   ├── ConvaiPakManagerEditor.h          (module header + log category)
│   │
│   ├── Types/                            ← MOVED FROM RUNTIME
│   │   ├── CPM_WidgetTypes.h
│   │   └── CPM_WorkflowTypes.h           (NEW - workflow states/context)
│   │
│   ├── Proxy/                            ← MOVED FROM RUNTIME
│   │   ├── CPM_Proxy.h
│   │   └── CPM_GithubProxy.h
│   │
│   ├── Utility/                          ← MOVED FROM RUNTIME
│   │   ├── CPM_Log.h
│   │   ├── CPM_Utils.h
│   │   └── CPM_UtilityLibrary.h
│   │
│   ├── Infrastructure/                   ← NEW (from Context)
│   │   ├── CPM_DIContainer.h
│   │   ├── CPM_Result.h
│   │   ├── CPM_Service.h
│   │   └── CPM_ServiceScope.h
│   │
│   ├── Async/                            ← NEW (from Context)
│   │   ├── CPM_AsyncOperation.h
│   │   ├── CPM_CancellationToken.h
│   │   └── CPM_AsyncProgress.h
│   │
│   ├── Services/                         ← NEW
│   │   ├── IPakWorkflowService.h
│   │   ├── IConfigFetchService.h
│   │   └── IPackagingService.h
│   │
│   ├── MVVM/                             ← NEW (from Context)
│   │   ├── CPM_ViewModelBase.h
│   │   └── FAssetUploaderViewModel.h
│   │
│   └── UI/
│       ├── Shell/                        ← NEW
│       │   └── SCPM_AssetUploaderShell.h
│       │
│       ├── Pages/                        ← NEW
│       │   └── SCPM_AssetUploaderContent.h
│       │
│       └── Widgets/
│           ├── Core/                     ← MOVED FROM RUNTIME
│           │   ├── SCPM_Button.h
│           │   ├── SCPM_ComboBox.h
│           │   ├── SCPM_EditableTextBox.h
│           │   ├── SCPM_IconButton.h
│           │   └── SCPM_Label.h
│           │
│           ├── Composite/                ← MOVED FROM RUNTIME
│           │   └── SCPM_KeyValueRow.h
│           │
│           └── Custom/                   ← NEW
│               ├── SCPM_ThumbnailPicker.h
│               ├── SCPM_AssetPathPicker.h
│               └── SCPM_WorkflowProgress.h
│
└── Private/
    ├── ConvaiPakManagerEditor.cpp        (updated - DI, services, menu)
    │
    ├── Proxy/                            ← MOVED FROM RUNTIME
    │   ├── CPM_Proxy.cpp
    │   └── CPM_GithubProxy.cpp
    │
    ├── Utility/                          ← MOVED FROM RUNTIME
    │   ├── CPM_Log.cpp
    │   ├── CPM_Utils.cpp
    │   └── CPM_UtilityLibrary.cpp
    │
    ├── Infrastructure/                   ← NEW (from Context)
    │   ├── CPM_DIContainer.cpp
    │   └── CPM_ServiceScope.cpp
    │
    ├── Async/                            ← NEW (from Context)
    │   └── CPM_AsyncOperation.cpp
    │
    ├── Services/                         ← NEW
    │   ├── FPakWorkflowService.cpp
    │   ├── FConfigFetchService.cpp
    │   └── FPackagingService.cpp
    │
    ├── MVVM/                             ← NEW (from Context)
    │   ├── CPM_ViewModelBase.cpp
    │   └── FAssetUploaderViewModel.cpp
    │
    └── UI/
        ├── Shell/                        ← NEW
        │   └── SCPM_AssetUploaderShell.cpp
        │
        ├── Pages/                        ← NEW
        │   └── SCPM_AssetUploaderContent.cpp
        │
        └── Widgets/
            ├── Core/                     ← MOVED FROM RUNTIME
            │   ├── SCPM_Button.cpp
            │   ├── SCPM_ComboBox.cpp
            │   ├── SCPM_EditableTextBox.cpp
            │   ├── SCPM_IconButton.cpp
            │   └── SCPM_Label.cpp
            │
            ├── Composite/                ← MOVED FROM RUNTIME  
            │   └── SCPM_KeyValueRow.cpp
            │
            └── Custom/                   ← NEW
                ├── SCPM_ThumbnailPicker.cpp
                ├── SCPM_AssetPathPicker.cpp
                └── SCPM_WorkflowProgress.cpp
```

### Files to DELETE after migration:

```
Source/ConvaiPakManager/                  (entire Runtime module folder)
```

### .uplugin changes:

Remove the Runtime module entry, keep only Editor module.

---

## Summary

This migration plan gives you:

1. **Single Editor-only module** - No runtime overhead, smaller shipped builds
2. **Clean separation of concerns** - UI binds to ViewModel, ViewModel calls Services
3. **Predictable async handling** - Result types, cancellation tokens, game thread marshalling
4. **Reuse of existing code** - Proxy classes moved (not rewritten), existing Slate widgets reused
5. **Incremental migration** - Each phase is independently testable
6. **Resilient workflows** - Retry, circuit breakers, explicit error propagation

### Migration Order Summary:

| Phase | Focus | Outcome |
|-------|-------|---------|
| **Phase 0** | Consolidate modules | Single Editor module compiling |
| **Phase 1** | Infrastructure | DI container, Result types, Async helpers |
| **Phase 2** | Services | Workflow orchestration, config fetch |
| **Phase 3** | ViewModel | State management, input binding |
| **Phase 4** | UI | Slate window, form widgets |
| **Phase 5** | Integration | Menu/toolbar, window lifecycle |
| **Phase 6** | Testing | All workflows, error handling |

### Key Architectural Decisions:

1. **Editor-only** - All functionality is development-time only
2. **MVVM pattern** - Clean separation between UI and logic
3. **Service layer** - Reusable, testable business logic
4. **DI container** - Loose coupling, easy testing/mocking
5. **Result types** - Explicit error handling, no hidden exceptions
6. **WeakPtr pattern** - Safe async callbacks

The architecture follows the proven patterns from your Context reference while adapting them to the Pak Manager's specific needs.

---

## Next Steps

Ready to begin implementation? The recommended starting point is:

1. **Phase 0** - Consolidate to single Editor module (low risk, verifiable)
2. **Phase 1** - Copy and adapt infrastructure from Context folder
3. Then proceed with Services → ViewModel → UI

Would you like to start with Phase 0 (module consolidation)?