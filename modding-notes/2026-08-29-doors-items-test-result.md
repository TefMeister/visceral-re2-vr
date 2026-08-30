# Doors + items test result: the HOLD state itself is the blocker (2026-08-29, home PC, flat)

User ran the probe-v3 control test from the design spec
([2026-08-29-always-ready-design-spec.md]):

- **Doors: cannot be opened** while the latch is on — **and also cannot be
  opened while aiming vanilla-style (RMB held)**.
- **Items: cannot be picked up** — same in both cases.
- Latched behavior exactly mirrored vanilla-aim behavior, as predicted (the
  latch forces the same HOLD bit RMB sets).

So the block is **not our latch — it's the game's own aim state**. Interaction
is bundled into HOLD along with everything else the spec wants gone: the slow
aim-walk speed and the aim body animation. The user also confirmed from play
experience: **raising movement speed alone does not change footstep sound, leg
animation, or (almost certainly) enemy alerting** — those would need real
rewiring if we kept the character in the aim state and tried to fix it from
the inside.

## The reframe this forces

Forcing HOLD on was the right *proof* (firing works without touching the aim
button) but it is the wrong *feature lever*: latching HOLD drags the entire
stance bundle along — interaction lock, speed cap, banned animation. Fighting
all three from inside the state is the "ton of work" path.

The spec's four requirements all point at the opposite move: **leave HOLD
alone entirely and instead un-gate the fire path** — find where the game
ignores the ATTACK input when the character is not in the aim stance, and
remove that one check. If the trigger can fire from the normal
still/walk/run locomotion state:

1. doors work (never aiming),
2. items work (never aiming),
3. speed/footsteps/awareness stay 100% vanilla (normal locomotion the whole
   time — nothing to rewire),
4. the vanilla aim animation never plays (requirement 4 satisfied for free —
   the body is always in exactly the three states the spec wants).

All four requirements collapse into one hunt: **who consumes ATTACK, and what
condition gates it on the stance?**

## Risk / fallback

The risk: firing may not be a gated check but *structural* — the gun-fire
logic may only run inside the aim state's FSM node, in which case there is no
single check to remove. The FSM-writing lane is known dangerous (IsJog freeze
lesson). If the hunt finds structure instead of a check, the fallback is the
original 08-27 design: **latch HOLD only while the finger is on the trigger**
— all the stance downsides then exist only during the moment of firing, and
you are never touching the trigger while opening a door or grabbing an item.
(The banned-aim-animation requirement would then still need its own separate
solution for that firing moment.)

## Next

Probe v4: instrument the fire path — live method dump around the equipped
weapon (`app.ropeway.implement.Gun` and relatives), hook the candidate
trigger/fire methods, log call-site conditions with HOLD on vs off, and find
the gate.
