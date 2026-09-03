# Remove the ConvaiJobSystem dependency

Status: `ready-for-agent`

The Pak Manager is the only consumer of `ConvaiJobSystem` in this project — Convai and ConvaiHTTP do
not reference it. It costs a second plugin in every release zip, a CI clone pinned at `v2.3.9`, and
an "unzip both plugins" line in the install instructions. We use roughly a fifth of what it ships.

**Do this last.** It rewrites the same subsystem that issues 01-03 touch, so it goes behind them,
and only once they are committed and green.

## What actually depends on it

| Where | What |
|---|---|
| [ConvaiPakManager.uplugin](../../../ConvaiPakManager.uplugin) | one `Plugins[]` entry |
| [ConvaiPakManager.Build.cs:29](../../../Source/ConvaiPakManager/ConvaiPakManager.Build.cs#L29) | one `PublicDependencyModuleNames` entry |
| [CPM_PublishJobs.h](../../../Source/ConvaiPakManager/Public/Jobs/CPM_PublishJobs.h) + [.cpp](../../../Source/ConvaiPakManager/Private/Jobs/CPM_PublishJobs.cpp) | 1009 lines; five Jobs implementing `IJobInterface`, reading `UWorkflowContext` |
| [ConvaiPakEditorSubsystem.cpp:642-891](../../../Source/ConvaiPakManager/Private/ConvaiPakEditorSubsystem.cpp#L642-L891) | ~160 lines: build `FWorkflowRequest`, `ICreateWorkflow`, `ICancelWorkflow`, progress/finished handlers |
| [.github/workflows/release.yml:77-121](../../../.github/workflows/release.yml#L77-L121) | clones the JobSystem repo and bundles it into the release zip |
| Tests | none |
| UI | none — ADR-0008 already keeps the UI on chunk status rather than workflow events |

Five includes in total: `Interface/JobInterface.h`, `Interface/WorkflowInterface.h`,
`Type/JS_Definations.h`, `Core/WorkflowContext.h`, `Core/WorkflowManagerSubsystem.h`.

Used: sequential queue, typed context over four struct types, IO-spec validation at queue build,
per-job timeout (two jobs, 120s and 30s), retry (one job, `MaxRetries = 1`, a file write),
cancel-with-force, progress and finished delegates.

Unused: `JobGroup`/parallel, Precheck and skip, the listener interface, the Blueprint library,
`bContinueWorkflowOnFailure`, `RetryDelaySeconds`, `FWorkflowConfig`, workflow history records.

## The order, which is the whole risk

The publish path has **no test coverage of the workflow layer**, and it is the path that cooks Paks,
mints upload URLs and writes AssetIDs. A silent regression here costs a creator a long cook and can
orphan an Asset on Convai. So:

1. **Write the test first, against the current JobSystem implementation.** Queue ordering, cancel
   mid-run, a failing job stopping the queue, the finished-during-start case. It must pass before
   anything is removed. That test is the entire safety net, and it is worth having either way.
2. Only then swap the runner. Green test on the new one is the proof.

Do not skip step 1 to save time. Without it this is a rewrite of untested async orchestration, which
is the exact shape of change that ships a confident silent bug.

## The replacement

`UCPM_PublishJobBase` is already the shim — it owns Report, the two-part cancel, and the context
read. Change what sits under it:

- Base class stops implementing `IJobInterface`; becomes a plain abstract `UObject` with
  `Execute()` / `Cancel(bForce)` / `Name()`.
- `UWorkflowContext` becomes one plain `FCPM_PublishContext` struct holding the four types
  (`FCPM_PublishRequest`, `TArray<FCPM_PakArtifact>`, `FCPM_RawArchive`, `FCPM_PublishedAsset`).
  That deletes the `FInstancedStruct` plumbing and the IO-spec validation with it: five static jobs
  do not need runtime type matching that a compile-time struct gives for free.
- A runner on the subsystem: index into `TArray<Job*>`, advance on report, `FTSTicker` for the two
  timeouts, and inline the single retry into `WriteCreateAssetData` where it belongs.
- Delete the CI bundle step and the two-plugin line in the release notes.

Expect the pak manager to come out roughly flat on line count. The win is one fewer plugin, one
fewer CI clone, and a simpler install.

**Free cleanup while you are there:** `bStartingWorkflowFinished` / `StartingChunkId`
([ConvaiPakEditorSubsystem.h:339-350](../../../Source/ConvaiPakManager/Public/ConvaiPakEditorSubsystem.h#L339-L350))
exists only because `ICreateWorkflow` runs the queue synchronously inside the create call. A runner
that defers the first job by one tick deletes that whole hack. Do not port it forward.

## Done when

The publish-flow test passes on the new runner, `ConvaiJobSystem` appears nowhere in the plugin, a
real publish completes end to end in an editor, and cancel still resolves as Cancelled rather than
Failed.
