# Nothing writes `<Platform>_PakSize` into the metadata document

Status: `needs-triage` — split out of [08](08-diff-the-publish-metadata-payload-against-legacy.md)
while fixing the rows that were four literals. This one is not a literal, and the shape of the fix
is a decision.

Legacy's `GetUpdateMetaData` wrote one size field per artefact it had just uploaded:
`Append(EnumToString(Platform), "_PakSize")` → `CPM Get File Size` of that platform's Pak, or of the
raw project zip when the platform was `Raw`. So a legacy-published Asset carries `Windows_PakSize`,
`Linux_PakSize` and `Raw_PakSize` on its server record.

Nothing in this tool writes any of them. An Asset last published by legacy keeps the sizes of the
**legacy** Paks forever, and every Asset created since the rewrite has none at all.

## What the read for issue 08 settled, and what it did not

Settled: the data is reachable at compose time, and nothing has to be declared to reach it. The
queue is `PackagePaks → ArchiveRawProject → CreateAsset → UploadArtifacts → PersistChunkState`
(`ConvaiPakEditorSubsystem.cpp:1400-1421`), and since the runner swap the artefacts are plain fields
on `FCPM_PublishContext` — `Context->Paks` and `Context->RawArchive`, with `Context->bHasRawArchive`
saying whether the second means anything. `UCPM_CreateAssetJob` already reads `Context->Request` and
`Context->bHasRawArchive` the same way. There is no IO spec to extend and no `Configure` to add.

Not settled — and this is what wants a decision:

- **Where the number lands.** `ComposePakMetadataAt` writes the document to disk and the Job then
  loads it. Sizes are per-run facts, not part of the Draft and not part of the server's echo, so
  either the composer grows a `TMap<FString, int64>` parameter or the Job edits the composed
  document before sending. The first keeps one composer; the second keeps run facts out of the
  cached document ADR-0005 describes.
- **Whether anything reads it.** Register gap 31 filed `CPM_GetFileSize` as a UI affordance ("show
  the upload size before a Publish"). Nobody has confirmed the server or any product reads
  `<Platform>_PakSize`. If nothing does, this is a field to drop from the parity list rather than
  build.

## Done when

Either the sizes of this run's artefacts reach the published document under legacy's key names,
with a test that reads the composed document, or the field is dropped from the parity list with the
answer to "who reads it" written down.
