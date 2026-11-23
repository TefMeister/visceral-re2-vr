# 2026-11-23 (late evening, /lm by hand-on, PC1, Opus): "any gun" poses, the Matilda's slip cleared, the W-870's stages by robot

Carries on from `2026-11-23b-hand-stages-for-hand-poser-and-the-trigger-finger.md` (same rig: the desk headset, VR Bench's hands,
Claire's all-weapons save, Two Frame Sequential for pictures; nobody wearing it). Sheets: `2026-11-23c-sheets/`. Build 219.

## A correction to the note before

- That note said Hand Poser's script reads "any gun @ <spot>" as a fall-back. **It did not, for the docked spots** (slide,
  magazine, magazine in hand, handle, shells ...): only "ready", "free", the holsters and the guard fell back to "any gun"
  `[inferred-static, hand_poser.lua load_own_pose / load_chain]`. So every gun but the Matilda showed the game's own fingers at
  every stage, whatever was saved.

## The Hand Poser tool job (the board row "A WAY TO SAVE ONE POSE FOR EVERY GUN"), hand-poser-vr

- **The window's outside input** (`app/Remote.cs`, `app/Pose.cs`): `"save_as": "any gun"` keeps the hand at the spot, as the
  window holds it, as "any gun @ <spot>" (only the joints of the hand at that spot); `"unset": [joints]` takes rows out of the
  pose in the window and saves at once (as the row's ✕). The robot's `pose.py` sends them: `save_as=any`, `unset=a,b` /
  `unset=left` (every left row the window holds).
- **The script** (`hand_poser.lua`): a docked spot a gun has no pose for takes "any gun @ <spot>" (then that of the spot it falls
  back to). Village has no such files, so nothing changes there. Built, its self-test passed, installed; in RE2 by
  `build.sh --deploy --layer 3` (build 219).
- Run `[verified-live 2026-11-23, robot, n=1]`: the window answered `saved as 'any gun @ slide' (17 joints)`, the same for
  "magazine" and "magazine in hand"; the files are in the game's `hand_poser/guns/`.

## The Matilda's general pose: the slip cleared

`wp0000.json` held 17 left-hand rows (the evening's slip) and nothing else. `pose.py unset=left` with the Matilda in hand and the
left hand at no part: the window answered 17 unset, the file is now `"joints": {}` `[verified 2026-11-23]`.

## Poses that serve every gun

From the Matilda's three kept stage poses (the game's curl, slightly changed: coarse), each saved as "any gun" with the hand at
the stage: **slide** (stage 3, `re_go.py _01`, LG), **magazine** (stage 4: `re_seek.py _04 "left hand 4" 6 lg` found +6 -6 0 cm
from `_04`), **magazine in hand** (stage 13, B). Every other gun now shows them at those stages until it has its own. NOT judged
on the other guns by picture.

## The W-870 by robot `[verified-live 2026-11-23, robot, n=1 each]`

- Held level and near (`right=0.15,-0.05,0.25,90,82`; at 0.32 m out the fore-end was beyond the drawn arm: "wrist 18.5 cm from
  _01"). Stage **5** with LG alone on the fore-end (Hand Poser names it "handle"), **3** with LT, **11** pulled back 10 cm.
- Shells: `addItemCount(16, 12)` (testing only, memory). LG at the pouch: "2 in the left hand", stage **9**; at the port
  (`re_seek.py _100 "left hand 20" 6`: +12 0 0 cm from `_100`) stage **20**; LT: "shells in (loaded 3)".
- Pictures (`w870-*.jpg`): the pump hand lies along under the fore-end, the back of the hand to the camera, the fingers up out of
  sight behind it; by numbers the docked wrist sits on the fore-end joint (0.4 cm) and every finger joint is bent 38-54 degrees.
  What could not be judged: whether the fingertips pass into the pump (one camera angle; the far side is hidden). Not changed
  (the dock is the worn one). Shells in the hand: pinched between thumb and fingers; at the port the hand is under the receiver
  with the shells at the fingertips: both look like hands holding shells.

## The carried magazine (the Matilda), looked at, not changed

`q_re_maghand.lua` again: the magazine joint 13.5 cm out of the palm side of the wrist. Four pictures
(`matilda-magazine-carried-four-sides.jpg`): the magazine touches the fingertips but does not sit in the palm (from one side the
fingers reach past it, from below it hangs under them). It IS in the hand, held low and loose: flagged for Tefa, Tefa's worn hold
numbers left alone.

## Not done

- **The LE 5**: not in the save, and the game refused it (`addAndEquipMainWeapon(23)` ran, `hasWeapon(23)` stayed false: the
  memory inventory is full after the gifted guns, KNOWN.md). Its bolt and magazine wait for a fresh start of the game.
- The Matilda's magazine was dropped on the way back to the well (the robot's reach missed); harmless.
- No pump, shell or port pose was saved as "any gun" (the W-870 is the only gun with those stages here).

## Build 220 (after Tefa's answers, the same night)

- **Tefa: "move it into the palm please"** (the carried Matilda magazine). `game/guns_table.cpp`: Claire's Matilda hold was
  `HOLD_GENERIC` (the placeholder: 13.5 cm out of the palm side); it now takes Leon's Matilda hold. Run `[verified-live
  2026-11-23, robot, n=1]`: the magazine joint 2.4 cm off the palm, among the fingers (`q_re_maghand.lua`: 9.9 / -2.4 / 5.6);
  four pictures: the fist closes round it (`2026-11-23c-sheets/matilda-magazine-in-the-palm-b220.jpg`). ⚠️ Claire's other
  magazine guns (M19 100, JMB Hp3 200, MQ 11 2000, SLS 60 7000 ...) still use the generic holds: the same may hold for them.
- **Tefa: "hand should always ride along in any games on any weapoins"** and the drum by motion: shared `guns/cylinder.cpp`
  (RE3's MGL; RE2 has no swing-out drum): see RE3's note. Built here, copied by `sync_shared.py`.
- **The LE 5**: Tefa will empty the inventory; then the board row's steps.
