# Let the creator pick a texture asset as the thumbnail

Status: `needs-triage` — built, tested and driven from the button in a real editor.

Today a thumbnail is either captured from the viewport or imported from a file on disk. A creator
who already has the art as a `UTexture2D` in the project has to export it first.

Most of this exists.
[`ConvaiPakManager::Thumbnail::ImportImageFile`](../../../Source/ConvaiPakManager/Public/Thumbnail/CPM_Thumbnail.h)
already re-encodes an arbitrary image to PNG at the destination and refuses blank ones, and
`WritePng` takes raw `FColor` pixels. What is missing is the picker and the texture-to-pixels step.

Laziest route: an `SObjectPropertyEntryBox` filtered to `UTexture2D` in the detail panel next to the
existing thumbnail controls, then read the texture's source mip and hand the pixels to `WritePng`.
Reuse `HasContent` for the same blank check the other two paths get — do not let a third entry point
be the one that skips it.

Watch for textures with no `Source` (transient, or built with source data stripped) and say so
plainly rather than writing a black PNG.

## Done when

A `UTexture2D` picked in the panel becomes the chunk's thumbnail, blank and source-less textures are
refused with a message naming which, and the existing capture and file-import paths still work.

## Built — 2026-09-04

### The read

`ConvaiPakManager::Thumbnail::ReadTextureSource` reads the texture's **authored source** —
`FTextureSource::GetMipImage(0)`, then `FImage::ChangeFormat(BGRA8, sRGB)`, then a memcpy of
`AsBGRA8()`. It refuses a null texture, and refuses one whose `Source` is invalid with a message
containing "no source" (a render target, or an import built with source data stripped).

`UCPM_UtilityLibrary::Texture2DToPixels` was deliberately **not** reused, even though it exists and
returns exactly this shape: it writes `SRGB` and `CompressionSettings` on the creator's texture and
re-cooks it to read the platform data back. Making a thumbnail is not a reason to edit somebody's
art asset.

`ImageCore` joined `PrivateDependencyModuleNames` — `FImage::ChangeFormat` is the only symbol needed
from it, and the link failed without it.

### The Command and the button

`UConvaiPakEditorSubsystem::SetThumbnailFromTexture(ChunkId, PackageName, OutWhy)` loads the
package's first asset through the Asset Registry, refuses a non-`UTexture2D` naming the class it
found, reads the source, refuses a blank one, and writes the PNG at `GetThumbnailPath`. All three
paths that make a thumbnail now pass through the same `HasContent` gate.

The panel gets a third button, **Use selected texture**, under *Choose image...*; its handler reads
the Content Browser selection the same way *Use selected asset* does, then reloads the asset and
forces the preview brush to re-read from disk.

An `SObjectPropertyEntryBox` filtered to `UTexture2D` — the route the issue suggested — was
considered and skipped: the selection button matches the pattern already in this panel, adds no
widget type, and needs no extra state to hold the picked object.

### Coverage

`ConvaiPakManager.Thumbnail.ReadsATextureSource` builds transient textures with
`Source.Init(W, H, 1, 1, TSF_BGRA8, ...)` and asserts the round trip (width, height, pixel values),
that a blank source reads but fails `HasContent`, and that `UTexture2D::CreateTransient` — platform
data and no source — is refused with "no source".

The Command's own wording ("… is a Blueprint, not a texture", "… is blank") is **not** in the suite:
reaching the Command needs a texture the Asset Registry can find, which a `-NullRHI` automation run
has no way to make. Those are M05 in issue 11.

```
Build.bat ...  Result: Succeeded (no new warnings)
Automation RunTests ConvaiPakManager  ->  59 Test Completed, 59 Result={Success}
  (55 baseline + this wave's 1 + 3 from the metadata wave in the same tree)
```

## Fixed after review — 2026-09-04

A picked texture keeps its **own** shape — so does a file picked with *Choose image...* — and the
panel was drawing it stretched. The preview brush was built as
`FSlateDynamicImageBrush(Path, FVector2D(96, 192))`, the box's shape rather than the file's, and an
`SImage` fills its box, so a 512x512 texture drew at twice its height: the exact distortion the
square-render-and-crop in [04](04-thumbnail-front-facing-and-1-2.md) exists to avoid. Both halves
were wrong, so both are fixed — the brush is sized from the decoded file, and the `SImage` sits in an
`SScaleBox(ScaleToFit)` so anything of another shape is fitted into the box instead of filling it.

The preview *window* was reading the size the same way twice: it loaded the file again and decoded it
with a PNG-only wrapper, falling back to the written constants when that failed — which it does for
the Scene path's JPEG-in-a-`.png` ([16](16-the-scene-capture-writes-jpeg-bytes-into-a-png.md)). It
now takes the size off the brush, and that decode is gone.

The Command's refusal wording ("… is a Blueprint, not a texture", "… is blank") is still asserted
**nowhere**: the automation run cannot register a package, and the editor toast was never actually
seen either (issue 11, M05). It is three `Printf` strings, read rather than exercised — said plainly
here rather than logged as an editor check that did not happen.

The panel change itself has no coverage: Slate, editor-only, and no live editor was started for it
(two automation runs from sibling waves held the tree). It is a read, not a check.
