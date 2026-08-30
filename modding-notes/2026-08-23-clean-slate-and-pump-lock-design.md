# 2026-08-23 — The clean slate, and the first design note (pump lock)

## The clean slate

Tonight the user deletes Resident Evil 2 and every RE2-related file from this
PC — game, mod files, study material, all of it. From here on, Visceral is
built with exactly this:

- **Our GitHub** as the single source of truth: the frozen Arcade Controls
  repositories (knowledge and history), these Visceral repositories, and the
  engine dossier / playbook. Everything we do gets pushed as we go.
- **Local clones as the fallback** if GitHub is ever unreachable (network
  trouble or similar) — same content, synced whenever the connection is back.
- A **fresh game install with base REFramework only** when work begins.
- **No downloaded prior art.** Andyalpa's mod is no longer on this PC to study
  for hand animations and manual reloads — those get figured out from zero,
  documented here, and built as our own version. When we get stuck, we
  research new approaches rather than peeking at someone else's solution.

Before deletion, every RE2 folder on the machine was hash-audited against the
repositories; everything that was ours and unique is archived (see the
Arcade Controls dev-archive, `history/2026-08-07_uninstall_backup/` and the
final-sweep commits of 2026-08-23).

## First design note: the shotgun pump lock

Arcade Controls used an LG+LT gesture to rack and pump (a study-derived
mechanic). That feature is retired. The Visceral design, stated by the user
tonight:

- **RG + LG = both hands on the shotgun**, with LG holding the pump handle
  (the fore-end).
- **LT does not pump.** Instead, LT **releases the action lock** (the
  slide-release) — after which the pump can be worked **as many times as
  wanted** with the LG hand's own motion, without letting go of LG.

In other words: the trigger-hand button models the real slide-release lever;
the pumping itself is physical hand motion while the off-hand stays gripped.
Details (thresholds, chambering rules, interaction with shell insertion) are
future work — this entry just pins the intent before the rebuild starts.
