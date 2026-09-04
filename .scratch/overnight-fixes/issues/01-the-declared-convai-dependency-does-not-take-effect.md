# The declared Convai dependency does not take effect in the running editor

Status: `ready-for-agent` — **the reported bug does not reproduce.** The declaration works. The
reporting gap is now closed and the two related questions are decided in the comments; nothing is
waiting on Anmol.

## What was reported

Selecting a MetaHuman avatar failed reference validation, believed to be **with `71aa9b4` in the
branch**:

```
/JBILN5CDNI4TRYELD6CS/BP_Hana illegally references: /ConvAI/MetaHumans/Animations/Convai_MetaHuman_FaceAnim
/JBILN5CDNI4TRYELD6CS/BP_Hana illegally references: /ConvAI/MetaHumans/Animations/Convai_MetaHuman_BodyAnim
/JBILN5CDNI4TRYELD6CS/BP_Hana illegally references: /ConvAI/ConvaiConveniencePack/ConvaiBPComponent/BP_ConvaiChatbotComponent
  - You may only reference assets from EngineContent, ProjectContent, and Plugin:JBILN5CDNI4TRYELD6CS here.
    (AssetValidator_AssetReferenceRestrictions)
```

## What is actually true

The commit was in the branch; it was not in the **running editor**. That session was an editor
started before the build, so `EnsureConvaiDependency` never executed in it.

| Evidence | Failing session (`Saved/Logs/Dev_CPM_58-backup-2026.09.03-17.00.17.log`) | Session after the build (`Saved/Logs/Dev_CPM_58.log`) |
|---|---|---|
| `Declared ConvAI as a dependency` | **0** | 1 (`:3105`, 17:35:24) |
| `Updating asset referencing domain DB` | 0 | 1 (`:9942`, 17:37:33) |
| `illegally references` | **4** | **0** |

Three further checks, all pointing the same way:

- `Plugins/JBILN5CDNI4TRYELD6CS/JBILN5CDNI4TRYELD6CS.uplugin` on disk now carries
  `"Plugins": [ { "Name": "ConvAI", "Enabled": true } ]`. The descriptor write lands.
- A `-run=DataValidation` pass over the whole project in a **fresh process** reported
  **no `AssetReferenceRestrictions` reference errors anywhere**. `/JBILN5CDNI4TRYELD6CS/BP_Hana`
  came back *contains valid data, but has warnings* — the warnings being a deprecated-node notice
  from Convai's own `ConvaiBaseCharacter`.
- BP_Hana still holds the same `/ConvAI/` references it always did; the dependency copy never
  rewrote them. Same asset, same references, same validator, now clean.

## Why the "stale domain database" hypothesis is dead

It was a good hypothesis and it is not what happens. `MarkDirty`'s next-tick timer is belt-and-braces:
the validate-on-save path force-refreshes the domain database **three** times before it checks —
`FEditorDelegates::OnPreAssetValidation` → `UpdateDBIfNecessary`
(`AssetReferencingPolicySubsystem.cpp:54`), and two unconditional `GetDomainDB()` calls, each of
which runs `UpdateIfNecessary()`. A same-frame save cannot outrun it.

Do not spend the restart test on this. If it ever reappears, the disproof is one grep: a session log
containing `Declared ConvAI as a dependency` **and** an `illegally references: /ConvAI/` error after
it. The failing log has the second without the first.

## What is left to do

1. ~~**A failed declaration still reaches nobody.**~~ **Done.** `PrepareEntryPoint` now hands the
   failure back through a new `OutDeclarationWarning`, and `SetEntryPoint` folds it into
   `OutSetupNotes`, so the panel's existing setup-notes row says it at the pick. The Warning log
   stays for the publish path, which runs the same check with nobody standing in front of it. Kept
   out of `OutChanges` on purpose: that array is what decides whether the creator's blueprint is
   re-saved, and a note about the descriptor must not save an asset nothing changed on.
