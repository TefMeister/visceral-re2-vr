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
| b063 | b062 + layer probe (read-only) | - | - | - | - | Leon: gun away = KFF_ clips; gun out = OFF_; aiming = **HG2_** idle/strafes, not in our spliced file |
| b064 | b063 + the cpA path filled with the game's own cpB list (one quick-reload slot), so the stock Matilda falls back to our spliced aim file | - | - | - | - | not worn; replaced by b065 |
| b065 | b060 + that cpA fallback + layer probe (no straightener) | **no** | gone | slow, careful | body and legs right | layer probe: pistol out = OFF_ (gun-drawn, angled stance); our aim splice also uses OFF_ since 09-24 |
| b066 | b065 with Leon's pistol-out file (every OFF_ slot) and aiming file (walks, idle, raise) filled with the KFF_ no-weapon clips | **no** | gone | slow (wanted) | **pistol out = straight, same as no gun**; aim-walk turns body LEFT | ⭐ BREAKTHROUGH: "no gun and gun out movement are identical ... everything is turned the right way". Probe: aiming still plays HG2_ |
| b067 | b066 + file-access log | - | - | - | - | log: the upgraded Matilda (stock + a second part) aims with `hdg_hold_cpAC_01` (+ stLIGHT_/stWATER_ versions); the cpA stand-in missed it |
| b068 | b066 + the six stock-part lists (cpA, cpAC x plain/stLIGHT/stWATER) replaced by the game's own EMPTY list `hdg_hold_01` | **no** | gone | **not slow, preferred** | **straight** | ⭐ "aim-walk is straight now, no shake, this is like it was meant to be like this" |
| b069 | the golden package alone, on a fresh Steam install | **no** | gone | not slow | straight | "the body pose is working exactly like we left it"; NEW: gun flickers / turns slightly in the right hand, left hand cannot dock |
| b070 | b069 with Leon's `base_hdg_move` + `base_hdg_hold` rebuilt by `motlist_clip_swap.py` (/pd 2026-10-05): every no-weapon KFF_ motion keeps its bones and sync clip and takes the stock pistol motion's switch clip (left-hand hold `IKBlendRatio` 1.0 and the other stock pistol switches) | - | - | - | - | not worn yet; aim: the left hand docks on the Matilda again and the gun stops flickering, with the pose unchanged |

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
- **ROOT CAUSE OF "IT WORKED, THEN THE NEW SAVES BROKE IT" (2026-10-04 22:55)** `[verified-live 2026-10-04, layer probe + archive read]`: the all-weapons saves gave Leon the **Matilda with the stock** (custom part A). Aiming with it plays its own list `pl00/list/hdg/hdg_hold_cpA_01.motlist.524` (HG2_ idle and strafes, SMG1_ raise and shots), which overrides the slots of our spliced `base_hdg_hold`. Our hunch/walk fix never covered it. Claire's `pl10` cpA exists too. cpB/cpC lists hold one quick-reload slot each. cpA's layout differs (null slots, pointer table at 0xe0, collection block 7784 bytes, not 72 x 30), so `motlist_splice.py` refuses it; a proper cpA splice needs that format handled.
- **Leon's pistol files, complete list from the access log (b067)** `[verified-live 2026-10-04]`: BASE_HDG_HOLD/MOVE/FINGER; overrides HDG_HOLD_01 (empty), HDG_HOLD_cpA_01, cpAC_01 (stock part: HG2_ idle/strafes, SMG1_ raise/shots, 18-19 slots, non-standard layout), cpC_01 (quick reload), each also as stLIGHT_ and stWATER_; HDG_MOVE_cnFINE_stCOMBAT_01 / stNORMAL_01 (both empty); HDG_FINGER_01, stLIGHT/stWATER (+ _cpA). Saved: `D:/RE2 REFramework builds/logs-2026-10-04/accessed-files-b067-upgraded-matilda.txt`.
- **Tefa's finding, b062, 2026-10-04 ~22:45: with the pistol PUT AWAY the body naturally has the right pose, no spine twist** `[reported 2026-10-04]`. Tefa's idea: force that no-weapon body pose on every state (weapon out, and aiming). History to read before acting: runtime bank poison and TargetBankType=0 failed on 2026-08-30 (`2026-08-30-aim-pose-and-foot-grounding-solved.md`); the file splice works (`motlist_splice.py`), but its walk slots were deliberately taken from the gun-drawn OFF_ set (`hdg/base_hdg_move`) on 09-24, not the no-weapon set (`2026-09-24-item-22-first-run-legs-work-left-hand-ik-starves.md`). Tefa, 2026-10-04: "we have been down this road many a time now" -> this problem has resisted many attempts; next attempt with Fable, starting from this page.
- The leg twist cannot come from this script: it never touches the legs. Body AND legs turned the same way = the whole character
  facing slightly off the view direction `[hypothesis]`.
