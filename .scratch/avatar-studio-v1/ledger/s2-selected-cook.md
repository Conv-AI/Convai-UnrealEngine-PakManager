# S2 - selected child cook inputs

Approved specification: host Context/V1/Avatar-Studio-V1-Implementation-Design.md sections 6.3/6.4, G04, A07 and S2/S4. Coordinator assigned only new CPM AvatarCookInputs header/source/test files and this ledger. Root owns builds, tests, child/editor operations, configuration integration and commits. Existing V0 packaging and linked labels remain untouched.

## Round 1 - plan

Status: implementing only deterministic cook-label preparation. No cooked output, real worker integration or full G04/A07 acceptance is claimed.

Public Prepare must run on the game thread inside the expected persistent child project at Host/Saved/ConvaiAvatarStudio/Uploader/AvatarStudioUploader.uproject. Verify current project identity, physical child Content and the actual /Game mapping; refuse ordinary host execution. Validate the selected plugin mount, saved/clean explicit package closure, declared prerequisite packages, settled registry and every dependency result before writing. The caller retains the per-host/avatar lease and monotonic source monitor; callbacks fail on cancellation or unknown/changed snapshot.

Create one fresh job-owned /Game/ConvaiAvatarStudioJobs/Job_<GUID>/PAL_Selected_<GUID> label. It contains explicit native asset object paths and Blueprint generated-class soft paths from on-disk registry tags, never a directory scan. AlwaysCook plus the persistent nonzero Chunk ID applies only to its explicit validated closure; disable directory labeling and recursion, leave the label editor-only as an object. Save only this new child package, then read its serialized Asset Registry bundle directly from disk through IAssetRegistry::LoadPackageRegistryData and compare the complete Explicit bundle. Unknown saves/bundles fail the job; no linked label is loaded/resaved to repair it.

Native evidence: PrimaryAssetLabel.h marks ExplicitAssets/ExplicitBlueprints with AssetBundles=Explicit. DataAsset.cpp:49-69 rebuilds and serializes that metadata in PreSave. AssetManager.cpp:4495/4530-4575 loads registered labels and uses CachedAssetBundles for manager edges; ModifyCook:4796-4805 requires the primary asset type bIsEditorOnly=false before AlwaysCook can pull assets. BaseGame.ini:247-255 documents the same distinction. The label object's bIsRuntimeLabel=false avoids exporting the job helper as runtime data; this is distinct from the type's bIsEditorOnly flag.

Deterministic host automation will test the same private label-write helper through a test-only friend at an owned Saved fixture mount, using real saved textures/Blueprints and direct on-disk bundle extraction. Public entrypoint refusal tests prove it cannot write an ordinary host job label. This avoids shadowing global /Game or changing FPaths' current project to fake a child. Production entrypoint acceptance must then be run in the actual provisioned child and remains a separate integration gate.

Root real-cook profile: write only child-owned configuration under its serialized job lease; replace the inherited broad PrimaryAssetLabel scan with one SpecificAssets job label, bIsEditorOnly=false, nonrecursive AlwaysCook rules and bGenerateChunks=true. Disable unrelated engine/plugin defaults and unrelated maps/content sources, retaining verified runtime prerequisites. Never call CPM_PackageProject as-is: EditorUtils.cpp:96 unconditionally resaves all labels before UAT. Preserve its cooker-only bAutoStartServer=False MCP override in the isolated child command. Proposed first experiment uses a new cook output with no iterative cache, verifies selected A's nonzero chunk on the first cook after adding an asset to its closure, verifies B/unrelated host/child maps absent from actual cook manifests/Pak listings, and compares linked A/B source/label bytes and SDK trees before/after. Repeat with B and a fresh job-owned label, maintaining permanent chunk identities. Configuration and label rules alone do not prove those inclusion/exclusion outcomes.

No new module, generic worker framework or V0 API change. Current unverified gates include actual child lifecycle/handshake, receipt and profile validation by orchestration, linked-source monitor, fresh first-cook chunk output, imported label collisions, host StagingOnly/InstalledForUse cook roles, source/Pak coherence and worker recovery/cancellation.

## Round 1 - implementation decisions

The narrow service and two deterministic automation cases are now released in the three agreed CPM Source paths; staging copies remain under `.scratch/avatar-studio-v1/test-staging/CPM_AvatarCookInputs*`. No build or test has run for this new seam. Public Prepare checks current project identity and physical local child job paths before creating any package; a mounted /Game elsewhere or a reparse path to child Content is refused. The registered selected mount, saved/clean package list, nonempty on-disk registry assets and complete package-dependency closure are required. Unknown registry dependency results fail closed. Native /Script dependencies are validated by the caller's prerequisite profile; the cook bundle contains only actual source assets. Host /Game, duplicate/misclassified packages, redirectors and linked labels are refused.

Both regular asset paths and Blueprint generated-class paths come from saved registry records. A generated-class tag must resolve to the same exact package and a _C asset. The helper writes a fresh label with no directory, collection or recursive rules, then verifies its complete serialized Explicit bundle using LoadPackageRegistryData before reporting success. It never loads a linked label or calls the V0 ResavePrimaryAssetLabels helper. Cancellation/snapshot rejection after save fails the job and leaves that job-owned package for recovery; it does not claim the label was rolled back or ready to cook.

