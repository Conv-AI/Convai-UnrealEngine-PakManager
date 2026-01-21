# Slate Async Patterns & Design Guide

> A reference guide for implementing robust async workflows in Unreal Engine Slate-based editor plugins.  
> Extracted from patterns used in the ConvaiEditor module.

---

## Table of Contents

1. [Core Principles](#core-principles)
2. [Result Type Pattern](#result-type-pattern)
3. [Async Operation Wrapper](#async-operation-wrapper)
4. [Resilience Patterns](#resilience-patterns)
5. [MVVM Integration](#mvvm-integration)
6. [WeakPtr Safety Pattern](#weakptr-safety-pattern)
7. [Game Thread Marshalling](#game-thread-marshalling)
8. [Event Aggregator Pattern](#event-aggregator-pattern)
9. [Service Architecture](#service-architecture)
10. [Quick Reference Cheat Sheet](#quick-reference-cheat-sheet)

---

## Core Principles

Create clean, reusable, maintainable code

### The Layered Architecture

```
┌─────────────────────────────────────────────────────────┐
│  UI Layer (Slate Widgets)                               │
│    └─ Binds to ViewModel properties                     │
│    └─ Subscribes to OnInvalidated(), OnLoadingChanged() │
├─────────────────────────────────────────────────────────┤
│  ViewModel Layer                                         │
│    └─ Manages loading/error states                       │
│    └─ Calls services, handles callbacks                  │
│    └─ Uses WeakPtr for safety                            │
├─────────────────────────────────────────────────────────┤
│  Service Layer                                           │
│    └─ Business logic, HTTP calls                         │
│    └─ Composes resilience patterns                       │
│    └─ Returns results via delegates                      │
├─────────────────────────────────────────────────────────┤
│  Infrastructure Layer                                    │
│    └─ TResult<T> - error handling                        │
│    └─ FCancellationToken - cancellation                  │
│    └─ FCircuitBreaker, FRetryPolicy - resilience         │
└─────────────────────────────────────────────────────────┘
```

---

## Result Type Pattern

### Why Use Result Types?

- **Explicit error handling** - Compiler reminds you to handle failures
- **No exceptions** - UE4/5 doesn't use exceptions; Result types are the alternative
- **Chainable** - Monadic operations like `Map`, `Bind`, `Tap`
- **Context preservation** - Error messages can be enriched with context

### Basic Implementation

```cpp
template <typename T>
class TResult
{
public:
    bool IsSuccess() const { return bSuccess; }
    bool IsFailure() const { return !bSuccess; }
    
    const T& GetValue() const 
    { 
        check(bSuccess);
        return Value.GetValue(); 
    }
    
    const FString& GetError() const 
    { 
        check(!bSuccess);
        return ErrorMessage; 
    }

    // Factory methods
    static TResult<T> Success(const T& InValue);
    static TResult<T> Success(T&& InValue);
    static TResult<T> Failure(const FString& InError);

private:
    TOptional<T> Value;
    FString ErrorMessage;
    bool bSuccess = false;
};

// Void specialization
template <>
class TResult<void>
{
public:
    static TResult<void> Success();
    static TResult<void> Failure(const FString& InError);
    // ... similar interface
};
```

### Monadic Operations

```cpp
// Map - Transform success value
auto DoubledResult = Result.Map([](int Value) { return Value * 2; });

// Bind - Chain operations that return Results
auto ChainedResult = FirstResult.Bind([](const FString& Data) 
{
    return ParseJson(Data);  // Returns TResult<FJsonObject>
});

// Tap - Side effects without modifying result
Result
    .Tap([](const T& Value) { UE_LOG(LogTemp, Log, TEXT("Got value")); })
    .TapError([](const FString& Error) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); });

// LogOnFailure - Automatic error logging
auto FinalResult = Service->DoSomething()
    .LogOnFailure(LogMyPlugin, TEXT("Operation failed"));

// OrElse - Fallback on failure
auto ResultWithFallback = PrimaryOperation()
    .OrElse([]() { return FallbackOperation(); });
```

### Usage Example

```cpp
TResult<FUserData> FetchUserData(const FString& UserId)
{
    if (UserId.IsEmpty())
    {
        return TResult<FUserData>::Failure(TEXT("UserId cannot be empty"));
    }

    FHttpResponse Response = HttpClient->Get(BuildUserUrl(UserId));
    
    if (!Response.IsSuccess())
    {
        return TResult<FUserData>::Failure(
            FString::Printf(TEXT("HTTP %d: %s"), Response.Code, *Response.Error));
    }

    FUserData Data;
    if (!ParseUserData(Response.Body, Data))
    {
        return TResult<FUserData>::Failure(TEXT("Failed to parse user data"));
    }

    return TResult<FUserData>::Success(MoveTemp(Data));
}

// Caller
FetchUserData(UserId)
    .LogOnFailure(LogMyPlugin, TEXT("User fetch failed"))
    .Tap([this](const FUserData& Data) 
    {
        UpdateUI(Data);
    });
```

---

## Async Operation Wrapper

### Core Interface

```cpp
enum class EAsyncOperationState : uint8
{
    NotStarted,
    Running,
    Succeeded,
    Failed,
    Cancelled
};

class FAsyncOperationBase : public TSharedFromThis<FAsyncOperationBase>
{
public:
    virtual void Start() = 0;
    virtual void Cancel() = 0;
    virtual bool IsRunning() const = 0;
    virtual bool IsComplete() const = 0;
    virtual EAsyncOperationState GetState() const = 0;
    virtual TSharedPtr<FCancellationToken> GetCancellationToken() const = 0;
};
```

### Typed Implementation

```cpp
template <typename TResult>
class FAsyncOperation : public FAsyncOperationBase
{
public:
    // Delegate for completion
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnComplete, const TResult<TResult>&);
    
    // Work function signature
    using FWorkFunction = TFunction<TResult<TResult>(
        TSharedPtr<FCancellationToken>, 
        TSharedPtr<IProgressReporter>)>;

    FAsyncOperation(FWorkFunction InWork, TSharedPtr<FCancellationToken> Token = nullptr);

    virtual void Start() override
    {
        State = EAsyncOperationState::Running;
        
        // Execute on thread pool
        TWeakPtr<FAsyncOperation> WeakThis = AsShared();
        Async(EAsyncExecution::ThreadPool, [WeakThis]()
        {
            if (auto This = WeakThis.Pin())
            {
                This->ExecuteWork();
            }
        });
    }

    // Register completion callback
    FDelegateHandle OnComplete(TFunction<void(const TResult<TResult>&)> Callback);

    // Chain operations
    template <typename TNext>
    TSharedPtr<FAsyncOperation<TNext>> Then(TFunction<TResult<TNext>(const TResult&)> Continuation);

private:
    void ExecuteWork();
    void CompleteWith(const TResult<TResult>& Result);
    void BroadcastCompletion();
};
```

### Usage Pattern

```cpp
// Create async operation
auto Operation = MakeShared<FAsyncOperation<FHttpResponse>>(
    [Request](TSharedPtr<FCancellationToken> Token, TSharedPtr<IProgressReporter> Progress)
    {
        return PerformHttpRequest(Request, Token, Progress);
    });

// Register callback
Operation->OnComplete([this](const TResult<FHttpResponse>& Result)
{
    if (Result.IsSuccess())
    {
        ProcessResponse(Result.GetValue());
    }
    else
    {
        ShowError(Result.GetError());
    }
});

// Start execution
Operation->Start();

// Later: cancel if needed
Operation->Cancel();
```

---

## Resilience Patterns

### Circuit Breaker

Prevents cascading failures by "tripping" after consecutive errors.

```cpp
enum class ECircuitState : uint8
{
    Closed,    // Normal operation
    Open,      // Failing fast - rejecting requests
    HalfOpen   // Testing if service recovered
};

struct FCircuitBreakerConfig
{
    int32 FailureThreshold = 5;      // Failures before opening
    int32 SuccessThreshold = 2;      // Successes to close from half-open
    float OpenTimeoutSeconds = 30.0f; // Wait before testing recovery
    FString Name = TEXT("Default");
};

class FCircuitBreaker
{
public:
    template <typename TResult>
    TResult<TResult> Execute(TFunction<TResult<TResult>()> Operation)
    {
        if (!CanExecute())
        {
            return TResult<TResult>::Failure(
                FString::Printf(TEXT("Circuit '%s' is OPEN"), *Config.Name));
        }

        TResult<TResult> Result = Operation();

        if (Result.IsSuccess())
            OnSuccess();
        else
            OnFailure();

        return Result;
    }

private:
    bool CanExecute();  // Check state, timeout elapsed
    void OnSuccess();   // Reset failures or close circuit
    void OnFailure();   // Increment failures, maybe open circuit
};
```

### Retry Policy

Automatic retry with configurable backoff.

```cpp
enum class ERetryStrategy : uint8
{
    None,
    Fixed,       // Same delay each time
    Linear,      // Delay increases linearly
    Exponential  // Delay doubles each time
};

struct FRetryConfig
{
    int32 MaxAttempts = 3;
    float BaseDelaySeconds = 1.0f;
    float MaxDelaySeconds = 30.0f;
    ERetryStrategy Strategy = ERetryStrategy::Exponential;
    bool bEnableJitter = true;  // Prevent thundering herd
    
    // Custom predicate for retryable errors
    TFunction<bool(const FString& Error, int32 Attempt)> ShouldRetry;
};

class FRetryPolicy
{
public:
    template <typename TResult>
    TResult<TResult> Execute(TFunction<TResult<TResult>()> Operation)
    {
        int32 Attempt = 0;
        TResult<TResult> Result;

        while (Attempt <= Config.MaxAttempts)
        {
            Result = Operation();
            
            if (Result.IsSuccess())
                return Result;

            Attempt++;
            
            if (Attempt > Config.MaxAttempts)
                return Result;  // Exhausted retries
            
            if (Config.ShouldRetry && !Config.ShouldRetry(Result.GetError(), Attempt))
                return Result;  // Non-retryable error

            float Delay = CalculateDelay(Attempt);
            FPlatformProcess::Sleep(Delay);
        }

        return Result;
    }

private:
    float CalculateDelay(int32 Attempt) const;
};
```

### Composing Resilience Patterns

```cpp
// Composition: CircuitBreaker → Retry → HTTP
TResult<FHttpResponse> ExecuteWithProtection(
    const FHttpRequest& Request,
    TSharedPtr<FCircuitBreaker> CircuitBreaker,
    TSharedPtr<FRetryPolicy> RetryPolicy)
{
    auto HttpOperation = [&Request]() { return PerformHttp(Request); };

    if (CircuitBreaker && RetryPolicy)
    {
        // CircuitBreaker wraps Retry wraps HTTP
        return CircuitBreaker->Execute<FHttpResponse>([&]()
        {
            return RetryPolicy->Execute<FHttpResponse>(HttpOperation);
        });
    }
    else if (CircuitBreaker)
    {
        return CircuitBreaker->Execute<FHttpResponse>(HttpOperation);
    }
    else if (RetryPolicy)
    {
        return RetryPolicy->Execute<FHttpResponse>(HttpOperation);
    }
    
    return HttpOperation();
}
```

---

## MVVM Integration

### ViewModel Base Class

```cpp
class FViewModelBase : public TSharedFromThis<FViewModelBase>
{
public:
    virtual void Initialize() { bInitialized = true; }
    virtual void Shutdown() { bShutdown = true; }

    // Loading state management
    void StartLoading(const FText& Message);
    void StopLoading();
    bool IsLoading() const { return bIsLoading; }
    FText GetLoadingMessage() const { return LoadingMessage; }

    // UI refresh notification
    DECLARE_MULTICAST_DELEGATE(FOnInvalidated);
    FOnInvalidated& OnInvalidated() { return InvalidatedDelegate; }

    // Loading state change notification
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLoadingStateChanged, bool, const FText&);
    FOnLoadingStateChanged& OnLoadingStateChanged() { return LoadingStateChangedDelegate; }

protected:
    void BroadcastInvalidated() { InvalidatedDelegate.Broadcast(); }

private:
    bool bInitialized = false;
    bool bShutdown = false;
    bool bIsLoading = false;
    FText LoadingMessage;
    FOnInvalidated InvalidatedDelegate;
    FOnLoadingStateChanged LoadingStateChangedDelegate;
};
```

### Observable Properties

```cpp
template <typename T>
class TObservableProperty
{
public:
    explicit TObservableProperty(const T& InitialValue = T())
        : Value(InitialValue) {}

    const T& Get() const { return Value; }
    
    void Set(const T& NewValue)
    {
        if (Value != NewValue)
        {
            T OldValue = Value;
            Value = NewValue;
            OnChangedDelegate.Broadcast(OldValue, NewValue);
        }
    }

    // For Slate attribute binding
    TAttribute<T> AsAttribute()
    {
        return TAttribute<T>::Create(
            TAttribute<T>::FGetter::CreateLambda([this]() { return Get(); }));
    }

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnChanged, const T&, const T&);
    FOnChanged& OnChanged() { return OnChangedDelegate; }

private:
    T Value;
    FOnChanged OnChangedDelegate;
};

// Convenience typedefs
using FObservableString = TObservableProperty<FString>;
using FObservableBool = TObservableProperty<bool>;
using FObservableInt = TObservableProperty<int32>;
```

### ViewModel Usage in Slate

```cpp
// ViewModel
class FAccountViewModel : public FViewModelBase
{
public:
    void LoadAccount(const FString& ApiKey)
    {
        StartLoading(FText::FromString(TEXT("Loading...")));
        
        TWeakPtr<FAccountViewModel> WeakThis = SharedThis(this);
        
        AccountService->GetAccount(ApiKey, 
            FOnAccountReceived::CreateLambda([WeakThis](const FAccountData& Data, const FString& Error)
            {
                if (auto This = WeakThis.Pin())
                {
                    This->StopLoading();
                    
                    if (Error.IsEmpty())
                    {
                        This->AccountData.Set(Data);
                        This->BroadcastInvalidated();
                    }
                }
            }));
    }

    TObservableProperty<FAccountData> AccountData;
};

// Slate Widget
void SAccountPage::Construct(const FArguments& InArgs)
{
    ViewModel = MakeShared<FAccountViewModel>();
    
    // Subscribe to changes
    ViewModel->OnInvalidated().AddSP(this, &SAccountPage::RefreshUI);
    ViewModel->OnLoadingStateChanged().AddSP(this, &SAccountPage::OnLoadingChanged);
    
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        [
            SNew(STextBlock)
            // Bind directly to observable property
            .Text(ViewModel->AccountData.AsAttribute().Transform([](const FAccountData& Data)
            {
                return FText::FromString(Data.UserName);
            }))
        ]
    ];
}
```

---

## WeakPtr Safety Pattern

### Why WeakPtr Matters

Async callbacks often outlive the object that initiated them. Using raw `this` pointers causes crashes.

### The Pattern

```cpp
void FMyService::StartAsyncOperation()
{
    // WRONG - may crash if service is destroyed during operation
    SomeAsyncCall([this](const FResult& Result)
    {
        this->ProcessResult(Result);  // 💥 Crash if 'this' is gone
    });

    // CORRECT - safely handles destruction
    TWeakPtr<FMyService> WeakThis = AsShared();  // Requires TSharedFromThis
    
    SomeAsyncCall([WeakThis](const FResult& Result)
    {
        if (TSharedPtr<FMyService> This = WeakThis.Pin())
        {
            This->ProcessResult(Result);  // ✅ Safe
        }
        // else: object was destroyed, silently skip
    });
}
```

### Chained Async with WeakPtr

```cpp
void FAuthService::Authenticate(const FString& EncryptedKey)
{
    TWeakPtr<FAuthService> WeakThis = AsShared();

    DecryptionService->DecryptAsync(
        EncryptedKey,
        // Success callback
        [WeakThis](const FString& DecryptedKey)
        {
            TSharedPtr<FAuthService> This = WeakThis.Pin();
            if (!This.IsValid()) return;

            This->ApiKey = DecryptedKey;

            // Chain second async operation - capture WeakPtr again
            This->ValidationService->ValidateKey(
                DecryptedKey,
                [WeakThis](bool bValid)
                {
                    if (auto This = WeakThis.Pin())
                    {
                        This->OnValidationComplete(bValid);
                    }
                });
        },
        // Error callback
        [WeakThis](const FString& Error)
        {
            if (auto This = WeakThis.Pin())
            {
                This->OnAuthFailed(Error);
            }
        });
}
```

---

## Game Thread Marshalling

### The Problem

Slate UI can only be modified from the game thread. Async operations often complete on worker threads.

### The Solution

```cpp
// AsyncTask to game thread
AsyncTask(ENamedThreads::GameThread, [Result, Callback]()
{
    Callback(Result);  // Now safe to update UI
});

// Or using delegates that auto-marshal
void FMyService::OnHttpComplete(const FHttpResponse& Response)
{
    // This might be on HTTP thread
    
    TWeakPtr<FMyService> WeakThis = AsShared();
    
    AsyncTask(ENamedThreads::GameThread, [WeakThis, Response]()
    {
        if (auto This = WeakThis.Pin())
        {
            This->ProcessResponseOnGameThread(Response);
        }
    });
}
```

### Complete Pattern

```cpp
void FDataService::FetchData(FOnDataReceived Callback)
{
    auto Operation = MakeShared<FAsyncOperation<FData>>(
        [](TSharedPtr<FCancellationToken> Token, ...) -> TResult<FData>
        {
            // This runs on worker thread
            return PerformFetch();
        });

    Operation->OnComplete([Callback](const TResult<FData>& Result)
    {
        // Completion callback - ensure game thread
        AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
        {
            if (Result.IsSuccess())
            {
                Callback.ExecuteIfBound(Result.GetValue(), FString());
            }
            else
            {
                Callback.ExecuteIfBound(FData(), Result.GetError());
            }
        });
    });

    Operation->Start();
}
```

---

## Event Aggregator Pattern

### Why Use It?

- **Decoupling** - Publishers don't know about subscribers
- **Cross-cutting concerns** - Network state, auth changes affect multiple components
- **Automatic cleanup** - WeakPtr subscriptions auto-unsubscribe

### Basic Implementation

```cpp
class FEventAggregator
{
public:
    static FEventAggregator& Get();

    // Subscribe with auto-cleanup on object destruction
    template <typename TEvent, typename TObject>
    FEventSubscription Subscribe(
        TWeakPtr<TObject> WeakRef, 
        TFunction<void(const TEvent&)> Handler);

    // Publish to all subscribers
    template <typename TEvent>
    void Publish(const TEvent& Event);

    // Publish ensuring game thread execution
    template <typename TEvent>
    void PublishGameThread(const TEvent& Event);
};

// RAII subscription token
class FEventSubscription
{
public:
    ~FEventSubscription() { Unsubscribe(); }
    void Unsubscribe();
    
    // Move-only
    FEventSubscription(FEventSubscription&&) = default;
    FEventSubscription& operator=(FEventSubscription&&) = default;
};
```

### Event Types

```cpp
// Define events as structs
struct FNetworkRestoredEvent
{
    double DowntimeSeconds;
    int32 CircuitBreakersReset;
    
    FString GetEventName() const { return TEXT("NetworkRestored"); }
};

struct FAuthStateChangedEvent
{
    bool bIsAuthenticated;
    FString UserName;
    
    FString GetEventName() const { return TEXT("AuthStateChanged"); }
};
```

### Usage

```cpp
// Subscriber (e.g., ViewModel)
void FHomeViewModel::Initialize()
{
    TWeakPtr<FHomeViewModel> WeakThis = SharedThis(this);
    
    // Store subscription - will auto-unsubscribe when this is destroyed
    NetworkSubscription = FEventAggregator::Get().Subscribe<FNetworkRestoredEvent>(
        WeakThis,
        [WeakThis](const FNetworkRestoredEvent& Event)
        {
            if (auto This = WeakThis.Pin())
            {
                This->RefreshAllContent();
            }
        });
}

void FHomeViewModel::Shutdown()
{
    NetworkSubscription.Unsubscribe();  // Explicit cleanup (optional)
}

// Publisher (e.g., NetworkMonitor)
void FNetworkMonitor::OnConnectivityRestored()
{
    FEventAggregator::Get().Publish(FNetworkRestoredEvent{
        .DowntimeSeconds = DowntimeDuration,
        .CircuitBreakersReset = ResetCount
    });
}
```

---

## Service Architecture

### Dependency Injection Container

```cpp
// Service registration
DIContainer.RegisterService<IAuthService, FAuthService>(
    EServiceLifetime::Singleton);

DIContainer.RegisterService<IDataService, FDataService>(
    EServiceLifetime::Transient);

// Service resolution with Result type
auto Result = DIContainer.Resolve<IAuthService>();
if (Result.IsSuccess())
{
    auto Service = Result.GetValue();
    Service->DoSomething();
}

// Or require (crashes if not found - use for critical services)
auto Service = DIContainer.ResolveRequired<IAuthService>();
```

### Service Interface Pattern

```cpp
// Interface (pure virtual)
class IDataService : public IService
{
public:
    virtual void FetchData(const FString& Id, FOnDataReceived Callback) = 0;
    virtual void CancelAllRequests() = 0;
    
    static FName StaticType() { return TEXT("IDataService"); }
};

// Implementation
class FDataService : public IDataService
{
public:
    virtual void Startup() override;
    virtual void Shutdown() override;
    
    virtual void FetchData(const FString& Id, FOnDataReceived Callback) override;
    virtual void CancelAllRequests() override;

private:
    TSharedPtr<FCircuitBreaker> CircuitBreaker;
    TSharedPtr<FRetryPolicy> RetryPolicy;
    TSharedPtr<FAsyncOperation<FData>> ActiveOperation;
};
```

---

## Quick Reference Cheat Sheet

### Starting an Async Operation

```cpp
TWeakPtr<FMyClass> WeakThis = AsShared();
MyService->DoAsyncThing(
    [WeakThis](const FResult& Result)
    {
        if (auto This = WeakThis.Pin())
        {
            AsyncTask(ENamedThreads::GameThread, [This, Result]()
            {
                This->HandleResult(Result);
            });
        }
    });
```

### HTTP with Full Protection

```cpp
auto Op = FHttpAsyncOperation::CreateWithProtection(
    FHttpRequest(URL).WithVerb("POST").WithBody(Body),
    CircuitBreaker,
    RetryPolicy,
    CancellationToken);

Op->OnComplete([this](const TResult<FHttpResponse>& Result)
{
    Result
        .LogOnFailure(LogMyPlugin, TEXT("Request failed"))
        .Tap([this](auto& Response) { ProcessSuccess(Response); })
        .TapError([this](auto& Error) { ShowError(Error); });
});

Op->Start();
```

### ViewModel Async Pattern

```cpp
void FMyViewModel::LoadData()
{
    StartLoading(LOCTEXT("Loading", "Loading..."));
    
    TWeakPtr<FMyViewModel> WeakThis = SharedThis(this);
    
    DataService->Fetch([WeakThis](const FData& Data, const FString& Error)
    {
        if (auto This = WeakThis.Pin())
        {
            This->StopLoading();
            if (Error.IsEmpty())
            {
                This->Data.Set(Data);
                This->BroadcastInvalidated();
            }
        }
    });
}
```

### Cancellation

```cpp
// Create token source
auto TokenSource = MakeShared<FCancellationTokenSource>();
auto Token = TokenSource->GetToken();

// Pass to operation
auto Op = MakeShared<FAsyncOperation<T>>(WorkFunc, Token);
Op->Start();

// Later: cancel
TokenSource->Cancel();

// In work function: check cancellation
if (Token->IsCancellationRequested())
{
    return TResult<T>::Failure(TEXT("Cancelled"));
}
```

### Event Subscription

```cpp
// Subscribe
TWeakPtr<FMyClass> WeakThis = AsShared();
Subscription = FEventAggregator::Get().Subscribe<FMyEvent>(
    WeakThis,
    [WeakThis](const FMyEvent& Event)
    {
        if (auto This = WeakThis.Pin())
            This->HandleEvent(Event);
    });

// Publish
FEventAggregator::Get().Publish(FMyEvent{ ... });

// Cleanup (in Shutdown or destructor)
Subscription.Unsubscribe();
```

---

## Summary: Key Takeaways

1. **Use Result types** - Make errors explicit and chainable
2. **WeakPtr everywhere** - Async callbacks must safely handle object destruction
3. **Game thread marshalling** - UI updates only on game thread
4. **Composable resilience** - CircuitBreaker + RetryPolicy as reusable primitives
5. **MVVM separation** - ViewModels manage async state, Views just bind to properties
6. **Event aggregator** - Decouple cross-cutting concerns like network state
7. **Service interfaces** - Enable testing and flexibility

---

*This guide is based on patterns from the ConvaiEditor module. Adapt to your specific needs.*
