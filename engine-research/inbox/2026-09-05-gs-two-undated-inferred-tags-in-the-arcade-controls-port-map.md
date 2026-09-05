# Two undated `[inferred]` tags in the Arcade Controls port map (and one that is fine)

Filed by `/gs`, 2026-09-05 16:10, home PC, after the four-lane concurrency run. Read-only sweep;
nothing was edited.

## The three hits

`modding-notes/2026-09-05-arcade-controls-port-map.md`, written by `/pd` during the run:

| line | text | verdict |
| --- | --- | --- |
| 31 | *"`[inferred]` appears next to the claim"* | **fine** — prose explaining the convention, not a tag in use |
| 411 | *"…`sync_weapon_chamber_display_zero()`. `[inferred]` reason: it sets the count wi…"* | **real use** |
| 909 | *"…falls through to the `drop_sec` default (0.18 s). `[inferred]` intentional; make it explicit"* | **real use** |

`[inferred]` is not one of the eight names, so both real uses read as a confidence claim to a human
and count as **nothing** to every tool. They are also **undated**, which check 4 would flag
separately if the name were valid.

## What they should be

Both look like static reasoning about the shipped Lua — *why* a call exists, *why* a branch is
absent — so **`[inferred-static 2026-09-05]`** is very likely right in both cases:
CONVENTIONS defines it as *"read out of a binary/config, never seen running"*, which is exactly
that. If either conclusion is a guess rather than a reading, `[hypothesis]` is the honest one.
Owner's call — I have not read the surrounding argument closely enough to choose for you.

⚠️ **`[inferred]` is a recurring near-miss, not a one-off.** `enslaved-vr` carries two of them
(`status/enslaved-vr.md:156` and `dev-archive/recon/2026-09-02-first-live-runs/README.md:35`), and
a `/gs` drop about those has been open since 2026-09-04. The tag name `inferred-static` seems to
lose its suffix in a hurry, which is worth knowing when writing rather than when sweeping.

## ⚠️ These two also expose a hole in the scanner, which is being reported separately

`gs-scan.sh` check 3b was patched earlier today to split hits into **IN USE** (carries a date or
`n=`) and **BARE** (no args, usually prose about a tag). Lines 411 and 909 are **real uses that
carry no date**, so they landed in BARE, whose label reads *"usually prose ABOUT a tag; confirm
before acting"* — soft-pedalling a genuine defect.

Nothing was lost (both were printed, which is why this drop exists), but the label is misleading in
exactly the direction that matters. The fix belongs in `claude-memory/tools/gs-scan.sh`, not here.
Recorded in this drop only so that a future reader of these two lines knows the scanner
under-reported them rather than that nobody noticed.
