# The server is the record of what an Asset holds

Supersedes [ADR-0005](0005-the-creators-project-is-the-record-of-their-assets.md).

The Pak Manager reads an Asset back from the backend that holds it — at editor start-up for every
Chunk that has published, and once more before an update composes what it is about to send.

ADR-0005 declined that read, and named the exact condition under which it would have to be taken
back: *"Should a second writer ever appear, this decision is void and the reconciliation this ADR
declines becomes mandatory."* A second writer has appeared. The Pak Manager is not the only way an
Asset is edited — other tooling writes the same records — so the creator's project is no longer the
only copy, and the cached copy of the server's document goes stale behind the tool that keeps it.

## What did NOT change

**The Draft still wins every field it names.** What a creator typed into the Pak Manager is still
what publishes: the composer lays the Draft over the server's document, in that order, exactly as
before. The read refreshes the half of the document the creator never types — the half the server
and other tools own — and nothing else.

That is why this is a smaller change than voiding ADR-0005 sounds. Two writers do not mean two
authorities over the same fields; they mean each side keeps what it owns, and the tool stops
pretending the server's half cannot move.

## Consequences

- **A failed read changes nothing.** The cache keeps whatever it last held, and the caller carries
  on with it. A backend that is unreachable, unauthenticated, or answers something this version
  cannot parse is not a reason to refuse to publish — it is a reason to publish from the last thing
  known, which is what the tool did unconditionally until now.
- **Start-up costs one request per published Chunk.** External projects hold one. Internal projects
  hold over a hundred, and pay a hundred requests at every launch. Nothing is asked for a Chunk that
  has never published to the current backend, and nothing at all when the project holds no API key.
- **The read never touches the Chunk's identity.** `CreateAssetData_<N>.json` holds the AssetID and
  is the only copy of it in the creator's world; only a create response writes it. A read refreshes
  `PakMetaData_<N>.json` and nothing else.
- **An Asset that is gone from the server is not yet modelled.** A read that 404s for an id the
  project still records leaves that record in place. Whether that should orphan-recover — clear the
  id and offer Create again — is open, and is the same unresolved question ADR-0005's consequences
  raised from the other direction.
