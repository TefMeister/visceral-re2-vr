# Visceral RE2 clean rebuild: implementation plan

> **For agentic workers:** work inline (Tefa's standing rule: no subagents), with superpowers:executing-plans. Steps use
> checkbox (`- [ ]`) syntax. Tick them in this file as they are done and commit the file with the step's work.

**Goal:** one clean native plugin for RE2: the body moved over as it works today, the guns and hands built off Village's code
through a per-game layer, the game's own finger animations on our hands, one Lua file of ours.

**Architecture:** a new tree at `dev-archive/native/` with `src/core`, `src/game`, `src/body`, `src/guns`, `src/hands`,
`src/tools`. Body files are moved, not rewritten. Guns and hands files are Village's, diffed so that the only differences are
calls into `src/game/game.h`. Installed in 8 layers, each a saved build, each worn before the next.

**Tech Stack:** C++20, REFramework plugin API 1.15, CMake + Visual Studio 2022 Build Tools, our patched REFramework 76298bd
(dinput8 `0fe6beab`), Lanes builds.py, the OpenXR Simulator + VR Bench, Python 3 for the clip tools.

**Spec:** `modding-notes/2026-11-07-clean-rebuild-design.md` (approved 2026-11-07). Item numbers below are its section 4 numbers.

## Global Constraints

- Every number named in `src/core/settings.h`; nothing in it read from a file; nothing in a file duplicated in it.
- Files 800 lines soft, 1,500 hard. Numpad keys only, never F-keys.
- Game animation data (takes, clips, any game file) never on GitHub. Clips live in `reframework/data/visceral/clips/` and the
  work folder only.
- `~` for the home folder in anything committed; the privacy guard is never skipped.
- A `guns/` or `hands/` file includes only `core/` and `game/` headers.
- The four loader exports keep their exact names and signatures: `visceral_vr_buttons`, `visceral_sub_weapon_out`,
  `visceral_dock_socket`, `visceral_hands_frozen`.
- Hand smoothing runs only with a real gun in hand (the loader's filter; the plugin never adds a second one).
- `build.sh --deploy` installs exactly its list and first removes any of OUR files in `reframework/` that are not on it.
- Every install is a builds.py snapshot in the series `clean rebuild` (b001...). Every build kept.
- Outside `/lm`: say what will launch and wait for Tefa's yes. Close the game with its own quit at the end of every run.
- Log tag `[visceral-native]`; every module's first log line names the module.

## Review Focus

1. A save loaded with a gun already in hand at the first frame: every module must find its objects late, never cache a null
   player. Pinned in Task 6 (first-frame test on the simulator: load straight into a save holding the W-870).
2. A character other than Claire (Leon, Ada, Hunk): clips and rest poses fall back to Claire's without a log flood. Pinned in
   Task 12.
3. A cutscene or an enemy grab mid-reload: the reload state must drop cleanly and the hands come back. Pinned in Task 9.
4. The loader without the plugin (the plugin failed to load): the stock mapping must stay; the plugin's exports must never be
   called before `reframework_plugin_initialize`. Pinned in Task 5.
5. A deploy over an old install: no gen-1 file may survive. Pinned in Task 5 (`build.sh --deploy` run over a folder seeded with
   one gen-1 Lua and one gen-1 data file).

---

## Layer 0: back up, archive, wipe (Tasks 1-4)

DONE 2026-11-07 (Opus): Tasks 1-3. The folder held exactly build b192; 256 files moved to
`D:/claude video game stuff/re2-before-clean-rebuild-2026-11-07/ (outside every git repo: "D:/Visceral build versions" is a junction into the builds repo)game/`, all hashes matched, 0 left; saves copied (9 files).
`builds.py restore visceral-re2-vr 192` puts the old install back. Task 4 waits for Tefa's yes.

### Task 1: The inventory and Tefa's tuned numbers, read-only

**Files:**
- Create: `dev-archive/rebuild/2026-11-07-inventory.md` (every file in the game folder's `reframework/` and root mod files,
  with its sha256 and verdict)
- Create: `dev-archive/rebuild/tuned-numbers-2026-11-07.json` (Tefa's tuning, copied, plus what each index means)

- [x] **Step 1:** list every non-game file in the game folder with its sha256 into the inventory (`find` + `sha256sum`).
- [x] **Step 2:** a verdict per stray Lua (design section 3), from its header and the board. Proposed, to confirm by reading:
  `visceral_title_rain`, `_title_one_scene` = item 10 (port native in Task 7); `_steady_light` = item 5 (port native in Task 7
  if `exposure.cpp` does not already do what it does; say which); `_mute_music` = layer 1 only, then native; `_hand_smooth` =
  the smoothing's menu: its JSON file stays, the script goes (no menu, Q3); `_body_anchor`, `_body_direct`,
  `_lefthand_hold` = the 2026-09-24 aim-stance probes, archive unless the board shows one still worn as a fix;
  `_flicker_catch` + `flickers_begone.dll` = a tool, out of the player build; `visceral_grip_sockets.txt` = read what writes it.
- [x] **Step 3:** copy `visceral_tuning_leon.json`, `visceral_slide_grip.json`, `visceral_slide_fingers.json`,
  `visceral_shell_tune.json` into `tuned-numbers-2026-11-07.json`, with a note of the non-zero holds `[measured 2026-11-07]`:
  WP0000 "gun" and "slide", WP1000 "gun"; index meaning from `tune.h` (item move 3, item turn 3, hand move 3, hand turn 3,
  17 finger joints x 3).
- [x] **Step 4:** commit both files (`git add dev-archive/rebuild/`).

### Task 2: Save the current state as a build, then move every mod file out

**Files:** none in the repo; the backup goes to `D:/claude video game stuff/re2-before-clean-rebuild-2026-11-07/ (outside every git repo: "D:/Visceral build versions" is a junction into the builds repo)`.

- [x] **Step 1:** `builds.py which visceral-re2-vr`; if the folder matches no build, `builds.py snap visceral-re2-vr
  "before the clean rebuild" --note "the whole gen-1 install as worn up to b192"`.
- [x] **Step 2:** MOVE (never delete) every file of the inventory to the backup folder, keeping paths; copy (not move)
  `re2_fw_config.txt`, the patched `dinput8.dll` and `openxr_loader.dll` to the backup as well; copy the saves
  (Steam `userdata/<id>/883710/`) to `.../saves/`.
- [x] **Step 3:** check: `sha256sum` of every moved file at its new place equals the inventory; the game folder holds no file
  from the inventory. Record both counts in the inventory file and commit it.

### Task 3: Archive the old source trees in the repo

**Files:**
- Move: `dev-archive/native/` -> `dev-archive/archive/native-gen1/`
- Move: `dev-archive/plugin/` -> `dev-archive/archive/plugin-gen0/`
- Move: `dev-archive/reframework/` -> `dev-archive/archive/lua-gen1/` (`mod/` is the released mod: left as it is until the
  rebuilt mod replaces it)

- [x] **Step 1:** `git tag pre-rebuild-2026-11-07 && git push origin pre-rebuild-2026-11-07`.
- [x] **Step 2:** `git mv` the three trees; delete the `build/` folders' CMake caches from the move (they are not tracked; check
  with `git status`).
- [x] **Step 3:** a README line in `dev-archive/archive/` saying what each tree is and the tag. Commit
  ("archive gen-1 trees, move only").

### Task 4: Vanilla check (NEEDS TEFA'S YES TO LAUNCH)

- [x] **Step 1:** ask in one line: "RE2 goes vanilla now: Steam verify, then a flat start to the main menu. OK?"
- [x] **Step 2:** Steam verify (`steam://validate/883710`); wait for it to finish; record "N files updated".
- [x] **Step 3:** launch flat with nothing of ours; State-o-matiC verdict at the menu; quit with the game's own quit.
- [x] **Step 4:** board row + the inventory file: "vanilla, menu reached `[verified-live 2026-11-xx, n=1]`". Commit.

## Progress 2026-11-07 (Opus, home PC; Tefa: "please do as much as you can on your own, then save it for a headset test")

- Task 4: Steam verify clean in 40 s (nothing replaced); the plain game reached its title flat; State-o-matiC taught the title
  screen (routes/resident-evil-2/states.json); closed with its own window close.
- Layer 1 deployed, saved build **b193** (series "clean rebuild"). Layer 2a deployed, saved build **b194**, simulator run
  16:20 (log kept in `D:/claude video game stuff/re2-clean-rebuild-logs/`): every body hook in, the loader took our four
  functions, title rain at attempt 1, music muted, the shadow head and hair copies made, the minigun put away and taken out by
  the right hand + RG at the shoulder through VR Bench. The one game exception at the title is the same as b192's.
- LAYER 2 SPLIT (my call): 2a = the body EXACTLY as it ran in b192 (gen-1 body C++ moved, the gen-1 Lua that was really
  installed, kept as Lua); 2b = those Lua scripts made native one at a time, each checked against itself. Why: the five
  "body" scripts the design named (item 11) were NOT in the game folder in b192; what ran were body_anchor, body_direct,
  lefthand_hold, title_rain, title_one_scene, pickup_world, steady_light, mute_music. "The body as it works now" is that set.
- Task 6 as built: `src/core` (common, bridge, joints, weapons, settings as gen-1, exports, plugin), `src/game/game.h` (empty
  until layer 3), `src/guns/guns_api.h` + `guns_stub.cpp` (what fire / suppress / holster / Ghost Hand ask the gun code;
  "nothing of ours" until layer 3), `src/hands/ghosthand`. The bridge Lua is still the gen-1 one, unchanged (it pairs with
  bridge.h's slot numbers; trimming waits for layer 3, when the slots that stay are known).
- Task 11 step 1: RE2's reload is ONE motion per gun on layer 3 (2000:1200), no per-round cycle
  (`dev-archive/rebuild/re2-motion-table.md`): round_in must be cut from the loading hand's curl, new work.

## Layer 1: the loader (Task 5 part A)

### Task 5: build.sh, the install list, and layer 1

**Files:**
- Create: `dev-archive/native/tools/build.sh` (new), `dev-archive/native/tools/install-list.txt`
- Create: `dev-archive/native/game-folder/re2_fw_config.txt` (from the backup, unchanged)
- Create: `dev-archive/native/tools/test_deploy.py`, `deploy.py` (build.sh calls it; done 2026-11-07: test ALL PASS, a
  dry run on the real folder finds all 24 layer-1 files)

**Interfaces:**
- Produces: `build.sh [--deploy] [--layer N]`; `install-list.txt` lines `<layer> <repo path> <game path>`; `--layer N` installs
  every line with layer <= N.

- [x] **Step 1: the failing test.** `test_deploy.sh` makes a temp folder shaped like the game folder, seeds
  `reframework/autorun/visceral_tuning.lua` and `reframework/data/visceral_slide_grip.json`, runs `build.sh --deploy --layer 1
  --game <temp>`, and asserts: the two seeded files are gone (moved to `<temp>/_removed-by-deploy/`), every layer-1 file is
  present and byte-equal, no file outside the list beginning `visceral_` remains. Run: `bash test_deploy.sh` -> FAIL (no build.sh).
- [x] **Step 2:** write `build.sh`. "Ours" = any path under `reframework/` whose name starts with `visceral_`, `visceral/`, or
  is listed in the backup inventory; those not on the list are moved to `_removed-by-deploy/<date>/`, never deleted.
  `--game` defaults to the Steam path `/c/Steam/steamapps/common/RESIDENT EVIL 2  BIOHAZARD RE2` (two spaces).
- [x] **Step 3:** layer-1 lines: the patched `dinput8.dll` (from the backup; its hash `0fe6beab...` checked before copying),
  `openxr_loader.dll`, `_interaction_profiles_oculus_touch_controller.json`, `ref_ui.ini`, praydog's package scripts (`re2_*`, `re4_vr_crosshair`, `re8_vr`, `utility/`, `vr/`) from the 76298bd
  package zip, `re2_fw_config.txt`, `visceral_mute_music.lua` (layer 1 only: the line carries `max=1`). Run the test -> PASS.
- [x] **Step 4: Review Focus 4.** In the test, assert that `install-list.txt` puts the plugin DLL in no layer below 2.
- [x] **Step 5:** commit; deploy layer 1; `builds.py snap ... "layer 1: the loader"`; ask Tefa to wear it: picture in both
  eyes, hands from the first second, height, music silent. `GATE: VR USER`.

## Layer 2: the body (Tasks 6-7)

### Task 6: The new skeleton, core, the bridge, the per-game layer

**Files:**
- Create: `dev-archive/native/CMakeLists.txt` (one target `visceral_native`, option `VN_TOOLS` default OFF, sources by glob
  per folder, `src/tools/` only when ON)
- Create: `src/core/plugin.cpp` (wiring only), `common.h/.cpp`, `joints.h`, `bridge.h/.cpp`, `settings.h`, `weapons.h/.cpp`,
  `json_mini.h/.cpp`, `player.h/.cpp`, `exports.cpp`
- Create: `src/game/game.h`, `src/game/game_re2.cpp`
- Create: `lua/visceral_bridge.lua` (trimmed), `lua/jitterlog.lua` (copied)

**Interfaces:**
- Produces, `game.h` (namespace `vn::game`), grown only as files need it, starting with:
  `extern const wchar_t* const LEFT_FINGERS[17]; extern const wchar_t* const RIGHT_FINGERS[17];` (RE2 order of
  `handpose_data.h kBones`, right side `r_hand_*`); `MO* player(); MO* player_transform(); MO* equipment();`
  `int held_weapon();` (WP number, -1 none); `const char* weapon_key(int wp);` ("wp0000" form, as the takes and Hand Poser
  name guns); `const char* character_key();` ("pl1000" form); `enum class Dock { LOADER };` with
  `void set_dock_socket(const DockSocket&)` writing the struct `visceral_dock_socket()` returns.
- Produces, `exports.cpp`: the four loader exports, `visceral_hands_frozen()` returning 0.
- Produces, `bridge.h`: today's slots 0-84 and 537-539 unchanged in number (the Lua side and the loader agree on them); every
  tuning slot removed; the array length stays 604 until a later commit shrinks it on both sides at once.

- [x] **Step 1:** CMake + `plugin.cpp` with only the bridge installed; build; deploy as layer 2a; simulator run
  (`tools/re2_sim_boot.py`, route A/A/A): the log shows `[visceral-native] bridge: attached` and `S_ACK = 1`.
- [x] **Step 2: Review Focus 1.** Load into a save holding the W-870 at frame one: `player()` returns null for the first
  frames and the log shows `player: found` once, no repeated failures (one line per second at most).
- [x] **Step 3:** commit ("skeleton, core, bridge, game layer").

### Task 7: The body files, moved; the body Lua made native

**Files:**
- Move (copy from `archive/native-gen1/src/`, includes fixed only): `ladder`, `menu_body`, `menu_tint`, `pickup`, `shadowhead`,
  `exposure`, `run`, `fire`, `suppress`, `holster`, `shortcut`, `subweapon`, `options`, `flashlight`, `rainhold`, `spread`,
  `sfx` -> `src/body/` (spread -> `src/guns/`)
- Create: `src/body/locomotion.cpp`, `spine.cpp`, `foot_ground.cpp`, `cinematic.cpp`, `crosshair.cpp`, `music.cpp`,
  `title.cpp` (title rain + one scene), `pickup_world.cpp`, and `steady_light.cpp` if Task 1 found it needed
- Create: `src/hands/ghosthand.cpp/.h` (Village's, joint names through `game.h`): Ghost Hand rides along from this layer
  on (Lanes PROTOCOL 20), with `jitterlog.lua` (PROTOCOL 18)

- [x] **Step 0:** the 80 hold-pose files of item 0b go back from the backup (install-list lines with a `backup:` source,
  never pushed); the in-game hold must look as it did in b192.
- [x] **Step 1:** copy each moved file; a diff against the gen-1 copy shows only `#include` lines and namespace-qualified names
  changed. Record the diff count per file in the commit message.
- [ ] **Step 2:** port each body Lua line for line: the same objects, the same numbers (into `settings.h` under the script's
  name), the same frame point (a `re.on_pre_application_entry("X")` becomes `on_pre_application_entry("X")`). One file per
  script, each under 400 lines.
- [ ] **Step 3:** simulator check per feature, each a log line proving it ran: the crosshair element skipped, `Jog` flipped at
  2.1, the spine offset applied, the pelvis lowered while aiming, the cinematic gate true in the opening cutscene, music
  volume 0, the title rain at attempt 1.
- [ ] **Step 4:** deploy layer 2; snap "layer 2: the body"; Tefa wears it: the ladder, menus over the world, a pick-up, the
  shadow head, the flashlight, running, the holsters and the knife, the title rain. Nothing worse than b192. `GATE: VR USER`.

## Layer 3: magazines, rounds, racking (Tasks 8-9)

### Task 8: The reload family off Village

**Files:**
- Create (from `visceral-re8-vr/dev-archive/native/src/`): `src/guns/reload.cpp/.h`, `reload_internal.h`, `reload_pose.cpp`,
  `reload_throw.cpp`, `magdisp.cpp/.h`, `rack.cpp/.h`, `pumpbolt.cpp/.h`, `dryfire.cpp/.h`, `eject.cpp/.h`, `shells.cpp`,
  `foley.cpp/.h`, `equipguard.cpp/.h`, `zones.cpp/.h` (the grab zones only, no spheres drawn)
- Modify: `src/game/game.h` (the calls these files need; each new function documented with the Village line it replaces)
- Modify: `src/core/weapons.cpp`: RE2's WP table gains Village's `Weapon` fields (`Reload`, `Shape`, `rack`, `pump`, `breech`)

**Interfaces:**
- Consumes: Task 6's `game.h`.
- Produces: `vn::reload::busy()`, `vn::reload::on_round_in()`, `vn::reload::on_mag_out()`, `vn::reload::on_mag_in()`,
  `vn::rack::on_rack()`: the moments Task 11's clip player is called from (Village's names kept).

- [ ] **Step 1:** copy each file; replace every Village-only reference with a `game.h` call; then
  `diff -u <village file> <ours>` must show only those calls. Save the diff list in the commit message.
- [ ] **Step 2:** the RE2 weapon table: magazine guns = every HANDGUN-holster gun except the three revolvers (the Lightning
  Hawk a magazine gun, Q1), plus MQ 11 and LE 5; ROUND = W-870, Remington 870, M3; BREAK = GM 79 (1300, 4100);
  CYLINDER = Quickdraw Army (300), SLS 60 (800), Chief Irons (3200); NONE = specials, knife, grenades.
- [ ] **Step 3:** the rack's dock goes through `game::set_dock_socket` (the loader docks), never by writing the wrist.
- [ ] **Step 4:** simulator checks with VR Bench: Matilda magazine out with B into the hand, LT seats the same one, a pouch
  magazine seats by bringing it to the well; Lightning Hawk the same; W-870 two shells, LT puts them in; slide rack any time;
  RT on empty plays the game's own empty sound, not restarted within 0.15 s; a casing only after a real shot.
- [ ] **Step 5:** commit.

### Task 9: Review Focus 3 and layer 3 install

- [ ] **Step 1:** on the simulator, start a magazine change and trigger a cutscene (the RPD hall door, `re2_sim_boot.py`
  route): the log shows `reload: dropped (cutscene)` and the left hand back on the controller within 0.5 s.
- [ ] **Step 2:** deploy layer 3; snap "layer 3: magazines, rounds, racking"; Tefa wears it. `GATE: VR USER`.

## Layer 4: the opening guns (Task 10)

### Task 10: Break action and revolvers off Village

**Files:**
- Create: `src/guns/breech.cpp/.h`, `gate.cpp/.h`, `drumrounds.cpp/.h`

- [ ] **Step 1:** copy and diff as Task 8; RE2's GM 79 joints from the 2026-11-01 recon (`root, _00, _01, _03, _04, _05, _06,
  _100, _101`); the hinge joint is NOT known (`MODEL: FABLE` row if a side view on the simulator does not show it in two tries).
- [ ] **Step 2:** revolver joints: read each revolver's joint list once (the gun survey's way, log only) and name drum, cover
  and hammer in `game_re2.cpp`.
- [ ] **Step 3:** simulator checks: the GM 79 opens with LG + LT at the front grip and closes with a flick up; a Quickdraw Army
  chamber filled by the right hand; the thumb over the hammer.
- [ ] **Step 4:** deploy layer 4; snap; Tefa wears it. `GATE: VR USER`.

## Layer 5: recoil, rumble, the ammo counter (Task 10b)

### Task 10b: recoil.cpp, haptics.cpp, ammo_hud.cpp off Village

**Files:** Create `src/guns/recoil.cpp/.h`, `haptics.cpp/.h`, `ammo_hud.cpp/.h`; the per-weapon recoil numbers for RE2 from
RELOADED's `re2_vr_recoil.json` (in `archive/native-gen1/`'s sources of `weapons.cpp`) into `settings.h`.

- [ ] **Step 1:** copy and diff as Task 8. Rumble goes out through the bridge's rumble slots (RE2's slot numbers 10-13).
- [ ] **Step 2:** the ammo counter's GUI element: find RE2's ammo GUI name with one flat run (the GUI draw hook logs element
  names while a gun is in hand), then place it at the right hand while RG is held; told on `on_mag_out()`.
- [ ] **Step 3:** deploy layer 5; snap; Tefa wears it: a shot one- and two-handed, the counter. `GATE: VR USER`.

## Layer 6: the hands (Tasks 11-12)

### Task 11: The clip tools made game-agnostic, RE2's motion table first

**Files:**
- Create: `dev-archive/native/tools/clips/motion_table.py`, `make_finger_book.py`, `make_rest_poses.py` (from Village's tools,
  with a `--game re2|re8` switch and a per-game table file `tables/re2.json`)
- Create: `dev-archive/native/tools/clips/test_clips.py`
- Create: `dev-archive/rebuild/re2-motion-table.md` (the measured table, committed; it holds ids, not animation data)

- [x] **Step 1:** `motion_table.py` reads the takes' `motions` column (`D:/claude video game stuff/finger-takes/2026-11-02/
  re2-claire/`) and prints, per gun, the motion ids of the reload in order with their durations and the cycle count. Write
  the table to the note.
- [ ] **Step 2: the failing test.** `test_clips.py`: for the W-870 take, the cutter finds as many `round_in` cycles as the take
  loaded shells (the take's `reloading` column rises that many times); for the Matilda take, `mag_out` ends before `mag_in`
  starts and both are longer than 0.2 s; every written clip's header lists only `l_hand_*` / `r_hand_*` joints. Run -> FAIL.
- [ ] **Step 3:** `tables/re2.json` from the measured table; generalise the cutter. Run -> PASS. If RE2 shows no clean
  per-round cycle on the shotguns: `MODEL: FABLE` row, stop that gun, carry on with the others.
- [ ] **Step 4:** `make_rest_poses.py --game re2`: one Hand Poser file per gun and spot into
  `reframework/data/hand_poser/guns/` (file names `<wp key>[ @ <spot>].json`, the Village format with `"game":"re2"`); Tefa's
  tuned numbers (Task 1) applied as offsets on WP0000 gun / slide and WP1000 gun, and written down in each file's `source`.
- [ ] **Step 5:** commit the tools, the test and the note (never the clips or the takes).

### Task 12: shellclip, fingers, righthand, smooth off Village; Review Focus 2

**Files:** Create `src/hands/shellclip.cpp/.h`, `fingers.cpp/.h`, `righthand.cpp/.h`; clips read from
`reframework/data/visceral/clips/<character>/<gun>_<event>.csv`, falling back to `clips/pl1000/` (Claire's id: CHECK it is
pl1000 or pl2000 from the takes' file names before writing the constant).

- [ ] **Step 1:** copy and diff as Task 8; joint names from `game::LEFT_FINGERS` / `RIGHT_FINGERS`.
- [ ] **Step 2:** the write points: LateUpdateBehavior post + PrepareRendering post first; if the simulator shows the loader
  rewriting the fingers after them (a played clip invisible), BeginRendering pre. Write down which one held.
- [ ] **Step 3: Review Focus 2.** Load Leon's save: the log shows `clips: Leon falls back to Claire's` once, not per frame.
- [ ] **Step 4:** wire the moments: `on_round_in`, `on_mag_out`, `on_mag_in`, `on_rack`.
- [ ] **Step 5:** deploy layer 6; snap; Tefa wears it on Leon and Claire: fingers move the game's way on a round in and a
  magazine change; hands roughly right on every gun (the acceptance line of Q4). `GATE: VR USER`.

## Layer 7: timings and the Animation & Timing hooks (Task 13)

### Task 13: timings.cpp and the window's files

**Files:** Create `src/core/timings.cpp/.h` (Village's, the RE2 names), the `clips_state.json`, `scrub.json`, `edit.json`
handling in `shellclip.cpp` (Village's 2026-11-07 code).

- [ ] **Step 1:** copy and diff as Task 8; every timing name used by the guns files listed in `timings.json` with its default.
- [ ] **Step 2:** a run with the Animation & Timing window: it lists RE2's clips and holds one on a frame.
- [ ] **Step 3:** deploy layer 7; snap; Tefa tries the window once. Then the rebuild is done: board row, front page, the
  `mod/` README rewritten for the new file list (a later packaging job).

## Afterwards (not in this plan)

Praydog's current nightly with our patches rebased (layer 8, its own decision); RE2's own menu (Tefa designs it); RE3 the same
way; Village moved onto the same `game/` layer so the guns and hands files are shared for real.
