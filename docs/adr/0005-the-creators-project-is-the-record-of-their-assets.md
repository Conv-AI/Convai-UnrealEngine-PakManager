# The creator's project is the record of their Assets

An Asset's name, description, type and thumbnail are authoritative in the creator's project and
pushed to Convai on Publish. The Pak Manager does not fetch them back, and there is no reconciliation
between the two copies.

This is safe only because of a product fact that is invisible from the code: a creator can manage
their Assets from the generated Unreal project and nowhere else — no dashboard, no web editor, no
second tool. With exactly one writer, the two copies cannot disagree, so the machinery for noticing
and resolving disagreement would guard against nothing. It also keeps the metadata where it belongs:
beside the content it describes, versioning with it in the creator's own source control, and
readable with no network.

## Consequences

The link between a Chunk and its published Asset exists **only** in the creator's project. Losing
that state orphans the Asset permanently — no update and no delete, because the only copy of its
identity is gone. The Modding Tool documentation warns creators never to touch that folder, which is
a warning standing in for a recovery path that does not exist. Should a second writer ever appear,
this decision is void and the reconciliation this ADR declines becomes mandatory.
