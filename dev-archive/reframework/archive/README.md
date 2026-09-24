# Archived scripts

Scripts that were tried in the game, did not do their job, and were taken back out. Kept here so they can be
picked up again; none of them is installed.

- `visceral_raise_mute.lua` (2026-09-24): set layer 3's BlendRate to 0 while a Hold_Start raise slot plays, to
  stop the raise pulling the hips at the aim press. It ran as designed (muted and restored on every press) but the
  measured hip movement at the press did not change (run C, `/lm` 2026-09-24 21:56). The nudge's likelier source is
  `visceral_spine_straighten.lua` (see modding-notes 2026-09-24, `/lm` 21:50 section).
- `visceral_freeze_idle_probe.lua` (2026-09-24): TEMPORARY measurement aid, sets layer 0 speed to 0 so the idle sway stops and only the aim press moves the body. It did its job (found `FirstPerson_RotateBody`); kept for the next such measurement.
- `visceral_fire_nohold_test.lua` (2026-09-25): forced the ATTACK order (setForcePrecede) while the fire button was down, without aiming. Result: the order is accepted but the game plays only a dry fire (no bullet). Kept as the reference for the micro-latch work.
