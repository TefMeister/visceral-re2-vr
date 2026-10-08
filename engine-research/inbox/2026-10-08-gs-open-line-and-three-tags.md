# The board's OPEN line is unreadable to /gates, and a few tags are off-vocabulary

From `/gs`, 2026-10-08. Owner: modding (both files below).

1. **`claude-memory/status/visceral-re2-vr.md` line 10** begins `OPEN (2026-10-06 ~10:50, home PC, Opus: ALL
   THREE OPUS PORTS BUILT …` and runs on for a long paragraph of earlier headers. `gate-scan.sh --check` wants
   exactly `OPEN (YYYY-MM-DD):`, so it reports `OPEN line present but not "OPEN (YYYY-MM-DD):"` and the board's
   rows may not reach `/gates`. Fix: make the line `OPEN (2026-10-06):` and move the running header text into
   the log below.

2. **`modding-notes/2026-10-07-fire-without-the-aim-state-three-switches.md` line 27**: `[inferred 2026-10-07]`
   is not one of the eight names. Probably `[inferred-static 2026-10-07]` if it was read from code or data, or
   `[hypothesis]` if not.

3. **`claude-memory/status/visceral-re2-vr.md` lines 54 and 68** carry `[inferred …]` and line 140 a bare
   `[verified]`. Last sweep (2026-10-04) reported the same two `[inferred …]` tags at lines 24 and 38; the
   lines moved, the tags stayed. Lines 54 and 68 sit in superseded rows, so the cheapest fix is the one word.
   Line 140 is a changelog line describing an earlier tag fix and may be fine as prose: read it first.
