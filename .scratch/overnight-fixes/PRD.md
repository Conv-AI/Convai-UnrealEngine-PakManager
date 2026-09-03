# Overnight fixes: the testing pass, plus what #263 left open

A batch of bugs found in a real editor testing session, the removal of the `ConvaiJobSystem`
dependency, and the three register items plus the verification debt that
[#263](https://github.com/ar-convai/ConvaiTask/issues/263) did not finish.

One brief because one agent runs the lot overnight. Most of these touch
`ConvaiPakEditorSubsystem.cpp` or the dependency copy, so they are **not** safely parallel — see
Ordering.

## Why these are grouped

Three of the reported bugs land on work that just went in on this branch. `71aa9b4` made a pick
declare Convai in the Modding Plugin's `.uplugin`, and `e55b7b2`/`24f86a8` added the dependency
gather — see [ADR-0011](../../docs/adr/0011-a-pak-holds-only-its-own-mount.md) and
[../dependency-gather/issues/01](../dependency-gather/issues/01-gather-dependencies-into-the-plugin.md).
The MetaHuman validation failure happened **with that fix present**, which makes it a defect in the
mechanism rather than a missing decision. Issue 01 has the leading hypothesis and the one-minute test
that confirms or kills it.

Read ADR-0011 before touching anything in 01, 02 or 03. The policy questions those bugs look like
they are asking — should SDK content be copied into the Pak, should engine content — are already
decided and written down. Reopening them by accident would undo a verified change.

## Ordering

Serialize. Do not fan out across these — the file overlap guarantees conflicts.

1. **01, 02, 03** — the dependency cluster. Blocks MetaHuman avatars outright, so it is worth the
   most of the night.
2. **08** — the publish metadata diff. Silent, and it hits every publish; #263 flagged it as the
   nastiest thing still open.
3. **04, 05, 06** — the small UI ones. Cheap, independent, good filler.
4. **07** — the JobSystem removal. **Last**, and only if 01-03 are committed and green: it rewrites
   the same subsystem those fixes touch, and its own issue says why it goes behind a test.
5. **09, 10, 11** — carried from #263. Take them only if the night has room.

## Ground rules

- Branch per issue or per cluster, off `fix/modding-plugin-convai-dependency` — **not** `main`. That
  branch carries `71aa9b4`/`e55b7b2`/`24f86a8`, which issues 01-03 depend on. Commit after each
  sub-task that builds and passes.
- **Nothing is pushed and no PR is opened.** Both wait on Anmol's word, every time.
- A C++ change is not done without `lastBuild.status == "succeeded"` from a real build.
- **Never trigger Live Coding in this project** — close the editor and do a full build. The DLL is
  locked while the editor runs, so building at all means closing it. Carried from
  [the dependency-gather handoff](../../docs/handoff/2026-09-03-dependency-gather.md), where it was
  an explicit correction.
- Editing docs by whole-paragraph replacement kept splicing duplicated text into `CONTEXT.md` and the
  legacy-parity register. Match the file's own line wrapping and read the diff afterwards.
- This run has a real editor, which #263 did not. That makes issue 11 possible for the first
  time — take the editor-only checks whenever a fix lands in something they cover. Budget for the
  close-build-relaunch cycle each one costs.
- Record what you could not verify. #263's honesty about read-only verification is why this brief
  can exist; keep it.

## Carried over from #263

Register items 4, 5, 6 are closed. **1, 2, 3 are open** and are issues 09, 08, 10 here. Parent
[#261](https://github.com/ar-convai/ConvaiTask/issues/261) stays open until they are finished, and
its board item stays In progress rather than Sync for the same reason.

One correction #263 asked about and never got an answer on: its comment labels the dead-helper
census "§B.3". That census is register item 4; §3 is the `assets/get` item, which is **not** done.
Fix that line on the issue while you are there.
