# "Copy into plugin" copies nothing

Status: `ready-for-agent` — the relocate half is fixed and verified in the editor. What is left is
the gather's reference fixup, diagnosed in the comments.

The button reports success, or reports nothing, and the asset is still outside the Modding Plugin.

`HandleRelocateEntryPoint` at
[SCPM_AssetDetailPanel.cpp:1009](../../../Source/ConvaiPakManager/Private/UI/SCPM_AssetDetailPanel.cpp#L1009)
→ `RelocateEntryPointIntoPlugin` at
[ConvaiPakEditorSubsystem.cpp:599](../../../Source/ConvaiPakManager/Private/ConvaiPakEditorSubsystem.cpp#L599).

This is the **relocate** half, not the gather. `GatherDependenciesIntoPlugin` landed and was verified
in `e55b7b2`/`24f86a8`; relocate is its older sibling and did not get the same attention — and
[#263](https://github.com/ar-convai/ConvaiTask/issues/263) explicitly lists the Copy-into-plugin
button under "no visual pass", so there is no prior evidence it works in a real editor at all.

Suspects, in the order they are cheap to check:

- The "this project records no modding plugin to copy into" path at
  [:606](../../../Source/ConvaiPakManager/Private/ConvaiPakEditorSubsystem.cpp#L606) —
  `GetModdingMetadataForChunk` returning a chunk with no plugin name.
- A copy report whose `FailedPackages` is being swallowed by `WhyCopyFailed` and shown as nothing.
- The relocate succeeding on disk but never writing the new package back as the chunk's entry point,
  so the panel still shows the old path and the creator reads that as "nothing happened".
- `bReferencesFixedUp` false — copies landed, nothing points at them. Note the header change in this
  branch: that flag is "ran to its end without reporting an error", not a per-package guarantee.

Check whether the fixup gained by `AdditionalPackagesToFixup` in `24f86a8` needs the same treatment
here. Relocate moves the Entry Point itself, so the packages needing repointing are different from
the gather's, and the newer option may simply not be wired into this path.

## Done when

The button copies a project-content entry point and its closure into the Modding Plugin, the panel
shows the new package path, and the chunk records it. Whatever the failure was, it reports itself to
the creator rather than succeeding silently.

## Comments

### What the logs established before touching anything

Neither session log contains `is this chunk's entry point now`, and the session that produced the
report (`Dev_CPM_58-backup-2026.09.03-17.00.17.log`) contains no `CPM_DependencyCopyAPI:` line at
all — zero, against 2835 of them in the session after the build. **No relocate ever reached the
copy.** Whatever happened, it happened in one of the silent early returns, and every one of them —
five in `RelocateEntryPointIntoPlugin` and two in the panel's `HandleRelocateEntryPoint` — left
nothing in the log to say which.

### What was changed

- Every refusal in `RelocateEntryPointIntoPlugin` now goes through one `Refuse` lambda that words it
  for the creator *and* logs it at Warning with the package. A copy that never starts leaves a trace.
- The tail refusal could return `false` with an **empty** `OutWhy` — `GetChunkStatus().Message` is
  empty whenever the status was not set — and the panel writes that straight into `EntryPointError`,
  whose row also carries the Copy-into-plugin button. Empty text collapses the row, so a failing copy
  could take both the message and the button off the screen. `OutWhy` is now never empty on failure.
- `HandleRelocateEntryPoint` notifies on failure as well as writing the row. The row is where the
  refusal that produced the button already sits, so replacing its text is a report a creator can
  miss; the gather path next to it already notified.

### What was changed after review

- **The real "reports nothing" path was the stale status, not the empty one.** Every
  `PrepareEntryPoint` refusal sets a status, so `GetChunkStatus().Message` is empty only for a chunk
  that has had none this session; the path that reached the tail refusal *without* setting one was
  `WriteDraftFields` returning `false` (unparseable or unwritable `Draft_N.json`), and then the
  message is the **previous** refusal — on this flow, the pick's own "X is outside the plugin", the
  sentence already in the row. `SetEntryPoint` now sets `Update_Failed` with its own reason when the
  Draft cannot be written, which fixes the panel's `HandleUseSelectedAsset` read of the same status
  as well. The empty-guard in the relocate stays as belt-and-braces.
- The `Refuse` log prefix said `Did not copy %s into the modding plugin` on the one refusal that
  runs *after* the copy landed. It now reads `Copy into plugin refused for %s: %s.`
- The relocate dropped the copy's setup notes on the floor. `RelocateEntryPointIntoPlugin` returns
  them (`OutSetupNotes`) and the panel shows them, so "added BP_ConvaiChatbotComponent ..." and the
  failed-declaration warning reach the creator on this path too, not only on a pick.

No automated test for the status hole, deliberately. Reaching `WriteDraftFields` means getting past
`PrepareEntryPoint`, which needs a real asset **on disk, of the matching kind, under this project's
modding plugin mount** ([:453-484](../../../Source/ConvaiPakManager/Private/ConvaiPakEditorSubsystem.cpp#L453));
it is not reachable under `-nullrhi` with no packages. A test that saves a world into
`/JBILN5CDNI4TRYELD6CS/` would pass here and fail in any other checkout, which is worse than no
test. The check that belongs in the suite is a seam nothing else wants; the honest coverage is the
editor pass in [issue 11](11-clear-the-editor-only-verification-debt.md).

### M02, in the editor — the relocate works

A fresh `/Game/CPM_CopyTest/BP_CopyTest` was refused as outside, then copied in through
`RelocateEntryPointIntoPlugin`: `Copied 1 packages into /JBILN5CDNI4TRYELD6CS/ (0 skipped);
/JBILN5CDNI4TRYELD6CS/CPM_CopyTest/BP_CopyTest is this chunk's entry point now.` The file landed
under `Plugins/JBILN5CDNI4TRYELD6CS/Content/CPM_CopyTest/`, `Draft_10.json` recorded the copy, and
the panel drew the new path in its Selected asset row. Details and screenshots in
[issue 11](11-clear-the-editor-only-verification-debt.md). So the reported bug does not reproduce on
this binary either — which is consistent with the logs, since it never ran on the old one.

### M03, in the editor — ADR-0011's gather does **not** hold, and here is why

Picking `BP_Hana` again still offers a gather. The Dependencies window says it plainly: *737 packages
are reachable from BP_Hana. 157 of them are outside /JBILN5CDNI4TRYELD6CS/* — after two gathers had
already copied 580 and then 88 packages in.

Diagnosed before touching anything, in the order the brief asks:

- **(a) The copies are all there.** `/ControlRig/Controls/X`, `/Niagara/Enums/X`,
  `/Engine/EditorBlueprintResources/StandardMacros` and both `/Game/MetaHumans/` survivors all have
  copies under the plugin mount (engine content lands under `EngineContent/`, which is what
  `MakeDestinationPackage` does). Not a copy failure, and not the engine-duplication bug `24f86a8`
  fixed — that guard works: the second gather skipped 650 game packages, and on this binary it skips
  the engine ones too.
- **(c) The registry is not stale.** A byte scan of the saved
  `Plugins/JBILN5CDNI4TRYELD6CS/Content/BP_Hana.uasset` finds exactly three outside package names in
  the file — `/Engine/EditorBlueprintResources/StandardMacros`,
  `/Game/MetaHumans/Common/Face/Face_AnimBP` and `.../CasualSneakers/mh_ShoeTongue_CtrlRig` — against
  32 in-plugin ones. The references really are on disk.
- **(b) It is the fixup, and it *is* the object map — the map has no keys for subobjects.**
  `FixupAllHardReferences` builds `OldToNewObjects` from
  [`ForEachObjectWithPackage(Package, ..., /*bIncludeNestedObjects*/ false)`](../../../Source/ConvaiPakManager/Private/CPM_DependencyCopyAPI.cpp#L841)
  filtered on `IsAsset()`, then matches source to dest by class and name
  ([:878](../../../Source/ConvaiPakManager/Private/CPM_DependencyCopyAPI.cpp#L878)). That is one key
  per top-level asset and none for anything beneath one.
  [`FArchiveReplaceObjectRef`](../../../Source/ConvaiPakManager/Private/CPM_DependencyCopyAPI.cpp#L938)
  replaces exact pointer keys, so a reference whose target is a **subobject** of a copied asset has
  no key and survives by construction — the archive would rewrite it happily if the key were there.

  The saved `BP_Hana.uasset` shows exactly that shape: it carries **both** paths for each survivor —
  `/Engine/EditorBlueprintResources/StandardMacros` *and*
  `/JBILN5CDNI4TRYELD6CS/EngineContent/EditorBlueprintResources/StandardMacros`,
  `/Game/MetaHumans/Common/Face/Face_AnimBP` *and* `/JBILN5CDNI4TRYELD6CS/MetaHumans/Common/Face/Face_AnimBP`
  — next to `K2Node_MacroInstance`, `EdGraph` and `ForEachLoop`. The asset reference was repointed at
  the copy; the reference *into a subobject of that asset* — a macro instance's `FGraphReference` to
  the `ForEachLoop` `UEdGraph`, which is outered to the Blueprint and so never `IsAsset()` — still
  imports the original. Walking the closure over the Asset Registry finds only **ten** such edges,
  from four packages: `BP_Hana` (3), the copied `Niagara/Modules/Emitter/EmitterState` (4) and
  `System/SystemState` (2), and `mh_ShoeTongue_CtrlRig` (1, the `/CharacterParts/` one —
  [issue 12](12-controlrig-references-a-mount-this-project-lacks.md)). Stable across two gathers,
  which is what "by construction" predicts.

  **The earlier reading here was wrong** and is corrected above: it said "the map contained them, so
  the residue is references the archive does not rewrite", on the strength of
  `Mapped object /Game/MetaHumans/Common/Face/Face_AnimBP.Face_AnimBP_C -> ...` in the gather log.
  That line says the *asset* was mapped, which is not what the surviving imports point at. Nobody
  needs to hunt an unrewritable property; there isn't one.

**The fix**: extend the map build. For each mapped source→dest asset pair, walk the source asset's
subobjects and add each one keyed to the destination subobject at the same relative path. No new
archive, no property hunt. A post-fixup closure re-walk is worth adding *on top* as an honesty
guard — today `bReferencesFixedUp` is true for this gather and the Command reports success while 157
packages stay outside the Pak, which is the same silent-absence failure ADR-0011 was written against
— but the guard is not the fix.

**Not implemented here**, per the brief's hour rule; the diagnosis is now down to a named line of
code rather than a suspicion.

Provenance, since it decides what can be re-checked: the wave-1 session logs have rotated out of
`Saved/Logs`, so the log-derived counts above (the second gather's 650 skips) can no longer be
re-run. What the correction rests on can: the map build is in the source, and the double paths in
`Plugins/JBILN5CDNI4TRYELD6CS/Content/BP_Hana.uasset` re-scan on demand
(`grep -a -o -E "[/A-Za-z0-9_]*(StandardMacros|Face_AnimBP)[/A-Za-z0-9_.]*"`).
