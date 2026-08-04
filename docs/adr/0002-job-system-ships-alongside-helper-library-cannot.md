# The Job System ships alongside; the Helper Library cannot

Asynchronous work runs on the Convai Job System, distributed as a sibling plugin bundled with the
Pak Manager's own release rather than vendored into this repository — a vendored copy had already
drifted behind the real one and would again.

The Pak Manager keeps its own `CPM_*` API proxies even though `ConvaiHelperLibrary` wraps the same
four endpoints with near-identical signatures. This looks like duplication a future reader should
delete, and it is duplication — but the Helper Library lives in a private repository and the Pak
Manager ships publicly, so depending on it would make the plugin unbuildable for the creators it
exists to serve. The constraint is distribution, not design.

## Consequences

Every new plugin dependency is another thing a creator must install, so the bar for adding one is
whether the capability can be had no other way. Should the Helper Library ever ship publicly, the
proxy layer becomes deletable — and that is a behaviour-preserving swap to be proven by tests, not
folded into unrelated work.
