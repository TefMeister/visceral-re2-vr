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
