# Avatar Studio V1 — S0 contracts and execution ledger

Specification: host `Context/V1/Avatar-Studio-V1-Implementation-Design.md`, visual companion, 2026-09-08 handoff, full meeting transcript, and context brief. The implementation request supersedes the handoff's pending approval.

## Resume here — current progress

This is the durable entry point requested by the user. Read this section and relevant per-item ledgers before continuing. **V1 is not complete.** Verified prerequisites do not establish the required create → publish → download → edit → same-ID update flow.

| State | Work and evidence |
|---|---|
| Verified and committed | CPM S0 `ad59e4a`, explicit source policy `23097c2`, shared-preparation facades `d67bc0d`, selected source archive `974b9a3`. SDK shared preparation `c9effc2c`. Each has independent final approval; existing V0 defaults retained. Archive's four real ZIP tests passed after the recorded native rename fix. |
| Verified and committed | SDK read-only library `9af9b044` (final review round 3), durable workspace `f39034fd` (round 2), exact asset details `3a4ad0c8` (round 2). Canonical integration fast-forwarded to `3a4ad0c8`; exact scoped integration stash removed after source/HTTP-flag verification. Seven native workspace fixtures passed. |
| Verified | `json-contract-build.json` succeeded, 5 actions. `JsonContractTests/index.json`: 24 clean success + one successful preparation case with unrelated Google timeout warning, zero failed/not-run. Correct pre-DOM key collision guard and all five details cases passed, including actual catalog/get reads and synthetic native lifecycle. All seven correctly named `ConvaiAvatarPreparation` cases ran. Earlier misspelled preparation filter and failed wire-key cases remain history. |
| Verified, pending commit | Selected cook-label seam builds (`selected-cook-build.json`, 5 actions, one deprecated fixture-call warning). `SelectedCookTests/index.json`: 2/2 success, zero warnings/errors. Root reviewed the full code/tests; child-owned fresh explicit bundles and host refusal are proven by native UObject fixtures. Accepted public preflight in the actual child and first cook/Pak isolation remain required. |
| Real operation failed; rework active | Provisioning source review round 2 approved, but actual `LocalProvisionTests/index.json` failed at promotion after copying the complete installed template/SDK/plugin source and receipt. Error: Cannot promote the prepared uploader atomically. `Uploader.preparing` and durable `pending-provision.json` are retained; no child build ran. PakManager agent owns native error diagnosis plus explicit validated recovery/repair, with final round 3 review remaining. |
| Implementation active | Input monitor files released for root build/real UPackage save tests/review. SDK agent owns format1 source inspection/staging via engine libzip; root owns eventual Workspace install transaction and shared Build.cs. No remote download/install integration is claimed. |
| Still required | Delta replacement, creation wizard/commands, selected child cook and host cook roles, actual physical-SDK child build, supervised worker/credential IPC/progress/cancel, install/prerequisites/restart, publish/update/delete/Get Latest/recovery, actual MetaHuman A/B/fresh-host acceptance matrix. |
| Contract limits | User confirmed no distributed V1 template exists. All PakManager proxies were rechecked: update/get/delete exist, but stable authenticated principal, conditional mutations, idempotency/uncertain-create reconciliation and source/package revision correspondence are not established. Real selected asset details advertise only 5.5/5.6 Raw, neither advertise nor map 5.8 source. Usage probe returned200 without allowlisted principal; exploratory profile GET404. Do not invent these capabilities or count diagnostics as contract success. |

Paths and coordination:

