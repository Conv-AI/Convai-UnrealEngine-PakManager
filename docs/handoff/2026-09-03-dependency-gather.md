# Handoff: the Pak boundary and the dependency gather

Written 2026-09-03, from the session that landed `71aa9b4`, `e55b7b2`, `24f86a8`. Everything those
commits do is in the commits, in [ADR 0011](../adr/0011-a-pak-holds-only-its-own-mount.md), and in
[the issue](../../.scratch/dependency-gather/issues/01-gather-dependencies-into-the-plugin.md).
This file is only what a fresh agent cannot read off those.

## The fact the session turned on

The work started from a creator-facing validation error and ended somewhere else, because the
premise everything was written against turned out to be backwards. The old belief, stated in
`ListDependencies`' own doc comment, was that a Primary Asset Label's recursive rule drags outside
dependencies into the Pak — so the risk was a Pak *growing* by a folder of test content.

The measurement that settled it:

```
UnrealPak.exe pakchunk10-Windows.pak -List
  → Map.umap, Map.uexp, NewMap.umap, NewMap.uexp     (nothing else)
```

Only the plugin's own packages, despite `bApplyRecursively = true` on the label. So the real failure
is the opposite and quieter: nothing grows, nothing warns, and the creator finds out when a Convai
product opens their Asset and draws none of it.

**Re-run that listing before trusting any of this.** It is one hand-made pak on one project, and it
is the entire evidential basis for ADR 0011 and for the feature built on it.

## What is verified, and how

| Claim | Evidence |
|---|---|
| It compiles | `Build.bat Dev_CPM_58Editor Win64 Development` → `Result: Succeeded`, no new warnings |
| Tests pass | `UnrealEditor-Cmd … -ExecCmds="Automation RunTests ConvaiPakManager;Quit"` → 55/55 |
| The gather works | **Not verified.** Nobody has clicked it |

The third row is the one that matters. There is no automation test for the copy or the repoint —
they need a real Asset Registry and packages on disk. To verify by hand: open the editor, pick a
level that references `/Game/` or engine content, accept the modal, then list the pak again and
check those packages are in it.

## What the review already ruled on

A 38-agent adversarial review ran over the diff: 8 findings confirmed and fixed, 8 refuted. Do not
re-raise the refuted ones without new evidence — notably "engine defaults like `WorldGridMaterial`
get duplicated" and "the asset registry is stale when the dialog reads it". Both were traced and
killed.

Two engine behaviours were verified in UE 5.8 source and are worth knowing before touching this code
again:

- `FPackageResourceManagerFile` resolves a package name by trying `.uasset` **before** `.umap`, so a
  world saved under the wrong extension silently shadows the real map file rather than failing.
- `AssetTools.RenameReferencingSoftObjectPaths` registers its remaps in `GRedirectCollector` for the
  rest of the editor session (`AssetRenameManager.cpp:1584`). They are removed after the pass now;
  leaving them installed silently repoints every later save in the project.

## Where the work is unfinished

The three open gaps are listed in the issue file. One of them is a design call rather than a bug, and
it is the one to settle first: **publish never re-checks for outside dependencies.** The gather is
offered at the pick and nowhere else, so a creator who wires up a `/Game/` reference after picking
publishes broken with no warning. `PrepareEntryPoint` is the shared seam — it already runs on every
publish and package, and it is where `EnsureConvaiDependency` was put for exactly that reason.
Whether the publish path should refuse, warn, or gather silently is a product decision.

Also live: `.scratch/overnight-fixes/`, opened separately against the same code. Its issue 01 asks
why a creator hit the MetaHuman validation error **with `71aa9b4` in the branch**. Measured since:
the commit was in the branch but not in the editor that was running, so the fix never executed
there.

- The Modding Plugin's `.uplugin` on disk carries `"Name": "ConvAI", "Enabled": true`, so
  `EnsureConvaiDependency` does land.
- A `-run=DataValidation` pass over the whole project in a **fresh process** produced **no**
  `AssetReferenceRestrictions` reference errors at all. `/JBILN5CDNI4TRYELD6CS/BP_Hana` came back
  "contains valid data, but has warnings" — the warnings being a deprecation notice from Convai's
  own `ConvaiBaseCharacter`, nothing to do with references.
- The failing session's log carries **no** `Declared ConvAI as a dependency` line and no domain-DB
  rebuild, while the session after the build carries both and zero reference errors. That editor was
  started before the build: the commit was in the branch, not in the running binary.

So there is no bug here to fix, and the "stale domain database" hypothesis is dead on its own terms
too — the validate-on-save path force-refreshes that database three times before it checks. Issue 01
now scopes what is actually left, which is that a *failed* declaration still reaches nobody but the
Output Log. The runtime half of the `/ConvAI/` exclusion question is untouched and still open — read that
brief before editing `ContentEveryProductShips()`, because both pieces of work move the same list.

That same run turned up the project's only validation error, which is a copy artefact and belongs to
the "ControlRig assets are not getting copied" report:
`/JBILN5CDNI4TRYELD6CS/MetaHumans/.../mh_ShoeTongue_CtrlRig` soft-references a missing package under
`/CharacterParts/`, a mount this project does not have. Nothing can copy what is not there, so the
gather cannot fix that one — it needs a decision, not a bug fix.

## Working notes

- **Never trigger Live Coding in this project.** Ask for the editor to be closed and do a full
  build. This was an explicit correction during the session.
- The editor must be closed to build at all — the DLL is locked while it runs.
- Doc edits by whole-paragraph replacement kept splicing duplicated text into `CONTEXT.md` and the
  legacy-parity register. Match on the file's own line wrapping, and read the diff afterwards.

Skills worth loading next session: `diagnose` if the hand verification finds the gather misbehaving,
`triage` for the open issue files, `code-review` before anything else lands on this code.
