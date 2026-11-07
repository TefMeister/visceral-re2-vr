# Visceral RE2: the clean rebuild (design, 2026-11-07, Fable, home PC)

Tefa, 2026-11-07: *"rebuild RE2 from scratch: keep the body as it works now, port the Village weapon parts RE2 lacks, add the
game's own hand animations. everything as native c++ as much as possible"*; and: *"the code must be a bit messy by now and i
want a clean base to work with"*. "From scratch" means what Village's 2026-10-31 rebuild meant: the game folder wiped, only what
Tefa has seen working kept, rebuilt in layers with a saved build per layer, the weapon code written fresh off Village's.

This document is the design. A builder (the Opus sessions that follow) plans and builds from it without coming back to Tefa,
except for the ticks and questions at the end. The plan is written separately (`2026-11-07-clean-rebuild-plan.md`).

## 1. Why

- The RE2 plugin grew by 192 numbered builds in five weeks: 35 source files plus the 2,639-line first-generation plugin
  beside them, 7 Lua files of ours in the build and 13 more stray in the game folder, two parallel hand-pose systems
  (RELOADED's saved poses in `handpose.cpp` + 8 slider "holds" in `tune.cpp`) and Hand Poser on top, and the game folder holds
  11 scripts nobody installs on purpose (`[measured 2026-11-07]`, section 3).
- Village's weapon half is a generation ahead: magazines that keep their own rounds, shells that fly, revolvers loaded round
  by round with the right hand, break actions, recoil and rumble, the empty click, the game's own finger movements cut from
  recorded takes and played on our hands, resting poses from the takes, named timings edited live. RE2 has the raw material
  for the hands (all 20 guns' takes recorded 2026-11-02, Claire) and none of the machinery.
- Every port so far was piecemeal ("rebuilt there, not copied", 2026-11-01), so the two mods drift apart and every Village fix
  has to be re-invented for RE2.

## 2. Goals, non-goals, anti-goals

Goals
1. One clean native plugin for RE2 whose weapon half is Village's code with a thin per-game layer, so a fix lands in both.
2. The body exactly as it works today (section 4, group A), moved over as files, not rewritten.
3. The game's own hand animations on our hands: the finger clips cut from the takes, resting poses from the takes, per gun,
   both hands, wrists free (Tefa 2026-11-02: "wrists are not locked and hands can still be naturally moved in vr").
4. As little Lua as possible: one bridge script, nothing else of ours (section 6).
5. A game folder where every file is installed by one script from the repo, and nothing else is in it.

Non-goals (not this rebuild)
- RE3 (it follows RE2 afterwards, the same way).
- New features nobody asked for (grip-zone spheres, an ammo counter at the hand, body turns; section 4 says which).
- Fixing the open body faults (the ladder flick's last 5 deg, the Single Frame Multipass flicker): the body comes over as it
  is, faults and all; they stay their own board rows.
- Changing REFramework. We stay on the build every finding was read against (section 7). Moving to praydog's current nightly
  is a later, separate layer with its own saved build.

Anti-goals (a failure even if it compiles)
- A rebuild that loses something Tefa saw working (the keep list is the contract; a kept item that stops working is a bug).
- Village's code copied with Village assumptions left in (its Lua VR layer, its joint names, its weapon ids): the port goes
  through the per-game layer or it does not go in.
- Game animation data (the takes, the cut clips) on GitHub. They stay on this PC, in the work folder and the game's data folder.
- A second hand-pose system beside the clips. The old slider holds (`tune.cpp`) and RELOADED's saved poses (`handpose.cpp`)
  do not come over; Hand Poser is the one tuning tool for still poses, the Animation & Timing window the one for clips.

## 3. What is in the game folder today `[measured 2026-11-07]`

| Group | Files |
| --- | --- |
| Ours, installed by build.sh | visceral_native.dll; visceral_bridge, _tuning, _pickup_world, jitterlog (Lua); custom_sfx/common/dry_hammer.wav |
| Ours, in the shipped mod/ but not in build.sh | visceral_locomotion, _spine_straighten, _foot_ground, _cinematic_gate, _crosshair (Lua, the body) |
| Ours, strays (probes, old fixes, by hand) | visceral_body_anchor, _body_direct, _lefthand_hold, _mute_music, _title_one_scene, _title_rain, _steady_light, _flicker_catch, _hand_smooth (Lua); flickers_begone.dll; visceral_grip_sockets.txt |
| Other people's tools | hand_poser.lua (Hand Poser); REFramework.NET + managed/ + source/ (the REFramework MCP); EMV_Engine + EnhancedModelViewer (data) |
| praydog's package, as shipped | re2_sharpness_removal, re2_smooth_movement, re2_vr_crosshair, re2_vr_grenade, re2_vr_melee, re4_vr_crosshair, re8_vr.lua, utility/, vr/ |
| Game root | dinput8.dll (REFramework 76298bd, pd-upscaler build), openxr_loader.dll, PDPerfPlugin.dll + nvngx_dlss.dll (DLSS, OFF since 2026-10-31), re2_fw_config.txt |
| data/ | re2_vr/, visceral/, hand_poser/, custom_sfx/, 9 visceral_*.json/.txt settings and switches |

Which of the stray scripts is still doing something is NOT known: `visceral_title_rain.lua` and `_title_one_scene.lua` may be
the Lua half of the title-rain fix (the native `rainhold.cpp` is the other half), `_steady_light.lua` is the 10-31 light fix,
`_mute_music.lua` the music rule. The builder settles each by reading its header against the board before the wipe (step 1 of
the plan): a stray that a kept feature needs becomes a native module or a listed install; the rest go to the archive.

## 4. Every feature: keep, rewrite, drop (Tefa ticks; my proposed verdicts)

KEEP = moved over as the file it is (new folder, new build, same code). REWRITE = built fresh off Village's file. DROP = out of
the game folder, kept in the archive. Headset-proven = Tefa saw it work (the board's done / "worn" rows).

Group A: the body and the controls (KEEP, as files)
0b. Our relaxed hold poses: 71 `.motlist.524` + 9 look-at `.user.2` files under `natives/` (generated, PC only; found by
    the 2026-11-07 inventory), back in layer 2 from the backup. The controller bindings json (LT free for the
    flashlight) goes in layer 1.
1. The ladder and cupboard view hold + the climbing body guard (`ladder.cpp`) `[verified-live, many]`; the last 5 deg stays its row
2. No third-person body in the menus, menus over the live world, no inventory colour filter (`menu_body.cpp`, `menu_tint.cpp`)
3. The world visible behind a pick-up (`pickup.cpp` + its Lua half, which goes native)
4. The shadow-only head copy, the neck kept, hair hidden in play (`shadowhead.cpp`) `[verified-live 2026-10-28]`
5. The automatic brightness off, steady light (`exposure.cpp`; the Lua `_steady_light` folded in if it still does anything)
6. Running stops at once; RT fires without aiming; RG keeps the gun in hand (`run.cpp`, `fire.cpp`, `suppress.cpp`)
7. Holster spots bound to the headset = the four shortcut slots; the knife and grenades on the back spot, thrown with a swing
   (`holster.cpp`, `shortcut.cpp`, `subweapon.cpp`)
8. Run Type Hold, auto reload off, aim assist off, set once (`options.cpp`); the idle pose off (fw config)
9. The flashlight like RE3's (`flashlight.cpp`)
10. The title rain (`rainhold.cpp` + whatever Lua half it needs) `[verified-live 2026-10-17]`
11. CORRECTED 2026-11-07 (the inventory): these five were NOT installed in b192, so they are NOT part of the body as it
    works now and stay out. What ran instead: body_anchor, body_direct, lefthand_hold, title_rain, title_one_scene,
    pickup_world, steady_light, mute_music (layer 2a as Lua, 2b native). The original line, for the record:
    11. Locomotion (aim-speed + run animation off real speed), spine straighten, foot grounding, the cinematic gate, the crosshair
    off: the five body Lua scripts of the shipped mod, PORTED TO C++ as they are (no behaviour change; section 6)
12. Hand smoothing, guns only, never melee (today's `visceral_hand_smooth.lua` + the plugin's part; Village's `smooth.cpp` is the
    native model) `[verified-live, simulator 2026-11-02]`

Group B: the weapons (REWRITE off Village; each item's Village file in brackets)
13. Magazines by hand: the left hand on the magazine, B drops it into the hand, LT seats the SAME magazine; a new one from the
    pouch goes in by bringing it to the well (the game's own animation, no LT); a magazine keeps its own rounds; a chambered round
    fires with the magazine out (`reload.cpp`, `reload_pose.cpp`, `reload_throw.cpp`, `magdisp.cpp`) `[Village: verified-live 2026-10-31]`
14. Single rounds: shells into the shotguns at the port, two shells docked, LT puts them in (`reload.cpp` ROUND path; RE2's b185 port
    of Village b058 is the reference for what Tefa approved)
15. Racking: slide rack with the left hand (LG on the slide, LT pulls), racks any time, the slide stays open on empty across a
    swap, casing only after a real shot (`rack.cpp`); the pump the same, the game's own pump clip cut (`pumpbolt.cpp` + RE2's
    `pump_native.cpp` idea)
16. Break action: the GM 79 opens with LG + LT on the front grip, the barrel swings, closes with LG + LT or a flick up
    (`breech.cpp` BREAK) `[RE2 b188 worn 2026-11-01, LT not RT]`
17. Revolvers by hand: Quickdraw Army, SLS 60, Chief Irons: drum open with LG + LT, rounds from the pouch by the right hand one
    chamber at a time, cases out, close with the swing; the thumb over the hammer (`gate.cpp`, `drumrounds.cpp`, `breech.cpp`
    CYLINDER) `[Village: Tefa 2026-11-07 "hand on the hammer works, it's really cool"]`
18. The empty click: the game's own empty-trigger sound per weapon on every RT press, never restarted within 0.15 s; the hammer
    as fallback (`dryfire.cpp`; RE2's dry_hammer.wav stays the fallback)
19. Casings and shells fly out on a rack after a shot (`eject.cpp`, `shells.cpp`); handling sounds (`foley.cpp`, native, no Lua)
20. Recoil on the hands, per weapon, one or two hands (`recoil.cpp`) `[Village: worn since 2026-10-15]`
21. Rumble: the game's own on the controllers, left only while supporting the gun, a shot in both hands only while both are on it
    (`haptics.cpp`) `[Village: "sooooooo satisfying"]`
22. The right hand never loses the gun on a swap; a gun already in hand is put away, not re-equipped (`equipguard.cpp`)
23. The game's own left-hand dock OFF while LG is up: in RE2 this is ALREADY our patched loader's job (section 7), so
    Village's `nodock.cpp` is NOT ported. The four exports the loader looks up stay, same names and signatures:
    `visceral_vr_buttons`, `visceral_sub_weapon_out`, `visceral_dock_socket`, `visceral_hands_frozen` (now always 0).
24. Named timings, live from a file (`timings.cpp`), the Animation & Timing window reads/writes them
24b. The ammo counter at the right hand (`ammo_hud.cpp`), shown while RG is held, told the moment a magazine comes out
    (Tefa 2026-11-07, Q2: "yes please")
25. Bullet spread tiers (`spread.cpp`, RE2's own, b125): KEEP as a file (it is RE2-only and worn)

Group C: the hands (NEW for RE2, Village's design)
26. The clip player: the game's own finger movements, cut from the 2026-11-02 takes, played on the hand that does the thing at the
    moment our code does it (round in, magazine out, magazine in, rack), blended in and out, wrists free (`shellclip.cpp`, the
    cutter `make_finger_book.py` made game-agnostic) `[Village: verified-live 2026-11-05..07]`
27. Resting poses for the docked hands from the takes, one Hand Poser file per gun and spot (`make_rest_poses.py`), tuned in Hand
    Poser as before; the relative mode (a clip as a change on top of the tuned pose)
28. The fingers open as the hand reaches for the slide / pump and curl closed on the grab; the right hand's grip tightens on a
    reload as the game does it (`fingers.cpp`, `righthand.cpp`)
29. Ghost Hand and the jitter recorder wired in from layer 2 (`ghosthand.cpp`, `jitterlog.lua`: Lanes' rule, not a tick)
30. Per character: Leon, Claire, Ada, Hunk have different hands (Tefa 2026-10-13). Clips and rest poses are keyed by gun AND
    character; a missing character falls back to Claire's (the only takes). DECIDED (Tefa 2026-11-07, Q4): no new takes for
    this rebuild; Claire's clips on every character. The layer 6 test is Tefa's words: *"as long as hands are roughly placed
    at the right positions when holding guns"*, every gun, Leon and Claire. (Where the hand SITS comes from the dock and the
    rest pose, not from a clip; a clip only moves fingers.)

Group D: DROP (archived, out of the game folder)
31. The hand tuning sliders, 8 holds, per character (`tune.cpp`, `visceral_tuning.lua`, the 5 tuning json files): replaced by
    Hand Poser (still poses) + Animation & Timing (clips + the whole-hand nudge). Tefa's tuned numbers in
    `visceral_tuning_leon.json` and the slide/shell json are NOT lost: the builder converts them to Hand Poser files for the same
    gun and spot before the wipe (one script, checked by reading the angles back).
32. RELOADED's saved left-hand poses (`handpose.cpp`, `handpose_data.h`): replaced by the rest poses from the takes
33. The first-generation plugin (`dev-archive/plugin/`, 2,639 lines, read-only since 2026-10-05): archived
34. Every probe and old fix script (section 3, strays), `reload_block.cpp` (Village's reload.cpp owns the game-reload block
    itself), `spreadprobe.cpp`, `menu_probe.cpp`, the shell-clip test file
35. Hand pairs, grip-zone spheres, body turn, hold-to-keep: never in RE2, not added (Tefa dropped 15/20/23 in Village; the
    11-01 list said spheres only if wanted). The ammo counter is NOT dropped: item 24b (Tefa 2026-11-07, Q2).
36. DLSS: PDPerfPlugin.dll + nvngx_dlss.dll not installed (off since 2026-10-31); the fw config keeps the upscaler off

## 5. The new plugin: shape

- Repo folders: the old tree is tagged `pre-rebuild-2026-11-07` and MOVED to `dev-archive/archive/native-gen1/` (and
  `plugin/` to `dev-archive/archive/plugin-gen0/`), kept, never deleted. The new plugin lives at `dev-archive/native/` again, so
  lanes.conf's build check and build.sh's path keep working. One CMake project, one DLL: `visceral_native.dll`.
- Source layout (the first time Visceral uses folders; the 800/1,500-line rule holds per file):

  | folder | holds | source |
  | --- | --- | --- |
  | `src/core/` | plugin.cpp (wiring only), common, bridge, joints, player, weapons (RE2's WP table + the game's enum), settings.h, timings, menu (native, section 6), json_mini, log | RE2's + Village's, merged |
  | `src/body/` | group A, each file as it is today, includes fixed | RE2 |
  | `src/guns/` | group B: reload family, rack, pumpbolt, breech, gate, drumrounds, dryfire, eject, shells, foley, recoil, haptics, equipguard, nodock, spread | Village (spread: RE2) |
  | `src/hands/` | group C: shellclip, fingers, righthand, rest poses loader, smooth, ghosthand | Village |
  | `src/game/` | THE PER-GAME LAYER: `game.h` is the one header the guns and hands files include for anything that differs between RE2 and Village: joint names (RE2 `l_hand_index_0`.. 17 per hand; Village `L_IndexF1`.. 15), the player/equipment lookups (`app.ropeway.*`), how the VR layer reports the two-handed grip and the hand poses, the weapon id form (WP number), the hand-pose write points | new |
  | `src/tools/` | perfprobe, gunreport, dockreport, the recorders: compiled in only with a CMake option (`VN_TOOLS=ON`); never in a player build | Village |

- The rule that keeps the two mods together: a file in `guns/` or `hands/` may include only `core/` and `game/` headers, never
  anything RE2-specific by name. When it is ported it is DIFFED against Village's copy; the only differences allowed are calls
  into `game.h`. Village gets the same `game/` folder later (its own rebuild step, not this one), so the files become shared.
- Hooks and frame points stay the two mods' common ones: UpdateBehavior pre (the frame), LateUpdateBehavior post (joints after
  the game's animation), PrepareRendering post and BeginRendering pre (the final writes before drawing), UpdateMotion pre (clip
  cuts), the GUI draw hook (pick-ups, menu tint, crosshair). The order of writers at the render points is Village's: still
  hands first, the opening hand, the reload's item, the clip player LAST (it wins while it plays).
- Settings: one `settings.h` (names for every number, grouped by module), plus the live files the windows edit
  (`reframework/data/visceral/`: timings.json, zones/holds the menu saves, hand_poser/ as today, clips/ with the game's data).
  Nothing in `settings.h` is read from a file; nothing in a file is duplicated in `settings.h`.
- Build and install: `tools/build.sh [--deploy]` builds and installs EXACTLY the list in section 8 and nothing else; `--deploy`
  first removes any file in the game folder's reframework/ that is ours and not on the list (so a stray cannot survive a
  deploy). Every install is saved as a numbered build with Lanes' builds.py (`visceral-re2-vr/clean rebuild/` b001...).

## 6. The Lua boundary: one script

REFramework's plugin API 1.15 gives C++ no VR calls (controller buttons, poses, rumble, the VR layer's offsets). The bridge Lua
reads them and hands them over through the shared float array (today's mailbox hook; the mechanism stays). So:

- `visceral_bridge.lua` STAYS, trimmed to readings only: buttons, grips, triggers, sticks, head and hand poses, rumble out, the
  VR layer's view readings for the ladder and the menus. Every slot that carried tuning numbers (S_TUNE.., S_HT.., S_FREEZE) goes.
- `jitterlog.lua` stays (Lanes' recorder, not ours). `hand_poser.lua` stays (the Hand Poser app's).
- Everything else goes native: the five body scripts (item 11), the pick-up world half, the hand smoothing, the sounds (Village
  plays handling sounds from Lua; RE2 plays them natively through the game's own sound calls, `foley.cpp` the way `sfx.cpp`
  already does), the finger recorder (a tool, `VN_TOOLS`).
- NO MENU IN THIS REBUILD (Tefa 2026-11-07, Q3: *"re2 will have its own style so leave that out for now"*). Every setting a
  menu would carry lives in a file under `reframework/data/visceral/` (timings.json, the hand_poser/ files, small switch files
  like today's `visceral_*_off`), re-read when it changes, so Hand Poser and Animation & Timing keep working and a test switch
  is a file. When Tefa designs RE2's own menu it goes native through `on_imgui_draw_ui` (the shared ImGui context, praydog's
  example plugin's way), as its own layer with a saved build `[hypothesis]`.
- The REFramework MCP's own plugin (REFramework.NET) is a dev tool: installed by the MCP, not by build.sh; it stays out of the
  player package.

## 7. REFramework: stay on OUR PATCHED 76298bd for the rebuild

CORRECTION (2026-11-07, Opus, before the plan): the installed loader is not stock. It is our own build of 76298bd with the
eleven cumulative patches in `dev-archive/reframework-patch/` (the newest, `2026-11-02-re2-hand-smoothing-guns-only.patch`,
contains all the earlier ones), `dinput8.dll` sha256 `0fe6beab...82b6` `[measured 2026-11-07]`. Those patches ARE kept
features: the left hand docks only while LG is held and on the gun's own aid joint, the knife / grenade buttons taken from the
plugin's `visceral_vr_buttons()` export, hold-hands-still, one grip per gun and no steering on the slide, the inventory close
without the cutscene path, the switch start keeping the head turn, menu text not squished, the menu close hold, and the hand
smoothing for guns only (it reads `data/visceral/hand_smooth.json`). So: KEEP item 0 = this exact dinput8, restored from the
backup (and its patch kept as the source). In RE2 the left-hand dock lives in this loader, not in a plugin file: item 23
(`nodock.cpp`) is NOT ported; the guns code talks to the dock through the plugin exports the patch already reads, and
`game.h` names that route. A plugin export the patch looks up must keep its exact name and signature (the builder lists them
from the patch before layer 2: `visceral_vr_buttons`, `visceral_hands_frozen`, any others).

Every engine finding of ours (the title rain's scratch-page counter at `ctx+0x78`, FirstPerson's smoothing at a menu open, the
inventory close frame, the ladder's rendered-view lag) was read against this build, and the body comes over unchanged. Changing
the loader under it would muddy every layer test. DLSS is off, so the fork's upscaler is unused; that is a reason to move to
praydog's nightly LATER (a layer of its own with a saved build, after the rebuild is worn), not now. My call.

## 8. The layers (one saved build each, a short headset check after each; the Village way)

| layer | what goes in | the check |
| --- | --- | --- |
| 0 | Step 1 of the plan: back up the whole mod state (game folder's mod files, data/, saves, the system save), convert Tefa's tuning numbers to Hand Poser files, archive the old trees. Wipe. Steam verify. Flat start to the main menu with nothing of ours | the menu with nothing installed |
| 1 | REFramework: OUR PATCHED dinput8 `0fe6beab` (section 7) + openxr_loader, praydog's package scripts as shipped, our `re2_fw_config.txt` (idle timer 86400, Single Frame Multipass, upscaler off, no DLSS files); the music: RE2 keeps it in the system save, not a file, so `visceral_mute_music.lua` goes in for this layer only and becomes native in layer 2 | picture in both eyes, hands from the first second, height |
| 2 | the body plugin: group A + the bridge + jitterlog + Ghost Hand; the five body scripts native | ladder, menus over the world, pick-up, shadow head, flashlight, running, holsters, the rain; nothing worse than today |
| 3 | guns, magazines and rounds: items 13-15, 18-19, 22-23 | Matilda / Lightning Hawk magazine out and in by hand, the W-870 shells, the slide, the empty click, casings |
| 4 | guns, the opening ones: 16-17 (the GM 79, the three revolvers) | the GM 79 swing, a revolver loaded by the right hand, the hammer |
| 5 | recoil, rumble, the ammo counter: 20-21, 24b | a shot in one and two hands; the counter at the hand while RG is held |
| 6 | hands: 26-28, 30 (Claire's clips and rest poses on every character) | the fingers move the game's way on a round-in and a magazine change; hands roughly right on every gun, Leon and Claire |
| 7 | timings: 24, the Animation & Timing hooks (clips_state, scrub, edit.json); no menu (Q3) | the window edits a clip live |
| 8 | (later, its own decision) praydog's current nightly | everything above again |

Each layer: built on the OpenXR Simulator + VR Bench first (RE2 boots on the simulator by `tools/re2_sim_boot.py`, route A/A/A
into the last save), State-o-matiC's verdict before any picture, the per-run performance recorder on, BeG0nE reports read after
every headset test, the collector shelving every run. Tefa wears a layer; the next goes in only after their word.

## 9. The game's own hand animations in RE2: what is known and what the builder must measure first

Known `[measured 2026-11-07 from the 2026-11-02 takes]`: 40 takes for Claire (`re2__draw_idle__wpNNNN` and
`re2__reload_full__wpNNNN`), 148 columns: t, gun, reloading, motions (every layer's motion id and frame), 17 finger joints per
hand as local quaternions (`l_hand_index_0..2, middle_0..2, ring_0..3, little_0..3, thumb_0..2`, the same on the right) and the
two wrists as reference. Village's cutter keys on Village's motion ids (2420 idle, 2511 reach, 2512 round in, 2513 close, 2500
magazine). RE2's ids are NOT known; the builder reads them from the takes' motions column first (a table per gun: which id plays
during the reload, how many cycles for the round-by-round guns) and only then generalises the cutter. The player
(`shellclip.cpp`) is already header-driven (a clip names its joints), so it needs no RE2 table; only `game.h`'s joint lists and
the right "moment" calls (round in, magazine out / in, rack) from the guns files.

Decided: wrists are never written by a clip; the clip drives the fingers of the hand the take shows moving; a clip on a hand
replaces the one playing there; blended in from the pose found and out to it again; written last. The relative mode comes over.
The right hand's hold on the gun (trigger finger) comes from the `draw_idle` takes as a rest pose per gun.

Open for the builder (measure, do not guess): whether REFramework's C++ VR layer in RE2 rewrites finger joints after
PrepareRendering (Village's Lua layer does not; RE2's `handpose.cpp` wrote at LateUpdate + PrepareRendering and held, so the
same two points are the first try, BeginRendering the second).

## 10. Constraints that are standing rules (not re-decided here)

Native C++ by default; numpad keys only; files 800 soft / 1,500 hard; every number named in settings.h; hand smoothing for guns
only, never melee; every build kept; nothing of the game's data on GitHub (the takes, the clips, any game file); a file that did
not work leaves the game folder the same session; the jitter recorder and Ghost Hand in every build; the collector fed; the
close-the-game rule; claims tagged; one writer per file; `~` for the home folder in anything committed.

## 11. What is left to the builder

The per-file port order inside each layer; the exact `game.h` surface (grown as files are ported, never ahead of them); the RE2
motion-id table; the ImGui linkage; the stray-script verdicts of section 3; where each kept Lua line lands in C++; the tuning-
number conversion script; the build.sh list; the test scripts per layer. Anything that resists several attempts or turns up
engine behaviour nobody wrote down is a `MODEL: FABLE` row, said at once, not pushed through on Opus.

## 12. Model plan (Tefa 2026-11-07: Fable only where needed, the hardest Fable work first, then pause for Opus)

Fable's share of this rebuild is this document and the plan's architecture: the folder split, the per-game layer, the Lua
boundary, the hand-animation design, the layer order. Done here. Everything in section 8 is Opus work with a clear method, with
these likely Fable rows (say `MODEL: FABLE` the moment one resists, do not grind): how REFramework's C++ VR layer reports the two-handed grip (never written down; RE2's rack.cpp guessed 0.45 m);
RE2's motion-id table if the takes do not show a clean per-round cycle.

## 13. Tefa's ticks and answers (2026-11-07): the design is APPROVED

No number of section 4 was named wrong (silence = agreed). The answers, folded into the sections above:
- Q1. The Lightning Hawk stays a magazine gun (no objection).
- Q2. The ammo counter at the right hand: IN ("yes please"); item 24b, layer 5.
- Q3. The menu: OUT for now ("re2 will have its own style so leave that out for now"); settings live in files (section 6).
- Q4. Leon's takes: OUT; Claire's clips on everyone, "as long as hands are roughly placed at the right positions when holding
  guns" (the layer 6 test, item 30).

Next: `MODEL: OPUS` writes `2026-11-07-clean-rebuild-plan.md` from this document and starts layer 0.
