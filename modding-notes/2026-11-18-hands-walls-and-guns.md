# 2026-11-18: hands, walls and guns, as in Village (Developer PC, Opus; RE2 b216 / RE3 b026)

Tefa: *"please give re2 and re3 hands, walls and guns the same collision treatment"* (Village b138-b142 the same day:
hands stop at walls, a gun turns at the wrist away from a wall instead of the hand sliding back, a gun pushes doors).

## How it is built (one code for both games; RE2's repo owns it, `tools/sync_shared.py` copies it to RE3)
- **Our REFramework** (`reframework-patch/2026-11-18-hands-walls.patch`): `FirstPerson.cpp update_player_arm_ik` hands
  each hand's place and turn, worked out from its controller, to the plugin's `visceral_hand_wall()` before anything else
  uses them. Here, unlike Village, the hands are placed inside our REFramework, so this hook is where the plugin can act.
- **The plugin** (`src/hands/walls.cpp`): writes the hand as the controller put it and as handed back into the bridge
  (slots 608-625), keeps a hand on the player's side of the wall plane the Lua found (626-639), and puts the Lua's turn on
  the right hand (640-644). The input is the controller's hand every call, so nothing is applied twice.
- **The Lua** (`lua/visceral_walls.lua`): a ray a frame from about each shoulder to the hand; for the gun, where its muzzle
  would be with the hand where the player holds it (learned from the drawn gun), a ray hand -> muzzle; on a hit the smallest
  turn away from the wall and a little up (2-degree steps, up to 80, eased) keeps the muzzle 3 cm off it. Doors are never
  walls (names with "door" / "gate"). It writes the drawn muzzle for `doorpush.cpp`, which now pushes with the muzzle and the
  barrel's middle too.
- **The wall layer is measured, not assumed:** RE2 has no `app.CollisionManager.Layer` list. Rays from the head per layer
  (RE2, the police station start): 2 and 3 hit the room's `st4_101_0_Terrain_00ms`, 7 trigger boxes (`AdditionalNotice`,
  `Env_...`), 6 the player (`pl5700`), 12 a `StaticCompounder` a little off. So the Lua picks, once in play, the layer with
  the most "Terrain" hits in 12 level rays round the head: layer 2 in both games (RE2 5 of 12, RE3 10 of 12).

## Simulator `[verified-live 2026-11-18, simulator, n=1 each]`
- **RE2:** a long gun aimed level into the wall, the right grip held (the gun is raised only then; without it, it hangs
  muzzle-down): with the walls script paused the muzzle sat IN the wall; on, the gun turned 10-26 deg at the wrist and the
  muzzle was clear, the hand moved 0.00 m. A free left hand pushed into the wall stopped 9 cm short of where it would have gone.
- **RE3:** Jill's pistol at yaw -60 / -30 into the save room's wall: paused = in the wall; on = turned 8 / 56 deg, clear,
  the hand unmoved. (Right after loading, the hanging pistol met the wall of that tight corner and turned up to 80 deg: as
  meant, unseen.)
- The gun pushing a door was not run in RE2 / RE3 (Village's run proved the rule; same code shape here).
- Test trap: RE2's gun joints read right (no staleness): a 0.97 m "pistol" was a long gun hanging muzzle-down.
- `tools/sim` in visceral-re8-vr: `q_wall_scan.lua` (nearest walls round the head), `q_muzzle_wall_re2.lua` (is the muzzle in a
  wall), `hands_live.py` with `HL_RGRIP=1` (the right grip held), all with `RV_GAME_DIR` pointing at RE2 / RE3.
