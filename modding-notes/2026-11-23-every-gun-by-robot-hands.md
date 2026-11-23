# 2026-11-23 (/lm, PC1, Fable): RE2, every by-hand gun run by robot hands (the real headset lying on the desk)

Tefa's ask of the morning (grip zones; magazines, reloads, racking; the hand and finger poses), Village first (its notes:
visceral-re8-vr `modding-notes/2026-11-23b` and `23c`), then RE2. Rig: Two Frame Sequential, the headset on the desk streaming,
VR Bench's hands, nobody wearing it. Build 217 as installed, unchanged. Save: the all-weapons save (slot 7, Claire, the main
hall), reached with three presses of A.

## How it was driven (tools in visceral-re8-vr `dev-archive/native/tools/sim/`, with `RV_GAME_DIR` = this game)

- `drive_real_re.sh re2` starts it and pictures the title (it presses nothing); `vb.py x press a` three times.
- `q_re_equip.lua` (`E.want = <WeaponType>`): `Inventory.equipMainSlot(app.ropeway.inventory.Slot)` swaps the gun in the hand
  about 3 s later `[verified-live 2026-11-23, n=9]`. ⚠️ `equipSlot(Int32)` is accepted and does nothing.
- RE2 has no zones file, so `q_re_parts.lua` gives the gun's joints and both wrists in the camera's frame, `re_go.py <joint>
  [offset]` walks the left controller until the drawn wrist is there (two corrections), `re_move.py` slides it along the
  barrel, `re_seek.py <joint> "<log text>"` tries places round a joint until the mod's log says the text, `re_pistol.sh
  <WeaponType>` is the whole magazine-gun routine. The magazine joint `_04` travels with a magazine that is out of the gun:
  the well is found from the gun's root `_00`.
- The gun's own forward is NOT the controller's: `hv` pitch 55 pointed it 26 degrees down, pitch 82 held it level.

## The table `[verified-live 2026-11-23, robot, n=1 each unless said]`

| gun | slide / bolt / pump by hand | B drops the magazine | spare from the pouch seats | rack after | hand on the seated magazine, B into the hand, back in with LT |
| --- | --- | --- | --- | --- | --- |
| Matilda (WP0000) | pass (n=3) | pass | pass (n=2; the wrist 3 cm off the gun's root) | pass | NOT CONFIRMED: no "the left hand on it" in 40 places within 8 cm of the magazine |
| JMB Hp3 (WP0200) | pass | pass | pass | pass | not run |
| M19 (WP0100) | pass | pass | pass ⚠️ | pass | not run |
| Lightning Hawk (WP3000) | pass | pass | pass | pass | not run |
| MQ 11 (WP2000) | pass | pass | pass ⚠️ | pass | not run |
| LE 5 (WP2200) | bolt `_02` pass | - (long magazine: LT with the hand on it) | not run | - | not run (needs the hand on the magazine) |
| W-870 (WP1000) | pump pass (the gun held level and near; LG at the fore-end, LT, 10 cm back, LT let go) | - | 2 shells from the pouch, "2 at the port (0.096 m)", LT: "shells in (loaded 3)" | pump pass | - |
| Spark Shot (WP4300) | rack pass ("pump done") | - | - | - | - |

- ⚠️ M19 and MQ 11: the log said "0 carried" at the drop, and the pouch still gave a magazine that seated (the gun stayed at 1
  loaded). Either the count is of another ammunition or the pouch gives an empty magazine: not looked into `[n=1 each]`.
- The hand on the seated magazine: with Claire every gun but the Matilda, M19, JMB and MQ 11 uses one generic hold
  (`guns_table.cpp HOLD_GENERIC`, about 18 cm from the magazine joint), and the grab is measured to THAT spot (15 cm reach).
  The robot's scan (8 cm each way round the magazine) never hit it on the Matilda. Not a proven fault: the scan was small.
- Not by hand in this build: SLS 60, GM 79, Chemical Flamethrower, Minigun (the game's own reload).
- All racks said "for show": the guns held rounds; firing to empty was not run.

## Hands and fingers

RE2 has no finger poses of its own and no reload stages a pose could hang on: the wrist goes to hard-coded holds (slide, pump,
magazine dock, shell dock), the fingers are the game's animation `[inferred-static 2026-11-23]`. Hand Poser's script is not
installed here; it would give one pose per gun, held only while the left hand is on the gun, and it reads slot 84 for the gun,
which this build never writes. One picture pair of the Matilda's slide hold (the hand over the top of the slide) looked
sensible. So Tefa's "hand poses through reload and rack" needs building here first: stage numbers from the plugin (as
Village's slot 153), then Hand Poser. That is a design job, on the board.
