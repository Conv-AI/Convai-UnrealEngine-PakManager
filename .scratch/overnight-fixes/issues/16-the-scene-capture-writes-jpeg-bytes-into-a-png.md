# The Scene capture writes JPEG bytes into `Thumbnail_N.png`

Status: `needs-triage` — split out of [04](04-thumbnail-front-facing-and-1-2.md), where it was found
and left as out of scope. Nothing is known to be broken by it; it is a lie in a filename.

`CPM_TakeViewportScreenshot` finishes with `FImageUtils::ThumbnailCompressImageArray`
([ConvaiPakManagerEditorUtils.cpp:212](../../../Source/ConvaiPakManager/Private/ConvaiPakManagerEditorUtils.cpp#L212)),
which is a **JPEG** encoder. The file it writes is named `Thumbnail_<ChunkId>.png` and starts
`FF D8 FF E0 JFIF`.

Nothing in this tool reads by extension — every decode goes through `IImageWrapper`, which detects
the real format — so the Scene path works today, preview included.

**It does not reach the server either**, which was the reason first given for leaving it alone and is
wrong: `UCPM_CreateAssetJob` loads the file into a `UTexture2D` with `CPM_LoadTexture2DFromDisk`
([CPM_PublishJobs.cpp:375](../../../Source/ConvaiPakManager/Private/Jobs/CPM_PublishJobs.cpp#L375))
and the proxy re-encodes it with `Texture2DToBytes(..., EImageFormat::PNG)`
([CPM_Proxy.cpp:109](../../../Source/ConvaiPakManager/Private/Proxy/CPM_Proxy.cpp#L109)) before the
multipart write. The server gets a PNG whatever is on disk.

So the only cost is anything **outside** this tool that opens the file by extension — a creator's
image viewer, a browser, an artist dragging it somewhere.

## Done when

`CPM_TakeViewportScreenshot` writes through `ConvaiPakManager::Thumbnail::WritePng` — the same
encoder the Avatar render and both import paths already use — and a captured Scene thumbnail starts
`89 50 4E 47`. One call swap; the pixels are already the `TArray<FColor>` `WritePng` takes.
