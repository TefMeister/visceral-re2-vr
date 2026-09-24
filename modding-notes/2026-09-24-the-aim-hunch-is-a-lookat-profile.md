# The aim hunch is a LookAt profile, not an animation (2026-09-24 afternoon, home PC, static)

Follows `2026-09-24-item-22-first-run-legs-work-left-hand-ik-starves.md`: three animation splices played
while aiming and Claire stayed hunched in all four stances. Tefa asked for the hunt to start. Nothing was
launched; everything below comes from the SDK dump, the game's own settings files and the earlier logs.

## What was found

1. **RE2 bends the survivor's spine toward the aim point with a LookAt profile chosen per stance.** The files
   are `UserData/Character/Survivor/pl0000/LookAt/*.user.2` (`Default`, `Jog`, `Light`, `Hold`, `Hold_HG`,
   `Hold_SG`, … — the game loaded all of them this morning). Each holds a list of joints with the pitch/yaw
   range the look-at may bend them by. Decoded with the new `lookat_read.py` `[measured 2026-09-24]`:
   - `Default` (weapon drawn, not aiming): spine_2 and spine_1 may pitch **-5..10°**.
   - `Hold_HG` (aiming the handgun): spine_2 **-30..75°** plus a 0..55° world-pitch pre-correction, spine_1
     -5..15° (world -10..30°), spine_0 world -10..25°.
   That is the arch. `[inferred-static 2026-09-24]` until the run below.
2. **It runs after the animation and after our Lua straightener.** `visceral_spine_straighten.lua` writes the
   spine at `LateUpdateBehavior`, before IK; the look-at bends it again afterwards. So that script never
   touched this, and any animation swap cannot either.
3. **Ruled out:** `IkAttitude` (ground lean), `IkController` SPINE kind (`IkSpineConformGround`), the flashlight
   and `cpB` hold override lists (reload only). Arcade Controls had reached the same wall from the other side
   (case study 2026-08-05) and settled on an after-the-fact spine write.

## What was built (no launch)

- `dev-archive/tools/re-engine/lookat_read.py` — lists a profile's joint records.
- `dev-archive/tools/re-engine/lookat_patch.py` — copies every `Hold*` profile with spine_0/1/2 set to
  Default's ranges, in place (same size), and re-reads its own output. `--zero` = no spine bend at all.
- `dev-archive/reframework/autorun/visceral_spine_probe.lua` — read-only; logs spine_0/1/2/neck_0/head local
  rotation before IK and just before render, once a second, with `hold=`. The stock profile should show a
  `d=` of tens of degrees on spine_2 while aiming; the patched one ≤ ~10°. This is how the run tells "no
  effect" from "wrong lever".
