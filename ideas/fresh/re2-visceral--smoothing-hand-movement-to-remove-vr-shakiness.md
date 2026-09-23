# Smoothing hand movement to remove VR shakiness

Order: 5005
From: mod-ideas `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-09-23

`[raw]` · `[not judged]` — *nothing checked against the game*

> "smoothing of hand movement, makes it a little laggier, but removes the shakyness that comes from
> the slightest hand movement in vr"

Verbatim record: [`inbox/2026-09-15e-re2-village-hand-smoothing.md`](../inbox/2026-09-15e-re2-village-hand-smoothing.md)

Filed on both this page and [`re-village.md`](re-village.md) — same idea, tagged for both games.
A trade explicitly acknowledged by Tefa: a small amount of added lag in exchange for less jitter on
tiny controller movements. The usual route for this is a rolling average or low-pass filter on the
tracked hand pose before it reaches the game — cheap in principle, but "how much smoothing before it
feels laggy instead of steady" is a headset judgement call, not something to tune on paper.

---
