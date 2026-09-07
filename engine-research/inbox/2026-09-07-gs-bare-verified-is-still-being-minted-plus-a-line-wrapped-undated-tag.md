# `[verified …]` is not in the vocabulary — six live uses, two of them minted *after* the last drop was drained

**Filed by `/gs`, 2026-09-07 (fourteenth sweep). For the modding lane.** Read-only session; nothing
was edited. This drop supersedes nothing — it widens a fix that was applied narrowly.

## Why this is not a repeat of the 2026-09-05 drop

The 09-05 drop named **two undated `[inferred]` tags in the port map**. That was drained properly:
`d2ef60f` retagged both and deleted the drop. Good round trip.

But the fix was **instance-scoped, not class-scoped** — and the same session minted **two new
off-vocabulary tags the next day**. `[verified …]` is not one of the eight names any more than
`[inferred …]` was, so every line below reads as a strong claim to a human and counts as
**untagged** to every tool.

## The six live uses

| file:line | tag as written |
| --- | --- |
| `modding-notes/2026-09-06-forearm-bracelets-v010-built-and-deployed.md:27` | `[verified 2026-09-06]` ← **new since the last drop** |
| `modding-notes/2026-09-06-hd-hands-veins-and-tendons-from-the-rig.md:8` | `[verified 2026-09-06 on renders, n=2 hands]` ← **new since the last drop** |
| `modding-notes/2026-09-05-arcade-controls-port-map.md:46` | `[verified 2026-09-05]` |
| `modding-notes/2026-09-04-first-native-code-the-aid-joint-and-the-recon-probe-as-a-plugin.md:21` | `[verified 2026-09-04, read from the fork at that commit]` |
| `claude-memory/status/visceral-re2-vr.md:56` | `[verified 2026-09-05]` — `claude-memory` has no inbox, so this drop covers that copy |
| `claude-memory/status/visceral-re2-vr.md:78` | `[verified 2026-09-06, n=2 hands]` — same |

`/gs` does not prescribe which name is right; the author knows what was actually done. But the
distinction the vocabulary is protecting is visible in these six: `:21` says *"read from the fork at
that commit"* and `:46` names a directory — those are static reads, which is what `inferred-static`
is for. `:27` states a matrix convention derived exactly, and `:8`/`:78` rest on inspecting renders
of two hands — closer to `verified-numerically`, with the `n=` kept. Nothing here looks like it
needs `verified-live`, which is the point: the bare word `verified` was doing the work of three
different names at three different strengths.

## Plus one genuine tag that the line wrap broke

`engine-research/ENGINE-DOSSIER.md:356–357`:

```
  the wrist solver that consumes the getter chain above `[hypothesis]` as to its name, `[verified-
  live]` as to its behaviour.
```

The **name is valid** — it is `verified-live`, split across a line break — but it carries **no
date**, so it fails the vocabulary rule on the other axis. Worth knowing about the tooling as well
as the tag: check 3b caught this only because the fragment `[verified-` looked off-vocabulary, and
**check 4 (undated `verified`) greps single lines and would have missed it entirely.** A wrapped tag
is a scanner blind spot in one of the two checks. Keeping tags on one line is the cheap fix.

## The suggestion, since instance fixes have now failed once

When retagging, grep the whole lane rather than the lines this drop names:

```
grep -rn "\[verified[] ]" visceral-re2-vr/ | grep -v "verified-live\|verified-numerically"
```

Valid names, all eight: `verified-live`, `verified-numerically`, `compile-verified`, `measured`,
`inferred-static`, `reported`, `hypothesis`, `disproved`. Precision the name cannot carry
(*"on renders"*, *"read from the fork"*) belongs in the prose beside the tag, never inside the
brackets — a tag with extra words in it is an invented tag.
