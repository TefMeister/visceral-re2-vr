# The hold follows the aim input one-to-one; flat, it never drops (2026-10-02, home PC, `/lm`, two flat launches)

**Question (board, 2026-09-30):** on the relaxed-walk splice, a shot fired while stopping ends the aim (hold) state
by itself ~50 frames later in VR (b039, n=4, right grip still squeezed). Why?

**Game:** `D:/RE2 test copy` (b040 → b041), Leon, handgun, the RPD main-hall save, flat (the VR loader parked for
the run and put back after), praydog pd-upscaler a24c3459, our b015 lists + `visceral_lefthand_hold.lua` +
`visceral_swing_probe.lua` + `visceral_title_one_scene.lua` in place throughout. New: `visceral_hold_exit_probe.lua`
(`dev-archive/reframework/autorun/`) and `dev-archive/tools/hold_exit_drive.py`. Evidence:
`dev-archive/recon/2026-10-02-hold-exit-flat/` (probe-line extracts of both runs).

## What is now established

1. **IsHold has no writer. It is a read-out of the motion FSM** — `get_IsHold` = `StateTagHandle.hasTag(HOLD)`; the
   tag bits are rewritten only when a node is set up on a layer (`MotionFsmTagCollector.onTransitionHandler`,
   TransitionState Setuped) `[measured 2026-10-02, reader, disassembly]`. So "the hold dropped" = the FSM set up a
   node without the HOLD attribute on layer 0. Nothing in that chain reads a motion ID or a bank, so the splice is
   invisible to the tag `[inferred-static 2026-10-02, reader]`.
2. **HOLD is a Petient order (16; HOLD_WALK 64) decided every frame by `PlayerActionOrderer.checkOrder(Petient)`**:
   not InConstraint · the 0.5 s `ForbidHoldDeferTimer` not completed (it runs while `Condition.IsForbidAim` =
   `PlayerForbidAimController.get_IsForbid`, the stock "gun lowers at a wall" cast from the camera's view position
   along its forward axis) · equipment valid · `InputSystem.isOn(HOLD 0x40)` · `Equipment.get_EnabledHoldMainWeapon`
   `[measured 2026-10-02, reader, disassembly]`. The shot's `SurvivorRejectPrecedeOrdersTrack` is Precede-only and
   cannot touch HOLD; ammo, `requestFire` and reload are not in the decision.
3. **Flat, with the game's own HOLD input latched (`InputSystem.setForce(64, true)`), the hold NEVER dropped**
   `[verified-live 2026-10-02, n=14 real shots at the stop (ammo counted 12→6, `Equipment.requestFire` seen per shot),
   forward and back, W released 0.25 s before / at / 0.3 s after the shot]`. Layer 0 goes relaxed walk →
   `HG_Hold_Idle_Loop` directly at the stop, ~50 frames after the shot, hold up throughout, `IsForbidAim=false`,
   `precede=0`, `petient` 64 → 16. So **the FSM does not exit on its own on the splice; something in VR refuses the
   HOLD order.**
