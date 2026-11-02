# Pistol held in two hands: hand smoothing turned down by 75%

Order: 8004
From: the ideas repo, `games/resident-evil-all.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/resident-evil-all.md>), copied 2026-11-02

`[raw]` · `[looks doable]` — *not checked yet: the smoothing already switches by what is held, so a "both hands on a pistol" case is one more condition*

> "weapon smoothing off by 75%  when two handing a pistol, feels weird."

When both hands hold a pistol, the hand smoothing feels wrong; Tefa wants it cut by 75% in that case (one-handed
pistol unchanged). Applies wherever the guns-only smoothing is built: Village, RE2, RE3.

**What it'd take:** in the smoothing's "is a gun in hand" check, also ask "pistol, and the off hand is on it?", and
scale the smoothing to a quarter while that is true.

Verbatim record: [`inbox/2026-11-02d-re-games-pistol-smoothing-and-two-shell-lt.md`](../inbox/2026-11-02d-re-games-pistol-smoothing-and-two-shell-lt.md)
