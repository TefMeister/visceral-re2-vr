# Two-handing is what makes a weapon accurate

Order: 5008
From: mod-ideas `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-09-23

`[raw]` · `[not judged]` — ⚠️ **nothing checked against the game; see the caveat below**

> "two handing a weapon is what makes the bullet spread go away and the weapon becomes accurate.
> shooting with only one hand makes the bullet spread area larger. in RE2 there is a mechanism that
> makes the accuracy lower when moving, then i think it is better when just starting to aim and gets
> nice and accurate when not moving for a few seconds. i want to use that accurate state the moment
> second hand gets docked on the weapon, handguns and long weapons alike, and the "just pressed aim"
> state when one handing any weapon in the game"

Verbatim record: [`inbox/2026-09-20c-re2-two-handing-sets-the-accurate-state.md`](../inbox/2026-09-20c-re2-two-handing-sets-the-accurate-state.md)

Nothing new is being invented here: the game already swings between a loose spread and a tight one,
and the idea is to hang that swing on **how many hands are on the weapon** instead of on **how long
you have stood still**. Both hands on it = the settled, accurate end, immediately. One hand = the
just-raised, loose end, for every weapon in the game including handguns.

⚠️ **The caveat, and it is the whole of the judging:** this rests on the game having an accuracy
value that *settles over time while you stand still*. That is read off how the game feels to play,
not out of its data — nobody has looked. If the spread turns out to be a fixed per-weapon number
with a separate movement penalty bolted on, the idea still works but is a different job.

Reads together with the entry just above: that one wants "the weapon is in both hands" separated
from "the game thinks you're aiming", and this one needs that separation to exist before it can
hook onto it.

---
