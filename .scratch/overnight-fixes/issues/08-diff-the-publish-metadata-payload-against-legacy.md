# Diff the publish metadata payload against legacy, field for field

Status: `needs-triage` — the diff is done and the two rows that were literals are fixed and tested
(see the last comment). What is left is one judgement call to confirm — every new Asset is now
pinned `private` — and the two rows that moved to issues
[13](13-nothing-writes-the-per-platform-pak-size.md) and
[14](14-the-raw-version-slot-was-renamed-on-the-wire.md).

Carried from [#263](https://github.com/ar-convai/ConvaiTask/issues/263), register item 2. **#263's
own recommendation is that this is the next thing to do, ahead of the Scene audit** — the Scene gap
merely leaves something unbuilt, while a mismatch here is silent and corrupts every published
record.

Legacy `GetCreateMetaData` (97 nodes) and `GetUpdateMetaData` (67 nodes) have never been diffed
against the tool's `FillRequiredMetadataFields` / `ComposePakMetadataAt`. The PRD's own words: "A
silent mismatch here corrupts every published record."

Go field by field. For each one in the legacy graphs, find its counterpart, and record: present and
matching, present but differently named or typed, or missing. A missing field that the server
tolerates today is still worth logging — tolerated is not the same as unused.

Do not stop at the create path. Update is the one that overwrites a record that already exists, so a
wrong field there destroys data rather than merely publishing it wrong.

## Done when

Every field in both legacy graphs is accounted for in a written diff, every mismatch is either fixed
or logged as its own issue with the reason it was left, and the fixed ones have a test that reads
the composed payload rather than trusting the composer.

## Comments

### 2026-09-04 — the diff, read out of the legacy graphs

Read from a running editor, not from bytes. `E:\Downloads\LegacyUploader` opened in the same 5.8
engine (its `ConvaiPakManager` is 2.3.11) with the Dev_CPM_58 editor closed, and the widget
`/ConvaiPakManager/Editor/AssetUploader` dumped over MCP — node titles, pin literals and every
connection — for `GetCreateMetaData` (97 nodes), `GetUpdateMetaData` (67), `GetCreatePakParams`,
`GetUpdatePakParams`, `GetTags`, `GetAssetType`, `GetEngineVersionString`, `Asset Is Actor`,
`ConvaiCreateAsset`, `ConvaiUpdateAsset`, `ConvaiUpdateRawAsset` and `UpdateAssetMetaData`. The raw
dumps live in that session's scratchpad, not in the repo.

Two things the brief asked to settle up front, both settled: legacy **did** send `visibility`, and
legacy's update **did** re-send a full document. Details in the rows.

There is no `ShouldCreateAsset` in the legacy widget. The create path is `EventCreateAsset` →
`ConvaiCreateAsset`, gated by `SanityChecking` and the status enum; the only per-platform gate is
`ShouldUpdateAssetForPlatform`, on the update side.

#### What legacy composed

`GetCreateMetaData` builds a fresh `MetaDataObject` and a fresh `EntityDataObject` from nothing. A
`Sequence` fans out three ways: the top-level fields, then the entity object, then `Get Json String`
→ return.

`GetUpdateMetaData` does **not** start from nothing. It loads `Get Asset Meta Data String` — the
document the last `assets/get` saved locally — parses it, overwrites a subset, and serialises the
whole thing back. So the update payload is the full previously-published record with six fields
refreshed, and everything else (`asset_name`, `asset_description`, `asset_type` and the whole of
`entity_data`) survives untouched from the server's own last answer. That `Sequence`'s `then_1` pin
is not connected at all — it is the branch that would have rewritten the entity object, and legacy
left it dangling on purpose.

#### Create — the metadata document

| Field | Legacy value / source | Tool counterpart | Verdict |
|---|---|---|---|
| `project_name` | `Get Project Name` | `FillRequiredMetadataFields`, `CPM_Chunk.cpp:1007` | matching |
| `plugin_name` | `L_PluginName` off `Get Modding Metadata` | `:1008` | matching |
| `asset_type` | `GetAssetType` — the modding metadata enum → string → `ToLower` | `:1009`, `Modding.AssetType.ToLower()` | matching |
| `asset_name` | the `AssetName` text box, no fallback | Draft field, falling back to the project name (`:1023`) | matching — the fallback is new and only fires on a blank box |
| `asset_description` | the `AssetDescription` text box | Draft field, default `""` (`:1024`) | matching |
| `root_path` | `Append("/", L_PluginName, "/")`, written unconditionally | `DefaultString` (`:1020`), and the pick writes the package's real mount root into the Draft (`ConvaiPakEditorSubsystem.cpp:576`) | deliberately-changed — an internal project can mount its content outside a Modding Plugin, and the pick knows the true root where this formula only guesses |
| `content_path` | `Append("../../../", GetProjectName, "/Plugins/", L_PluginName, "/Content/")` | the same string, `Printf`-built at `:1013` | matching — this settles the `RepairsAnOlderDocument` question: legacy never pointed at the project's own `Content/`, so a document that does was written by an intermediate Pak Manager, not inherited |
| `level_name` | Scene branch: the `AssetPath` box verbatim. Avatar branch: an empty literal | Draft (`:581` / `:587`); `ComposePakMetadataAt` upgrades a leaf-only value to a package path (`CPM_Chunk.cpp:1261`) | matching |
| `blueprint_class_path` | Avatar branch: the `AssetPath` box. Scene branch: an empty literal | Draft (`:591` / `:583`) | matching |
| `blueprint_class` | Avatar: `Load Blueprint Class(AssetPath)` piped straight into a string field. Scene: `Get Class` of an empty `Make Asset Data`, which serialises as `None` | the Draft writes `/Script/Engine.BlueprintGeneratedClass'<package>.<leaf>_C'`; `DefaultString` supplies `None` (`:1030`) | matching in shape — the tool's literal was taken verbatim from a published avatar, so it is the better evidence of what the server stores; how legacy's `SetJsonField` stringified a `UClass` was not read |
| `entity_data` | rebuilt from an empty `Make JsonObject` on every create | merged, never rebuilt (`:1035-1071`) | deliberately-changed — ADR-0005 and the `KeepsWhatItDoesNotOwn` test; the server puts things in this object that nothing here would know to put back |
| `entity_data.avatar_name` | `Split(AssetPath).FilenamePart` | `LeafOf(blueprint_class_path)`, else `asset_name` | matching |
| `entity_data.gender` | literal `male` | `DefaultString(…, "male")` | matching |
| `entity_data.avatar_config` | empty `Make JsonObject` | empty object when absent | matching |
| `entity_data.scene_name` | `Split(AssetPath).FilenamePart` | `LeafOf(level_name)`, else `asset_name` | matching |
| `entity_data.scene_description` | literal `Pak scene` | `DefaultString(…, "Pak scene")` | matching |
| `entity_data.scene_metadata` | empty `Make JsonObject` | empty object when absent | matching |

Every key legacy wrote is present, and the tool writes no key legacy did not. The `metadata` half of
the create payload is clean.

#### Create — the multipart form

`GetCreatePakParams` fills `FCPM_CreatePakAssetParams`; the form is built at `CPM_Proxy.cpp:31-119`.

| Form field | Legacy value | Tool counterpart | Verdict |
|---|---|---|---|
| `metadata` | `GetCreateMetaData` | the composed document (`CPM_PublishJobs.cpp:395-464`) | matching |
| `entity_type` | `GetAssetType` | `Modding.AssetType.ToLower()` (`:426`) | matching |
| `thumbnail` | the `ThumbnailUI` widget's texture | `CPM_LoadTexture2DFromDisk` off the Chunk's PNG (`:431`) | matching |
| `tags` | Avatar `["Pak","Avatar"]`; **Scene `["Pak","ConvaiSim","Background3D","Scene"]`**; a raw upload appends `"Raw"` | `PakTagsFor` (`CPM_PublishJobs.cpp:39-43`) returns `["Pak","Scene"]` or `["Pak","Avatar"]`, and nothing ever adds `Raw` | **missing** — a Scene published by this tool loses `ConvaiSim` and `Background3D`, and no artefact is ever tagged `Raw` |
| `version` | literal `TempAsset`; on success `ConvaiCreateAsset` immediately calls `Convai Delete Asset` for that same `TempAsset` version | `VersionSlotFor(first platform to package)`, i.e. `ue-5.8-Windows` (`:429`) | deliberately-changed — this tool uploads the first platform's Pak to the URL the create call mints, so there is no throwaway version left to delete; the residual risk is that a create now claims a real platform slot where the server may have expected a placeholder |
| `visibility` | literal `private` | `FCPM_CreatePakAssetParams::Visiblity` is never assigned, so the field is omitted entirely | **missing** — legacy pinned every new Asset private; this tool leaves the default to the server |

The update form re-uses the same builder, sends `asset_id` instead of `entity_type`, sets `version`
from `GetEngineVersionString(Platform)` = `"ue-" + first 3 chars of the engine version + "-" +
Platform` — `ue-5.8-Windows`, identical to `FCPM_PakArtifact::VersionSlotFor` — and leaves
`visibility` empty. The one exception is the raw slot: legacy sent `ue-5.8-Raw`, the tool sends
`raw` (`CPM_PublishTypes.cpp:33-51`, with its reason at the constant). Deliberately-changed, but it
is a wire-visible rename and belongs on the list if the server keys versions by string.

**Legacy did send `tags` on update**, and the full set. `GetUpdatePakParams` calls the same `GetTags`
the create path calls, with `bRawAsset` wired to `Platform == Raw`, so every per-platform update
overwrote the Asset's tags with `["Pak","ConvaiSim","Background3D","Scene"]` (or `["Pak","Avatar"]`)
and only the Raw call appended `"Raw"`. Sending the whole set on update is therefore parity rather
than a new risk, whichever way the server resolves it. One difference this tool cannot avoid: legacy
sent one call per platform, this tool sends one per publish, so where legacy's `Raw` tag stood only
because the raw step ran last, here `Raw` is decided once, from `Context->bHasRawArchive`.

#### Update — the metadata document

| Field | Legacy value / source | Tool counterpart | Verdict |
|---|---|---|---|
| the base document | `Get Asset Meta Data String` — the local copy the last `assets/get` saved | this Chunk's `PakMetadata_<env>.json`, the last echo the server sent, overlaid by the Draft (`ComposePakMetadataAt`, `CPM_Chunk.cpp:1209-1272`) | matching in shape |
| `project_name`, `plugin_name`, `root_path`, `content_path` | rewritten exactly as on create | the same code path — `FillRequiredMetadataFields` runs on every compose | matching |
| `level_name`, `blueprint_class_path`, `blueprint_class` | the same `Asset Is Actor` branch as create | same | matching |
| `<Platform>_PakSize` | `Append(EnumToString(Platform), "_PakSize")` → `CPM Get File Size` of that platform's Pak, or of the raw project zip when the platform is `Raw`. So `Windows_PakSize`, `Linux_PakSize`, `Raw_PakSize` | nothing writes any of these, anywhere | **missing** — register gap 31 filed `CPM_GetFileSize` as a UI affordance ("show the upload size before a Publish"); it was also a published field, and a record updated by this tool keeps whatever size the last legacy publish left on it |
| `asset_type`, `asset_name`, `asset_description`, `entity_data` | **not written on update** — the `Sequence`'s `then_1` is unconnected, so all four survive from the server's own last answer | rewritten on every compose, with the Draft winning | deliberately-changed — ADR-0005, the creator's project is the record of their Assets. Worth knowing anyway: an update from this tool overwrites a name the server holds with the name in the Draft, where legacy could not |

#### What to do about the rows that are not matching

- **`tags`** — put the legacy sets back in `PakTagsFor` and add `Raw` for the raw archive. Four
  string literals, and they change what the server can find. A `CPM_AssetMetadataTest` case cannot
  see it — tags are a form field, not part of the document — so it wants its own small test on
  `PakTagsFor`.
- **`visibility`** — set `Params.Visiblity` to `private` on the create path only; legacy never sent
  it on update. Worth asking whether the server still defaults new Assets to private, because
  sending it is the safe half of that question and omitting it is what every publish since the
  rewrite has done.
- **`<Platform>_PakSize`** — write it per Version slot from the artefact the run actually built.
  `ComposePakMetadataAt` has no artefact paths, so this is the one row with a design question
  attached: it may belong on the Job that knows the Pak's path rather than in the composer. Left
  registered rather than guessed at.
- **`raw` vs `ue-5.8-Raw`** — no change proposed without knowing whether an existing Asset already
  carries a `raw` slot from this tool. Renaming it back would strand those; leaving it strands the
  legacy ones. A question for Anmol, not a fix.
- **`version` on create** — no change proposed. The `TempAsset`-then-delete dance existed because
  legacy's create uploaded nothing. Recorded so nobody re-derives it.

Register item 2 in `.scratch/legacy-parity/PRD.md` stays open until those are fixed or logged: the
diff is done, the fixes are not.

#### One thing this read settles for issue 10

Legacy's `UpdateAssetMetaData` macro is the `assets/get` caller the register was hunting for. It ran
after every successful upload — `ConvaiUpdateAsset` and `ConvaiUpdateRawAsset` both end in it — and
what it saved is exactly the document `GetUpdateMetaData` re-read on the next publish. That is why
the refresh mattered there and does not here: this tool recomposes from the Draft instead.

### 2026-09-04 — the fixes, and where the other two rows went

Two of the four non-matching rows were string literals with a clear legacy answer; both are fixed.
The other two are not fixes, and each has its own issue file now.

**Fixed — `tags`.** A Scene now leaves as `["Pak","ConvaiSim","Background3D","Scene"]` and an Avatar
as `["Pak","Avatar"]`, verbatim from legacy, and a run that also sends the Raw Project Archive
appends `"Raw"`. The Raw tag is taken from `Context->bHasRawArchive` — written by
`UCPM_ArchiveRawProjectJob` and read again by `UCPM_UploadArtifactsJob`, so the tag and the upload
cannot disagree even if the creator toggles Upload Raw Project Archive during the cook. Reading the
setting again here would have been that disagreement. Meaning is unchanged from legacy: legacy's
raw upload was `ConvaiUpdateRawAsset` against the same `asset_id` at `ue-5.8-Raw`, so its `Raw` tag
also sat on an Asset carrying the Pak Versions.

**Fixed — `visibility`.** `private` on create, and nothing at all on update, which is exactly what
legacy did. **This is the row worth a second pair of eyes**: from this build on, every Asset the Pak
Manager creates is private at birth, where everything published since the rewrite inherited whatever
the server defaults to. If the server's default is already private the change is a no-op; if it is
not, creators will have to open new Assets up on the website. The update path deliberately keeps
sending nothing, so an Asset a creator has since made public is not quietly shut again.

**Moved out — `<Platform>_PakSize`** → issue 13. The read for this issue settled that the artefacts
are already on `FCPM_PublishContext` when the document is composed, and the runner swap left them as
plain fields, so there is nothing to declare. What is still open there is where the number lands — a
composer parameter or the Job editing the composed document — and whether anything reads the field.

**Moved out — `raw` vs `ue-5.8-Raw`** → issue 14. Both names exist on the backend now; picking one
strands artefacts under the other. A question for Anmol.

**Unchanged, as the table said** — `version` on create (the `TempAsset`-then-delete dance existed
because legacy's create uploaded nothing), `root_path`, `entity_data` being merged rather than
rebuilt, and the update path rewriting `asset_type` / `asset_name` / `asset_description` /
`entity_data` where legacy's dangling `then_1` left them alone. That last one is ADR-0005 on
purpose: the creator's project is the record of their Assets.

#### Where the fix lives

Tags, visibility, `entity_type` and the Version slot moved out of `UCPM_CreateAssetJob::IExecute`
into `ConvaiPakManager::Publish::FillPublishFormFields` (`CPM_PublishJobs.h` / `.cpp`). It is pure,
so a test reads the values that go on the wire instead of trusting the Job that fills them. The
document composer was not touched — the create document's key set already matched legacy exactly,
and T03 now pins that. `CPM_Proxy.cpp` was not touched either: it already sends `visibility`
whenever the field is non-empty, so no multipart-form extraction was needed.

#### Build and tests

```
E:/Software/UE_5.8/Engine/Build/BatchFiles/Build.bat Dev_CPM_58Editor Win64 Development -Project=E:/UEProjects/UE5.8/Dev_CPM_58/Dev_CPM_58.uproject
Result: Succeeded
```

```
UnrealEditor-Cmd.exe Dev_CPM_58.uproject -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
65 tests completed, 65 Success, 0 failures
```

Three of those are new, all in `CPM_AssetMetadataTest.cpp`:
`Publish.Metadata.CreateDocumentKeysMatchLegacy` (the exact top-level and `entity_data` key sets for
a Scene and an Avatar), `Publish.Metadata.SendsTheLegacyTagSets`, and
`Publish.Metadata.PinsANewAssetPrivate`.

What no automation test can reach: the multipart body itself, and therefore that the server accepts
these tags and honours `visibility`. That is a real Publish, and it belongs in issue 11's list.
