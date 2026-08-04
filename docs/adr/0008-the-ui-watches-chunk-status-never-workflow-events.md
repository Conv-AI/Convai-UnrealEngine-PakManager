# The UI watches Chunk status, never workflow events

The subsystem subscribes to its own Workflows, keeps one status per Chunk in the Pak Manager's own
vocabulary, and broadcasts a single change notification. The UI watches that. It never subscribes to
the Job System, and the Job System is not among the UI layer's dependencies.

A future reader will notice the Job System already broadcasts everything the UI wants, and that a
widget could subscribe directly and filter by the Workflow Handle it was given — the abandoned Slate
refactor did exactly that, storing a handle in the widget and hand-rolling the filtering. That extra
hop is the point rather than an oversight: this seam is the whole reason for the refactor, and a
widget filtering workflow events is business logic living in the UI again.

Status is expressed in this domain's terms — packaging, creating, updating, uploading, deleting,
each begun, succeeded, failed or cancelled — not in the Job System's. What a creator is told should
not change shape because the machinery underneath was swapped.

## Consequences

Tests assert on a Chunk's status after calling a Command, with no listener interface, no handle
bookkeeping and no knowledge of how many Jobs ran. That is the cheap test, and cheap tests are the
ones that get written.

Progress and the running Job's name travel as their own fields alongside the status rather than as
more status values, so that showing "Packaging Windows, 40%" costs a passed-through string and
leaves the status vocabulary flat.
