# `assets/get` has no caller

Status: `wontfix`

Carried from [#263](https://github.com/ar-convai/ConvaiTask/issues/263), register item 3.

`GetAssetProxy` was dead — nothing called it. Legacy used the endpoint to refresh the local echo of
the record after every create, update and raw upload.

## What goes stale: nothing

The refresh is unnecessary, not deferred. Four independent reasons, each checked against the code:

- **The Pak Manager is the only writer of an Asset.** `CONTEXT.md:145` — a creator has no dashboard
  and no web editor, so nothing changes an Asset's name, description, type or thumbnail behind this
  tool's back. There is no second author for a GET to discover.
- **The local echo is written from the server's own words.** `UCPM_CreatePakAssetProxy::HandleSuccess`
  (`CPM_Proxy.cpp:133-149`) writes `PakMetaData` straight out of the create response. A GET would ask
  for the string it was just handed.
- **Every update re-composes the echo before sending.** `UCPM_CreateAssetJob::IExecute_Implementation`
  (`CPM_PublishJobs.cpp:410`) calls `ComposePakMetadata` — this Chunk's Draft laid over what this
  backend last echoed — and fails the Publish if it cannot. What the server holds is what this tool
  last sent, so the composed document is the refresh.
- **Delete deliberately needs no record.** `DeleteVersion`'s contract
  (`ConvaiPakEditorSubsystem.h:383-393`) is that a fresh clone, a second machine or a lost marker
  must not lock an operator out; the request simply changes nothing when there is nothing there. It
  never reads a record, so it cannot read a stale one. The raw-archive marker is local by design
  (`HandleWorkflowFinished`).

And a fresh clone has no AssetID to GET with in the first place — the AssetID lives only in the
record the create wrote.

## Deleted

Grepped across `Source/`, then scanned all 1549 `.uasset`/`.umap` under `Plugins/` and the project's
`Content/`: not one references `/Script/ConvaiPakManager` at all, so no Blueprint can hold these
types. Scan for the *serialised* name if you repeat this — UE writes the struct as `CPM_AssetData`,
never the `F`-prefixed C++ spelling, so a scan for `FCPM_AssetData` comes back empty either way.

- `UCPM_GetAssetMetaDataProxy` — `CPM_Proxy.h` (class block), `CPM_Proxy.cpp` (all five members)
- `GetPakAssetURL` — `CPM_Proxy.cpp`, the only caller was the above
- `FCPM_GetAssetsHttpResponseCallbackDelegate` — `CPM_Proxy.h`, no other user
- `UCPM_UtilityLibrary::ExtractAssetListFromResponseString` — declaration and body
- `FCPM_AssetResponse` and `FCPM_AssetData` — `CPM_Utils.h`. `FCPM_AssetData` existed only as that
  response's array element. `FCPM_Asset`, `FCPM_CreatedAssets` and the create path are untouched.

## Register

Register item 3 in `.scratch/legacy-parity/PRD.md` is closed by this deletion, and the legacy-read
item has already flipped it there — "closed, deleted", `PRD.md:337`.

The correction #263 asked for is posted: the sign-off comment's "§B.3" label is register item 4.

## Verification

Both run 2026-09-04, after the deletions:

- `Build.bat Dev_CPM_58Editor Win64 Development` — `Result: Succeeded`, "Target is up to date"
  (03:26 IST). These deletions were compiled into the 02:52:52 link of
  `Binaries/Win64/UnrealEditor-ConvaiPakManager.dll`, which post-dates them; "up to date" is what
  proves no source file has changed since that link.
- `Automation RunTests ConvaiPakManager` — 65 passed, 0 failed (03:25 IST,
  `Saved/Logs/Issue10Run.log`). The suite held 55 before this wave added tests; none of them
  referenced the proxy, and no symbol from it survives anywhere under `Source/`.

## Done when

Either the proxy is deleted with a note saying what made it unnecessary, or the refresh is restored
on the paths that need it, with the staleness it prevents written down.
