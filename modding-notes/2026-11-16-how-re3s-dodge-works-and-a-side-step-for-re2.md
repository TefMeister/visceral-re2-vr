# 2026-11-16: how RE3's dodge works, and a side step for RE2 (Developer PC, Opus, both games on the simulator)

Tefa: *"please don't use these external mods and instead see if you can find out what makes re3 dodge work, then we need
to somehow make this work in re2 - not like re3 ... see if you can understand how re3 dodge works so we can make a plan
on how to implement your idea of a side step into re2. pc all yours"*. No outside mod was read or used.

## RE3's dodge, read live `[verified-live 2026-11-16, simulator, n=1 dodge]`
RE3 (namespace `offline`) is built on RE2's code (`app.ropeway`); its additions carry "Es" (Escape, RE3's working name).
- **It is one animation, and the animation moves Jill.** Dodge to the right (left stick + right stick click through our
  patch): motion bank 0, id 1035, 76 animation frames (about 1.27 s at 60). The move comes from the animation's own
  root motion through `via.physics.CharacterController`: about 2.0 m in all: slow for the first ~0.2 s (0.2 m), fast
  in the middle (~1.3 m over ~0.4 s), a slow tail (~0.5 m). Bank 1 / id 190 before it was the walk start (the stick
  was pushed 0.3 s before the click).
- **A normal dodge does NOT make Jill invincible.** `offline.HitPointController` `Invincible`, `TrackInvincible` and
  `NoDamage` stayed off all the way through. What the animation turns on is `TrackEmergencyDodge` (set by a track on the
  animation, `EsPlayerFlagTrack` / the dodge's tracks), on for the first ~0.23 s (animation frames 2 to 14).
- **The perfect dodge**: a hit that lands while `TrackEmergencyDodge` is on goes to `offline.Collision.HitController`
  `callbackDamageEmergencyDodge` / `EmergencyDodgeDamageHitHandler` instead of doing damage, then
  `PlayerCondition.onHitEmergencyDodge`: the slow motion (`DodgeSlowTime` = 1.0 s, `offline.escape.SlowControllerBehavior`
  slows the motion layers), the camera turned to the attacker (`EsPlayerMoveSystem` `DodgeForceTwirler...`), and a
  second dodge allowed (`Enable2SDodge`, `SecondEnableEmergencyDodge`, `ThirdEnableEmergencyDodge`). Outside the window
  a hit lands as usual.
- The request: `offline.survivor.player.EsPlayerActionOrderer` (`checkOrder`, `EnableHoldStartDodge`, the
  `...StartDodgeTimer`s: a dodge from aiming and from a step are allowed at their own moments).

## What RE2 has to build one from `[verified-live 2026-11-16, simulator, n=1 each]`
- The same base parts as RE3 without any of the dodge ones: `app.ropeway.HitPointController` with `Invincible` and
  `NoDamage` (flipped on and off by script, read back each time), `app.Collision.HitController` (`checkInvincible`, an
  avoid list), `via.physics.CharacterController` (`warp`), `MotionFsm2`, `PlayerActionOrderer` (no Es move system, no
  slow controller, no dodge animation for Leon or Claire).
- **Moving the player by script works and respects walls**: the player's `Transform.set_Position` nudged a little every
  `UpdateBehavior` frame moved Claire 1.0 m over 15 frames and she stayed there; pushed 4 m into something she stopped
  after 0.6 m and slid along it instead of passing through (two other directions went the full 4 m in the open).
- b212 (the cutscene screen switched on) loaded and played on the simulator for ~4 min with no fault (no cutscene met).

## The plan for RE2's side step (for Tefa's yes)
1. **Input**: left stick direction + right stick CLICK (the shared layout); stick centred = a step back. In our native
   plugin, C++ (the button state is already there for the shared layout).
2. **The move**: along the stick's direction in the head's yaw, about 1.0 m over about 0.3 s, eased like RE3's (slow
   in, fast, slow out), by the transform nudge each `UpdateBehavior` frame (the game's collision keeps it out of walls).
   Numbers in one settings table, tuned in the headset.
3. **Not being hit**: RE2 has no perfect-dodge window; the plain way is `HitPointController.Invincible` on for the
   step's first ~0.25 s (RE3's window length), then off. (A later step could copy the perfect dodge: a hit inside the
   window = a short slow motion; RE2 has no slow controller, so it would be the game's time scale.)
4. **Feel**: a short buzz, ~0.8 s before the next one, none while aiming down a long gun two-handed or during a grab,
   ladder, cutscene or menu. Leon and Claire have no dodge animation: the body (legs) may just slide; the view moves.
5. **Checks**: flat first (the step's distance and the walls by the log), then the simulator (VR Bench presses), then
   Tefa in the headset for comfort (a 1 m move in 0.3 s may want to be shorter or slower).
`MODEL: OPUS` to build; the slow-motion perfect dodge later is `MODEL: FABLE`.

## Built the same afternoon (Tefa: "yes and if this is possible to do, then this type of dodge would be preferred in re3 as well")
- `src/hands/sidestep.cpp` (shared with RE3 through `sync_shared.py`): 1.0 m over 0.30 s, smoothstep, Invincible for 0.25 s
  (left alone if the game had it on), 0.8 s from step to step, a short buzz, only while FirstPerson drives the view and
  not with the left hand on the gun. Every number in `sidestep.h`. The bridge Lua sends the right stick click (slot 39),
  the left stick turned by the game camera into a world direction (604-605) and the camera's level forward (606-607).
- RE3: our REFramework (`2026-11-16b-re2-re3-sidestep-click.patch`) gives the right stick click to the side step when
  `visceral_native.dll` exports `visceral_own_sidestep()` = 1; the game's dodge action itself still dodges.
- Simulator `[verified-live 2026-11-16, simulator, n=1 each]`: RE2 (b213): stick right = the camera's right, centred = straight
  back, left = left; the step to the right went into a wall, stopped at 0.28 m and slid along it. RE3 (b022): the click
  side-stepped, Jill's dodge animation did not play, Invincible on for ~0.26 s. Not worn; no enemy has swung at it yet.

## ⚠️ What was really installed today (found 16:2x)
`--deploy` installs the REFramework named in `tools/install-list.txt` (RE2: the 11-02 build; RE3: d60128e7 of 11-02) and the
repo's `game-folder/re2_fw_config.txt`, so every REFramework copied in by hand was put back by the next --deploy. Tests run
right after a hand copy (no deploy after it) were valid: RE3 b022 side step, b023 roll off, b024 A / Y, RE2 b214 A / Y. NOT
valid: RE2 b213 and its REFramework parts (b213's snapshot holds the 11-02 REFramework). Fixed: both install lists now name
our builds of patch 2026-11-16e (kept in each backup's `loaders/`) and the repo's config carries the cutscene-screen switch.