- Host (not Git): `E:/UEProjects/UE5.8/Dev_CPM_58`; evidence under `Saved/ConvaiAvatarStudio/Development`.
- CPM: `Plugins/Convai-UnrealEngine-PakManager`, `feat/avatar-studio-v1`, base `staging@0218c2b`. SDK authoritative worktree: `E:/UEProjects/UE5.8/avatar-studio-v1-sdk`, `feat/avatar-studio-v1`, base `WebRTC-Video@6300c57a`; canonical integration branch `feat/avatar-studio-v1-integration`, both at `3a4ad0c8`.
- Preserve canonical `Source/Convai/Convai.Build.cs` user flag true; it remains uncommitted and byte-verified after integration. Preparation status entries have no content diff (line-ending/stat refresh), not an agent delta implementation.
- Root owns `.codex/coordination/locks/{git-exclusive,unreal-exclusive}` and agent registration `avatar-studio-v1.md`. Root alone builds, tests, drives editor and commits. Other agents edit only their released/assigned source. Confirm processes before shared work; never steal locks or close the user's editor.
- Guarded build: `Saved/ConvaiAvatarStudio/Development/BuildAndLaunchGame.ps1 -NoLaunch`, engine `E:/Software/UE_5.8`. It refuses open editors, preserves logs, never Live Codes or closes editors. Never use the unguarded global-kill VibeUE script.
- Test editors for JsonContractTests, LocalProvisionTests and SelectedCookTests self-exited. Most recent test session40954 exit0; actual report says2/2, not merely exit0. New tests/builds must still be serialized. Journals use `preparation_journal.py` with explicit report and build filenames.
- Gallery/details screenshots at800/1100/1600 plus narrow Back were captured using real Home/Enter keyboard input and visually reviewed. `TransportAuditTests`2/2 proves queued cancellation and synthetic credential/signed-URL log privacy, not strict active-I/O deadlines. Historical baseline74, prep84 and V0 rerun counts stay in their item ledgers.
- Native oplock actual-user report `OplockProbe/7553597cb3af47fab3e17e494f98b157/results.json`: attribute R detects write/restore, allows share-zero writes, refuses existing writable mapping, and real overflow/cancel observed. Monitored rename/replacement failed despite unmonitored controls; production pre-save release remains a real test gate.
- Existing [ConvaiTasks #255](https://github.com/ar-convai/ConvaiTask/issues/255) remains In progress. No duplicate, push, PR, merge or release bump. Move to Sync only after finished work with honest remaining scope.

Next: root integrate/build/review monitor and run real synchronous/asynchronous package saves; finish selected-cook local commit; diagnose/recover real provision and build persistent child; continue S3 install and remaining S2–S6 integration. New source/cache changes must be verified against the actual provisioning profile; a stale built child is not evidence for later code. Representative authoring assets found via Asset Registry are `/Game/Blueprints/BP_Hana` and `/Game/MetaHumans/Hana/BP_Hana`.

Per-item ledgers live in each owning repo `.scratch/avatar-studio-v1/ledger/`; no extra summary documents. SDK provisioning/monitor/install ledgers are authoritative in its isolated worktree until committed; canonical copies may be absent or stale.
## Plan

Deliver R01–R18 through S0–S6; preserve V0 and the separate Stage feature. One persistent uploader per host, permanent avatar plugins, linked content, physical SDK copy, and same-ID updates remain mandatory. Unproved external capabilities remain gates.

| Item / owner | Files / responsibility | Dependencies / verification |
|---|---|---|
| s0-contracts / root | This ledger; baseline, contract and feasibility evidence | Baseline build; G01–G04. |
| s1-library / SDK agent | SDK `Source/ConvaiAvatarStudioEditor/` catalog, UI, module entry point and tests | S0 wire findings; A01 parser, context changes, stale/error/source states; real ownership/pagination and screenshots still required. |
| s2-preparation / preparation agent | SDK `Source/ConvaiAvatarPreparationEditor/`; CPM dependency/avatar/thumbnail facades and tests | Existing V0 implementation; A04 integrity/failure/no-op/delta and A11 compatibility. Strict refresh may block until safe replacement is proved. |
| s2-workspace / root | Studio workspace/orchestration files only; root owns all shared descriptors/integration | S0 link/template gates; A02/A07/A09/A10 identity, single child, link/SDK isolation, recovery. |
| s3-source-install / unassigned until contracts | Validated source adapter and transactional installation | S1 + real source contract + workspace; A03/A05/A10. |
| s4-source-policy / policy agent | CPM publish types, subsystem policy resolution, dedicated tests | S0 policy findings; preserve default V0 semantics, explicit omit/fresh/denial tests. This seam alone does not implement selected-avatar archives. |
| s4-create-publish / root integration | Wizard, selected worker cook/archive and bindings | S2 + G01–G04; A02/A04/A07/A08 real publish. |
| s5-round-trip / root integration | Get Latest, cloud deletion and process/journal reconciliation | S3/S4; A04–A06/A09 live flow and recovery. |
| s6-verification / independent reviewers + root | Actual diff review; existing README/CONTEXT/ADRs only where behavior implemented | A01–A11, real MetaHuman/fresh-host/Pak/archive evidence; no fixture-only sign-off. |

Wave 1 runs library, preparation and source policy with disjoint ownership. Root serializes descriptors, integration, builds, tests, editor operations and commits. Subsequent dependent work starts only when its prerequisite is usable. Each item's implementation is reviewed independently; maximum three review rounds per item. The SDK agent does not own root's workspace files. No push, PR, merge, release bump, Live Coding or unattended host-editor closure.

## Round 1 - implementation

Status: partial. Source investigation is complete; runtime and external gates remain open.

- Separate repo baselines: PakManager `staging@0218c2b571e4541e399d35aa324772388146c751`, SDK `WebRTC-Video@6300c57a`. Both local feature branches are `feat/avatar-studio-v1`. SDK isolated worktree: `E:/UEProjects/UE5.8/avatar-studio-v1-sdk`.
- Preserve canonical SDK `Source/Convai/Convai.Build.cs`: existing user change is `BEnableConvaiHttp=false` → `true`. It is excluded from this task's commits.
- No matching SDK migrated memory; matching PakManager memory read, including `no-live-coding.md`. Root and nested AGENTS/CLAUDE, coordination and full requested design documents read. Meeting's permanent-plugin decision at 00:33:55 supersedes temporary dispatcher; one-child decision at 00:41:15 supersedes global cache discussion.
- Existing ConvaiTasks issue #255 (Asset Uploader : Asset management UI) reused after searching Avatar Studio, V1 and uploader issues and inspecting likely matches. Moved Tasks → In progress; Stage #270 remains separate.

### G01 — uploader distribution

Observed UE `E:/Software/UE_5.8`, Build.version 5.8.1 / CL56057345; engine source, UBT, editor and TP_Blank exist. Existing Modding Tool creates a wrapper from installed `Templates/TP_Blank` (`core/unreal_engine_manager.py:155`) and resolves separate plugin releases; no verified V1 distributed template manifest/checksum. Local template construction can be tested without pretending distribution is solved. Its HTTP flag transform is reusable; host flag may already be true.

Installed VibeUE omits the documented build script. Actual helper `E:/Scripts/Repos/VibeUE/BuildAndLaunchGame.ps1` globally kills Unreal processes and deletes logs. A task-local copy at host `Saved/ConvaiAvatarStudio/Development/BuildAndLaunchGame.ps1` instead refuses any open editor, disables destructive clean flags, preserves logs, adds NoLaunch and hidden launch. Task helper supplies atomic coordination-directory locks because referenced lock helpers are absent. Never force-release another owner's lock.

Verification this session: guarded helper `-NoLaunch` ran UBT for Dev_CPM_58Editor Win64 Development, **Result: Succeeded**, target up to date, zero compilation actions. `Saved/VibeUE/last-build.json` reports succeeded. This is baseline evidence, not verification of subsequent C++ edits. Existing warning: `Plugin 'ConvAI' does not list plugin 'ConvaiHTTP' as a dependency, but module 'Convai' depends on module 'CONVAIHTTP'.`

### G02 — catalog, source and concurrency

HelperLibrary `AssetsAPIs.cpp:392–474` proves POST assets/list shape; its resolver hardcodes beta. `ConvaiAssetManagerUtility.cpp:886–1040` preserves asset_id/user_id/entity_type/versions/version_urls. SDK adaptation must use captured environment/auth and strict parsing; avoid importing private HelperLibrary. Generic signed_url and current cooked-platform fallback are not editable source. Request exact `ue-<engine>-Raw` slot; broad listing must not filter away other engine/platform assets.

No demonstrated pagination or current account's stable user_id contract; SDK auth stores username/email, which cannot prove ownership. No demonstrated CAS, conditional delete, idempotency or source/package correspondence. Existing assets/update with asset_id and artifact version is not an atomic concurrency contract. Unknown ownership disables mutation; uncertain create cannot automatically retry. Signed URLs remain transient. Current proxy factories reread URL/auth, so EnvironmentSlug alone cannot freeze a V1 job context.

Live source download/ownership/pagination/concurrency: **not run**. Configured `http://127.0.0.1:8000/mcp` refused connections; no editor process was running. No credentials read or cloud avatar mutated by S0.

### G03 — source policy

Current CPM resolves policy before queue creation, then restricts raw with project setting. Add per-run default/omit/include-fresh choice without changing existing defaults. Explicit fresh must fail if policy denies source. Local Modding Tool policy enables Windows Shipping and Raw, but that is not proof of the live policy retrieved from its configured main URL. Current archive is rebuilt when queued; Raw timestamp is display state, not proof of correspondence. Selected-avatar sanitized archive remains separate required work.

### G04 — linked content and worker

Source proves hazards, not feasibility: `ConvaiPakManagerEditorUtils.cpp:37–96` unconditionally resaves every label to repair a real first-cook stale-bundle regression. Retain this V0 safeguard and cook-only MCP override at lines 119–120. V1 needs child-owned cook inputs; simply skipping refresh is invalid.

`BeginPolicyRun` calls PrepareEntryPoint before policy; it modifies descriptor/Blueprint and saves. An unchanged V0 worker would write through linked host content. Whole-project UAT/archive include unrelated plugins. Package Cancel reports completion without stopping UAT. New worker lifecycle must own process tree termination and reconcile remote uncertainty.

Dependency copy can skip failed loads/saves and existing game destinations yet return success. Extract existing implementation with compatibility facade and explicit strict validation; do not call that a proven incremental refresh. First-cook A-without-B, immutable linked labels/source, real input-change monitoring, host cook roles, worker process identity/cancellation, archive isolation and fresh-host rebuild are **not run**.

### Decisions

1. Preserve V0 defaults while introducing opt-in strict preparation and per-run source choice; global behavior changes would mix V1 with existing uploader workflows.
2. Separate SDK worktree avoids changing shared canonical branch or swallowing the existing HTTP flag edit. Root integrates reviewed exact files under locks for build verification; no remote publication required.
3. Unknown backend capabilities are represented as unavailable/review-required, never guessed fields or fixture success. Full V1 readiness remains hold until real gates pass.

### Runtime evidence and user clarification

- `WorkflowService.get_environment()` ran in the actual UE Python commandlet, exit 0; source/engine paths verified. Its missing compiler environment variable is not a missing compiler: real UBT selected MSVC 14.44.35228 and Windows SDK 22621. Artifact: host `Saved/ConvaiAvatarStudio/Development/environment.json`.
- Baseline V0 automation: 74 tests, 72 successes plus 2 warnings, zero failed. Shared preparation/source-policy build: 52 compile/link actions, succeeded; subsequent 80 tests: 78 successes plus the same 2 warnings, zero failed. Reports: `BaselineTests/index.json`, `PreparationTests/index.json`; build manifest: `preparation-build.json`, all under that Development directory. These predate review rework and are not evidence for the revised strict path.
- Native filesystem probe: symlink creation requires Administrator privilege on this machine. A directory junction works, reflects target edits and can be removed nonrecursively while preserving the target. This is filesystem capability evidence, not Unreal mount/cook proof. `link-probe.json` records the owned fixture.
- Fetched actual configured upstream policy from ModdingTool main; blob `b5b61eaa280603b42eed51e6c75049db8a3a84e4`: Windows Shipping enabled, Linux disabled, raw-project-upload enabled. Host override and source freshness remain separate. The independently reviewed source-choice seam is committed locally as `23097c2`.
- Initial Studio build required correcting the include to existing SDK convention `Convai/Convai.h`; the rerun succeeded. First real Studio automation: 6 successes, 2 failures. Real `assets/list` returned a successful HTTP response but unsupported parser shape; strict parser fixtures also found Unreal JSON number-to-string coercion. Selection retention and actual 800/1100/1600 error-state screenshots passed. Report: `StudioTests/index.json`. Process exit 0 did not imply test success: always read report states.
- Independent review found native UE HTTP logs expose custom credential headers and signed URLs. Windows-native HTTPS transport is being validated instead of inventing header aliases or suppressing global logs. No further native-UE-HTTP live requests are authorized by this implementation path until fixed.
- User clarified the Pak Manager proxies are the backend reference and **no V1 uploader template manifest exists**. Reinspect all proxies for exact guarantees. Provision locally from the installed exact engine template and resolved physical plugin copies, recording a compatibility receipt and verifying a real child build; do not invent a downloadable template or claim a distribution contract.