2. **The Dependencies window cannot show what the validator rejects.** `ListDependencies` stops its
   walk at `ContentEveryProductShips()`, so `/ConvAI/` references appear in neither bucket. Deliberate
   per [ADR-0011](../../../docs/adr/0011-a-pak-holds-only-its-own-mount.md), but it means a creator
   debugging a reference error is shown nothing. Decide whether "correct" here is a third bucket:
   *referenced, not copied*.
3. **A pick queues validation of everything the tool saved.** One pick dumped 581 assets' worth of
   pre-existing warnings into the log, because the tool's `UPackage::SavePackage` triggers
   validate-on-save for the whole saved batch. `FScopedDisableValidateOnSave`
   (`EditorValidatorSubsystem.h`) around the save silences it, at the cost of a `DataValidation`
   module dependency — and at the cost of also hiding genuine errors on the asset just edited.
   Log noise only; decide whether it is worth the trade.

## The ControlRig report, answered

Moved to [issue 12](12-controlrig-references-a-mount-this-project-lacks.md); kept here because it is
what the validation sweep above turned up.

"Assets in the ControlRig module are not getting copied" — the same validation run found the
project's only error, and it is this:

```
/JBILN5CDNI4TRYELD6CS/MetaHumans/Common/Female/Medium/NormalWeight/Shoes/CasualSneakers/mh_ShoeTongue_CtrlRig
  soft references a missing package:
  /CharacterParts/Female/Medium/NormalWeight/Shoes/CasualSneakers/f_med_nrw_shs_casualsneakers
```

The ControlRig **was** copied. What it points at is a `/CharacterParts/` mount this project does not
have, so there is nothing on disk to copy or repoint — the gather cannot fix it and neither can the
creator. It needs a decision (drop the reference, or refuse the pick and say so), not a bug fix, and
it deserves its own issue.

## Done when

The reporting gap in (1) is closed: a creator whose descriptor could not be written is told at the
pick, not in the Output Log. (2) and (3) are decisions to record. `EnsureConvaiDependency` still has
no automation coverage beyond `DeclareConvaiDependency`'s unit test.

**What landed.** `PrepareEntryPoint` gained `OutDeclarationWarning`; `SetEntryPoint` appends it to
`OutSetupNotes`; the header doc at `ConvaiPakEditorSubsystem.h:222` no longer says the descriptor
write is unreported. Verified in the editor as M01 — with the descriptor stripped of its `ConvAI`
entry and made read-only, a pick of `BP_Hana` returned the sentence and the panel drew it under the
Selected asset row (`Saved/VibeUE/Captures/m01-setup-notes-row.png`, and the counts, in
[issue 11](11-clear-the-editor-only-verification-debt.md)). Decisions (2) and (3) are below; the
ControlRig report moved to [issue 12](12-controlrig-references-a-mount-this-project-lacks.md).

## Comments

**(2) No third "referenced, not copied" bucket, for now.** The bucket would exist to help a creator
debug a reference error, and the declaration is what stops those errors happening: the whole-project
`-run=DataValidation` sweep above found none once it had landed. When the declaration cannot be
written, item (1) now says so in the panel in the creator's own words, which is a better answer than
a list of `/ConvAI/` paths they can do nothing about. Revisit only if a creator reports a reference
error the declaration does not fix — that would be a case the bucket could actually explain.

**(3) `FScopedDisableValidateOnSave` declined.** It would silence the 581-asset validation storm a
pick triggers, and with it the validation of the very asset the tool just edited — the one save in
the batch where an error would matter. The cost of keeping it is log noise; the cost of taking it is
a genuine error on the creator's blueprint going unseen, plus a `DataValidation` module dependency.
Not worth the trade. If the noise ever has to go, the narrow version is to scope the disable to the
dependency copy's own saves and leave the entry point's save validated.

Investigated 2026-09-04. Log A/B above, plus a fresh-process `-run=DataValidation` sweep of the whole
project. Findings also recorded in
[docs/handoff/2026-09-03-dependency-gather.md](../../../docs/handoff/2026-09-03-dependency-gather.md).
