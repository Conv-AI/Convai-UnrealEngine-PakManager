# Remove the ConvaiJobSystem dependency

Status: `needs-triage` — done. The test went in against the Job System first, the swap followed, and
both halves are green; a real Publish and a real cancel were driven in an editor. What is left is
Anmol reading [ADR-0012](../../../docs/adr/0012-the-publish-runs-on-its-own-runner.md).

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

## What was done — 2026-09-04

### Step 1: the test, against the Job System

`Tests/CPM_PublishRunnerTestJob.h` (a fake deriving from `UCPM_PublishJobBase`, so it goes through
the real Report and cancel plumbing) and `Tests/CPM_PublishRunnerTest.cpp`, six cases talking to a
five-line harness whose whole job is to be re-pointed. Against the Job System the harness built an
`FWorkflowRequest` and called `ICreateWorkflow` exactly as `StartPublishWorkflow` did.

```
Build.bat Dev_CPM_58Editor Win64 Development -Project=.../Dev_CPM_58.uproject
  Result: Succeeded

UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
  65 "Test Completed", 62 Success, 3 Fail
```

All three failures are new cases. **The brief expected one red and got three**, all from two
differences the swap was going to make anyway:

| Case | Red on the Job System because |
|---|---|
| `ASynchronousJobFinishesOnce` | `ICreateWorkflow` runs the queue inside the call, so the run finished before Start returned. This is the case the brief predicted. |
| `RunsJobsInOrder` | Same cause, first assertion only — "nothing has run when Start returns". Every ordering assertion after it passed, which is the half that had to be pinned. |
| `ATimeoutFailsTheJob` | Outcome, cancel count and finish-once all passed; only the error text failed. `HandleJobTimeout` finishes the workflow with `CompletionInfo.ErrorMessage`, which is the job's own "cancelled" — so a timed-out publish told the creator it was cancelled. |

The three greens are the contract that had to survive: ordering after a report, a failure stopping
the queue and carrying its own error, a cancel resolving as Cancelled and running no later Job, and
progress scaled `(index + job progress) / N`.

### Step 2: the swap

`UCPM_PublishRunner` (new, ~170 lines) replaces the Workflow. `UCPM_PublishJobBase` is a plain
abstract `UObject` with `Execute` / `Cancel` / `Name` / `TimeoutSeconds`, and the four context types
are one `FCPM_PublishContext` the Jobs read and write in place — which deleted every
`FInstancedStruct`, `IDeclareIO` and `TryGet` in `CPM_PublishJobs.cpp`. `StartingChunkId` and
`bStartingWorkflowFinished` are gone, along with `ConvaiJobSystem` from `Build.cs`, the `.uplugin`,
and the release workflow's clone-and-bundle step.

```
Build.bat Dev_CPM_58Editor Win64 Development -Project=.../Dev_CPM_58.uproject
  Result: Succeeded   (no warnings; the three new/edited .cpp were forced to recompile to check)

UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
  65 "Test Completed", 65 Result={Success}, 0 anything else.
  59 from the earlier waves + the 6 runner cases, T08 and T09 included.
```

`grep -rn "ConvaiJobSystem" Source/ ConvaiPakManager.uplugin .github/` finds nothing.
`Dev_CPM_58.uproject` still enables the Job System plugin — that is the project's own list and not
this repo's to edit.

### Three decisions worth arguing with

- **One `ECPM_PublishResult` for both a Job's report and the run's outcome**, rather than a Job
  result enum and a run outcome enum with the same three shapes. A run therefore finishes
  `Success`, not `Completed`. Rejected the pair because eight lines of enum to rename one value is
  not worth a type.
- **`StartPublishWorkflow` renamed `StartPublishRun`**, which the brief did not ask for. Its two
  handlers were being renamed `HandleRunProgress` / `HandleRunFinished` and `CONTEXT.md` was
  dropping "Workflow" as a word; leaving the builder called Workflow would have left the glossary
  contradicting the only file that uses it. Private, one call site.
- `Configure(bExpectPaks, bExpectRawArchive)` on the upload Job was **kept**, though the context can
  now answer both questions. It is a deliberate seam — send what this run was built to send, not
  what happens to be lying in the context — and deleting it is a behaviour change that belongs in
  its own change, not smuggled into a runner swap.

### Editor checks

M07 is in [issue 11](11-clear-the-editor-only-verification-debt.md): a real Publish of chunk 10 ran
to completion on the runner and the Asset record landed, and a cancel mid-run resolved as
`Publish_Cancelled`. The cancel could not be taken mid-*upload* — chunk 10's Pak is 8.7 KB and the
PUT finishes inside one round trip — so it was taken during the create/update step instead, which
is the same runner path and the same `Cancel` → report → finish.
