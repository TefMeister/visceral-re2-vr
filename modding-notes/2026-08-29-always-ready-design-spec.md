# Always-ready state: the four requirements (user dictation, 2026-08-29, home PC)

The no-grip-to-shoot latch (`setForce(HOLD, true)`, proven 2026-08-27) graduates
from "possible" to "feature" only if it clears these four. Dictated by the user;
order matters — 1 and 2 are the gate for everything else.

## 1. Doors must still open while latched

## 2. Items must still be pickable while latched

## 3. Player speed must keep driving the world — and speed comes from the stick, not a button

There is **no run button**: Leon runs from left-stick deflection alone.

| LS deflection | speed tier |
|---|---|
| 1–30% | sneak |
| 30–70% | walk |
| 70–100% | run |

Enemies are alerted by player speed, so whatever we do to the weapon-ready
state's movement, speed must still trigger:

- a) **enemy awareness** (the noise/alert system keyed off movement speed),
- b) **footstep sound** (sneak/walk/run footsteps must match the actual tier),
- c) **footstep animation** (leg movement must match the tier).

The tricky part: we change the movement speed of **only** the "weapon ready and
aiming" state — nothing else. And if at all possible, footstep sound and leg
animation swap per tier too, so a latched player at 80% stick reads to the
game (and the ear) exactly like a running player.

## 4. The game's default aim animation is banned

We cannot use the vanilla aim body animation at all. The body needs exactly
**three states**, driven by the same stick tiers:

- standing still,
- walking (anything moving below 70%),
- running (70–100%).

The spine-correction knowledge from the AC era (studied, not copied — see
`2026-08-27-spine-twist-mechanism-study.md`) is the relevant prior art for
divorcing the upper body from the locomotion animation.

## Status

Probe v3 (dev-archive `bbc015e`) is deployed and answers 1 & 2 empirically:
latch with NUMPAD7, try a door and a pickup. Control test matters — first try
the same door/pickup while aiming *vanilla-style* (hold RMB): the latch forces
the same HOLD bit vanilla aiming sets, so latched behavior should mirror
vanilla-aim behavior. If vanilla-aim already blocks interaction, the gate
exists regardless of us and becomes a hunt of its own; if vanilla-aim allows
it, the latch almost certainly does too.

Items 3 & 4 are big (locomotion-speed override scoped to one state + animation
layer replacement) and wait until 1 & 2 pass.