- **Installed for the test:** the nine patched `Hold*.user.2` under
  `<RE2>\natives\STM\SectionRoot\UserData\Character\Survivor\pl0000\LookAt\` and the probe in
  `reframework\autorun\`. Archived with checksums in `D:\RE2 REFramework builds\extracted (game data - never
  commit)\lookat-archive\v1-default-spine\` (built from game data, so not on GitHub).

## The one flat run

Draw the handgun, aim standing and walking, flashlight off and on. Read:
- Tefa: is the arch gone (standing / walking / light on)?
- Log: `[visceral_spineprobe] hold=1 … spine_2 … d=` — under ~10° means the profile is the lever; still
  tens of degrees means something else bends it after render prep (then `--zero` is the next try, and after
  that the LookAt component itself is the hook point: `LookAtUserDataHolder.Accessor.getUserData`).
- `reframework_loose_files.txt` must list `Hold_HG.user.2` as taken.

Per the standing rule, if it does nothing the files come out again and stay in the archive.

## Result of the flat run, and the next lead (same afternoon)

**It worked.** Tefa: *"it worked!"* — the arch is gone in all four stances `[verified-live 2026-09-24, n=1]`.
The probe agrees: over 18 aimed seconds, spine_2's post-animation bend read **d = 0.0° average, 0.0° max**
(31 unaimed seconds: 0.2° avg, 4.9° max); `Hold_HG.user.2` was taken by the loose-file loader. So the
LookAt profile was the whole hunch. Dossier §8g promoted from `[inferred-static]` to verified.

Tefa's next ask, verbatim: *"now the feet and legs animation has to match the walking one as well, but the
body stays still!"* — read as: with the profile fix alone the legs still play the aim shuffle and the torso
is rigid; both should move as in the ordinary walk. That is the v3 splice (ordinary walk + ordinary idle in
the hold bank), which is **reinstalled** (`motlist_splice.py --game-dir`, default map, sha `1854e2af…`, same
bytes as the archived v3). Expected side effect: the left-hand flicker while walking forward aimed.

**Flicker lead (static):** the game pins the left hand with
`app.ropeway.survivor.SurvivorIKLeftArmController` — fields `IKEnable` (bool @0x78), `IKBlendRate`
(`DampingFloat` @0x80), `TargetMatrix` (@0x90), `CurrentTarget` (the weapon, via
`ISurvivorIKLeftArmTarget.getIKLeftArmMatrix`, i.e. our `ikL` hook), constant `IK_ENABLE_THREASHOULD =
0.01`; methods `updateIKEnable` @0x1417a8570, `updateBlendRate` @0x1417a6060, `updateTarget`, `applyMatrix`,
`handlerExtraBlendRate` (registered with `IkController.addExtraBlendRate`). Motion clips can carry a
`SurvivorIkLeftArmTrack { IKBlendRatio }` (looked up per motion through `MotionEx.getSameSequenceTrack`).
`[hypothesis]` the aim loops carry that track and the ordinary walk loops do not, so under the splice
`IKEnable` drops and the hand is only re-solved on a slow tick — the ~1/s flicker. Lever, once the
decompile confirms what `updateIKEnable` reads: hold `IKEnable=true` and the blend target at 1.0 from the
plugin while `IsHold`, and re-measure with the 1 Hz `hooks(aid ikL)` count (~72/s = fixed).

**Correction from Tefa (same afternoon):** *"it wasn't a complaint, that was meant as the hunching gone and a
good thing"* — so "the body stays still" = the arch is gone. The ask stands: legs (and the walk) to match
the ordinary walk while aiming, which v3 supplies. Also: *"the weapon flicker was gone last time as well"* —
consistent: that run had no splice installed, and the flicker only ever appeared with the splice.

**Open question from Tefa:** *"the body turning while aim is held, there is nothing that can remove that
without breaking the laser sight dot in the game world is there?"* What Arcade Controls actually recorded
(`case-studies/2026-08-06-laser-sight-drift-investigation.md`, round ten): the dot broke because the spine
was rewritten ONCE per frame while the native arm solve ran SIX times per frame, so four solves saw the
uncorrected spine — a write-timing fault, fixed by re-applying before every solve. It is not a rule that
"changing the body breaks the dot"; anything applied UPSTREAM of the arm solve (as the LookAt-profile change
is) leaves the dot consistent by construction. Which "turning" Tefa means — the whole character pivoting to
face the aim (head→body follow in FirstPerson + the game's aim-facing) or the torso yaw — is not yet
pinned down; asked.

**Answered: the torso twist is in the aim ANIMATION, not the LookAt.** Tefa: *"torso twisting while feet stay
put when AIM is active"*. The probe log of the LookAt-only run (no splice installed) gives the final pose
per joint `[measured 2026-09-24, n=1 run]`: unaimed, spine_0 yaw avg -0.6° (|yaw| 4.7°); **aimed, spine_0 yaw
avg -15.0° (|yaw| 16.1°)**, spine_1/2 yaw ≈ 1°. The hold profiles give spine_0 yaw 0..0 (stock and patched),
so the LookAt cannot be adding it; it is the `HG_Hold_Idle_Loop` / aim pose itself — the same Claire twist
Arcade Controls measured on spine_0 (case study 2026-08-05), which `visceral_spine_straighten.lua` lets
through by design (its baseline freezes on aim, so an aim-only twist counts as "live deviation"). **The v3
splice replaces exactly that animation with the ordinary idle/walk, so the run already queued (v3 + patched
profiles) decides it.** If a twist survives, the remaining aim-only clips are `HG_Hold_Start_*` (20 frames)
and the layer-4 shoot overlay. `lookat_patch.py --zero-yaw` was added (spine yaw 0..0 while aiming) but is
NOT installed — the LookAt is not the source, and it would only add a variable.

⚠️ **Probe caveat:** aimed `d=0.0` between `LateUpdateBehavior` and `PrepareRendering` means the LookAt pass
did not fall between the two reads (it runs earlier, in the motion/IK phase), so the probe measures the
FINAL pose, not the LookAt's own contribution. The posture proof therefore rests on Tefa's sighting plus the
final-pose spine_2 pitch reading 3.4° avg while aiming; a stock-profile run with the probe would give the
missing before number if ever needed.

## The flicker, read from the code, and the lever (same afternoon, static)

`SurvivorIKLeftArmController.lateUpdate` (re2.exe `0x1405c6900`) calls `updateBlendRate` (`0x1417a6060`)
then `updateIKEnable` (`0x1417a8570`) `[measured 2026-09-24, capstone listing]`:
- `updateBlendRate`: if `CurrentTarget` is null → `IKBlendRate._Target = 0`; else fetch the motion's
  `SurvivorIkLeftArmTrack` list (`MotionEx.getSameSequenceTrack`, type token at `0x14917aa28`); **no
  list or empty → `_Target = 0.0`; otherwise `_Target = last track's IKBlendRatio`**; then
  `DampingFloat.update` damps `Current` toward `_Target`.
