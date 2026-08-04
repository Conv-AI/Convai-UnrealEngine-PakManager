# Commands are typed functions, not a JSON command bridge

The Pak Manager exposes each Command as a typed, reflected function on an editor subsystem —
asynchronous ones answering with a Workflow Handle — rather than as a string-keyed registry taking
a JSON payload and returning a JSON reply.

The registry-and-JSON shape is the established pattern in `ConvaiAssemblyStudio`, so a reader coming
from there will expect to find it here and should know why it is absent: that bridge exists to cross
a real process boundary, because its UI is a browser talking over WebSocket or Pixel Streaming, and
serialisation is forced. The Pak Manager's UI is Slate inside the editor, and its other callers —
scripts, automation tests, Blueprint — are in that same process. Unreal's reflection already carries
typed calls across all of them, so a JSON layer here would buy nothing and cost compile-time
checking on every field of every Command.

## Considered options

- **String-keyed registry with JSON payloads.** Rejected: no process boundary to justify it. Its own
  reference implementation also shows the shape does not survive contact with asynchronous work —
  the handler signature returns its reply immediately, so every deferred-reply command there sits
  outside the registry in a hand-written branch chain. Nearly every Pak Manager Command is deferred,
  so nearly all of them would land in that escape hatch.

## Consequences

If a browser-based front end is ever wanted, the bridge is added as a transport adapter that
deserialises onto these same typed Commands. That is additive, and it keeps one definition of what
a Command does rather than two.
