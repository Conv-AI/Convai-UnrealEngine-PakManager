# One branch, many Chunks, one policy

The Pak Manager handles any number of Chunks in a project and discovers them from the project's
Primary Asset Labels. Creators are held to one by a project setting that defaults to one and is
enforced where a Chunk is created — not by a separate build, a separate branch, or a separate
capability.

This replaces two divergent lineages: a shipped single-Chunk one and an internal multi-Chunk one
whose art pipeline publishes well over a hundred Chunks from a single project. Keeping both meant
every fix landing twice, and the internal one had already drifted ahead by features the shipped one
never received.

Discovery rather than configuration is what makes one binary serve both: a generated creator project
contains one label and therefore has one Chunk, and an internal project contains many and has many,
with no flag distinguishing them. The limit is a stated policy, not an enforcement boundary — the
plugin ships as source, so a determined creator can remove it, and that is accepted.

## Consequences

If the one-Chunk rule ever protects something real — billing, or an invariant a Convai product
relies on — it has to be enforced server-side on the upload endpoint. This setting documents intent
for the creators who will never touch the source; it does not defend anything.
