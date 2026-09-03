# Let the creator pick a texture asset as the thumbnail

Status: `ready-for-agent`

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