- `updateIKEnable`: `IKEnable = (IKBlendRate.Current > IK_ENABLE_THREASHOULD 0.01)`.
So the hold is a per-motion property carried by the clip's tracks; the ordinary walk/idle loops carry
none, the hold bank's own loops do. Under the splice the hand hold switches off while aiming
`[inferred-static 2026-09-24]` — that is the flicker (the hand is only where the IK put it last).
`DampingStruct<float>` layout: `Current` @0x10, `_valueChanging` @0x30, `_Target` @0x34.

**Lever built (Lua, own file): `visceral_lefthand_hold.lua`** — pre-hook on `updateBlendRate`; while
`IsHold` and a target weapon exists it writes `Current = _Target = 1.0` and skips the original; otherwise
the original runs (so letting go of aim damps the hand off as stock). Logs `IKEnable / cur / tgt /
calls/s / forced/s` once a second. Installed with the v3 splice + patched profiles. Read on the run:
Tefa (flicker gone walking forward aimed?) + `[visceral_lefthand] hold=1 IKEnable=true cur=1.00` +
the plugin's `hooks(aid= ikL=)` back near the frame rate. Native port into `visceral_core.dll` after the
Plugin.cpp split. `tools/re-engine/disasm.py` (capstone, seconds) and `dumpq.py` (dump reader) added —
ghidrust's decompile of this exe takes minutes per function and its C output was not readable here.

## Run 3 (13:23): twist gone, flicker gone, legs still stiff while aiming

Tefa: *"twist is gone, flicker is gone, but legs and lower body still get that stiff leg animation
then aim is active"*. Log `[verified-live 2026-09-24, n=1]`:
- **Flicker lever proven:** `[visceral_lefthand] hold=1 IKEnable=true cur=1.00 tgt=1.00 calls/s=72
  forced/s=72` for the whole aimed stretch, and the plugin's `hooks(aid= ikL=)` sits at 73–76/s while
  aiming (was ~11/s under the splice without it). Dossier §8g.2 `[hypothesis]` → verified.
- **Twist gone with v3** (the aim idle replaced): so the spine_0 yaw was the `HG_Hold_Idle_Loop` clip.
- **Legs:** layer 0 plays `OFF_GazingWalk_*` from the spliced slots while aiming, and its reported
  `EndFrame` is ~62 both aimed (bank 2) and unaimed (bank 1) for the 367-frame loop (the idle reads its
  full 3354 both ways) — so the loop length is NOT what differs. `[hypothesis]` the hold tree's
  locomotion node blends idle↔walk by movement speed and aiming caps the speed, so the walk gets only
  part of the weight — a diluted half-step reads as "stiff". Probe added (`visceral_layer_probe.lua`,
  read-only): per second, aim state, real ground speed (m/s), and every node with weight per layer.
  Read on the next run: aimed-walking vs unaimed-walking speed, and the walk node's weight while aiming
  (≈1.0 kills the hypothesis; <1 with idle carrying the rest confirms it → the lever is the aim
  speed cap, i.e. spec v2 req 4, dossier §8d).

## Run 4 (13:36): the stiff legs are a SPEED mismatch, not a blend

