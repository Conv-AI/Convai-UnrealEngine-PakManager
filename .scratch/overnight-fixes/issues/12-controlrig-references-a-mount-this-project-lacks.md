# A copied ControlRig references a mount this project does not have

Status: `needs-triage`

Split out of [issue 01](01-the-declared-convai-dependency-does-not-take-effect.md), where it was
found. It is a decision, not a bug fix: nothing in the tool can copy a package that is not on disk.

## The finding, as issue 01 recorded it

"Assets in the ControlRig module are not getting copied" — a whole-project `-run=DataValidation`
sweep found the project's only error, and it is this:

```
/JBILN5CDNI4TRYELD6CS/MetaHumans/Common/Female/Medium/NormalWeight/Shoes/CasualSneakers/mh_ShoeTongue_CtrlRig
  soft references a missing package:
  /CharacterParts/Female/Medium/NormalWeight/Shoes/CasualSneakers/f_med_nrw_shs_casualsneakers
```

The ControlRig **was** copied. What it points at is a `/CharacterParts/` mount this project does not
have, so there is nothing on disk to copy or repoint — the gather cannot fix it and neither can the
creator. It needs a decision (drop the reference, or refuse the pick and say so), not a bug fix, and
it deserves its own issue.

## Confirmed again on 2026-09-04

Re-measured in the editor while diagnosing the gather for
[issue 02](02-copy-into-plugin-copies-nothing.md). Walking `BP_Hana`'s closure over the Asset
Registry, ten edges leave the Modding Plugin; nine point at packages whose copies are already under
`/JBILN5CDNI4TRYELD6CS/` and are the fixup's problem. This one is different — it is the only edge
whose target does not exist anywhere:

```
soft  /JBILN5CDNI4TRYELD6CS/MetaHumans/.../CasualSneakers/mh_ShoeTongue_CtrlRig
   -> /CharacterParts/Female/Medium/NormalWeight/Shoes/CasualSneakers/f_med_nrw_shs_casualsneakers
      (copy in plugin: MISSING — the source mount is not in this project either)
```

`/CharacterParts/` is a MetaHuman authoring mount that exists in the MetaHuman Creator source
project, not in a project the character was exported into. So every MetaHuman avatar picked in a
project like this one carries at least one dangling soft reference, and it survives the gather by
construction.

## The two options

- **Drop the reference.** Null the soft path on the copy after the gather, so the Pak carries a
  ControlRig that loads clean. Cheap, and the reference is dead weight in a cooked build anyway —
  but it edits a copy of the creator's asset in a way they did not ask for, and if a product ever
  does mount `/CharacterParts/` the reference is gone for good.
- **Refuse the pick and say so.** Tell the creator their MetaHuman references a mount this project
  does not have, and let them decide. Honest, and it blocks a pick over something that may never
  matter at runtime — the reference is soft, so a load resolves it to null rather than failing.

Unanswered either way: does a Convai product actually break on this, or does it draw the shoe fine
with a null control rig reference? Nobody has looked, and the answer decides which option is right.

## Done when

Anmol picks one, or says the dangling soft reference is acceptable and this closes as `wontfix`.
