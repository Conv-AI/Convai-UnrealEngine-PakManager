# Clear the verification debt that needed a real editor

Status: `needs-triage` — the register is closed: every bullet below ends in a check with
evidence or a stated reason it was left. The two left are the compatibility banner and the
once-per-session Ambiguous warning, neither of which this project can stage. The cook taken for
gaps 27/28 turned up [issue 15](15-a-pak-carries-more-than-its-own-mount.md).

Carried from [#263](https://github.com/ar-convai/ConvaiTask/issues/263). Nothing in that session ran
in a real editor — the suite runs `-NullRHI` and `unreal-mcp` was `ConnectionRefused` all session, so
the work below is read-verified only.

**This run has an editor.** Take these as the fixes they cover land, rather than saving them for the
end.

- Gap 23's Avatar thumbnail render has never executed anywhere. Issues 04 and 05 both touch that
  path, so it gets exercised either way.
- No visual pass on any new UI: the compatibility banner, the dependency window, the setup-notes
  line, the Copy-into-plugin button. Issues 01, 02 and 06 cover three of those four.
- Gaps 27/28, the in-editor half: a locked Pak's refusal reaching the panel, a stale Pak being gone
  before UAT runs, Live Coding restored after a real cook.
- The once-per-session Ambiguous warning across tab activations. `Dev_CPM_58` has no flat layout, so
  a headless run never enters that branch — it needs a project that does.

#263 also logged seven small follow-ups at sign-off: a dead `#include`, a one-line banner seed, a
JPEG test fixture and others. All optional, none blocking. Take them only if the night has room.

## Done when

Each item above is either checked in a real editor with the result recorded, or explicitly left with
the reason. Do not mark a check done on a read of the code — that is exactly the debt this issue
exists to clear.

## Wave 1 (issues 01, 02, 03) — 2026-09-04

Editor: `UnrealEditor.exe Dev_CPM_58.uproject`, launched twice (PIDs 26920 and 64484) against the DLL
built below. Driven through the Pak Manager's own Commands and, where a button was the point, through
the `SlateInspectorToolset` click tools. Chunk 10 is Avatar-typed with entry point
`/JBILN5CDNI4TRYELD6CS/BP_Hana`; it was restored to exactly that afterwards and the two test assets
were deleted.

### Build and suite

```
Build.bat Dev_CPM_58Editor Win64 Development -Project=.../Dev_CPM_58.uproject
  Result: Succeeded   (20 actions, no new warnings; the four C4996 ForEachObjectWithPackage /
                       GetObjectsWithOuter deprecations in CPM_DependencyCopyAPI.cpp pre-date this work)

UnrealEditor-Cmd.exe Dev_CPM_58.uproject -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
  55 "Test Completed" lines, 55 Result={Success}, 0 anything else — the 55 baseline, unmoved.
```

No new automation tests: this wave's changes are a message reaching the panel, five log lines, and a
notification, all of which need an Asset Registry with packages on disk and a Slate panel. They are
the M01–M03 checks below instead.

### M01 — a failed declaration reaches the creator. **Passes.**

`Plugins/JBILN5CDNI4TRYELD6CS/JBILN5CDNI4TRYELD6CS.uplugin` had its `"Plugins"` block removed and was
set read-only, then the editor was restarted (the descriptor is read into memory at startup, so
editing it under a running editor proves nothing — `EnsureConvaiDependency` only writes when the
declaration is actually missing). Picking `BP_Hana` returned, through `OutSetupNotes`:

```
Could not declare Convai as a dependency of the JBILN5CDNI4TRYELD6CS plugin
(.../JBILN5CDNI4TRYELD6CS.uplugin could not be written: Failed to write plugin descriptor file
'.../JBILN5CDNI4TRYELD6CS.uplugin'. Perhaps the file is Read-Only?), so asset validation will report
/JBILN5CDNI4TRYELD6CS/BP_Hana as illegally referencing Convai's content.
```

The panel drew it under the Selected asset row: `Saved/VibeUE/Captures/m01-setup-notes-row.png`. The
same sentence is in the log at Warning (`Dev_CPM_58.log:2948`), which is what the publish path still
relies on. Descriptor and read-only attribute restored afterwards; `git status` on the project clean.

**Noted, not fixed:** the row is the green setup-notes colour, because that is the row `OutSetupNotes`
feeds. Colouring it as a warning needs either a second out-param on the BlueprintCallable
`SetEntryPoint` or string-matching the note in the panel, and neither is worth it for a sentence that
opens "Could not declare Convai". Left as a follow-up if a creator reads past it.

### M02 — Copy into plugin. **Passes.**

A fresh `/Game/CPM_CopyTest/BP_CopyTest` (a bare Actor blueprint) was picked and refused as outside.
The panel drew the refusal in its error row — screenshot
`Saved/VibeUE/Captures/capture-window-20260904-010235.png` — and clicking **Use selected asset** with
that asset selected made the **Copy into plugin...** button appear beside it, read off the Slate
accessibility tree rather than a picture (`button "Copy into plugin..." [ref=b82]`, absent before the
refusal). The copy then ran through `RelocateEntryPointIntoPlugin`:

```
CPM_DependencyCopyAPI: Copy complete. Copied: 1, Skipped: 0, Failed: 0
ConvaiPakManagerLog: Copied 1 packages into /JBILN5CDNI4TRYELD6CS/ (0 skipped);
  /JBILN5CDNI4TRYELD6CS/CPM_CopyTest/BP_CopyTest is this chunk's entry point now.
ConvaiPakManagerLog: Prepared Avatar blueprint 'BP_CopyTest': added BP_ConvaiChatbotComponent, added
  ConvaiFaceSyncComponent.
```

`Plugins/JBILN5CDNI4TRYELD6CS/Content/CPM_CopyTest/BP_CopyTest.uasset` existed on disk, and
`Draft_10.json` recorded `"blueprint_class_path": "/JBILN5CDNI4TRYELD6CS/CPM_CopyTest/BP_CopyTest"`.
The panel's Selected asset row showed the new path: `capture-window-20260904-010737.png`.

**Not driven end-to-end from the button.** Clicking **Copy into plugin...** does fire the handler, but
its `FMessageDialog` confirm auto-answers **No** while an MCP Python call is in flight
(`GIsRunningUnattendedScript`), so the modal Yes cannot be clicked from this harness — the copy was
run through the same Command the button calls, one line below the dialog. The panel row was read
after reopening the tab: the row updates from `Asset->LoadFrom`, which the panel does not poll, so a
tab left open across a Command-driven change keeps showing the old path. That is worth knowing when
reading a screenshot, and it is not what the report described.

The closure was one package. Closure copying at scale is covered by M03's evidence (580 then 88
packages copied in), so no second fixture was built for it.

