# Audit the Scene path against legacy

Status: `needs-triage` — the audit is done and gaps 35–40 are in the register. Which of them to
build is a maintainer's call, not an agent's.

Carried from [#263](https://github.com/ar-convai/ConvaiTask/issues/263), register item 1.

All 29 gaps in the parity register are Avatar-only. The Scene half of the tool has never been diffed
against legacy at all — roughly half the tool is unaudited.

Known to have no counterpart:

- `AssetIsScene`
- `SceneIsValid` — legacy required a tagged spawn point in the loaded level
- `GetNumSubobjectInAsset`

Those three are the starting point, not the scope. Run the same triage over the Scene path that
produced the Avatar register, and write the gaps into the register in the same form, so the two
halves are comparable.

## Done when

The Scene path has a gap list of the same shape as the Avatar one, each entry triaged, and the
register says the audit is complete rather than untouched.

## Comments

### 2026-09-04 — the Scene audit, read out of the legacy graphs

Same read as [issue 08](08-diff-the-publish-metadata-payload-against-legacy.md): the legacy project
opened in the 5.8 engine with the Dev_CPM_58 editor closed, and
`/ConvaiPakManager/Editor/AssetUploader` dumped over MCP. Graphs read for this half:
`AssetIsScene` (5 nodes), `SceneIsValid` (14), `GetNumSubobjectInAsset` (21), `PickedAssetIsValid`
(38), `Asset Is Actor` (10), `CN_OnPickedAssetClicked` (34) and `CN_OnSpawnPointButtonClicked` (3).

Six gaps went into the register as 35–40, all `registered`. What each legacy function actually did:

**`AssetIsScene(Asset)`** — one expression:
`GetClassDisplayName(GetClass(Asset)).Contains("world")`, case-insensitive. It loads the asset
first (`CPM Load Asset by Path`). The tool's `EntryPointSuitsAssetType` matches
`FAssetData::AssetClassPath` and never loads anything, which is strictly better; it is registered
only so the difference is on the record.

**`SceneIsValid(AssetPath)`** — two hard requirements, both `Error`-level, both refusing the pick:

1. `Get All Actors Of Class with Tag(WorldContext = the loaded level, Tag = "editorspawn")` →
   `Is Valid Index` → else *"Add actor with editorspawn tag"*.
2. `Get Actor Of Class(WorldContext = the loaded level)` → `Is Valid` → else *"Add nav mesh bounds
   volume in scene"*.

Neither `ActorClass` pin's value survives the graph reader — `UEdGraphPin::DefaultObject` is not
reflected, so `get_node_pins`, `get_node_details` and `get_graph_definition` all return the pin
empty. The classes are taken from the widget's own name table, which carries exactly one match for
each message: `NavMeshBoundsVolume` for the second, and nothing spawn-point-shaped at all for the
first (no `TargetPoint`, no `PlayerStart`), so check 1 is any `AActor` carrying the tag. Recorded as
inferred, not read.

**`GetNumSubobjectInAsset(AssetRef, CompClassToCheck)`** — gathers the blueprint's subobjects
through `USubobjectDataSubsystem::GatherSubobjectDataForBlueprint` and counts the ones whose class
`IsChildOf(CompClassToCheck)`. `PickedAssetIsValid` calls it on the Avatar branch and refuses when
the count is **> 2**, with *"Kindly remove existing chatbot component from avatar"*. Same caveat on
the class pin; `ConvaiChatbotComponent` is in the name table and the message names it.

**`PickedAssetIsValid(AssetPath)`** — a `Sequence` whose `then_0` switches on the modding
metadata's asset type and whose `then_1` returns `true`, so every refusal is an early return out of
the switch:

- `Max` → *"Asset type is none"*, refuse.
- `Avatar` → not `AssetIsActor` → *"`<leaf>` is not an actor"*; else the subobject count above.
- `Scene` → not `AssetIsScene` → *"`<leaf>` is not a level"*; else `SceneIsValid`.

The two `Split` nodes the register called "path-split checks" are not checks: they take
`FilenamePart` of the picked path purely to name the asset in those two messages. The tool's
`EntryPointSuitsAssetType` says the same thing in terms of the class instead of the leaf.

**Order of operations at the pick.** `CN_OnPickedAssetClicked` ran `PickedAssetIsValid` on the
creator's original `/Game/` asset, *then* `CopyAssetToPlugin`, then `AssetIsInPlugin`, then a switch
that ran `ConfigureAvatarForUpload` for an Avatar and **nothing at all** for a Scene. So there is no
dropped Scene-side setup step — the Scene half of the pick was validation only, which is why this
audit produces gaps in validation and nowhere else.

**The one finding that is a live defect rather than a missing feature** is gap 38.
`GetSpawnPointStatus` (`ConvaiPakEditorSubsystem.cpp:227-256`) iterates
`GEditor->GetEditorWorldContext().World()` — whatever map is open in the editor — not the level that
was picked as the Entry Point. Legacy loaded the picked level and asked *it*. Pick a Scene, open a
different map, and the spawn-point row describes the wrong world; the "Add spawn point" button then
adds one to the wrong world too.

Nothing here was built. `FCPM_AssetViewModel::ValidationMessages`
(`CPM_PakManagerViewModels.cpp:155-186`) still requires a name, an Entry Point and a thumbnail, and
`BeginPolicyRun` (`ConvaiPakEditorSubsystem.cpp:1175-1237`) still re-checks only the Entry Point and
the thumbnail, so a Scene with no spawn point and no nav mesh bounds volume still badges "Ready to
publish".
