# Publish is one Workflow, and its Policy is resolved before the queue is built

A creator asks to Publish once, and that runs one Workflow whose Jobs cover everything from
gathering content to the last artefact landing in its Version. The alternative — separate Commands
the caller sequences — would put the ordering contract in the caller, so the Slate panel, the test
harness and any script would each hold their own definition of what publishing means, and would
eventually disagree.

The Publish Policy is fetched **before** that queue is constructed, never as its first Job. The
Policy decides which platforms are built and whether the Raw Project Archive is included, which is
to say it decides the queue's shape — and a Job System queue's shape may depend only on what the
caller knew before building it, so that a queue can be rejected as unsound before any of it runs.
Resolving the Policy first keeps that check switched on for the longest and most expensive path in
the product.

## Consequences

A Publish cannot start while the Policy is unreachable. That is a deliberate trade against caching
the last known Policy: the Policy exists precisely so Convai can change what a Publish produces, so
a stale copy is wrong exactly when it matters, and publishing from one yields an Asset missing a
Version — a failure that surfaces later, in a product, rather than here. Transient failures are
absorbed by the fetch Job's own retries.

Re-running a single step without re-running the rest is served by a Job's Precheck, which may
satisfy the Job's declared output without doing its work — not by exposing the steps as Commands.

Because the Policy is read over the network, there is no Workflow when the Publish Command returns
and therefore no Workflow Handle to answer with. The Command reports only whether the request was
accepted; everything after that — the step running, progress, success, failure — reaches the caller
as the Chunk's status, which is the one thing a caller has to watch anyway. Cancelling names a Chunk
rather than a handle for the same reason, and that is what a UI has to hand.