Job GUID appears in both the folder and label's short name. Native UPrimaryDataAsset derives its ID from type and short name (DataAsset.cpp:120+); different folders with the same PAL_Selected name still trigger duplicate PrimaryAssetID (AssetManager.cpp:1659). Unique job names prevent retained old jobs from sharing one label identity. Stable nonzero Chunk ID remains the avatar identity and is unchanged between fresh jobs. This does not by itself prevent an unrelated imported label from conflicting in a broad child scan; orchestration must restrict the current job's AssetManager scan.

`SavesFreshExplicitBundle` creates real saved textures for A/B and a compiled Actor Blueprint, writes a first job label, adds another A texture, and writes a second fresh job with the same chunk. It compares source/first-label hashes, refuses replacing the first label, unloads the owned packages without resetting the user's transaction buffer, reads the second bundle directly from disk before loading the label, and checks exact A/added/BP_C membership with B absent. Reloaded rule flags and restored unattended/silent scopes are checked too. `RefusesHostAndUnverifiedSnapshot` exercises the public entrypoint's host, cancellation and snapshot guards and checks that no host job package was created. The private test-only friend uses an owned Saved fixture mount, not a fake current child or /Game override. These cases do not execute accepted public preflight in a real child, a cook or a linked-source monitor.

### Coordinator first-cook experiment profile

These are experiment inputs, not installed configuration. Root must verify the child profile/lease and save the current child configuration before applying only there. Substitute the exact returned LabelObjectPath and persisted Chunk ID; retain no directory scan. For the minimal content fixture, the child-owned DefaultGame.ini AssetManager/packaging sections are:

```ini
[/Script/Engine.AssetManagerSettings]
!PrimaryAssetTypesToScan=ClearArray
+PrimaryAssetTypesToScan=(PrimaryAssetType="PrimaryAssetLabel",AssetBaseClass=/Script/Engine.PrimaryAssetLabel,bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=(),SpecificAssets=("/Game/ConvaiAvatarStudioJobs/Job_GUID/PAL_Selected_GUID.PAL_Selected_GUID"),Rules=(Priority=1,ChunkId=37,bApplyRecursively=False,CookRule=AlwaysCook))

[/Script/UnrealEd.ProjectPackagingSettings]
bCookAll=False
bCookMapsOnly=False
bGenerateChunks=True
bGenerateNoChunks=False
UsePakFile=True
bUseIoStore=False
bUseZenStore=False
!DirectoriesToAlwaysCook=ClearArray
!MapsToCook=ClearArray
```

DisableEnginePluginsByDefault in the child descriptor and only explicit verified prerequisites remain required. Child-owned DefaultEngine.ini should clear unrelated default maps and server transition/default maps for this content-only fixture. Do not carry host asset-label/map directories or cook-all settings into the child. Preserve required platform/SDK configuration instead of copying the whole host ini. Empty maps are an experiment input; actual cooker output must reveal any native engine/default-content inclusion rather than treating this configuration as isolation proof.

With the exact child target already built by root's guarded build and the current child preparation complete, a fresh UAT experiment can use the actual engine RunUAT.bat with argument vector:

```text
BuildCookRun -project=<absolute child .uproject> -nop4 -utf8output -nocompileeditor -skipbuildeditor -skipbuild -cook -stage -pak -manifests -archive -platform=Win64 -clientconfig=Development -archivedirectory=<fresh job-owned output directory> -AdditionalCookerOptions=-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False
```

Quote filesystem arguments as arguments, not concatenated shell syntax. Do not supply -iterate/-iterativecooking, -CookAll, host project paths or the V0 resave entrypoint. Root must use a fresh child cook sandbox or explicitly verify native non-iterative cleanup, and inspect both actual cooked AssetRegistry/manifests and UnrealPak -List for selected A in its expected nonzero chunk on this first cook, B and host content absent. A mere zero exit or an empty Pak is insufficient. Hash linked source/labels and SDK before/after; repeat B with its permanent chunk and a new uniquely named job label. Restore only owned child configuration as needed under the same serialized lease. This profile is not yet executed or a claim that AlwaysCook, scan overrides or plugin filtering guarantee isolation.

## Round 1 - independent review

Reviewer: root, independent of implementation. Verdict: **approved** for the selected cook-label prerequisite. Read complete public API, implementation and both native fixture tests against G04/A07 and engine AssetManager/PrimaryAssetLabel behavior. The public path validates expected/current child project, physical child /Game destination, saved exact package identity and explicit dependency closure before writing. The private writer refuses existing labels and duplicate primary identity, serializes a fresh explicit bundle, and re-reads on-disk registry bundles before success. Job GUID in the short name prevents PrimaryAssetId aliasing across retained jobs; persistent Chunk ID stays unchanged. V0 broad label refresh and cook MCP override are untouched. CPM is already Win64-only, so native Windows calls do not narrow an existing module platform.

Actual selected-cook-build.json succeeded (5 actions); one deprecated fixture boolean overload warning remains nonblocking. SelectedCookTests/index.json contains two Success with zero errors/warnings. Saved/reloaded bundle contains original A, newly added A and Blueprint generated class; B is absent; first label and all source hashes stay unchanged. Public host/cancel/invalid snapshot refusal creates no host job package. No blocker in this bounded seam. The caller still must validate prerequisite ownership, hold lease/continuous snapshot and restrict AssetManager scan in the actual child. Accepted public child preflight, first real cook/Pak chunk contents, linked-content byte integrity and process MCP isolation are not established by these fixtures and remain integration gates.