### M03 — ADR-0011's "a re-pick offers nothing". **Fails, diagnosed, not fixed.**

Picking `BP_Hana` again still offers a gather. The Dependencies window, screenshot
`Saved/VibeUE/Captures/capture-window-20260904-010816.png`:

> 737 packages are reachable from BP_Hana. 157 of them are outside /JBILN5CDNI4TRYELD6CS/ and will
> NOT be in the Pak - pick the asset again to be offered a copy of them.

157 = 58 `/ControlRig/`, 69 `/Game/`, 24 `/Engine/`, 6 `/Niagara/`, all reachable through **ten**
edges out of four in-plugin packages. The copies exist; the references were not rewritten. Full
diagnosis in [issue 02](02-copy-into-plugin-copies-nothing.md); the tenth edge is
[issue 12](12-controlrig-references-a-mount-this-project-lacks.md).

**Package now + UnrealPak -List: not run.** A package taken now would bake in a closure that is known
to be 157 packages short, which proves nothing about the boundary this check exists to measure — it
is worth doing after the fixup gap closes. The existing pak was listed instead, which re-runs the
measurement ADR-0011 rests on (it was built at 22:07, before the gathers):

```
UnrealPak.exe PackagedApp/Windows/Dev_CPM_58/Content/Paks/pakchunk10-Windows.pak -List
  mount point "../../../Dev_CPM_58/Plugins/JBILN5CDNI4TRYELD6CS/Content/"
  "Map.uexp"    540 bytes      "Map.umap"     699 bytes
  "NewMap.uexp" 2265 bytes     "NewMap.umap"  2084 bytes
  4 files (5588 bytes)
```

