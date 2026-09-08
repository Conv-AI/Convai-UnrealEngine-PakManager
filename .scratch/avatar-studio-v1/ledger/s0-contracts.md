# Avatar Studio V1 — S0 contracts and execution ledger

Specification: host `Context/V1/Avatar-Studio-V1-Implementation-Design.md`, visual companion, 2026-09-08 handoff, full meeting transcript, and context brief. The implementation request supersedes the handoff's pending approval.

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
