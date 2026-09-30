# The relaxed walks carry no left-hand track — the likely cause of the gun swing (2026-09-30, home PC, `/pd`, static)

**The game was not launched; nothing here has been run.** Build b036 is in the test copy, waiting for a
headset run.

## Where it stood

b012–b017 (2026-09-27) showed the gun is thrown aside on a two-handed shot **only while walking on the
spliced `base_hdg_hold`** — the twelve aim-walk slots replaced by the relaxed `OFF_GazingWalk` loops.
Standing on the stock aim idle is fine. Putting the stock aim-loop arm bones (b016) and spine (b017) into the
walk loops changed the direction of the throw but did not stop it. Work stopped for Fable.

## What the files show `[measured 2026-09-30, pl10 base_hdg_hold + base_cmn_move, all 30 + 8 entries]`

Each mot entry carries, after its bone tracks, one or two embedded `CLIP` blocks (header word `+0x72`: low
byte = clip count) that hold **non-bone tracks** — the per-motion switches game code reads with
`MotionEx.getSameSequenceTrack`. Listing the class and property names inside those blocks:

| motion (pl10) | clip tracks carried |
| --- | --- |
| `0110–0115 HG_Interpolation_*` (aim walks) | `SurvivorIkLeftArmTrack.IKBlendRatio` (1.0 → 1.0), `via.motion.MotionSyncPoint` |
| `0120–0136 HG_Strafe*` (aim walks, the slots the splice replaces) | `SurvivorIkLeftArmTrack.IKBlendRatio`, `MotionSyncPoint` |
| `0140–0153 HG_Hold_Start_*` (raise), `0165/0167 HG_Wheel_*` | `SurvivorIkLeftArmTrack.IKBlendRatio` |
| `0160 HG_Hold_Idle_Loop` (aim idle) | `SurvivorIkLeftArmTrack.IKBlendRatio`, `IkTwoLegFootLock.ContactCheck` |
| `1100–1102 HG_Hold_Shoot` (the kick; bones through `pl_common.jmap`) | `SurvivorRejectPrecedeOrdersTrack`, `VibrationTrack` |
| `1200 HG_Hold_Reload` | left-arm track + reject-orders + vibration |
| `1301/1311 holster` | `PlayerMainWeaponHandHeldSkipTrack`, left-arm track |
| **`0190–0198 OFF_GazingWalk_*_Loop` (what the splice puts in)** | **`MotionSyncPoint` only — no left-arm track** |
| `0192/0193 OFF_GazingWalk_End_*` | `SurvivorGpuClothControlTrack`, `MotionSyncPoint` |
| `0160 KFF_Gazing_Idle_F_Loop` | none; `0161 …_Relax_Loop`: `SurvivorLookAtTrack.IsDisable` |

So **every stock motion in the hold bank tells the left-hand IK to stay on, and none of the relaxed walks
do.** The 2026-09-27 note's line *"the left-arm IK track is the same in vanilla and walk motions"* is
withdrawn — the OFF walks have no such track at all. (That note's other measurements stand.)

Dossier §8g.2 already established what that means `[measured 2026-09-24, disassembly]`:
`SurvivorIKLeftArmController.updateBlendRate` takes the playing motion's last `IKBlendRatio`, **0.0 when the
motion carries none**, so while walking aimed on the splice the support hand comes off the gun. That was the
once-a-second flicker on 2026-09-24, cured by `visceral_lefthand_hold.lua` — **and the test copy never had
that script during the b012–b017 bisect.** The arm graft (`motlist_graft_arms.py`) moved bone tracks only,
never the clip block, so b016/b017 still ran with the hold off.

## Reading `[hypothesis]`

The shot kick (`1100`, additive over 64 bones) is authored with the support hand pinned to the gun by IK. With
the hold off, nothing anchors the two-handed pose through the kick, and the gun ends up aside. Direction
changed with the arm base pose (b016) because the unanchored kick lands on different arms.

What would show this reading wrong: b036 step 1 (hold forced on) still throws with the probe reporting
`leftIK=true` all through the walk.

## b036 — one launch, three answers (installed in `D:/RE2 test copy`, `[compile-verified 2026-09-30]` luac)

- the b015 lists (12 walk slots relaxed, raise + idle stock) back in, loose loader on
- `visceral_lefthand_hold.lua` — proven 2026-09-24; **NUM7** now flips it off/on
- `visceral_swing_probe.lua` (new) — **NUM6** holds the ARMFIT wall-fit arm IK off (`IkController.setEnable(4,false)`
  every frame before the IK pass, read back after); **every shot measured**: the gun joint's position relative
  to the head, in the camera's right/up/forward axes, 5 frames before the shot vs peak / 60 / 120 frames after,
  in cm. Vanilla should settle near 0; a throw is a large number that stays. Once a second: hold flag, layer-0
  motion, `EnableIkBits`, each IK kind on/off + blend, `IKEnable`.

Order for Tefa (walk + shoot two-handed after each): **(1)** as installed → no swing = the missing track;
**(2)** NUM7 → swing back = the same answer from the other side; **(3)** NUM6 (only if 1 still swung) → ARMFIT.

## The permanent fix, if (1) holds

Either keep the script (code route, already written) or graft the `SurvivorIkLeftArmTrack` clip block from a
stock aim walk into each OFF walk (data route: the clip block moves whole like a mot entry; its key frames are
`0 → 1.0` and `end → 1.0`, so only the end frame needs rewriting to the walk's length). Data route preferred
for release — no script, no hook.

## The Village link (Tefa's idea, same evening) — and it closes the "how" `[inferred-static 2026-09-30]`

Tefa: the Village rifle used to jerk left when fired two-handed; are they related? **Yes, through the VR
mod, not the game.** REFramework's `RE8VR.cpp` `update_hand_ik()` serves RE2 as well (`re8_vr.lua` calls
`re8vr:update_hand_ik()`), and the Village dossier §9cf/§9cg read it in full: while the left grip is held,
the gun is steered by the line from the right hand to the **animated left-hand socket**, read from the body
animation **every frame**. When the animation moves that socket under a held grip, the gun re-aims with both
real hands still — in Village the first shot after a take moved the muzzle 4.8° every time, and freezing the
socket at the take fixed it (worn 2026-09-21, *"it works :)"*).

That is the missing "how" for RE2: with the left-arm IK track gone, the support hand is no longer pinned to
the gun, so the shot kick moves the animated socket — and praydog's steering throws the gun after it. It
explains every fact of the bisect at once: only with LG held (the grip branch), only on the splice (the
IK pin is what keeps the socket still through the kick), standing fine (the stock idle carries the track),
and the direction flipping with the arm base pose (a different socket path). The probe now logs
`left_grip=` and `two_handed=` per shot; a one-handed shot on the splice is predicted **not** to swing.

Two fixes exist, and both should go in: restore the IK pin (this note), and the Village socket freeze —
which lives in the patched REFramework Village ran on 2026-09-21, **not** in the stock praydog build the
test copy runs now. Porting it means patching REFramework again, or asking praydog upstream.

## Worn — b037/b038, 22:40–22:48 `[verified-live 2026-09-30]`: the hold was ON and it still threw

- `visceral_lefthand_hold.lua` forced the left-hand IK on every aimed frame (`IKEnable=true cur=1.00`,
  72 forced/s) — **and the gun still threw while walking** (Tefa). So the missing track is real but is
  **not the throw** on its own. The IK-pin reading is `[disproved 2026-09-30]` as the sole cause.
- The probe's first version never saw motions 1100–1102 on any layer at `on_frame` time (0 shot lines in
  2½ minutes of firing); the fire request `Equipment.requestFire` catches every shot.
