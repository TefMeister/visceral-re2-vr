# The shake, hunch and twist hunt (started 2026-10-04)

**Why this page exists (Tefa, 2026-10-04):** *"we got to REALLY carefully document this, it's been a bastard for a
long time and i would rather not do this again."* One page, one row per test, nothing left to memory. When the goal
is reached, the exact recipe goes in the GOLDEN section at the bottom and that build is never touched again.

## The goal

All four at once, in the headset, on the current saves:

1. **No shake** of the camera, standing, walking or running.
2. **No hunch** when aiming.
3. **Relaxed walk always**, aiming or not; no slow, careful aim-walk; no speed change by pushing the player along
   (that clips through doors).
4. **Body and legs not twisted**, left or right.

## Ground rules learned the hard way

- **REFramework has never caused the shake** (Tefa). Smooth runs on any REFramework build came after a full clean
  reinstall; the shake appears only once our files are added. Do not swap REFramework to fix it.
  (`2026-10-04-correction-reframework-never-caused-the-shake.md`)
- **Test on the current saves only** while rebuilding; no save swapping (Tefa, 2026-10-04).
- **One change per build**, every build saved by `dev-archive/tools/snapshot_build.py` into
  `D:\Visceral build versions\` (full copy + MANIFEST + Tefa's result in `INDEX.md`). Roll back = copy a build folder
  over the game.
- **Run around** in every shake test; standing still does not show it.
- Base for all of this: b057 = praydog REFramework v1.5.9.1 RE2.zip (no openvr_api) + the six 76298bd DLSS files +
  PDPerfPlugin 1.1.2 + nvngx 3.10.5 + six-line config. Rebuild kit: `D:\Visceral RE2 rebuild kit\`.

## Test log

| Build | What was in the game (on top of b057) | Shake | Hunch | Aim-walk | Twist | Tefa's words / notes |
| --- | --- | --- | --- | --- | --- | --- |
| b054 (09-25 copy era) | 09-25 package + spine_straighten + foot_ground + locomotion + cinematic_gate | **yes** | - | legs through floor | legs left | "torso straight, legs twisted LEFT, aim-walk legs through the floor, camera shake back" |
| b056 | b048 + spine_straighten + locomotion + cinematic_gate + flashlight walks | **yes** | - | stiff legs | - | "legs still stiff with flashlight up AND shake while moving" |
| b057 | nothing of ours | no (maybe not run) | - | - | - | "the game is not shaking anymore" |
| b058 | body_anchor, body_direct, lefthand_hold, spine_straighten, locomotion, cinematic_gate, title_rain | not said | **yes** | - | body twists then returns; legs right | pistol: Leon grabs the front ~1 s, then docks right |
| b059 | b058 + walk lists + nine LookAt Hold files + LooseFileLoader on | **yes, running** | gone | slow, careful | legs left, less | |
| b060 | b059 minus spine_straighten, locomotion, cinematic_gate | **no** | gone | slow, careful | body and legs right | the three out = no shake |
| b061 | b060 + spine_straighten alone | **YES** | - | - | not said | "camera shake is back" -> **the straightener causes the shake** |
| b061, menu | strength 0, then ENABLED off | **no** (both) | - | - | - | shakes only at strength 1 -> **the bending itself shakes the view, not the timing of the writes** |
| b062 | b061 with our straightener swapped for Arcade Controls' own, unchanged (ACVR_final_unfinished.zip) | **YES** | - | - | torso straight; legs a little right | "screen shakes, body twist is gone on torso, legs are still facing right a little" -> **not the script: something around it** |

## What is known so far

- **`visceral_spine_straighten.lua` causes the running shake** `[verified-live 2026-10-04, n=1 each way: b060 smooth without it, b061 shakes with it alone]`.
  How it works: every frame it rotates spine_0/1/2 to cancel the slow average of the animation's twist, written at LateUpdateBehavior
  AND again inside every IkArmFit.updateIk call. Two ways that could shake the view: (a) the bending itself moves the head the camera hangs
  from, and while running the average lags the stride; (b) the repeated writes land at different moments than the camera reads the head.
  Live test without a new build: REFramework menu -> *Visceral: spine straighten* -> strength 0 (still writes, bends nothing).
  Shake stays at 0 = (b) the writing/timing; shake goes at 0 = (a) the bending.
- **The bending itself is what shakes** (strength 0 and disabled = smooth, strength 1 = shake) `[verified-live 2026-10-04, n=1]`.
- **Arcade Controls' final unpublished build (`staging/arcade-controls-re2-vr/ACVR_final_unfinished.zip`, 2026-08-23) runs the SAME soft-mode
  maths as ours, line for line** (EMA baseline tau 0.4 s, straighten the baseline, re-apply the live deviation, freeze while aiming, stale-read
  guard, LateUpdateBehavior + every IkArmFit.updateIk) `[inferred-static 2026-10-04]`, and its case study
  (`arcade-controls-re2-vr/modding-notes/case-studies/2026-08-16-subtract-the-offset-not-the-motion.md`) says it ran at full strength
  standing, walking, running and turning with no camera sway `[reported 2026-08-16]`. Its REFramework settings match ours too. So what
  differs is around the script, not in it `[hypothesis]`. AC's aim-walk speed (a changed `re2_smooth_movement.lua` that drives speed from
  the stick while aiming) is the push-the-player method Tefa does not want; not used.
- **b062: Arcade Controls' own straightener shakes here too** `[verified-live 2026-10-04, n=1]`, so the cause is what it runs WITH. `body_anchor`, `body_direct` and `lefthand_hold` only act while aiming, and the shake is while running `[inferred-static]`. Left: our walk/posture files (`base_hdg_hold` lists put ordinary walk clips in the pistol bank, which the straightener's average then follows) `[hypothesis]`. Free test: in b062, run with the pistol put away, then with it in hand.
- The leg twist cannot come from this script: it never touches the legs. Body AND legs turned the same way = the whole character
  facing slightly off the view direction `[hypothesis]`.
- The shake is one of `visceral_spine_straighten`, `visceral_locomotion`, `visceral_cinematic_gate`
  `[verified-live 2026-10-04, n=1 each side]`; three shaking builds had all three, two smooth builds had none.
- The walk lists + LookAt Hold posture files remove the hunch `[reported 2026-10-04]`.
- `visceral_locomotion`'s speed-up is OFF (multiplier 1.0), so the slow careful aim-walk is the game's own aim-walk
  speed. Raising it properly means finding where the game sets it `[hypothesis]`.

## GOLDEN (empty until reached)

When all four goals hold in one build: its number, its full file list with sha256, Tefa's words, and a copy in
`D:\Visceral build versions\GOLDEN\` that nothing ever writes into again.