4. **The hold follows the input one-to-one.** A 1-frame gap in the HOLD input at f+48 → hold 0 for exactly one frame,
   then `HG_Hold_Start_R0` (the raise) and the idle ~35 frames later `[verified-live 2026-10-02, n=2]`. A 12-frame
   gap → hold 0 for 12 frames, layer 0 `OFF_GazingWalk_End_RL` (the relaxed walk's own stop motion), then the raise
   when the input returns `[verified-live 2026-10-02, n=1]`. **That is the 2026-09-30 shape** (#4: `OFF_GazingWalk_End_LR`
   → `OFF_Gazing_Idle`, hands back when the hold returned). The 09-30 drops lasted ≥ 0.7 s (caught by a 1 Hz line, the
   gun still moving at 120 frames), so in VR the HOLD order was refused for a long stretch, not one frame.
5. **The stock wall rule did not fire flat here**: aiming into a privacy screen and then with the muzzle ~0.5 m from
   a wooden board, `IsForbidAim` stayed false `[verified-live 2026-10-02, n=2 tries]`. Why is open (distance, filter,
   or praydog's FirstPerson camera) — the reader is reading `PlayerForbidAimController` now.

## What is NOT established

- **Which of the two refusals it is in VR.** Only two can last that long: (a) the HOLD input itself missing — praydog's
  `re8_vr.lua` turns the right grip into `GamePadButton.LTrigBottom` every frame, so a stretch of frames without it
  (tracking, a gating condition in the script, the shot) would do exactly this; (b) `IsForbidAim` — in VR the camera
  the cast runs from is the headset, and both 09-30 throws came while stopping (walking up to something).
- Whether stock files ever drop in VR (b039 ran on the splice only). The splice cannot matter to the tag itself, but
  the relaxed walk's root motion may change where/when the player stops in front of things.

## The one VR run that decides it, and the two levers are already in the file (b041)

`visceral_hold_exit_probe.lua` in the test copy logs, at every hold change, `inputHOLD=` (the game sees the aim input)
and `IsForbidAim=`; a refusal of the HOLD order while the hold is up is logged with its inputs
(`checkOrder(HOLD) now false … inputHOLD= IsForbidAim= deferTimerDone=`). Headset on, walk, shoot two-handed, stop; the
line at the drop names the cause. Then: **NUM5** forces `PlayerForbidAimController.get_IsForbid` → false (if the wall
rule is it, the throws stop); **NUM6** latches the HOLD input on while the right grip is squeezed (`vrmod`), so a missing
input frame cannot drop it (forbid still wins — it is a hard AND). NUM1/NUM2/NUM4/NUM9 are the flat harness and stay
off in VR.

## Reader, second pass: the wall rule is not a wall rule `[measured 2026-10-02, disassembly]`

The cast (`PlayerForbidAimController.lateUpdate`): from the camera transform's position along its forward axis, 1 m, a
0.1 m sphere, every 0.1 s — and it forbids ONLY when the nearest hit carries an `app.ropeway.AimCandidate` flagged
`_IsForbidAim`. Walls and props never do, which explains point 5 above. In VR praydog's script writes the headset pose
into that camera transform every frame, so the cast runs from the headset along the gaze. It clears as soon as the
sphere stops hitting; the hold comes back the next frame (raise) — the 09-30 shape. For this to be the VR cause a
flagged object (ally NPC, gimmick — data, unknown) must be within 1 m in front of the headset at the shot. The b042
probe prints the target's name at a drop when `IsForbidAim` is true. Full text: dossier §8g.5.

## Side notes

- **The game went back to the title by itself once** (16:02:55, 3.5 s after shot #12, mid-rest, no key sent, window
  found minimised afterwards) `[verified-live 2026-10-02, n=1]`. No crash, no error line; the second load was fine.
  Cause unknown — a stolen focus is the suspect. Noted in the control profile as a hazard.
- Launching the copy: `Start-Process "D:\RE2 test copy\re2.exe"` with that folder as the working directory works
  (Steam running); a `cmd /c start` from bash hung. `/lm` runs RE2 from the copy by rule now (lanes 0.42.0).
- Ammo on this save is 12 + 2, so a flat run gets ~12 shots before a reload (R).
- `PlayerActionOrderer.checkOrder(HOLD)` is answered false AND true within the same frame, every frame, while idle
  aiming — two callers; the first version of the hook logged every flip and wrote a 12 MB log. The b041 probe logs a
  refusal only while the hold is up, once per 10 frames.
- The game runs ~200 fps flat here; "frames" in the probe lines are game frames.

**Automation:** self-launch ✅ (the copy's exe), menu→gameplay ✅ (title → Story → Continue, captures verified),
commands ✅ (numpad probes, HOLD/ATTACK through the game's own input system), character+camera ✅ (W/S walks, mouse
turn), self-close ✅ (`WM_CLOSE`, clean exit, twice).
