# A freshly cooked Pak carries 597 files from outside the plugin's mount

Status: `needs-triage` — a measurement, not a fix. It contradicts the one measurement
[ADR-0011](../../../docs/adr/0011-a-pak-holds-only-its-own-mount.md) rests on, and what to do about
it is a decision.

Found while taking gaps 27/28 in
[issue 11](11-clear-the-editor-only-verification-debt.md) (wave 4, M08d). Nothing was being tested
here — the cook was run to prove a stale Pak gets deleted before UAT, and listing what came out of it
happened to re-run ADR-0011's measurement on a Pak built by the current binary.

## What was measured

```
UnrealPak.exe PackagedApp/Windows/Dev_CPM_58/Content/Paks/pakchunk10-Windows.pak -List
  mount point "../../../"          1921 files (580314739 bytes)
    1324  Dev_CPM_58/Plugins/JBILN5CDNI4TRYELD6CS/Content/     the plugin's own mount
     421  Dev_CPM_58/Plugins/Convai/Content/                   the Convai SDK
     166  Engine/Plugins/Animation/ControlRig/Content/         engine content
      10  Dev_CPM_58/Content/MetaHumans/                       /Game/
```

Cooked 2026-09-04 03:08 from chunk 10, Entry Point `/JBILN5CDNI4TRYELD6CS/BP_Hana`, through
`PackageWithOptions(10, {Windows, override, reuse off})`.

The mount point itself is the first tell: a Pak whose entries all sit under one mount lists that
mount, which is why the 2026-09-03 listing read
`"../../../Dev_CPM_58/Plugins/JBILN5CDNI4TRYELD6CS/Content/"`. This one collapses to `"../../../"`
because its entries span four mounts.

## Why it matters

- **ADR-0011's premise does not reproduce on this Pak.** "A Pak holds only the label's own mount" was
  measured on the 22:07 Pak of 2026-09-03 — 4 files, the plugin's two maps, `BP_Hana` nowhere in it.
  With the Entry Point actually in the label, the recursive rule reaches across mounts.
- **The gather now duplicates rather than rescues.** `ControlRig_RoundedSquare_solid` is in the Pak
  twice: once as the gather's copy under `/JBILN5CDNI4TRYELD6CS/ControlRig/`, once as the engine
  original. 80 `.uasset` names appear under two mounts like that.
- **The Convai SDK is in the Pak.** ADR-0011 leaves SDK content out of both the inside and outside
  lists deliberately, on the grounds that every Convai product already ships it. 421 of its files are
  in this Pak anyway.
- **8.9 KB became 581 MB.** Whatever the right boundary is, a creator's Pak crossing that line
  silently is the thing the whole feature exists to prevent.

## What is not known

Which of the two measurements — 2026-09-03 22:07 and 2026-09-04 03:08 — is the odd one out. The
Pak this contradicts was built before the Entry Point joined the label, so the two are not the same
experiment and neither one on its own settles what the label does. The cheap next step is a third
listing from a chunk whose label has an Entry Point but no gather has run against it.

Do not fix anything off this file alone. It records what a listing said.
