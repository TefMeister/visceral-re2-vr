# A drop with two asks gets its cheap half done and its expensive half forgotten — and check 1 cannot see the difference

**Filed by `/gs`, 2026-09-07 (fifteenth sweep, the post-four-lane pass). For the modding lane.**
Read-only session; nothing was edited anywhere. **This drop is about how drops are written and
drained, not about visceral-re2-vr** — see "Why this landed here" at the end.

Supersedes: nothing. It corrects a diagnosis **I** gave the user earlier today, in chat only.

## 1. What I got wrong, so the evidence is not read the wrong way

I told the user far-cry-2-vr's dossier had carried a known-false date through three sweeps because
*"no lane has landed on that project"*, and that the fix was to make `/pd` drain estate-wide. **The
first half is false.** A modding-lane session was in that repo on 2026-09-05 and committed to
`engine-research/` — `4e55094`, 15:06.

## 2. What actually happened, commit by commit

| when | commit | what |
| --- | --- | --- |
| 09-04 19:02 | `09ab066` | `/sr` drops: #1253's last activity is **2020-04-22**, not 2019-11-23 |
| 09-05 14:42 | `6e99fff` | `/gs` drops: **the dossier still carries the superseded date, AND two off-vocabulary `[verified-static]` tags** |
| 09-05 15:06 | `4e55094` | modding lane acts — **24 minutes later** — retagging both `[verified-static]` → `[inferred-static]`. Two lines. |
| | | **The date was not touched. Neither drop was deleted.** |
| 09-07 11:25 | `1fd9b48` | `/gr` CHECK-IN stamp — correctly leaves the modding-owned inbox alone |

`ENGINE-DOSSIER.md:382` still reads *"still open, no Valve response, last activity 2019-11-23"*
today. `[verified-numerically 2026-09-07]`

So the session read the drop, did the two-line half, and left the research half. Leaving the file in
the inbox was arguably **right** — a non-empty inbox is a visible to-do, and the work was genuinely
unfinished. The defect is that nothing anywhere records *which* half remained.

## 3. The mechanical consequence: check 1 reports age, and age means two different things

`/gs` check 1 prints one number per file — its age. A drop that has been **read, half-satisfied and
consciously left open** is indistinguishable from one **nobody has opened**. Both far-cry-2 drops
have been reading as "1d / 2d, the owner hasn't run yet" for three sweeps, while the truth is "the
owner ran, did 40% of it, and moved on".

That is why the wrong date survived: every sweep since has classified it as *waiting*, which needs
no action, rather than *stalled*, which does.

## 4. The cause is on my side of the fence — the drop bundled two unrelated asks

The 09-05 `/gs` drop asked for a **factual correction sourced from someone else's research** and a
**two-line mechanical retag** in one file. Those have wildly different costs. Bundled, the cheap one
is done and the file still looks handled.

This is the same failure shape as the other finding in today's earlier sweep — the visceral
`[inferred]` drop got its *named lines* fixed and not its *class*, and two new bad tags appeared the
next day. **Drops get partially satisfied, and partial satisfaction is invisible.** Twice in three
days, in two different repos, is a pattern in how `/gs` writes drops, not a discipline problem in the
lanes reading them.

## 5. Three changes, cheapest first — all of them the modding lane's call

1. **One ask per drop file.** `/gs`'s job, and I will follow it from now on. If a sweep finds a fact
   correction and a tag correction in one dossier, that is two files. Filenames already carry
   date+author+slug, so two files never collide.
2. **A drainer who does part of a drop appends one line to it before leaving** — e.g.
   `PARTIAL 2026-09-05: tags done (4e55094); the #1253 date is NOT done.` This is the one place
   the create-only rule should bend, and only for the drop's own drainer, appending only. It costs a
   sentence and converts an ambiguous age into a status. Alternative if create-only must hold
   absolutely: file a **new** drop naming what remains.
3. **Then check 1 can grow a real signal** — a drop whose repo has a *later* modding-lane commit
   touching non-inbox files is **stalled**, not waiting. That is computable from git with no new
   convention at all, and would have flagged far-cry-2 on 2026-09-06. Worth doing even if (2) is
   rejected. `[hypothesis]` — I have not written it; `/gs` curates no tooling.

## 6. The scheduling gap is real too, just smaller than I claimed

Six projects hold drops with no modding-lane visit since:
`XIII2003-vr` (2, since 09-04) · `enslaved-vr` (2, since 09-04) · `mad-max-vr` (2, since 09-04) ·
`manhunt-2003-vr` (1, since 09-03) · `psychonauts-vr` (1, since 09-03) · `unreal-gold-vr`
(1, since 09-04). `[verified-numerically 2026-09-07]`

`/pd` is already estate-wide **in scope** (`commands/pd.md` §2) but drains only projects it *takes*
(§3.3), ordered by the home-PC desktop-shortcut order (§2) — an order with no relationship to where
the backlog is. **Suggestion: decouple the drain from taking a project.** A drain is static, cheap
and needs no analysis of that game; taking a project is the expensive part. Draining every in-scope
inbox would clear the estate's modding backlog in one run even when only two games get advanced.

## Why this landed here

`claude-memory` has no `inbox/`, and this concerns `commands/pd.md` and `/gs`'s own habits, so there
is no project-shaped home for it. It is filed in the **most-visited** modding project — this repo took
modding-lane commits on 09-04, 09-05, 09-06 and 09-07 — because a drop about drops going unread
should not be filed where drops go unread. **Nothing in it is about visceral-re2-vr**; fold it into
`claude-memory/commands/` and `CONVENTIONS.md`, not into this project's dossier, and delete it here.
