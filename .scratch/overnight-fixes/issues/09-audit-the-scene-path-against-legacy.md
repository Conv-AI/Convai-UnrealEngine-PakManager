# Audit the Scene path against legacy

Status: `ready-for-agent`

Carried from [#263](https://github.com/ar-convai/ConvaiTask/issues/263), register item 1.

All 29 gaps in the parity register are Avatar-only. The Scene half of the tool has never been diffed
against legacy at all — roughly half the tool is unaudited.

Known to have no counterpart:

- `AssetIsScene`
- `SceneIsValid` — legacy required a tagged spawn point in the loaded level
- `GetNumSubobjectInAsset`

Those three are the starting point, not the scope. Run the same triage over the Scene path that
produced the Avatar register, and write the gaps into the register in the same form, so the two
halves are comparable.

## Done when

The Scene path has a gap list of the same shape as the Avatar one, each entry triaged, and the
register says the audit is complete rather than untouched.
