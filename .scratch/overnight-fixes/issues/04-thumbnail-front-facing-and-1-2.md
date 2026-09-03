# Thumbnail: capture from the front, at a 1:2 ratio

Status: `ready-for-agent`

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
