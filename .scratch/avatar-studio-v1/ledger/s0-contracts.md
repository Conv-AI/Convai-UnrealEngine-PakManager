# Avatar Studio V1 — S0 contracts and execution ledger

Specification: host `Context/V1/Avatar-Studio-V1-Implementation-Design.md`, visual companion, 2026-09-08 handoff, full meeting transcript, and context brief. The implementation request supersedes the handoff's pending approval.

## Resume here — current progress

This is the durable entry point requested by the user. Read this section, then the relevant per-item ledger, before continuing. **V1 is not complete.** Do not mistake verified foundations for the required create/publish/download/edit/same-ID round trip.

| State | Work and evidence |
|---|---|
| Verified and committed | S0 contract record `ad59e4a`; S4 explicit source choice `23097c2` in PakManager. Default V0 semantics retained; source-policy review approved and three real tests passed. |
| Verified and committed | Shared preparation extraction SDK `c9effc2c` + CPM facades `d67bc0d`, final review round 3 approved. Revised build succeeded (51 actions); `ReviewedPreparationTests/index.json`: 84 tests, 81 success + 3 warnings, zero failed. The owned fixture warning was corrected/reviewed; focused `StudioLiveTests/index.json` passed 18/18 with zero warnings, including all seven strict-copy cases. |
| Verified, pending Studio integration commit | Workspace records, permanent plugins, single uploader path, native lease/junction links, orphaned-record recovery refusal; review round 2 approved. `StudioTransportTests/index.json`: all seven workspace tests passed, source hashes matched canonical. Actual uploader project/provisioning and Unreal mount/cook remain unfinished. |
| Verified, final review pending | Library/panel, stable selection, strict/null-aware parser, native Windows HTTPS transport, capture cleanup and queued cancellation. Real catalog loads 12 avatars (2 source-advertised, 3 Raw slots, no exact host URLs/owner IDs from list). `StudioLiveTests/index.json`: 18/18 passed, zero warnings; live gallery captures at all three widths inspected. Accepted-time timeout/priority code built successfully. `TransportAuditTests/index.json`: 2/2 passed, zero warnings; queued cancellation and real synthetic local failed-request diagnostic audit (no real credential). Exact-case context fixes are in progress before final S1 review. |
| Implemented, rework/review in progress | `provision-details-build.json` succeeded (18 actions). `ProvisionDetailsTests/index.json`: 14 success, 1 failed. Actual assets/get HTTP 200 returned one avatar, an owner ID, nine versions and ten URL-map entries, but no exact host Raw URL. The failing Expected/expected identity fixture exposed FString's case-insensitive comparison; SDK agent is fixing identity/context/slot comparisons and lifecycle coverage. Provisioning Round1 found four major issues (unexpected files, MCP config category, flag lexical parsing, changed engine dependency closure); Round2 source and 21-scenario fixtures released for SDK review. Real uploader source provisioning remains unrun. |
| Verified and committed | Selected-avatar sanitized archive `974b9a3`, final Round3 review approved. A real native probe isolated relative rename error87; full absolute destination/null RootDirectory preserves the held-file promotion contract and all four selected-source ZIP tests now pass. Earlier ArchiveProvisionTests records 5 success/4 archive failures; those failures are retained as history, not the current archive result. Archive/cook/monitor/publish integration remains partial. |
| Current verification defect | `exact-contract-archive-build.json` succeeded (16 actions). `ExactContractArchiveTests/index.json`: 95 success + 3 warnings, one Library.CaseSensitiveWireSlots failure. Unreal's DOM Values map folds case despite case-sensitive string interning, so a pre-DOM duplicate-key guard is in progress. All details exact-ID/lifecycle, selected archive and V0 cases in that report passed. The attempted preparation filter was misspelled (`Convai.AvatarPreparation`); it did not rerun the seven already-passing `ConvaiAvatarPreparation` cases and must not be counted as a fresh preparation run. |
| Still required | Delta replacement; wizard and command wiring; selected child cook/labels and host cook roles; physical-SDK child build; worker handshake/progress/cancel/credential IPC; editable installation/prerequisites/restart; publish/update/delete/Get Latest/recovery; actual MetaHuman A/B/fresh-host acceptance matrix. |
| Contract limits | No V1 distributed template exists (user confirmed): build local compatible provisioning from installed TP_Blank/resolved plugin copies. All PakManager proxies rechecked: same-ID update/get/delete exist, but no demonstrated conditional write/idempotency/uncertain-create reconciliation or stable authenticated user ID. Real assets/get supplies owner and exact Raw URLs for UE5.5/5.6; selected avatar has neither advertised nor mapped UE5.8 source, with no unadvertised Raw mismatch. Source/package revision correspondence is still unproved. Real POST /user/user-api-usage returned HTTP200 but no allowlisted principal field; GET of SDK's /user/profile URL returned404. No current ownership contract was established by either probe. |

