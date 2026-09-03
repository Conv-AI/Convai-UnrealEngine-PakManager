# The declared Convai dependency does not take effect in the running editor

Status: `ready-for-agent` — **the reported bug does not reproduce.** The declaration works. What is
left is the reporting gap and two related questions, scoped at the bottom.

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

1. **A failed declaration still reaches nobody.** `EnsureConvaiDependency` warns rather than refuses
   by design, so its three failure messages (no plugin of that name mounted, Convai not enabled, the
   `.uplugin` not writable) are Output Log only. At *pick* time the creator is standing right there
   and the fix is one file — report it. `SetEntryPoint`'s `OutSetupNotes` deliberately does not carry
   it today ([ConvaiPakEditorSubsystem.h:222](../../../Source/ConvaiPakManager/Public/ConvaiPakEditorSubsystem.h#L222)).
   This is the reason the bug above looked like a silent failure even though it was not one.
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

## Comments

Investigated 2026-09-04. Log A/B above, plus a fresh-process `-run=DataValidation` sweep of the whole
project. Findings also recorded in
[docs/handoff/2026-09-03-dependency-gather.md](../../../docs/handoff/2026-09-03-dependency-gather.md).
