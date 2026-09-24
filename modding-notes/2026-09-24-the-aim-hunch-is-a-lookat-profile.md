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
