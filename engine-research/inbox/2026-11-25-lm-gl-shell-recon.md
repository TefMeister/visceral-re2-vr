# 2026-11-25 (/lm reader, static only): the GM 79's rounds in RE2's files, for the spent-shell + coloured-band row

For the board row "SPENT SHELLS AS OUR OWN MODEL, A WORN COLOURED BAND ON EVERY ROUND, AS IN VILLAGE". Nothing was run in
the game; files only. Village's version: visceral-re8-vr `modding-notes/2026-11-25b-gm79-spent-shell-our-own-model-and-round-bands.md`.
Dossier sections it speaks to: the weapon list and the GM 79 / break-action notes. RE3's twin file:
visceral-re3-vr `engine-research/inbox/2026-11-25-lm-gl-shell-recon.md`.

Extracted with `dev-archive/tools/re-engine/pak_pull.py` + Ekey's `RE2_RT_STM_Release.list` into
`D:\RE2 REFramework builds\extracted (game data - never commit)\gl-shell-2026-11-25\` (game data, never commit). Meshes read
with RE Mesh Editor 0.66's parser outside Blender (the `mesh_parts.py` way; its cached temp copy was broken, a fresh copy of
the add-on's `modules/` works). No Blender was run.

## 1. The launcher and its round `[measured 2026-11-25]`

- The GM 79 is **WP4100** only: `sectionroot/character/weapon/wp4100/wp4100_grenadelauncher.mesh.2109108288`, one material
  `wp4100_Grenadelauncher_mt` (Weapon_Default.mmtr). The prefab uses `wp4100_Grenadelauncher_Mat.mdf2` (the `_mat` file,
  which adds the MTOS texture). WP1300 is in the WeaponType enum (= 14) and in RELOADED's data as "GM 79", but the file list
  has no wp1300 folder at all `[measured]`: an unused entry `[inferred-static]`. WP4100 = WeaponType 42.
- Joints (all bind rotations identity): root > _00 > { _01 (the barrel, hinge at z 0.187), _03, _06 (latch), _04, _100,
  _101, Chain00 }. `_04` sits on the bore axis at (0, 0.0231, 0.0804) (RE axes, metres, gun space; the muzzle is +z).
- **The round in the chamber is mesh part 3, all of it weighted to `_04`** (224 verts, same material as the gun). `_04` is a
  child of `_00` (the frame), not of the barrel `_01`, which is why the round did not ride the opened barrel in the 2026-11-01
  wear. `_100` / `_101` (at (0.0144, -0.019, 0.257 / 0.249)) carry NO vertices of the gun mesh; the prefab's second mesh
  `wp4110_GrenadeAmmo` is the round the game's reload moves, presumably on one of them `[inferred-static]`.
- **Part 3 is crude: a closed 24-sided solid, case and warhead ONE piece, no open mouth, no inside, no rim, no groove, no
  primer.** Profile along z (from `_04`): base disc at z 0.0805 (r 0.0196); case body r 0.0197 to z 0.1345 (5.4 cm); a step
  to the warhead r 0.0182 at z 0.1375; warhead to z 0.163, ogive to the nose tip at z 0.178. Whole round 3.95 cm across,
  9.75 cm long. Case and warhead can only be told apart by z (cut at 0.1345 .. 0.1375), not by mesh part or island.
- `wp4110_grenadeammo.mesh` (the round in the reload): one piece, r 0.0182 throughout (no case step), 3.63 x 7.45 cm,
  same material/texture as the gun.

So RE2 has **no empty case to copy** the way Village's part 9 was copied. Two ways, for Tefa to choose in Blender
`[hypothesis: both untried]`:
  a. Model the spent case from RE2's own part 3: keep z 0.0805 .. 0.1345, open the mouth (an inner wall r ~0.0185 and a floor
     ~1.1 cm down, as Village's and RE3's are), and cut a base groove for the band (RE2's round has none).
  b. Use RE3's case (wp4101 part 0, a real hollow case with rim, groove and primer: see the RE3 file), scaled from 4.22 to
     ~3.95 cm across. Needs RE3's files at build time; our built meshes are not in git anyway.

## 2. Pickups `[measured]`

`objectroot/setmodel/sm7x_item/sm70/sm70_106 / 107 / 108_grenadelauncher_*bullet01a_00ms.mesh`: ONE box model for all three
(same file hash; 18.5 x 20.7 x 21 cm, Item_Default.mmtr), told apart only by the material file:
sm70_106 = blank textures (NullBlack / NullNormal: a placeholder), sm70_107 = the acid-bullet texture with BaseColor
(1, 0.93, 0) yellow, sm70_108 = the same texture with BaseColor (0.91, 0.12, 0.12) red. RE3 ships the identical three boxes.

## 3. Bullet item ids

From the game's own type dump (`Enums_Internal.hpp`, REFramework, 2026-09-25, in `D:\Visceral RE2 archive (old, 2026-10-04)\`):
`app.ropeway.gamemastering.Item.ID`: **sm70_106 = 21, sm70_107 = 22, sm70_108 = 23** `[measured, static]`.
- **22 = Acid Rounds, 23 = Flame Rounds** `[inferred-static: pickup names + tints; matches RE3's numbering, where the same
  sm70_10x order is live-measured as 37 explosive, 38 acid, 39 flame]`.
- **21 = the explosive / plain grenade round slot, with a placeholder pickup** `[inferred-static]`. Whether RE2 ever loads it
  is NOT known: Tefa's row lists explosive gold for RE2, but the game data suggests RE2 has only acid and flame
  `[hypothesis: one live look at the slot's get_BulletID with the GM 79 loaded settles it]`.
- Also in the dump: `EnemyWeaponDataUserData.WeaponValueInfoBase.GrenadeLauncherShellType` Bullet 0 / Acid 1 / Fire 2.
- Not yet read live; `src/game/guns_table.cpp` `round_gun()` still returns nullptr ("the GM 79's flame / acid item numbers
  are not read yet"): 22 / 23 are the candidates for its `items`.

## 4. What our RE2 code knows now `[inferred-static, read 2026-11-25]`

- The clean tree has **no break action**: no `breech.cpp` under `dev-archive/native/src/` (only
  `dev-archive/archive/native-gen1/src/breech.cpp`, the b188 one: hinge `_01` 40 deg, latch `_06`, `g_chamber` = `_100`,
  `g_case` = `_101`). The GM 79 "by hand" is layer 4 of the clean rebuild, unbuilt.
- `src/game/weapons.cpp`: 1300 and 4100 both "GM 79", LONG holster. `src/game/guns_table.cpp`: `round_gun` = nullptr, no
  SHELL/SLIDE entry for 4100. Shared code that would carry it: `src/guns/rounds.{h,cpp}` (kinds per chamber, SPENT = -1,
  `get_BulletID` on the inventory main slot), `src/guns/cylinder.cpp`.
- Bridge (`src/core/bridge.h`, SHARED with RE3 via `tools/sync_shared.py`, edit HERE): the Lua array is 653 long; 651 / 652
  are kept for Hand Poser. **The equivalent of Village's S_GL_ROUND (280-281) goes at 653+** (e.g. `S_GL_ROUND = 653` with
  0 none / 1 live / 2 spent, a kind slot for the item id, a stamp), N grows, both games synced together. RE3 wants six
  chamber values there (its file), so one block sized for six serves both.
- Where the writer goes: a rebuilt `src/guns/breech.cpp` (Village's breech.cpp lines ~652-662 are the pattern). Until the
  break action exists, a band on the LIVE round can still come from the game alone: `ammo::loaded() > 0` + the slot's
  `get_BulletID`, drawn on `_04`.
- Spawning our own mesh in RE2/RE3 by script is proven by `lua/visceral_options_book.lua` (via.GameObject.create with the
  camera's folder + Mesh + setMesh / set_Material through resource HOLDERS; without a folder nothing drew). Village's
  `visceral_re8_gl_rounds.lua` calls setMesh with the resource directly: use the options-book route here.

## 5. Village's two build scripts, pointed at RE2

`build_gm79_spent_shell.py`: SRC `wp4100_grenadelauncher.mesh.2109108288`, CASE_PART `Group_3_Sub_0`, JOINT `_04`; export
`filename_ext=".2109108288"` and `.mesh.2109108288` names (as `tools/blender/build_bracelets.py` already does). But its
measured constants and its two geometric steps (split the inside off by inward normals; fill the base groove with the band)
assume a hollow case with a groove, which RE2's round does not have: with route (a) the case must be modelled first (Blender,
with Tefa); with route (b) point it at RE3's wp4101 (the RE3 file has the numbers) plus a scale and RE2's `_04` offset
(our mesh then sits with its base at `_04`, z 0.0805 = local +0.0001).
`mdf_build_gm79_shells.py`: output `.mdf2.21` (the same writer wrote the bracelets' mdf2.21); `--template`
`wp4100_grenadelauncher_mat.mdf2.21` (Weapon_Default; BaseColor / Metallic / Roughness present `[measured]`);
`--container` any ORIGINAL three-material Weapon_Default file: `wp0000_vp70.mdf2.21`, `wp0200_browningh.mdf2.21`,
`wp0200.mdf2.21` or `wp7000_samuraiedge.mdf2.21` (all three Weapon_Default `[measured]`); `--game re2` with an `re2.ammo`
row in `shell_bands.json` (`acid: [22], flame: [23]`, and `explosive: [21]` only if the live check finds it). The band's UV
trick (sample the base rim's bare metal) needs a new UV source: RE2's round has no rim; pick a worn-metal patch of
`wp4100_Grenadelauncher_ALBM`.
