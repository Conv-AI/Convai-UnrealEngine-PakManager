# Partition Asset records by Environment

Status: `ready-for-agent`

File everything the Convai server minted — AssetID, metadata cache, raw-archive marker — under a
per-Environment folder derived from the resolved base URL, so pointing the project at another
backend can never send one Environment's AssetID to another.

See [../PRD.md](../PRD.md) for the layout, the slug, the migration and the accepted tradeoffs.

## Comments
