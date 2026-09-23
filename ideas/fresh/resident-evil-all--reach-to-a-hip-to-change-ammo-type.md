# Reach to a hip to change ammo type

Order: 8000
From: mod-ideas `games/resident-evil-all.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/resident-evil-all.md>), copied 2026-09-23

`[considered]` — *answered 2026-09-14* · `[looks doable]` for the reaching part, `[not judged]` for the swap itself — *nothing checked against the game*

> "ammo type swap - 2 poches, one on left hip, other on right hip"

Two pouches on your body, one on each hip. Reach down to the left hip for one ammo type, the right
hip for the other, instead of pressing a button or opening a menu.

Verbatim record: [`inbox/2026-09-14-re-games-ammo-pouches-on-the-hips.md`](../inbox/2026-09-14-re-games-ammo-pouches-on-the-hips.md)

**⭐ Why this is a better idea than it might look.** There is a standing rule on this account, learned
the hard way in a headset: **the left hand must never sit behind the right**, because the headset's
own cameras lose sight of a controller hidden behind the other one and tracking goes unreliable at
once. **Hip pouches put both hands out at your sides, in clear view of the cameras.** So this is not
just a convenient place for a gesture — it is one of the *safest* places to put one.

**What it'd take**, in two halves, and they are not the same size:

- **Knowing you reached for your hip** — the cheap half, and the one we already have the pieces for.
  It is a distance check: where is the controller, relative to where your head is and which way your
  body faces. The Village scope work already reads head and controller positions every frame for a
  living. ⚠️ The one genuinely fiddly part is that a hip is *relative to your body, not the room* —
  so it has to follow you as you turn, and it needs a sensible guess at where your waist is given
  only a headset height.
- **Actually changing the ammo** — not judged yet. The types exist and the game already swaps
  between them, so the question is only whether the mod can ask it to from outside. That is a
  question for a session with the game running, not something to read off disk.

**✅ ANSWERED 2026-09-14 — and the answer is more specific than either option offered.** It is not
"two pouches" as a fixed layout; it is **one pouch that becomes two only when a weapon needs it**:

> "normally left hip has a ammo pouch that activates with left hand, but if a weapon has 2 different
> ammo types, example grenade launcher having flame rounds and acid rounds, then flame would be left
> hip, acid rounds right hip, activated with the left hand and LG to take ammo out."

Verbatim record: [`inbox/2026-09-14e-re-games-ammo-pouch-answer.md`](../inbox/2026-09-14e-re-games-ammo-pouch-answer.md)

In plain terms:

- **One ammo type (the normal case):** only the **left hip** has a pouch. The left hand reaches there.
- **Two ammo types (grenade launcher: flame / acid):** left hip is the first, **right hip** the
  second. Still the **left hand** that reaches, either side.
- **The grab is the left grip** — reach, grip, ammo comes out.

⭐ **This is a better shape than the fixed two-pouch version.** The right hip stays empty until there
is a real reason for it to exist, so there is nothing to reach into by accident on the many weapons
that take only one kind of ammo. It also teaches itself: a second pouch appearing *is* the signal
that this weapon has a choice to make.

⚠️ **It does add one thing that isn't free:** the pouches now have to appear and disappear as you
change weapon, which the fixed version did not. Still cheap, but it is a rule about the current
weapon rather than a fixed pair of zones. **Not checked against the game.**

⚠️ **And one thing still unstated:** reaching left-hand-across to the **right** hip pulls the left arm
over the body — the shape the headset cameras dislike, and the reason hip pouches looked safe in the
first place. Probably fine (the hand ends up out at the far side, not tucked behind the other
controller), but it is the specific thing to watch on the first wear.

⚠️ **Worth saying plainly:** a gesture like this is only good if it is reliable. A reach that misses
one time in ten during something frightening is worse than the button it replaced. Whatever the zone
ends up being, it wants to be generous, and it wants testing while something is actually chasing you
— not standing still in a safe room.