Still exactly the plugin's own two maps and nothing they reference. ADR-0011's premise holds.

### Items from the list above that this wave did not touch

- The compatibility banner and the Asset-type chip (issue 06) were not checked — nothing in this
  wave changed them.
- Gaps 27/28 (locked Pak, stale Pak, Live Coding after a cook) need a real Publish, which is
  issue 07's wave.
- The Avatar thumbnail render (issues 04/05) was not exercised.
- The once-per-session Ambiguous warning still needs a flat-layout project; `Dev_CPM_58` is not one.

## Wave 2 (issues 04, 05, 06) — 2026-09-04

Editor: `UnrealEditor.exe Dev_CPM_58.uproject`, launched twice against the DLL built below (PIDs
61048 and 48316 — the first was closed to rebuild once the yaw needed changing). Driven through the
Pak Manager's own Commands and, where a button was the point, through the `SlateInspectorToolset`
click tools. Chunk 10 is the Avatar chunk with entry point `/JBILN5CDNI4TRYELD6CS/BP_Hana`; it ends
this wave with a front-on 512x1024 avatar thumbnail and no scratch files beside it.

### Build and suite

```
Build.bat Dev_CPM_58Editor Win64 Development -Project=.../Dev_CPM_58.uproject
  Result: Succeeded   (twice: once for the wave, once to change the orbit yaw. No new warnings.)

UnrealEditor-Cmd.exe Dev_CPM_58.uproject -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
  59 "Test Completed", 59 Result={Success}, 0 anything else.
  55 baseline + Thumbnail.ReadsATextureSource (this wave) + 3 from the metadata wave in the tree.
```

### M04 — the Avatar render, front-on and 1:2. **Passes.**

`CaptureThumbnail(10)` wrote `ConvaiEssentials/ChunkId_10/Thumbnail_10.png` at **512x1024**, and the
image is the avatar seen from the front, correctly proportioned. The panel draws it in the 1:2
preview box: `Saved/VibeUE/Captures/m04-panel-front-on-1-2.png`.

The yaw was found here rather than reasoned about. At -180 the render is a clean **side profile**; at
-90 the face is at the camera. Two renders, two looks, and -90 is what is in the code.

Two things the render must not do, both checked afterwards in the same session:

- `BP_Hana.ThumbnailInfo` is still the creator's own `SceneThumbnailInfo_0` outered to `BP_Hana` —
  the transient one is put back.
- `EditorLoadingAndSavingUtils.get_dirty_content_packages()` returned `[]` after four renders. The
  render does not dirty the asset it renders.

**Scene half:** `UConvaiPakManagerEditorUtils.CPM_TakeViewportScreenshot` was called directly (it is
BlueprintCallable, so no Asset-type surgery was needed) and wrote **512x1024** —
`Saved/VibeUE/Captures/m04-scene-viewport.png`. The image is black, because the open level is an
empty grid; that is the case `FileHasContent` exists to reject, and the real capture path would have
refused it. So the *size* is verified and the *content* is not.

That capture also turned up the JPEG-in-a-`.png` finding recorded in
[issue 04](04-thumbnail-front-facing-and-1-2.md). Pre-existing, not fixed here.

### M05 — picking a texture. **Passes, with one thing not seen.**

Content Browser 1 opened from the Window menu (the drawer keeps no selection the subsystem can read),
then `sync_browser_to_objects` to select each asset and the **Use selected texture** button clicked
through Slate:

- `.../Clothing/DefaultMaps/color_spectrum` (512x512) — **written**, a 68018-byte PNG, and the
  panel's preview switched to the spectrum (`capture-window-20260904-021545.png`).
