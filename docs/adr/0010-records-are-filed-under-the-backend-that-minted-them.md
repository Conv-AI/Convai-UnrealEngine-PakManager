# Records are filed under the backend that minted them

Anything the Convai server minted for a Chunk — the AssetID, the metadata cache it echoes back, the
marker saying it holds the Raw Project Archive — lives in a folder named from the base URL the
request resolved to. The Environment is derived from that URL at the moment a request is built and
is never configured: a setting that disagreed with the URL would name a backend the bytes never
reached, and a custom URL would need a matching enum value that nobody remembers to add. What the
creator authored — the Draft, the thumbnail, what the Modding Tool decided about the project — stays
at chunk level, because it is the same whichever backend it is published to.

## Consequences

- A Chunk can hold one Asset per backend at once, and its folder shows which backends it has reached.
- A whole-asset delete clears only that backend's records; the Draft and thumbnail survive, because
  they are inputs to every backend rather than a record of one. This reverses part of D10 in
  `.scratch/slate-ui-rebuild/design.md` — a delete no longer empties the creator's form.
- ADR-0005's "the creator's project is the record" now holds *per backend*: there is still exactly
  one writer, but one project holds one record per Environment, and losing on-disk state orphans the
  Asset on each Environment independently.
- Downgrade is one-way. An older plugin reads the old flat locations, finds nothing, and shows Draft.