Paths and coordination:

- Host (not Git): `E:/UEProjects/UE5.8/Dev_CPM_58`.
- PakManager: host `Plugins/Convai-UnrealEngine-PakManager`, branch `feat/avatar-studio-v1`, base `staging@0218c2b`.
- SDK authoritative worktree: `E:/UEProjects/UE5.8/avatar-studio-v1-sdk`, branch `feat/avatar-studio-v1`, base `WebRTC-Video@6300c57a`, current commit `c9effc2c`. Canonical host SDK integration branch `feat/avatar-studio-v1-integration` was fast-forwarded to the same commit; its preparation source matches the committed Git content. Studio remains uncommitted. Root mirrors exact files for builds; module descriptor order is now normalized identically.
- Preserve canonical SDK's sole pre-existing edit: `Source/Convai/Convai.Build.cs` sets `BEnableConvaiHttp=true`. Exclude it from all task commits.
- Root owns `git-exclusive` and `unreal-exclusive` directory locks under host `.codex/coordination/locks`, plus coordination agent `avatar-studio-v1.md`. Other agents never build/test/commit/control editor. The last test editor self-exited; check real processes and lock ownership on resume, never force-steal locks.
- Guarded build helper: host `Saved/ConvaiAvatarStudio/Development/BuildAndLaunchGame.ps1 -NoLaunch`. It refuses open editors, preserves logs, never Live Codes or closes user editors. Engine `E:/Software/UE_5.8`. Do not use the original unguarded script that globally kills editor processes.
- Evidence root: host `Saved/ConvaiAvatarStudio/Development/`. Revised prep build/report: `preparation-reviewed-build.json`, `ReviewedPreparationTests/index.json`; journal `Saved/VibeUE/Runs/20260908T165338Z-A7D2B891.json` reports succeeded. Studio library report: `StudioLiveTests/index.json` (18/18); transport `TransportAuditTests/index.json` (2/2). Latest build/report: `archive-provision-reviewed-build.json`, `ArchiveProvisionTests/index.json` (5 success, 4 archive failures). Current gallery screenshots plus selected details at 800/1100/1600 and narrow Back were captured with real Home/Enter keyboard events; selected/back images visually inspected. Test process 52739 self-exited. Journal `20260908T174001Z-55686E57` ties the exact run to its report. Asset Registry read-only inventory found `/Game/Blueprints/BP_Hana` and `/Game/MetaHumans/Hana/BP_Hana` for later representative verification; no binary asset disk reads/mutations were used.
- Tracking: existing [ConvaiTasks #255](https://github.com/ar-convai/ConvaiTask/issues/255), In progress. No new duplicate issue, pushes, PRs, merges or release changes. Move to Sync only when work finishes; report unfinished scope honestly.

Next: integrate SDK pre-DOM key guard and rerun affected library/details plus correctly named preparation tests; finish S1 final review and commit verified Studio library/workspace. Provisioning Round2 source review and actual local provisioning/child build follow. Native oplock probes ran twice (corrected actual-user report `OplockProbe/7553597cb3af47fab3e17e494f98b157/results.json`): attribute-only R detected write/restore before writer close, allowed UE-like share-zero writers, refused an existing writable mapping, and real overflow/cancel were observed. Monitored replacement/root rename still failed despite successful unmonitored controls; production monitor remains gated. Provisioning agent is implementing scoped pre-save release plus real synchronous/asynchronous UPackage save fixtures. PakManager agent owns child-local selected-label cook inputs; no V0 broad label refresh is removed or called in the proposed V1 seam.

Per-item ledgers: PakManager `s2-preparation.md`, `s4-source-policy.md`, `s4-selected-source.md`; SDK worktree `.scratch/avatar-studio-v1/ledger/s1-library.md`, `s2-workspace.md`, and the new provisioning/detail ledgers as created. All are under `.scratch/avatar-studio-v1/ledger/` in their owning repository.

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