- `/JBILN5CDNI4TRYELD6CS/BP_Hana`, a Blueprint — **refused**; nothing written, thumbnail unchanged.
- `.../EngineResources/Black` (32x32, all black) — **refused**; nothing written.
- `.../Simplified/T_Flat_BentNormalAO` (8x8) — **written**, an 80-byte PNG. A flat normal map is
  blue, not blank, so it clears the content gate honestly.

**Not seen: the refusal text.** The notification toast is not reachable from here — it is not in the
Slate accessibility tree, and the editor window sits behind other windows on this desktop so a screen
grab does not catch it either (`m05-refusal-not-a-texture.png` is the panel at the moment of a
refusal, with no toast in it). The refusals themselves are confirmed — nothing was written in either
case — and the "no source" wording is asserted by the automation test. The class-naming wording is a
code read.

Worth knowing for the next session: **Python cannot read `OutWhy`**. A `UFUNCTION` that returns
`bool` and has an `FString&` out param comes back to Python as the out param on success and plain
`None` on failure, so `set_thumbnail_from_texture` and `capture_thumbnail` say nothing about *why*
they refused. Anything that needs the message needs the panel.

A real source-less `UTexture2D` **asset** was not built: the case is covered by
`Thumbnail.ReadsATextureSource` with `UTexture2D::CreateTransient`, and a render target asset would
be refused a step earlier as not being a `UTexture2D` at all.

### M06 — the Asset type chip. **Label passes, tooltip not seen.**

The chip reads **Avatar**, with nothing in brackets after it
(`Saved/VibeUE/Captures/m04-panel-front-on-1-2.png`, and the same in the two earlier captures). The
tooltip could not be raised: `SlateInspectorToolset.Hover` sets the hover state but does not start
Slate's tooltip timer, and `SetCursorPos` onto the chip did not either with the editor unfocused.

### The rest of #263's visual pass

- **The compatibility banner** was not captured: nothing in this project is outdated, so the banner
  does not draw. It needs a version mismatch staged first, which is issue 07's wave territory.
- **The dependency window** and **the setup-notes line** were captured in wave 1 (M01/M03 above);
  nothing in this wave changed either, so they were not re-shot.
- **The Copy-into-plugin button** — wave 1, M02.

### Left as it was found

Chunk 10's thumbnail is a fresh front-on Avatar render. Its previous contents were overwritten by
the first capture of this wave and are gone (`ConvaiEssentials/` is not under source control); the
file that stands now is what the tool would produce for this chunk anyway. The Content Browser has a
docked "Content Browser 1" tab it did not have before.

## Wave 3 (issue 07, the runner swap) — 2026-09-04

Editor: `UnrealEditor.exe Dev_CPM_58.uproject`, PID 64416, against the DLL built below. Driven
entirely through the Pak Manager's own Commands from Python — no button in this wave was new, so
nothing needed the Slate click tools.

### Build and suite

```
Build.bat Dev_CPM_58Editor Win64 Development -Project=.../Dev_CPM_58.uproject
  Result: Succeeded   (no compiler warnings; CPM_PublishRunner.cpp, CPM_PublishJobs.cpp and
                       CPM_PublishRunnerTest.cpp were touched and rebuilt to be sure of that)

UnrealEditor-Cmd.exe Dev_CPM_58.uproject -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
  65 "Test Completed", 65 Result={Success}, 0 anything else.
  59 from waves 1 and 2 + the 6 new Publish.Runner cases.
```

The step-1 run against the Job System is in [issue 07](07-remove-the-convaijobsystem-dependency.md),
with its three reds and why each one was expected to be red.

### M07a — a real Publish, end to end on the runner. **Passes.**

`PublishWithOptions(10, {Windows, override, reuse existing paks})`. Windows only and reuse-on so the
cook is skipped: chunk 10's Pak is 8.7 KB and already on disk. `bUploadRawProjectArchive` is False in
this project's `DefaultGame.ini`, so the queue is four Jobs, not five.

