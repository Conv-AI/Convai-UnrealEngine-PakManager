# The raw Version slot was renamed on the wire: `raw` where legacy sent `ue-5.8-Raw`

Status: `needs-triage` — split out of
[08](08-diff-the-publish-metadata-payload-against-legacy.md). Not a fix anyone can make without
knowing what is already on the server.

Legacy built every Version slot the same way — `"ue-" + first three chars of the engine version +
"-" + Platform` — so the Raw Project Archive went to `ue-5.8-Raw`. This tool sends `raw`
(`CPM_PublishTypes.cpp:33-51`, with its reason at the constant): the archive is engine-independent,
so pinning it to an engine version says something untrue about it.

The reasoning is sound. The problem is that Versions are keyed by string, and both names now exist
on the backend: `ue-5.8-Raw` on every Asset legacy touched, `raw` on every Asset this tool has
published since the rewrite.

- Renaming back to `ue-5.8-Raw` strands the `raw` slots this tool wrote.
- Leaving it strands the `ue-5.8-Raw` slots legacy wrote, and an Asset that has been published by
  both carries two raw archives under two names, one of them stale.

## Done when

Anmol says which name the backend should hold, and — if it is not the one already being sent —
whether the other is migrated or left. A one-sided change here loses an artefact either way, so
this is a question, not a task.
