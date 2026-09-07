# `gate-scan.sh --mine` on the home PC hides ALL 12 non-VR Visceral and Village Scope rows — two independent causes, either one sufficient

**Filed by `/gs`, 2026-09-07, home PC (twenty-fifth run).** Read-only sweep. The two temporary
edits described below were made to a scratch copy and reverted; `git status` on
`machine-assignments.tsv` is clean.

⚠️ **Filed here rather than in `claude-memory` because that repo has no `inbox/`.** The bug lives in
`claude-memory/tools/gate-scan.sh` and `claude-memory/machine-assignments.tsv`, both modding-owned.
This project's inbox is the closest honest home: Visceral is one of the two projects whose work is
being hidden.

## The symptom

`machine-assignments.tsv` was added 2026-09-07 to record the user's split — `HOME visceral-re2-vr`,
`HOME re-village-scope-vr`, `DEV *`. On this machine (`USER: TD3KX` → role `HOME`),
`gate-scan.sh --mine` reports:

```
=== PARALLEL DEVELOPMENT - NO GAME NEEDED - 0 items, 0 projects ===
=== NEEDS YOU, NOT THE GAME - 0 items, 0 projects ===
=== FLAT-SCREEN RUN - 0 items, 0 projects ===
=== HEADSET - 7 items, 6 projects ===
=== NOT THIS MACHINE - 53 item(s) ===
```

It should report **19 items**, not 7. With both defects fixed the same command gives
`PD 1 · USER 2 · FLAT 9 · VR 7`, and `NOT THIS MACHINE` falls to 41.
`[verified-numerically 2026-09-07]`

**So `--mine` and `--next` currently tell the home PC that the only two projects assigned to it
have no work at all, and the user schedules their evening off exactly that.** The full board
(`gate-scan.sh` with no flag) is unaffected — it does not consult the assignments — so the two
views disagree and the filtered one is the wrong one.

## Cause 1 — `machine-assignments.tsv` has CRLF line endings

```
0001540   o   p   e   -   v   r  \r  \n   D   E   V  \t   *  \r  \n
```

`owner_of()` reads it with `while IFS=$'\t' read -r role prefix`, so `prefix` is
`visceral-re2-vr\r`, which never equals `visceral-re2-vr`. Worse, the catch-all test
`[ "$prefix" = "*" ]` also fails against `*\r`, so `fallback` is never assigned from the file and
stays at the hard-coded `fallback=DEV`. **Every project resolves to DEV, including on the home PC.**

Isolated by experiment: with the correct clone passed explicitly and the file left as-is, `--mine`
gives `0/0/0/7`; with nothing changed but `tr -d '\r'` applied, the same command gives
`1/2/9/7`. `[verified-numerically 2026-09-07, n=2 runs]`

Fix: strip the CRs, and make `owner_of` tolerant — `prefix=${prefix%$'\r'}` — since the file will
be edited on Windows again.

## Cause 2 — `MEM` resolves to a stale clone that predates the file

Run the way the lane commands and the `/gs` skill actually invoke it — `bash
claude-memory/tools/gate-scan.sh` from a lane root — `./status` is not in the cwd, so the script
falls to its candidate list and takes the **first** hit:

| candidate | state on this machine |
| --- | --- |
| `D:/claude video game stuff/github-backups/claude-memory` | absent (that is the dev PC's path) |
| **`$HOME/github-backups/claude-memory`** | ← **chosen.** HEAD `40bc332`, **2026-09-03**. No `machine-assignments.tsv` at all |
| `$HOME/claude-memory` | current, has the file |

`owner_of()` opens with `[ -f "$ASSIGN" ] || { echo "$fallback"; return; }` — a **silent** `DEV`.
So even with Cause 1 fixed, invoking the script this way still hides everything, and says nothing
about why. `[verified-numerically 2026-09-07]`

The deeper point: **every gate row is read from `$REF` (`origin/main`) via git, but `ASSIGN` is read
from the working tree.** That is the one input that can silently go stale, and it is the input that
decides whose work is whose.

Suggested fixes, cheapest first:
1. Read the assignments from `$REF` like everything else: `git -C "$MEM" show "$REF:machine-assignments.tsv"`.
2. Failing that, **warn instead of defaulting**: if `machine-assignments.tsv` is missing, print one
   line saying so and that every project is being treated as `DEV`. A missing input that changes
   every answer must not be silent.
3. Prefer a candidate that actually has the file, not merely a `status/` directory.

## Why the SessionStart board looked fine

The home PC's hook calls the script by absolute path from the direct clone
(`C:/Users/TD3KX/claude-memory/tools/gate-scan.sh`), where `./status` and `./.git` both exist, so
`MEM="."` and Cause 2 does not bite. Cause 1 still does — but `--brief` does not filter by machine,
so nothing showed. **The bug is invisible in exactly the view the user sees every session.**

## What this does not affect

Nothing about any row's content, tag or gate. `gs-scan.sh` check 7 delegates to `gate-scan --check`,
which does not call `owner_of`, so the "gate blocks clean" result stands.

Lane: /gs