Status read from `GetChunkStatus(10)` between calls:

```
PACKAGING_BEGIN  0.00  "Reading publish policy"   idx -1  planned []
CREATE_BEGIN     0.25  "Creating asset"           idx  1  planned [Packaging, Creating asset, Uploading, Recording asset]
UPLOAD_PAK_BEGIN 0.75  "Uploading ue-5.8-Windows" idx  2
UPLOAD_PAK_SUCCESS 1.00 ""                        idx -1  planned []
```

That is the runner's own arithmetic showing through: `(index + job progress) / 4`. PlannedSteps are
the Jobs' `Name()` values, filled before the run starts and cleared when it finishes.

The Asset record landed: `ConvaiEssentials/ChunkId_10/Env_api.convai.com_29e2cb96/` gained
`CreateAssetData_10.json` with `"asset_id": "fcd9f8ee-48b1-4328-af0c-560c90af1d14"` and
`PakMetaData_10.json`. `Dev_CPM_58.log:3023` shows the reuse warning, so the Pak that went up is the
one that was already there.

### M07b — cancel mid-run resolves as Cancelled. **Passes, at the create step rather than the upload.**

The brief asked for a cancel *mid-upload*. Not reachable here: the Pak is 8.7 KB and the PUT finishes
inside one round trip — a first attempt cancelled from the next MCP call found the run already
finished. Taken during the create/update step instead, from a `register_slate_post_tick_callback`
that watched the status and called `CancelPublish(10)` the moment it read `CREATE_BEGIN`. Same runner
path either way: `Cancel` → the running Job reports Cancelled → the run finishes Cancelled.

```
PACKAGING_BEGIN     idx -1  "Reading publish policy"
CREATE_BEGIN        idx  1  "Updating asset"
CANCEL CALLED -> True
PUBLISH_CANCELLED   idx -1  progress 0.25
```

`PUBLISH_CANCELLED`, never `UPLOAD_PAK_FAILED`, and the progress it stopped at is preserved.

### Left as it was found, with one thing that could not be

Chunk 10 had no Asset before this wave, so the Publish created one. It was deleted afterwards
(`DeleteAsset(10, "", false)`) and the environment folder is empty again.

**That delete also took `Draft_10.json` and `Thumbnail_10.png` with it** — the documented behaviour
of the last Asset record going (`Chunk.Cleanup.TheLastDeleteTakesTheDraftAndThumbnail`), and not
something the wave anticipated. Both were rebuilt through the tool's own Commands:

- `SetAssetName(10, "Dev_CPM_58")` and `SetEntryPoint(10, "/JBILN5CDNI4TRYELD6CS/BP_Hana")` —
  `Draft_10.json` reads back with the same `asset_name`, `root_path`, `blueprint_class` and
  `blueprint_class_path` the pre-delete `PakMetaData_10.json` carried.
- `CaptureThumbnail(10)` — a 512x1024 PNG, the same front-on Avatar render wave 2 left there.
- `get_dirty_content_packages()` returned `[]` afterwards, so nothing in the project was left dirty.

**`asset_description` could not be recovered.** It was not in the part of the old `PakMetaData_10.json`
that was read before the delete, and the Draft now has no such field. If chunk 10 had a description
typed into it, it is gone and needs re-typing.

### Items from the list above that this wave did not touch

- Gaps 27/28's remaining half — a locked Pak's refusal reaching the panel, and Live Coding restored
  after a real cook — still need a run that actually cooks. This wave deliberately reused the Pak on
  disk, so `UCPM_PackagePaksJob` never parked Live Coding and never met a locked file.
- The compatibility banner still needs a version mismatch staged. Nothing in this wave changed it.
- The once-per-session Ambiguous warning still needs a flat-layout project.

## Wave 4 (this issue's own remainder) — 2026-09-04