`[visceral_layers]` `[measured 2026-09-24, n=1 run]`: while aiming and walking forward the walk node
carries **w = 0.97–1.00** (the idle↔walk blend hypothesis is dead), but the ground speed reads
**2.25–2.39 m/s aimed vs 1.58–1.84 m/s unaimed** (OFF and OLF walks alike). The extra is ours:
`visceral_locomotion.lua` v5 amplifies aim-walk by `aim_speed_mult = 1.3` (Tefa's tuning from 2026-08-30,
when aiming still played the slow shuffle). 2.3 / 1.3 ≈ 1.77 m/s, i.e. the game's own aim-walk pace is
about the same as its walk `[inferred 2026-09-24]` — the shuffle only *looked* slower. A walk clip
authored for ~1.7 m/s under a 2.3 m/s body = sliding, short-looking steps = "stiff legs". Tefa recalled
this speed-up and the collision guard around it (v4 "amplify only the stick-forward part").
**Change:** `aim_speed_mult` default 1.3 → 1.0 (the amplifier returns early at ≤1.0, writes nothing);
the slider and NUM7/NUM9 still exist for the later req-4 work (one cap for aim/walk/run with the jog
clip when fast). Game copy and dev-archive copy updated; `mod/` copy untouched until a release.

## Run 5 (13:45, IN THE HEADSET): legs sunk into the floor, stance change + hand twitch at the aim press

Tefa, with three Virtual Desktop screenshots: *"when i press aim, the whole posture still does visibly change
stance - it twists just a little bit to the right when standing still, legs take a different stance, like a
'combat ready' left leg in front right slightly at the back … legs clip through the floor - probably
because we changed the height settings for a good-enough result when we couldn't remove the hunch … when i
press aim (RG) there is a noticeable twitch in the hand - it quickly moves just a little bit with the
handgun and the whole posture just changes"*.

- **Sunk legs + the "combat ready" leg stance = `visceral_foot_ground.lua`** `[inferred 2026-09-24]`: it
  lowers the pelvis by 0.175 m while `IsHold` (tuned 2026-08-30 for the braced aim pose, whose hover it
  cancelled). With the ordinary idle now playing while aiming there is no hover, so the drop sinks the legs
  17.5 cm and the leg IK bends the knees into a crouched stance — exactly the change at the aim press.
  Tefa's own diagnosis, and it matches. **Taken out of the game folder** (kept in `dev-archive/` and
  `mod/`, identical copies; per the 2026-09-24 standing rule).
