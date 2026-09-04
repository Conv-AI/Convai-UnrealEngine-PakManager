# `assets/get` has no caller

Status: `ready-for-agent` — reopened. The premise the `wontfix` rested on was wrong.

`GetAssetProxy` was deleted as dead code because nothing called it, and because the Pak Manager was
believed to be the only writer of an Asset. It is not: other tooling edits the same records, so the
server is the record of what an Asset currently holds and the per-Environment cache goes stale
behind this tool. Restored, with the two callers it never had.

## What went stale, after all

The four reasons the `wontfix` gave are answered by one fact each:

1. ~~The Pak Manager is the only writer of an Asset.~~ **False.** A second writer exists. This was
   the load-bearing reason and every other one rested on it.
2. The create response is the server's own words — still true, and still only true *at the moment of
   the create*. It says nothing about what the Asset holds a week later.
3. The composer re-composes before every update — from the cache, which is the thing that went
   stale. Re-composing a stale document does not make it fresh.
4. A delete needs no record — still true. Delete is untouched.

`CONTEXT.md` and `docs/adr/0005` carried the same premise and are corrected;
`docs/adr/0013` supersedes 0005 and records what did and did not change.

## Restored

- `UCPM_GetAssetProxy` (`CPM_Proxy.h/.cpp`) — today's idiom, not the deleted one: no `WorldContext`,
  `const FString&`, and it writes the Chunk's cache itself the way the create proxy does.
- `assets/get` in the URL namespace.

Deliberately NOT restored: `FCPM_AssetData` / `FCPM_AssetResponse` and
`ExtractAssetListFromResponseString`. That parser read a flat envelope (`assets[N].asset_id`) and
appended two unrelated animation shapes into the same array. `GetCreatedAssetsFromJSON` already
parses the create envelope (`assets[N].asset.*`) and already yields the `MetadataString` the cache
wants, so it is reused.

## Open — the wire shape

**Nobody has captured a real `assets/get` response.** The restore assumes it matches the create
envelope. If it does not, the parse yields nothing, and that path is deliberately safe: it logs the
body at Warning, leaves the cache exactly as it was, and lets the caller carry on. So a mismatch
costs a log line, not a failed publish — and the log line contains the shape needed to fix it.

## Callers

- **Editor start-up.** `FConvaiPakManagerModule::RefreshPublishedAssets`, after the state-layout
  reconcile, which is already behind the Asset Registry scan that `Discover` needs. Asks nothing
  without an API key, and nothing for a Chunk that has not published to the current backend.
- **Before an update.** `UCPM_CreateAssetJob::Execute` reads the AssetID first and, when there is
  one, asks the server before composing. Best effort: either outcome continues to
  `ComposeAndSend()`, because a backend that cannot answer is not a reason to refuse to publish.

Not added to `UCPM_UploadArtifactsJob::MintUrlForNext`, which posts `assets/update` carrying only a
Version and an AssetID by design.

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
