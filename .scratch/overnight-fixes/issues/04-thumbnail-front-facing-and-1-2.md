# Thumbnail: capture from the front, at a 1:2 ratio

Status: `needs-triage` — built and checked in a real editor. The only thing left is Anmol
confirming that 1:2 (portrait) was the intent; changing it is one constant pair.

Two complaints about the captured thumbnail:

- It is taken from wherever the editor camera happens to sit. It should frame the avatar from the
  front.
- The image should be 1:2, not 16:9. The capture is hardcoded to 1920x1080 at
  [ConvaiPakManagerEditorUtils.cpp:186](../../../Source/ConvaiPakManager/Private/ConvaiPakManagerEditorUtils.cpp#L186).

**Confirm the ratio with Anmol before building it.** "1:2" as written is a tall portrait (e.g.
512x1024), which is an unusual card shape — 2:1 or a square are both likelier intents. Getting this
wrong means every thumbnail published in the meantime is the wrong shape, and the thumbnail is the
only thing a player sees before taking an Asset. If no answer comes overnight, build it as written
(1:2, width:height) and say so in the commit.

Front-on framing: position the capture camera from the avatar's forward vector and fit the bounds,
rather than reusing the current viewport transform. `SpawnAndSnapActorToView` in the same file is
the existing precedent for touching the viewport camera.

Keep the existing blank-capture guard — `ConvaiPakManager::Thumbnail::HasContent` — on the new path.
A front-facing capture of an unloaded scene is still a blank card.

## Done when

An avatar thumbnail is framed from the front at the agreed ratio, and the blankness check still
rejects an empty one. Note in the issue that this path has no automated coverage: it needs an RHI
and the suite runs `-NullRHI`, so it must be checked in a real editor.

## Built — 2026-09-04

Status of the two complaints: both done, and the ratio is still the one thing only Anmol can settle.

### The ratio: 1:2, built as written

`ConvaiPakManager::Thumbnail::WrittenWidth` / `WrittenHeight` (512x1024) in
[CPM_Thumbnail.h](../../../Source/ConvaiPakManager/Public/Thumbnail/CPM_Thumbnail.h) is now the one
place the shape is written down, and both paths read it: the Avatar render in `CaptureThumbnail` and
the Scene capture in `CPM_TakeViewportScreenshot` (which had the 1920x1080 literals). No answer came
overnight, so this is the issue's "build it as written" branch — **portrait, taller than wide**. If
2:1 or a square was meant, one constant pair changes and nothing else does.

The panel followed: the preview box is 96x192 and its brush the same, so the box stops implying a
16:9 card. `HandlePreviewThumbnail` already read the real size off the decoded PNG; only its
fallback, which was a hardcoded 1280x720, now reads the constants.

### Front-on: a transient thumbnail info, and the yaw found by looking

The engine renders a blueprint's thumbnail from `Blueprint->ThumbnailInfo` — a `USceneThumbnailInfo`
carrying OrbitPitch/Yaw/Zoom, which is *whatever the creator last dragged their Content Browser tile
to*, defaulting to -11.25/-157.5/0. `RenderBlueprintThumbnail` now swaps in a transient one at
pitch 0 / yaw -90 / zoom 0 for the duration of the render and puts the creator's back afterwards. It
is swapped, not written through, because `FBlueprintThumbnailScene::GetSceneThumbnailInfo` clamps
`OrbitZoom` in place — writing through would quietly edit the creator's asset.

The yaw was found by rendering, not derived: -180 gives a clean side profile, -90 puts the face at
the camera. Both renders are in this session's evidence (issue 11, M04).

### Rendered square, then cropped

The thumbnail projection is fixed at a 1:1 aspect — `FThumbnailPreviewScene::CreateView` builds it
as `FReversedZPerspectiveMatrix(HalfFOV, 1, 1, Near)` (ThumbnailHelpers.cpp:115) — so asking the
renderer for a 512x1024 target does not widen the view, it stretches the avatar to twice its height.
The render is therefore taken square at `Max(W, H)` and `CentreCrop` keeps the largest centred
rectangle of the asked-for shape — **both axes**, so a portrait pair takes the middle columns and a
landscape pair the middle rows. The bounds fit already leaves the sides empty, so the crop takes
empty space and no camera maths of our own is involved. The alternative — accept the stretch — was rejected on sight: a 2x-tall avatar is
exactly the kind of thing nothing downstream can fix.

`HasContent` still gates the write, after the render as before.

### The Scene path stays viewport-based

A level has no front, so `CPM_TakeViewportScreenshot` still captures whatever the editor camera is
looking at — only the size changed. Exercised at the new size: it wrote 512x1024 (issue 11, M04).

**Found while doing that, not fixed:** the Scene path writes **JPEG bytes into `Thumbnail_N.png`** —
`FImageUtils::ThumbnailCompressImageArray` is a JPEG encoder, and the captured file starts `FF D8 FF
E0 JFIF`. Nothing in this tool breaks on it (every read goes through `IImageWrapper`, which detects
the real format), and the upload never sends the file's bytes either — it re-encodes the loaded
texture as PNG. It predates this work, so it is left alone as scope, and now has its own file:
[16](16-the-scene-capture-writes-jpeg-bytes-into-a-png.md).

### Coverage

The render itself has **no automated coverage** and cannot get any here: it needs an RHI and the
suite runs `-NullRHI`. It is checked in a real editor instead — issue 11, M04. The crop it ends with
needs no RHI, so it is a function of its own — `ConvaiPakManager::Thumbnail::CentreCrop` — and
`ConvaiPakManager.Thumbnail.CropsToTheWrittenShape` asserts the rectangle at 1:2, at 2:1, at 1:1, on
a render the engine handed back smaller than asked for, and that whatever the shipped pair is set to,
the crop comes out at its ratio.

```
Build.bat Dev_CPM_58Editor Win64 Development -Project=.../Dev_CPM_58.uproject
  Result: Succeeded   (no new warnings; the pre-existing "Plugin 'ConvAI' does not list plugin
                       'ConvaiHTTP' as a dependency" warning is unrelated and predates this work)

UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
  59 "Test Completed", 59 Result={Success}, 0 anything else.
  55 baseline + 1 from this wave (Thumbnail.ReadsATextureSource) + 3 from the metadata wave
  running in the same tree.
```

## Fixed after review — 2026-09-04

The crop only cut columns. `Clamp(RenderedHeight * W / H, 1, RenderedWidth)` is right for a portrait
pair and clamps a landscape one to the full square, so a 2:1 pair — the flip this issue names as a
likelier intent — would have rendered 1024x1024. The promise above ("one constant pair changes and
nothing else does") was therefore false for the path it was written about.

It now crops both axes, in `CentreCrop(FIntPoint Rendered, FIntPoint Shape)`, with
`CropsToTheWrittenShape` covering the pairs. On the shipped 1:2 pair the new crop returns the same
rectangle the old code did — `(256, 0)–(768, 1024)` of a 1024 square — so M04's render evidence still
stands unaltered.

The suite's `WrittenHeight == 2 * WrittenWidth` assertion went with it. It was the second thing a
flip would have had to change, which is exactly what this issue promises does not happen; the new
test checks the ratio survives the crop instead of checking which ratio it is.

```
Build.bat Dev_CPM_58Editor Win64 Development -Project=.../Dev_CPM_58.uproject
  Result: Succeeded   (CPM_Thumbnail.cpp, CPM_ThumbnailTest.cpp and SCPM_AssetDetailPanel.cpp
                       recompiled and relinked; no new warnings.)

UnrealEditor-Cmd.exe Dev_CPM_58.uproject -ExecCmds="Automation RunTests ConvaiPakManager;Quit" -unattended -nullrhi
  66 "Test Completed", 66 Result={Success}, 0 anything else.
```