- **Small twist to the right while aiming:** the probe showed spine_2 post-anim bend 0.0°, but the run-4
  probe reads only the final pose; with Default's spine yaw ranges (-7..14° per joint) the LookAt can still
  turn the torso toward the aim point, which in VR sits right of centre (gun in the right hand).
  **v2 profiles installed: spine_0/1/2 yaw 0..0** (`lookat_patch.py --zero-yaw`; archived
  `lookat-archive\v2-no-spine-yaw\`). `[hypothesis]`; the spine probe's aimed yaw column decides it.
- **Hand twitch at the press:** not yet attributed. Candidates: the 20-frame `HG_Hold_Start_L0/R0` raise
  (layers 0 and 3) moving the right arm under the controller-pinned hand; our left-hand lever snapping the
  blend to 1.0 in one frame; the finger bank switch (`Hold_HG01` → `Hold_HG01_LGT`). Ask Tefa after this
  run whether the twitch is only at the press or continues.
- Layer probe, standing aimed vs unaimed: **the same `OFF_Gazing_Idle_F_Loop` at weight 1.00 on layer 0**
  in both states `[measured 2026-09-24]` — so no clip difference remains standing; everything left is
  post-animation (IK/LookAt/our scripts).

**Hand twitch, Tefa (before restarting):** *"happens only once after RG is pressed, very brief … the gun
just visibly moves with the player hand just a little bit … also the right hand plays a quick animation
then where the thumb moves slightly, maybe other fingers too"*. Two clips play exactly once at the press
`[measured 2026-09-24, earlier logs]`: the 20-frame `HG_Hold_Start_L0/R0` raise on layers 0 and 3 (the FSM
leaves it after ~12–14 frames — 0.19–0.24 s in the 11:51 log — so it is time-driven, not motion-end-driven,
which makes replacing it with a long clip safe `[inferred-static]`), and the finger bank switching
`FIN_HG01[_LGT]` (carry grip) → `Hold_HG01[_LGT]` (aim grip), a 30-frame clip with the thumb move.
**v4 built + installed:** hold bank = v3 + all six `Hold_Start_*` slots → `OFF_Gazing_Idle_F_Loop` (sha
`2926883b…`); finger banks `hdg_finger_stlight_01` / `hdg_finger_stwater_01`: slot `50_Hold_HG01_LGT` →
`02_FIN_HG01_LGT` (the light-on carry grip), `base_hdg_finger`: `50_Hold_HG01` (3 shared slots) →
`00_CAU_HG01` (the caution carry grip; the light-off FIN grip lives in a file not yet located). All
archived with hashes in `splice-archive\`. Expected: no raise motion and no grip change at the press;
if the aim state ever hangs on the raise, the motion-end guess was wrong and the Hold_Start mapping
comes out.

## Run 6 (14:01): v4 disproved — the raise clip must stay

Tefa: *"feet on the floor, legs stay the same when aiming both aim-walking and aiming while standing still.
body slightly turns right after aim is pressed and has no movement animation at all while walking or
standing, it's frozen in place … also the laser pointer from the JMB hp3 gun has disappeared"*.

- **Feet / stance: solved** (the foot_ground drop was it) `[verified-live 2026-09-24, n=1]`.
- **Frozen torso = my Hold_Start mapping.** Layer probe: **layer 3 kept `OFF_Gazing_Idle_F_Loop` at weight
  0.99 for the whole aimed stretch** (`frame … /3354`), standing and walking. So the raise state on the
  upper-body layer ends on MOTION END, not on a timer — `[disproved 2026-09-24]` the "time-driven" reading
  from the 11:51 timestamps (the 0.19 s there was the 20-frame clip itself). A 3354-frame idle on the
  upper-body-masked layer 3 over-rode the walking torso: "frozen in place".
- **Laser gone:** `[hypothesis]` the laser sight is switched on at the end of the raise (a track/event in
  `Hold_Start` or the state exit it never reached). Same cause.
- **Reverted to v3** (`base_hdg_hold` sha `1854e2af…`, the six `Hold_Start` slots original). The finger
  banks stay in (thumb twitch); the raise's own small arm motion at the press is accepted for now — the
  honest fix for that is in the plugin (hold the wrist IK through the raise), not in data.
- **Body turns right at the press — still there with spine yaw 0..0**, so it is not the LookAt spine
  records. Next suspect: the character's facing (root yaw) following the aim direction when the hold
  state begins. `visceral_spine_probe.lua` now logs `BODY yaw=` (player transform world yaw) once a second;
  a jump at the press settles it. If it is root yaw, the lever is where the hold state sets facing
  (`PlayerHoldedTurnTrack` / `HoldedTurn` in the FSM), or the spec-v2 route (we own facing in VR).

**Finger banks, corrected (Tefa's two screenshots, 14:02, before/after RG: the whole grip changes, thumb
over, fingers tighter).** The light-OFF carry grip is not a `FIN_HG01` clip at all: with the handgun out
and no light the game already plays `50_Hold_HG01` on layer 2 whether aiming or not (run-4 probe,
`hold=0 … L2: pl10_50_Hold_HG01`) `[measured 2026-09-24]`. So my `base_hdg_finger` mapping (`Hold_HG01` →
`CAU_HG01`) *introduced* a grip change at the press — the run-6 probe shows `CAU_HG01` while aiming.
**Taken out** (archived). The light-ON case really does switch `02_FIN_HG01_LGT` (carry) → `50_Hold_HG01_LGT`
(aim), and the `hdg_finger_stlight_01` / `stwater` splices (aim slot → the carry clip) stay in. What is left
at the press with the light on should be only the raise clip's arm motion under the pinned hand.

## Run 7 (14:12): laser back; two things left, both settings, not clips

Tefa: laser back; standing fine; **aim-walk still differs** (*"legs are more straight, like taking careful
steps forward, tip toes pointed out … relaxed walking has a natural whole leg movement"*); and the press
twitch *"goes through the whole body … for a quick second or less"* — top priority for VR.

Log `[measured 2026-09-24, n=1 run]`: at every RG press `BODY yaw` does not move (e.g. 173.2 → 173.2) and the
enabled IK kinds are `LEG,ARMFIT` in both states — so the "turn right" is not root facing and not an IK
kind flipping. Aim-walk vs relaxed walk: **same clips (`OFF_GazingWalk_F` ~0.9 + `L` ~0.1), same weights,
same 1.5–1.7 m/s**. So the leg difference is a *setting* the hold state changes (leg-IK options such as toe
control / centre adjust / lean, or the character controller), not animation.

Built for the next run:
- **`visceral_state_diff_probe.lua`** (read-only): 0.5 s after every aim change it reads every 0-arg
  `get_*`/`is*` getter returning a number/bool/enum on `via.motion.IkLeg`/`IkLeg2`/`IkLegSpine`, `via.motion.Motion`,
  `IkController`, `IkAttitude`, `SurvivorCondition`, `SurvivorIKLeftArmController`, `via.motion.IkLookAt`, the
  character controllers — and logs only the values that differ from the other state. Whatever flips is
  the lever list.
- **v5 hold bank** for the press twitch: the six `Hold_Start` slots now hold a COPY of the ordinary idle
  with its frame count patched to the raise's own length (20/22/24 frames; header +0x60 and its mirror
  +0x6c, confirmed `3354, 0, 0, 3354` on the idle) — `motlist_splice.py` mapping syntax `name@N`. The raise
  state still ends on motion end, now after 20 frames of the standing pose instead of a raise, on both
  layer 0 (whole body) and layer 3 (upper body). `[hypothesis]`: no body motion at the press, laser still
  switches on (the state still ends). If the file is rejected or the laser goes again, this comes out.
  sha `86760505…`, archived.

## Run 8 (14:26): v5 did not remove the twitch — but the diff probe named the switch

Tefa: *"twitch is still there and i almost feel the player body get stiff and tense when RG is pressed and
held. the hand still moves, and the little twitch still goes through the whole body."* (v5 raise-slot
idle copies loaded fine and the laser stayed — so the raise clip was not the twitch.)

`[visceral_diff]` over 13 aim changes `[measured 2026-09-24]`: apart from the expected condition flags
(`IsHold`, `IsHolding`, `EnableAttack`, `EnabledReticleFit`, `EnableUpdateHitCandidate`) and timer noise,
**exactly one body setting flips at every press: `SurvivorCharacterController.OffsetType` Joint(0) →
CameraY(3), and back on release.** `JointDefine.OffsetType = {Joint 0, Object 1, Camera 2, CameraY 3}`.
CameraY anchors the character's capsule/body offset to the camera — the third-person shoulder offset —
and in VR the camera is the headset: the body shifts at the press and is then held relative to the head
while aiming. `[hypothesis]` that is the whole-body twitch, the "stiff and tense" feel, and probably the
small turn to the right (the TPS shoulder offset is to the right). Nothing in `via.motion.IkLeg` flipped,
so the aim-walk "careful steps" are not leg-IK options either; if the anchor is the cause of the body
being dragged, it may also explain the leg look.

**Lever installed: `visceral_body_anchor.lua`** — pre-hook on `set_OffsetType` rewriting CameraY → Joint for
the player's controller, plus a pre-hook on `updateCharacterController` that resets the backing field if
the game wrote it directly; logs `OffsetType`, `getLocalOffsetPosition` and the hit counts once a second;
NUM5 toggles. Read on the run: `OffsetType=0` while `hold=1`, the local offset unchanged at the press, and
Tefa: no twitch, no tension, legs. If the hook counts stay 0 and the type still reads 3, the value is
written below the property (then: the `SurvivorCharacterControllerUserData` "Hold" shape entry, joint
`COG`, category 2, `DefaultSurvivorCharacterControllerUserData.user.2` pulled and hex-read; or the native
`updateCharacterController` @0x1411045d0).

## Run 9 (14:38): the camera jump is gone; body tension, small twist and careful steps remain

Tefa: *"this is half the win … the camera also jumped a little when RG was pressed. now it stays completely
still … in VR makes a monumental difference … but the body still tenses up after the press, torso twists a
little and walking feet step more carefully"*.

Log `[measured 2026-09-24]`: `visceral_body_anchor` holds `OffsetType=0` through every aimed second (the game
writes the field directly once per press — `set_OffsetType` is never called, the field reset fires once).
The capsule's local offset still changes while aiming (≈ (0.04, 1.22, −0.01) vs (0.06, 0, 0.06)) — the Hold
shape's own offset vector, noted, not yet touched.

Final pose aimed vs unaimed (27 vs 90 samples): spine_0 pitch 2.9 / yaw −5.8 / roll −2.0, spine_1 pitch 2.8,
spine_2 pitch 3.2; unaimed all 0.0 exactly (the straightener zeroes the sustained offset when not aiming and
freezes its baseline while aiming, so anything the hold state adds passes through). The v2 profiles still
allowed spine pitch −5..10 → **v3 profiles installed: pitch, yaw and world-pitch all 0..0 on spine_0/1/2**
(`lookat_patch.py --zero`, archived `lookat-archive\v3-zero-spine\`). The −5.8° spine_0 yaw has no LookAt
range left to come from; if it stays, it is something else (the straightener's aim freeze is the next
suspect: turn `freeze_while_aiming` off).

**One more flag flips at every press: `SurvivorCondition.IsDirectingBody` false → true** (with `IsWalk` /
`IsIdle`, which are just state). `get_IsDirectingBody` @0x141194460: true when `[this+0x140]->+0xf0 > 0`
(an active request count) or when the playing motion's `SurvivorDirectingBodyTrack.Enabled` says so — i.e.
the hold state asks the body to be directed at the aim. **Lever installed: `visceral_body_direct.lua`** —
post-hook on `get_IsDirectingBody` answering false while `IsHold` for the player; logs calls / game-true /
overridden per second; NUM6. If `calls/s` reads 0 the consumers read the field inline and the lever needs
the native route. `[hypothesis]` for the tension and the careful steps.

## Run 10 (14:53): light OFF nearly right (body pushed ~5 cm forward); light ON still twists and steps carefully

Tefa: *"without the flashlight out … the body and lower body gets pushed out forward like 5 cm … but the aim
animation both still and walking are the same as relaxed walking … with the flashlight out - still and
walking aim still have the body twist and careful steps."*

- `visceral_body_direct` answered false 144×/s while aiming (the game said true every call) `[measured]`;
  with it the tension is gone (Tefa: "this looked good"). Spine while aiming now: spine_0 pitch 2.3 / yaw
  −2.8, spine_1/2 ≈ −1.5 pitch — small residue, left for now.
- **Forward push ≈ 5 cm:** the hold state also requests its own capsule SHAPE (`SurvivorCharacterController.
  register(Request)` with `ShapeCategory Hold=2`): the local offset reads (0.02–0.07, 1.19–1.28, −0.04–−0.17)
  aimed vs (0.06, 0, 0.06) unaimed even with the anchor held on the joint. **Lever added to
  `visceral_body_anchor.lua`: drop Hold-category shape requests** (pre-hook on `register`, SKIP_ORIGINAL), so
  the Default capsule stays while aiming; counted in the 1 Hz line (`shape requests N (hold dropped M)`).
  `[hypothesis]`.
- **Flashlight ON:** the layer probe of run 4 already showed it — the relaxed light walk is `OLF_GazingWalk`
  / `OLF_Gazing_Idle` (light arm up) while the aimed walk came from the base hold bank's `OFF_` clips, and
  the light override list `cmn_hold_stlight` (which I had taken out in the tidy-up) only carried the six
  Interpolation slots. Its collection block has 42 slots with EMPTY placeholders for the strafe (0x78–0x88),
  idle (0xa0/0xa6) and Hold_Start (0x8c–0x99) numbers `[measured 2026-09-24]`. `motlist_splice.py` now takes
  `--fill 0xNUMBER=WALK[@N]` to put a clip into an empty slot; **v2 light list built: six Interpolation +
  six strafe → `OLF_GazingWalk_*`, both idle slots → `OLF_Gazing_Idle_F_Loop`, six Hold_Start → the OLF idle
  cut to 20/22/24 frames** (mirrors v5). Installed at `natives/STM/SectionRoot/Animation/Player/pl10/list/CMN/
  cmn_hold_stlight.motlist.524` (sha `087cf847…`), archived. Read: with the light on and aiming, layer 0
  should show `OLF_*` clips; Tefa: light-on aim = light-on relaxed.

## Run 11 (15:10): "nothing changed" — both guesses were the wrong file / the wrong field

- **Flashlight:** `CMN_HOLD_stLIGHT` was taken by the loader, yet the aimed layer-0 clips were `OFF_*` only
  (28 unaimed samples with `OLF_*` prove the light was on) `[measured 2026-09-24]`. So that list is not the
  one the handgun-with-light hold state reads (its `HGL_`/`KFL_` names suggest it is for holding the LIGHT
  as the item). **Taken out again** (archived as v2). The list that IS per-weapon and light-on is
  `hdg_hold_stlight_01` — 30 slots, only `HGL_Hold_Reload` filled, the other 29 empty placeholders with the
  same numbers as the base bank. **v1 built with `--fill`: Interpolation + strafe → `OLF_GazingWalk_*`, idle
  slots → `OLF_Gazing_Idle_F_Loop`, Hold_Start → the OLF idle @20/22/24; reload kept.** Installed beside
  `base_hdg_hold`, archived. `[hypothesis]` this is the list the aimed light-on state consults.
- **Forward push:** the Hold shape request was dropped (`hold dropped 1` at the press) and the local offset
  STILL read (0.08–0.12, 1.22–1.25, −0.13–−0.16) while aiming. So the capsule anchor JOINT / OFFSET change by
  another path (the `COG` joint of the Hold parameter, most likely applied inside `attainShape` from the
  condition rather than from the request). **Anchor script now remembers the relaxed `ConstJoint` and the
  `Offset` DampingVec3 (`Current` @0x10, `_Target` @0x50) and re-applies them before every
  `updateCharacterController` while aiming**; the 1 Hz line prints the joint name and the fix counts.
  Read: `joint=` the same name aimed and unaimed, `localOffset` ≈ (0.06, 0, 0.06) while aiming.

## Run 12 (15:17): light-on aim-walk SOLVED; the nudge and a small camera shift left at the press remain

Tefa: *"flashlight out aim walking is also sorted, thank you! but the body still nudges and also, when i
press RG, the camera also shifts a little bit to the left, just a tiny bit"*.
- `hdg_hold_stlight_01` v1 is the list the handgun-with-light hold state reads: 23 aimed samples with `OLF_*`
  on layer 0 `[verified-live 2026-09-24, n=1]`.
- The anchor script now holds `joint=root`, `localOffset=(0.060, 0.000, 0.060)` through every aimed second
  (`offset fixed` 72–73/s) `[measured]` — and the body still nudges. So the capsule was never the nudge.
- What still changes at the press in the final pose: **head pitch −6 → 0, head roll −1.8 → −6.6** (run-9
  probe) from the Hold profiles' head/neck records (`Hold_HG`: head −10..0 / −5..10, neck_0/1 −10..20)
  vs Default (head −5..15 / −20..30, neck_1 same, no neck_0). REFramework's FirstPerson hangs the VR camera
  off the head bone, so a head-bone change at the press is seen as the camera moving (left) and the body
  moving under it (forward) `[hypothesis]`. **v4 profiles installed: v3 (spine 0..0) plus head / neck_1 /
  neck_0 set to Default's records** (`lookat_patch.py --zero --head-default`; archived
  `lookat-archive\v4-zero-spine-default-head\`). Read: head/neck pre-values identical aimed and unaimed.

## Run 13 (15:24): head/neck now identical aimed vs unaimed — nudge and camera shift still there

Tefa: *"still the same and with the flashlight out it seemed to be pushing the body forward even more"*.
Probe: head pitch −2.5/−2.9, yaw 20/17, neck_0 31.9 both ways `[measured]` — the v4 profiles removed the
head-bone change, and it was not the nudge. Still differing while aiming: spine_1 pitch −4.8, spine_2
−4.3 (unaimed 0.0) with every LookAt spine range at 0 — an unexplained −4.5° at two spine joints.
No more guessing: **`visceral_press_probe.lua`** (read-only) records, frame by frame for 15 frames before
and 30 after every aim change, the player root position, the world position of `cog/hips/pelvis`,
`spine_0`, `spine_2`, `head` and the game camera. Whatever moves at the press, and by how much, is then
a number, not an impression. Read: which of root / pelvis / head / camera jumps, in which axis.

## Run 14 (15:39): the press, in numbers

`visceral_press_probe` over 14 aim changes `[measured 2026-09-24]`. At every aim-ON, relative to the last
frame before the press, sampled at +1/+5/+10/+20/+30 frames:
- **root: 0.000 in every axis, every press.** The character does not move.
- **hips (= spine_0): ramp over ~10–20 frames to (+0.04, −0.02, −0.03) m** in the first six presses and to
  **(+0.01, −0.018, −0.07) m** in the later ones (Tefa had the light on for the second set, and reported the
  light-on push as bigger — matches). Same direction at every press within a set: systematic, not an
  animation phase jump. spine_2 follows the hips; the head moves ~2–3 cm.
- **cam − head = (0.000, 0.040, 0.000) before and after, every press:** the VR camera rides the head bone;
  the "camera shifts a little left" IS the head being carried by the hips.
So something post-animation translates the pelvis a few centimetres (and lowers it 2 cm) with damping
when the hold state begins — the shape of a leg-IK / balance adjustment. Next probe pass: sample hips/head
at `LateUpdateBehavior` (before IK) as well as at `PrepareRendering`, plus `IkController.getBlendRate(kind)`
per frame; if the pre-IK hips stay put and the post-IK hips move, the mover is IK (LEG or the attitude
lean), and its blend curve names it.
