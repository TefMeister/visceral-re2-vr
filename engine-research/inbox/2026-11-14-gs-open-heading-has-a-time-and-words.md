# /gs: the board's OPEN heading carries a time and words (2026-11-14)

From the weekly tidiness sweep (part of the full scan, Keeper PC).

`claude-memory/status/visceral-re2-vr.md` line 10 reads
`OPEN (2026-11-12, 13:4x: b208 INSTALLED (...), SIX SPOTS + THE TORCH, UNWORN; older rows below):`.
`gate-scan.sh --check` only reads a heading of the exact shape `OPEN (YYYY-MM-DD):`, so it reports
"OPEN line present but not OPEN (YYYY-MM-DD):" and `/gates` may not count this board's rows.

Suggested fix (modding lane, one line): `OPEN (2026-11-12):`, with the b208 sentence moved to the first
row or the prose above the block. Same fault as Village's heading on 2026-11-07.
