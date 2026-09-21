# BrokeDJ local library

BrokeDJ's library is local-only. SQLite metadata, tags, playlists, history, content identities and waveform/analysis cache data stay on the machine unless the user explicitly creates a backup. BrokeDJ never deletes or overwrites original music as part of duplicate or moved-file handling.

## Duplicate and missing-file review

The native library search supports two exact review directives:

- `is:duplicate` — show non-missing tracks whose populated content hash is shared by another non-missing library record.
- `is:missing` — show records whose source file has been marked missing so they can be reconnected with **Relocate**.

Duplicate review is intentionally non-destructive. Matching records are candidates for the DJ to inspect; BrokeDJ does not auto-delete, merge or rewrite source audio.

## Content identity

When an imported local track reaches the SQLite library without an existing content identity, the database adapter computes a streaming SHA-256 over the file outside the realtime audio callback. The file is read in bounded chunks rather than loaded wholly into RAM. Relocating a record recomputes the identity for the selected replacement file before the new path is committed, so stale duplicate matches do not survive a content change.

Older migrated rows may still have an empty hash. They remain valid library records, but they are excluded from duplicate review until they are re-imported or a future explicit background backfill processes them. BrokeDJ does not perform an unbounded filesystem scan while opening the database or from the audio callback.

## Qualification limits

Automated tests cover known SHA-256 vectors, duplicate/missing review filters, relocation rehashing and a deterministic synthetic 1,500-row SQLite workload. That workload is a regression fixture, not a claim about maximum library size, storage latency or every Windows filesystem. Real large libraries, network/removable storage and long interactive sessions remain separate qualification work.
