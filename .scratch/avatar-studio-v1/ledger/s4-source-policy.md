# S4 - per-run source policy

Specification: `Context/V1/Avatar-Studio-V1-Implementation-Design.md` sections 8.2-8.3, G03, A08 and S4, relative to the host project root. This item implements the shared policy seam only; selected-avatar source output remains a dependency.

## Round 1 - implementation

Status: partial; code and focused automation written, coordinator build/test verification pending.

Decisions:

- Add `ProjectSetting`, `Omit` and `IncludeFresh` to existing `FCPM_PublishOptions`. The default preserves `!packageOnly && policy && projectSetting`. Explicit fresh overrides the local default but cannot add source disallowed by the resolved policy. Denial is reported before constructing the job queue; platform selection is unchanged.
- Keep the current archive job. Source inspection confirmed it already runs the zip writer every time it is queued; the writer truncates the prior zip. Raw upload markers only feed display state in this checkout. A reuse/fresh flag or marker-based optimization would add behavior without a requirement. Correct the obsolete marker text implying deleting it triggers another upload.
- No worker or selected-avatar archive capability is introduced. Existing `UCPM_ArchiveRawProjectJob` still selects V0 project-wide inputs, including V0 configuration behavior. `SourceChoice` documents this limitation. V1 must not route its persistent uploader through that archive job until a selected-avatar allowlist, sanitized rebuild metadata and real archive round-trip are implemented and verified. No claim of S4/A08 completion follows from this seam.
- Reuse the current automation framework and actual zip writer/reader for archive freshness evidence. The archive test writes only an owned GUID fixture under `Saved/Automation`; it never archives the user's project, invokes a backend, edits UObjects or advances a real cloud marker. Full queue/editor/live source integration remains unverified.

Files: `Public/Publish/CPM_PublishTypes.h`, `Private/Publish/CPM_PublishTypes.cpp`, `Private/ConvaiPakEditorSubsystem.cpp`, `Private/Tests/CPM_SourcePolicyTest.cpp`, under `Source/ConvaiPakManager`.

Verification to run by the integration owner:

- Prescribed full host build, with serialized editor/build ownership and no Live Coding; require current `lastBuild.status == "succeeded"`.
- Automation filters `ConvaiPakManager.Publish.SourcePolicy`, `ConvaiPakManager.Publish.Policy`, `ConvaiPakManager.Publish.Runner`, `ConvaiPakManager.Publish.Metadata`, plus the coordinator's V0 regression selection. All prefixes were checked against the current source.
- Source-policy cases: default policy/setting/package-only matrix; omit; fresh with disabled local default; policy-denied fresh despite platform override; unknown choice; real archive replacement with changed and removed inputs and an unchanged prior-upload marker.

Actual results: `git diff --check` passed (Git emitted LF-to-CRLF notices only). No build, automation, editor operation, remote request or commit run by this implementer. Root's pre-edit baseline build was reported successful; it does not validate these changes.

Open gate: G03 live resolved policy and source correspondence, selected-avatar archive generation, archive sanitation and source rebuild/download round-trip. Existing backend slot naming remains unchanged.

## Round 1 - review

Reviewer: independent preparation agent (`pakmanager_s0`), read-only source review against the base implementation. Verdict: approved for this bounded source-policy seam; S4 and A08 remain partial.

Findings: no blocker, major or minor source correctness issue found in the reviewed change. `Source/ConvaiPakManager/Private/Publish/CPM_PublishTypes.cpp:181` preserves the V0 default policy/setting/package-only decision, makes omission explicit and rejects policy-denied fresh source. It does not alter platform selection. `Private/ConvaiPakEditorSubsystem.cpp:1542` resolves the choice before queue construction and passes the same result into archive scheduling and upload configuration at lines 1624-1634.

Agreed implementation decision: reuse the existing writer. `Private/Jobs/CPM_PublishJobs.cpp:298` still invokes the archive operation on every queued run, and `Private/ConvaiPakManagerEditorUtils.cpp:237` opens the archive for replacement. `Private/Tests/CPM_SourcePolicyTest.cpp` exercises the actual writer and reader, checking changed bytes and removal of an old entry. The upload timestamp is unchanged by archive generation; it is not treated as source correspondence evidence. These findings do not validate the project-wide V0 input set for selected-avatar publishing.

Verification inspected from coordinator-produced artifacts in this session: host `Saved/ConvaiAvatarStudio/Development/preparation-build.json` reports `status=succeeded`, `verdict=succeeded`, `exitCode=0`, completion `2026-09-08T16:04:00.6407281Z`. Host `Saved/ConvaiAvatarStudio/Development/PreparationTests/index.json` records 78 successes, 2 successes with warnings, 0 failures and 0 not run; all three `ConvaiPakManager.Publish.SourcePolicy` cases are successful with no errors or warnings. The coordinator identifies the two warnings as baseline warnings. The reviewer ran no build, test, editor operation or commit.

Material acceptance gates remain: verify the live resolved publish policy; implement and validate a selected-avatar archive allowlist and sanitized rebuild metadata; establish coherent source/package correspondence; verify real archive download/rebuild and create/update integration. Helper tests and the real ZIP fixture are not backend integration or full S4 completion.