Editor: `UnrealEditor.exe Dev_CPM_58.uproject`, PID 41288, against the DLL wave 3 built — 02:52:52,
newer than every file under `Source/`, so it is the binary the night ends on. The panel was opened
from **Tools ▸ Pak Manager** through the `SlateInspectorToolset` click tools; both packaging runs
went through `PackageWithOptions(10, {Windows, override, reuse off})`, which is exactly what
**Package now** calls (`SCPM_PakManagerPanel.cpp:596`).

### Build and suite

```
Build.bat Dev_CPM_58Editor Win64 Development -Project=.../Dev_CPM_58.uproject
  Result: Succeeded   (editor closed, "Target is up to date", 0 actions. One pre-existing warning:
                       plugin 'ConvAI' does not list 'ConvaiHTTP' as a dependency.)

UnrealEditor-Cmd.exe Dev_CPM_58.uproject -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
  65 "Test Completed", 65 Result={Success}, 0 anything else — wave 3's count, unmoved.
```

Nothing in this wave is a code change, so the 0 actions are the point rather than a skipped step: at
03:13 the DLL every editor check above ran against was the build of the source as it then stood.

### M08a — a locked Pak's refusal reaches the panel. **Passes.**

`pakchunk10-Windows.pak` (8894 bytes, built 2026-09-03 22:07:44) was held open by a second process
with `[System.IO.File]::Open(..., FileShare::Read)`, which leaves the file readable and denies the
`DeleteFile` the Job needs. **Package now** refused, and the panel drew the reason in red across its
action bar — `Saved/VibeUE/Captures/m08-locked-pak-refusal.png`:

> could not remove the previous Windows Pak at
> ../../../../../UEProjects/UE5.8/Dev_CPM_58/PackagedApp/Windows/Dev_CPM_58/Content/Paks/pakchunk10-Windows.pak
> before packaging; it may be open in another program

`GetChunkStatus(10)` read `PACKAGING_FAILED` carrying that same sentence, and the Pak was still on
disk at its original size and mtime. This is the row that outlives the toast
(`SCPM_PakManagerPanel.cpp:952`); the toast itself fired while an MCP call was in flight and was
gone before the screenshot, exactly as in wave 2.

### M08b — the stale Pak goes before UAT runs, and the mtime moves. **Passes.**

Lock released, **Package now** again. Caught three seconds into the run:

```
03:05:28   PACKAGING_BEGIN   step "Packaging Windows"   PAK EXISTS: False
```

That step name only reaches the panel after `IFileManager::Delete` has returned true and
`CPM_PackageProject` has been called, so a missing Pak at that moment is the delete landing *before*
UAT rather than after it. The panel mid-cook, with the previous Pak already gone:
`Saved/VibeUE/Captures/m08-packaging-stale-pak-gone.png`.

The run finished `PACKAGING_SUCCESS` at 03:09 and the mtime moved as the check asked:

```
before   8894 bytes        2026-09-03 22:07:44
after    581790404 bytes   2026-09-04 03:08:52
```

Panel back to idle with the error row cleared:
`Saved/VibeUE/Captures/m08-after-cook-panel-clear.png`.

That size is not a typo. See M08d.

### M08c — Live Coding restored after the cook. **Left, with the reason.**

`LiveCodingSettings.bEnabled` read `False` off the running editor before and after the cook
(`unreal.load_class(None, "/Script/LiveCoding.LiveCodingSettings")` — `unreal.LiveCodingSettings`
does not exist), no `LiveCodingConsole.exe` was running at either point, and the session log carries
no `Starting LiveCoding` line.

That settings flag is a **proxy** for the session flag the Job reads, not that flag itself.
`ILiveCodingModule` is a plain `IModuleInterface` carrying no `UCLASS` (`ILiveCodingModule.h:31,54`),
so `IsEnabledForSession()` is unreachable from Python and has to be read from the Output Log or from
C++; `ULiveCodingSettings` is a `UCLASS`, which is why it is the one that got read. The proxy holds
here: `bEnabled` is what `StartupModule` tests before starting a session
(`LiveCodingModule.cpp:500-501`), and `IsEnabledForSession()` is true only in
`EState::RunningAndEnabled` (:601-607), which nothing in the log reaches.