- The shake is one of `visceral_spine_straighten`, `visceral_locomotion`, `visceral_cinematic_gate`
  `[verified-live 2026-10-04, n=1 each side]`; three shaking builds had all three, two smooth builds had none.
- The walk lists + LookAt Hold posture files remove the hunch `[reported 2026-10-04]`.
- `visceral_locomotion`'s speed-up is OFF (multiplier 1.0), so the slow careful aim-walk is the game's own aim-walk
  speed. Raising it properly means finding where the game sets it `[hypothesis]`.

## GOLDEN

**GOLDEN for the body pose: b068 (2026-10-04 23:40), Leon only - all four goals met in the headset, and CONFIRMED on a fresh install from the package alone (2026-10-05 ~00:10, Tefa: "the body pose is working exactly like we left it")** `[verified-live 2026-10-05, n=2: worn build + fresh install]`.

**Open on top of it (Tefa, fresh install):** the gun flickers and sometimes turns slightly in the right hand; the left hand cannot dock on it. Tefa's read: "like something got removed with the slow aim-walking pose". Suspects `[hypothesis]`: (1) the no-weapon KFF_ clips carry no weapon IK tracks (e.g. SurvivorIkLeftArmTrack, which `visceral_lefthand_hold` notes the hold needs), so hand IK switches off and on; (2) ~~the dock lived in our native plugin~~ WRONG (Tefa 2026-10-05): the magnum docks fine with none of our files, so the dock is praydog's own and our Matilda files broke it. Static check: game pistol clips carry `SurvivorIkLeftArmTrack` + `IKBlendRatio`; the KFF_ idle/walk do not `[verified-numerically 2026-10-05]`. Next: give the KFF_ clips that track (new code). Rule from Tefa: no old C++ plugin, only new code.
**b070, the left-hand switch put back (2026-10-05, /pd, static; the game was not launched)** `[verified-numerically 2026-10-05]`:
the stock pistol-out list has the left-hand hold switch in **all 46 slots**; the golden list has it in **1** (the stock
`KFF_Gazing_Idle_F_Relax_Loop`). Same in the aiming list for the 20 slots the golden recipe filled. A motion's switches live in
ONE property clip (kind 0) next to an optional sync clip (kind 3); the game never has two property clips in one motion
(counted over 129 entries), so `motlist_clip_swap.py` REPLACES the KFF_ property clip with the stock one from the same slot,
instead of adding beside it. Bones, sync clip and everything else stay byte-identical (independent re-check: 45 + 20 slots OK,
0 problems). Side effects to watch `[hypothesis]`: the jog cycles lose KFF's `IkTwoLegFootLock` (the stock jog has none) and
gain the stock `SurvivorChainGroupControlTrack`; the walks gain `PlayerGazingSwitchLimitedTrack`. If the pose changes, those
are the suspects, not the hold switch. Installed in the Steam game, snapshot `v0.2.0-b070`.

Tefa: *"aim-walk is straight now, no shake, this is like it was meant to be like this. also aim walking is not slow, and that is good,
i was wrong, i prefer it to be faster like it is now!"* Becomes GOLDEN once it passes Tefa's fresh-install test (too many times the
body poses held and then failed later).

The recipe (on top of the b057 base): Leon's `hdg/base_hdg_move` with every slot = the no-weapon KFF_ clip of the same number;
`hdg/base_hdg_hold` with walks, idle and raise = KFF_ clips; the six upgraded-Matilda lists (`hdg_hold_cpA_01`, `cpAC_01`, each also
`stLIGHT_` / `stWATER_`) = the game's own empty list `hdg_hold_01`; the nine LookAt Hold files; `visceral_body_anchor`,
`visceral_body_direct`, `visceral_lefthand_hold` (in the build, not proven needed); LooseFileLoader on. NO spine straightener.
Package with install steps and MANIFEST: `D:\Visceral build versions\GOLDEN6-10-04 Leon pose fix (from b068)\`.
