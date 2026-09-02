# A writable speed lever exists: the motion layer's playback speed — and with root motion it moves the feet and the body together

**Status:** 🆕 new · **Priority:** high — it answers the board's "MAIN technical risk" (does a
writable locomotion movement-speed parameter exist?) with a public, shipping precedent, and the
answer changes which lane req 4 lands in.

## What was found

Two public REFramework scripts change the player's movement speed in RE Engine titles without
touching the FSM `[reported 2026-09-02, from source]`:

- **`Junh2x/RE9-Movement-Speed-Mod`** (Requiem; the RE2 port "Better Movement Speed RE2" on Nexus is
  the same author's remake — its page 403s automated fetch, so the RE2-specific hook name is unread).
  It does two things each frame from a `LateUpdateBehavior` callback: it walks
  `CharacterManager → getPlayerContextRef() → get_GameObject() → getComponent(via.motion.Motion)
  → getLayer(0) → get_HighestWeightMotionNode() → get_MotionName()`, decides walk vs run by matching
  `"walk"`/`"run"` in the motion name (and the character by the `ch0100`/`ch0200`/`ch0300` prefix),
  and **writes `set_Speed(multiplier)` on the motion layer**. Separately it `sdk.hook`s
  **`app.MovementDriver.getMoveSpeed`** and scales the return value — that half is Requiem-specific
  (RE2 has no `app.MovementDriver`; its equivalent lives under `app.ropeway`).
- praydog's own `re2_smooth_movement.lua` in REFramework's `scripts/` takes the other route entirely:
  it replaces root motion by **writing the body `via.Transform` position each `UpdateMotion`** toward
  the left-stick direction, smoothed. That is a position write, not a speed parameter — the "manual
  root motion needs `warp()` too" caveat from the CAF topic applies.

## Why the motion-layer speed is the lever req 4 wants

`via.motion` is engine-level, present in every RE Engine title, and **`MotionLayer.set_Speed` is
writable from Lua** — it is what the Requiem script ships on. In RE2, locomotion is root-motion driven
(dossier §8: the same bank ids play whether armed or unarmed; movement comes out of the animation),
so scaling the layer's playback speed **scales the body's travel, the leg cycle and the footstep
events together**, because they are one animation. That is req 4's "drive footsteps, leg animation and
awareness from measured speed" satisfied by construction rather than by a blend parameter — the
speed *is* the playback rate.

What it is not: a blend between walk and run poses. A run animation at 0.6× is a slow run, not a
walk. So the unified aim/docked/run cap in req 4 becomes: pick the bank/state the game would pick,
then clamp the layer speed to `cap / nominal_speed_of_that_motion`, which the script's
motion-name match already shows how to gate. Aim and idle motions on the same layer must be
excluded by name, as the script does, or they slow down too.

## What is NOT established

- **Which layer.** The script uses `getLayer(0)`; RE2's locomotion may be on another layer index
  (dossier §8: the grip lives on a separate layer). One probe line — dump `get_MotionName()` per
  layer while walking — settles it.
- Whether RE2's `via.motion.MotionLayer` exposes `set_Speed` identically — near-certain
  (engine-level type), but `[reported]` until the reflection dump says so.
- Whether awareness (enemy hearing) reads the motion or a separate speed value — the Requiem script
  does not touch awareness, so req 4's awareness half is still ours to verify.

## Concrete next steps

1. Add to the queued reqs-1-and-3 probe: enumerate the player `via.motion.Motion` layers with
   `get_MotionName()` and `get_Speed()` while walking and running — reads only.
2. If locomotion is on a findable layer, prototype req 4's cap as a `set_Speed` clamp gated on
   motion name; measure travel speed against the existing telemetry.
3. Keep praydog's transform-write route as the fallback for locomotion that root motion cannot
   express (analog strafing), with the `warp()` caveat.

## Sources

- https://github.com/Junh2x/RE9-Movement-Speed-Mod — `reframework/autorun/re9_movement_speed.lua` (no licence file visible; study-only)
- https://www.nexusmods.com/residentevil22019/mods/2391 — "Better Movement Speed RE2" (403 on fetch; existence and feature list from search)
- https://github.com/praydog/REFramework/blob/master/scripts/re2_smooth_movement.lua — MIT; the transform-write route