- **Five shots measured (b038).** Four moved the gun 0.7–3.9 cm at peak and came back. **Shot #4 threw:
  peak 46.7 cm still growing at frame 120 — right 14, DOWN 37, forward 25 cm — and the left wrist left the
  gun, 7.4 → 52.5 cm.** Within 0.7 s of that shot the 1 Hz line read **`hold=0`** with layer 0 on the gun-out
  idle `OFF_Gazing_Idle_F_Loop`: **the aim state ended by itself right after the shot** — Tefa keeps the
  right grip squeezed the whole time, so it was not their input. Tefa's description fits the numbers:
  *"not an instant teleport, more like a seemingly smooth animation to the right … then snaps back to my
  hand"* — the lower-the-gun animation plays with the VR hand anchor giving way, then the anchor snaps the
  gun back to the controller.
- Playing as Leon (pl00); the pl00 list from b015 is in and plays (`pl00_0190_OFF_GazingWalk_F_Loop`).
- `re8vr.*` is not reachable from our script (nil), so the grip flags per shot are unknown here.

**Reading now `[hypothesis]`:** on the splice a shot while walking can end the hold state; leaving the
hold swaps layer 0 to the gun-out idle/walk and plays the lowering pose, and for those frames the wrist IK
that RE8VR uses to pin the hands to the controllers (`UseIkArmFitAsWrist`, ARMFIT on/1.00 throughout) is
skipped or blended away — the "smooth" part — until it re-engages: the snap. Two questions the b039 trace
answers per shot, every 6 frames: **which comes first**, the hold ending or the gun leaving, and whether
`SkipIkForWristBits` / `AppliedSkipIkForWristBits` change during it. NUM5 blocks the skip every frame.
Why the hold ends is the next question (a state-machine exit the OFF walk triggers? the `MotionSyncPoint`
track? an `Order` rejected by the kick's `SurvivorRejectPrecedeOrdersTrack`?).

## For the micro latch (item 22's next step) — Tefa asked for this kept as research

The micro latch plan is: RG = grip only, RT enters the hold state just for the shot and drops it after.
Tonight's numbers say what that costs if done naively: **leaving the hold while walking on the splice is
exactly the moment the gun leaves the hand** (37 cm down, 25 cm forward), because the exit plays the
lowering pose and the hand anchor gives way for a moment. So the latch must not drop the hold until the
kick and the return are over, or must keep the wrist IK pinned across the exit (NUM5's block, if it works),
or must exit into a state whose motion carries no lowering. Also: the hold can end **by itself** after a
shot on the splice (shot #4) — the latch needs to read the hold state back, not assume it.

## Rain (Tefa, same evening): the Story page rains, the main menu does not

Six rounds on 2026-09-27 ruled out lamps, effect players, the Story menu's own effect and keeping the Story
screen alive. Untried: `app.ropeway.gui.MenuStoryBehavior` switches to a **sub camera** on open
(`toSubCamera` / `outSubCamera`, `CameraStateValue` TO_SUB / OUT_SUB) `[inferred-static 2026-09-30, type dump]`.
`visceral_title_subcam_probe.lua` (in b036) logs those calls and the primary camera's name on every change, and
**NUM8 on the main menu** enters the sub camera ourselves (NUM8 again leaves it). Rain appears ⇒ the camera is
it, and round 12 of the title script enters it for the main menu; no rain ⇒ next suspect `IsCheckLatestLocation`.
