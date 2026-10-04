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
| b061 | b060 + spine_straighten alone | ? | ? | ? | ? | testing now |

## What is known so far

- The shake is one of `visceral_spine_straighten`, `visceral_locomotion`, `visceral_cinematic_gate`
  `[verified-live 2026-10-04, n=1 each side]`; three shaking builds had all three, two smooth builds had none.
- The walk lists + LookAt Hold posture files remove the hunch `[reported 2026-10-04]`.
- `visceral_locomotion`'s speed-up is OFF (multiplier 1.0), so the slow careful aim-walk is the game's own aim-walk
  speed. Raising it properly means finding where the game sets it `[hypothesis]`.

## GOLDEN (empty until reached)

When all four goals hold in one build: its number, its full file list with sha256, Tefa's words, and a copy in
`D:\Visceral build versions\GOLDEN\` that nothing ever writes into again.