So `IsEnabledForSession()` is false where `UCPM_PackagePaksJob::Execute` reads it,
`bParkedLiveCoding` stays false, and `RestoreLiveCoding` returns on its first line. The park/restore
pair was never entered, so this cook proves nothing about it either way — the honest result is
*not exercised*, not *passes*.

Entering that branch means enabling Live Coding for the session, which starts a Live Coding session
in this project. That is the one thing the brief forbids outright, so it is left for a session that
is allowed to take it. Everything up to the branch is as verified as Python allows: the setting that
gates the session was readable, and it read false either side of the cook.

### M08d — the fresh Pak carries 597 files from outside the plugin's mount

Listing what this cook produced re-runs the measurement ADR-0011 rests on, and it does not come back
the way it did on 2026-09-03:

```
UnrealPak.exe PackagedApp/Windows/Dev_CPM_58/Content/Paks/pakchunk10-Windows.pak -List
  mount point "../../../"          1921 files (580314739 bytes)
    1324  Dev_CPM_58/Plugins/JBILN5CDNI4TRYELD6CS/Content/     the plugin's own mount
     421  Dev_CPM_58/Plugins/Convai/Content/                   the Convai SDK
     166  Engine/Plugins/Animation/ControlRig/Content/         engine content
      10  Dev_CPM_58/Content/MetaHumans/                       /Game/
```

`ControlRig_RoundedSquare_solid` is in there twice — once as the gather's copy under the plugin, once
as the engine original — and 80 `.uasset` names appear under two mounts like that. Written up as
[issue 15](15-a-pak-carries-more-than-its-own-mount.md); it is a measurement, and what to do about it
is a decision.

### Left as it was found, with one thing that could not be

Chunk 10 ends this wave `PACKAGING_SUCCESS` with nothing uploaded — a package-only run creates no
Asset, so `Draft_10.json`, `Thumbnail_10.png` and the environment folder are untouched.

**The 8.9 KB Pak from 22:07 is gone**, replaced by the 581 MB one this cook built. Deleting it is the
behaviour under test, and `PackagedApp/` is not under source control; the next package rebuilds
whatever the label says at the time. The lock-holder process and its `Saved/release-the-pak.txt`
sentinel were removed afterwards.

## The register, closed

Every bullet from the top of this issue, with where it ended up.

- **Gap 23's Avatar thumbnail render** — **done**, wave 2 M04: a 512x1024 front-on render at
  `ConvaiEssentials/ChunkId_10/Thumbnail_10.png`, drawn in the panel's 1:2 preview box
  (`Saved/VibeUE/Captures/m04-panel-front-on-1-2.png`), with the creator's `ThumbnailInfo` restored
  and nothing left dirty.
- **The visual pass on the new UI** — **done across waves 1, 2 and 4**, except one piece:
  - the setup-notes line — wave 1 M01, `m01-setup-notes-row.png`;
  - the dependency window — wave 1 M03, `capture-window-20260904-010816.png`;
  - the Copy-into-plugin button — wave 1 M02, read off the Slate tree and
    `capture-window-20260904-010737.png`;
  - the Asset-type chip — wave 2 M06, reads "Avatar"; its tooltip could not be raised;
  - **the compatibility banner — left.** Nothing in this project is outdated, so the banner never
    draws. It needs a version mismatch staged first, which no issue in this brief had a reason to do.
- **Gaps 27/28, the in-editor half** — **done**, wave 4: the locked-Pak refusal reaches the panel
  (M08a) and the stale Pak is gone before UAT runs (M08b). The Live Coding half is **left** with its
  reason (M08c).
- **The once-per-session Ambiguous warning** — **left. `Dev_CPM_58` has no flat layout; needs a
  project that does.** `ConvaiEssentials/` here is already `ChunkId_10/…`, so
  `ReconcileStateLayout` never reaches the `Ambiguous` branch that raises the warning and the banner.

#263's seven small follow-ups were not taken. The night went on the eleven issues instead, and they
were flagged optional and non-blocking at sign-off.
