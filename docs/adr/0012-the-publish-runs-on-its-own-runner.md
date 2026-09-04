# The Publish runs on its own runner

Asynchronous work no longer runs on the Convai Job System. `UCPM_PublishRunner` — one index into an
array of Jobs, an `FTSTicker` for the per-Job deadline, and a two-part cancel — is what a Publish
actually needs, and it fits in under two hundred lines.

The Job System is a good piece of work and the Pak Manager used roughly a fifth of it: a sequential
queue, a per-Job timeout on two Jobs, one retry on one file write, cancel-with-force, and the
progress and finished delegates. Everything that made it worth being a plugin — job groups and
parallelism, prechecks and skipping, the listener interface, the Blueprint library, workflow
history, `bContinueWorkflowOnFailure` — went unused. Against that fifth stood a second plugin in
every release zip, a CI step cloning `v2.3.9` and bundling it, and an "unzip both plugins" line in
the install instructions. ADR-0002 set the bar for a dependency at "the capability can be had no
other way"; this one could.

## What was kept

- **The sequential queue.** Jobs run in order, each only once the previous reports.
- **The per-Job deadline.** Two Jobs have one — creating the Asset at 120s, recording it at 30s —
  and they are the two whose hanging leaves an Asset on Convai the project does not know about. A
  cook and an upload keep no deadline: their honest duration depends on the creator's project and
  connection, not on anything we can predict.
- **The two-part cancel.** The runner asks; the Job decides what to abandon and reports Cancelled.
  A Job that answers Failed while stopping still resolves the run as the cancel it was — Cancelled
  and Failed are different words to whoever reads them.
- **The typed context**, as a plain `FCPM_PublishContext` the Jobs read and write in place. Five
  Jobs whose shape is fixed at compile time do not need runtime type matching to find each other's
  values.

## What was dropped

- **The IO specification and its queue-build validation.** It caught a Job requiring a value no
  earlier Job produced; a struct field makes that a compile error instead. ADR-0004's "a queue can
  be rejected as unsound before any of it runs" is now the compiler's job rather than a check at
  queue build — the reason the Policy is resolved first is untouched, because the Policy still
  decides which Jobs exist.
- **The retry count.** One Job had one, on a file write. It is now a second immediate attempt
  written where it happens, which is easier to read than a number on a config a framework acts on.
- **`FInstancedStruct` outputs** and the `TryGet`/`TryGetMany` reads that went with them.

## Consequences

- The first Job runs a tick after the run starts, never inside the call. That is load-bearing and
  not an implementation detail: `ICreateWorkflow` both built and ran the queue, so a queue whose
  every Job completed synchronously — one packaging Job reusing the Pak already on disk — finished
  before the subsystem had anywhere to register it, and the subsystem carried a pair of members
  whose only purpose was to survive that. Both are gone.
- Cancelling has no time-box. Every Publish Job reports from inside its own cancel, so the run
  resolves on that stack; a Job that stopped doing so would leave the run in flight for the session.
  The Job System had a `CancelTimeoutSeconds` for this and nothing here has needed it.
- `ConvaiPakManager.uplugin` names one fewer plugin, `Build.cs` one fewer module, and the release
  workflow no longer clones or bundles anything. A creator unzips one folder.
- The publish path finally has tests. `CPM_PublishRunnerTest.cpp` was written against the Job System
  first and passed there before anything was removed — queue order, a failure stopping the queue,
  cancel resolving as Cancelled, progress scaled across the queue. Three of its six cases were red
  on the Job System, and each of the three is a difference this ADR chose deliberately: the deferred
  first Job (two cases), and a timeout that says it timed out rather than reporting the Job's own
  "cancelled".
