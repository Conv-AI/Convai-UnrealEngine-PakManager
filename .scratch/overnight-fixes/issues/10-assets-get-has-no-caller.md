# `assets/get` has no caller

Status: `needs-triage`

Carried from [#263](https://github.com/ar-convai/ConvaiTask/issues/263), register item 3.

`GetAssetProxy` is dead — nothing calls it. Legacy used the endpoint to refresh the local echo of the
record after every create, update and raw upload.

The open question #263 left is the right one: **what goes stale?** Answer that before writing code.
If the tool now derives everything it shows from what it already holds, the proxy is dead code and
should be deleted rather than wired up. If anything on screen or on disk can drift from what the
server holds, the refresh needs restoring and this becomes `ready-for-agent`.

Note that #263's sign-off comment mislabels the dead-helper census as "§B.3". That census is
register item 4 and is done; §3 is this item, which is not. Correct that line on the issue.

## Done when

Either the proxy is deleted with a note saying what made it unnecessary, or the refresh is restored
on the paths that need it, with the staleness it prevents written down.
