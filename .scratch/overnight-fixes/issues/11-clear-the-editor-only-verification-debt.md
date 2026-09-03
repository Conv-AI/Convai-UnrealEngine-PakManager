# Clear the verification debt that needed a real editor

Status: `ready-for-agent`

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
