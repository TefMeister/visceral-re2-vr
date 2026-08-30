# Study: how Arcade Controls raised the aim-walk speed (prior art, 2026-08-29)

Studied the frozen `arcade-controls-re2-vr-dev-archive/reframework/autorun/re2_smooth_movement.lua`
to answer "how did AC make aiming not force a walk?" Per the ground-up rule this is
KNOWLEDGE only — Visceral writes its own fresh. ([[cross-machine-brain]], no-copy directive.)

## The key realisation: AC did NOT touch a game speed variable
It never found or wrote an "aim-walk speed" value. Instead AC **already owns
locomotion** — it's a VR smooth-movement system that overrides the player's world
position every frame (`via.Transform.set_Position`), moving the body toward the
stick direction relative to camera facing, instead of relying on the game's native
root-motion movement. This matches (and explains) our own finding that the aim-state
slowdown is a separate cap that `Jog` can't override: you don't fight it, you
bypass it by driving position yourself.

## The exact mechanism
- Aim detection: `SurvivorCondition.get_IsHold()` (same signal we use).
- Per frame (`on_pre_application_entry("LockScene")` region):
  - **Not aiming:** measure the game's own per-frame position delta (`observed_speed`,
    capped at 1.0), EMA-smooth it, and re-aim that speed along the joystick+camera
    direction. So normal movement inherits the game's speed but goes where the stick
    points.
  - **Aiming:** *bypass* the throttled native magnitude entirely — compute
    `speed = stick_mag * AIM_SPEED_OVERRIDE_MPS * delta_t`, where
    **`AIM_SPEED_OVERRIDE_MPS = 1.3`** (m/s), the pace chosen to match walking so
    two-handing a weapon doesn't force a crawl. Deliberately does NOT feed
    `last_ema_speed`, so the EMA resumes cleanly when aiming ends.
  - Apply: `new_pos = last_player_position + (camera_relative_stick_dir * speed)`,
    keep current Y, `body_transform:set_position(new_pos)`.
- Camera-relative dir: flatten camera forward (y=0), quat it, rotate the stick
  vector `(axis_l.x, 0, -axis_l.y)`.

## What carries over to Visceral
1. **The aim-slowdown fix is architectural, not a variable poke:** own the
   locomotion (position override) and feed your own speed while aiming. Confirms the
   VR-native reframe in the design spec — the game's aim state need not gate movement
   speed because WE move the body.
2. **The `1.3 m/s` number** was AC's "match walking" pace. Visceral's spec v2 wants a
   different target — ONE unified cap = vanilla RUN (~3.77 u/s measured) with analog
   scaling by stick deflection — so we'd feed `speed = stick_mag * UNIFIED_CAP * dt`
   rather than a walk-matched 1.3. Same formula, different constant + analog input.
3. **Speed source = stick magnitude** (`axis_l:length()`, capped 1.0) → this is the
   analog deflection the spec's req 4 wants; in VR it's the real thumbstick, so the
   "1–30/30–70/70–100%" tiers fall out of `stick_mag` naturally.
4. **Footsteps/anim:** AC drove position directly; leg anim stays the game's (in VR
   first-person you rarely see your legs). Footsteps are velocity-driven
   (`WwiseVelocityTriggerList`) so real position movement should still trigger them.
   The spec's "speed-driven presentation" may largely reduce to: move the body at the
   right speed and let velocity-driven systems follow.
5. **Ordering:** AC wrote position around `LockScene` / the render-position hook, with
   a `has_valid_player_position` guard and re-seeding on scene locks — worth
   remembering for our own write timing.

## Open question this resolves vs leaves
- RESOLVES: docked-at-full-speed does NOT need a game speed variable — position
  override + our own speed does it (proven approach in AC, at 1.3).
- LEAVES (VR-only): does driving position while the sustained aim state is on cause
  any conflict (root-motion fighting our writes, footsteps, enemy awareness)? AC ran
  this way and shipped, so likely fine — verify in the headset for Visceral's tiers.

## Next
Visceral writes its own smooth-locomotion from scratch: position override, camera-
relative stick dir, `speed = stick_mag * UNIFIED_CAP * dt` (UNIFIED_CAP ≈ run), same
value whether aiming or not (spec v2 unified speed). Prototype once VR is on.
