# 2026-11-19: the RE2 ideas against the board and the code; the board's stale rows; code shape

By the /lm session's reader (read only), 2026-11-19, Fable session. Everything is `[inferred-static]` unless it quotes the board or Tefa. Line numbers are of `claude-memory/status/visceral-re2-vr.md` AS IT STOOD at 10:3x on 2026-11-19 (432 lines; the OPEN block was lines 11 to 301), BEFORE the same session moved the ticked rows out; they no longer match, search by the quoted words. Not checked: the game itself, the full text of every board row (rows older than 2026-11-01 were read by their first 200 to 430 characters), the REFramework patches beyond greps, the Hand Poser / builds repos. The reader edited, launched and committed nothing.

Code read: `dev-archive/native/src/` (the clean rebuild, 9,892 lines with the Lua), `dev-archive/native/tools/install-list.txt`, `dev-archive/reframework-patch/`, `modding-notes/2026-11-07-clean-rebuild-design.md`, `modding-notes/2026-11-07c-b198-headset-pass-tefa-results.md`.

## The ideas

### Fresh (number as in `ideas.py list visceral-re2-vr`)
1 PARTLY: a different ammo type on each hip. The shared gun code knows guns with several kinds of round (`src/guns/rounds.cpp`, `shells.cpp` line 105: LT steps the pouch's kind), but RE2's table is empty (`src/game/guns_table.cpp`: `round_gun` returns nothing, "the GM 79's flame / acid item numbers are not read yet"). Board line 27. Tefa scrapped the right-hip spot on 2026-11-10 (line 27: one pouch, LT steps the kind), so the idea as worded is overtaken by his own later word: ask.
2 PARTLY: the right grip holds the weapon, and a stick dodge. The dodge is built as the side step, left stick + right stick CLICK, not right stick down (`src/hands/sidestep.cpp`, b213, line 15, simulator only, unworn). "Let go of the right grip and the gun goes back to its holster" is not built: no such code in `src/body/` (RG takes out and puts away at a spot, `holster.cpp`).
3 PARTLY: a settings menu dressed as the officer's notebook. The notebook hangs in the world on X + A (`lua/visceral_options_book.lua`, b199, line 28, simulator only); no options on it yet, no page turn (line 40 has the plan).
4 NOT STARTED: pistol in two hands, smoothing down by 75%. The smoothing asks only "is a real gun in hand" (patch `2026-11-02-re2-hand-smoothing-guns-only.patch`); no two-hand case.
5 NOT STARTED: two LT presses, one shell each. Today ONE press starts the feed and both shells go in by themselves, a step apart (`src/guns/shells.cpp` lines 130 to 145: `g_feeding`, `SHELLS_STEP_S`). Also asked in line 29.
6 NOT STARTED as a held map. Menu button tap = the game's map screen is built (b210, line 19, unworn).
7 NOT STARTED: left- and right-handed profiles (no code).
8 NOT STARTED: VR comfort options (no vignette or snap-turn code of ours; the book's "Comfort" page is a plan, line 40).

### Chosen earlier (`ideas/chosen/`, in the order of `ideas/decided.md`)
- Red dot in the sight's glass: NOT STARTED (no code; praydog's `re2_vr_crosshair.lua` is installed as shipped).
- Third person for ladders, the plan B: NOT STARTED and probably not needed: the first-person ladder hold is in (`src/body/ladder.cpp`) and Tefa 2026-11-07: "Ladders: fixed" as far as he can tell.
- Coloured sphere while configuring a holster spot: NOT STARTED (the spots are fixed numbers in `src/body/settings_body.h`; no in-game configuring).
- In-VR REFramework panel, colour-coded: PARTLY, the same work as fresh 3 (the book, no options yet).
- Hand smoothing: BUILT, guns only (patch 2026-11-02, `lua/visceral_hand_smooth.lua`; lines 41, 53), simulator only; Tefa's point 7 ("do the sliders change anything") is unanswered. PARTLY until worn.
- Chest holster for the sub weapon: DONE IN ANOTHER FORM: the back-right spot on RG (`src/body/subweapon.cpp`, `holster.cpp`); Tefa 2026-11-07: "Grenade: comes out of the back (works)"; line 188 ticked. No chest spot exists.
- RG should not enter aim mode: DONE (`src/body/fire.cpp`, `suppress.cpp`; line 217 ticked, b121 worn "i can now run and shoot at the same time").
- Two-handing makes a weapon accurate / two hands, no spread / spread tiers (three ideas, one feature): DONE in code exactly as Tefa's numbers (`src/body/spread.cpp`, `settings_body.h` lines 121 to 133); b127 worn, "make this one the new baseline" (line 215). Still pinned open on one question: does it steer shots fired with RT alone.
- Fire means something different to each enemy: NOT STARTED.
- Reach to a hip to change ammo type: PARTLY, see fresh 1.
- No view relaxing back to centre when idle: DONE by a setting (`game-folder/re2_fw_config.txt`: `VR_MotionControlsInactivityTimer=86400`; line 79). From the file only; no headset word on the board.
- The game's own animations take over the hands: NOT STARTED (the finger takes are recorded, line 52; the clip player is design layer 6, not built).
- Controller drift when one controller hides behind the other: NOT STARTED in RE2 (only the recorder line in `lua/jitterlog.lua`).
- Casings stay on the floor: NOT STARTED. Note: casings flying out on a rack (design item 19) is not in the clean tree at all (no eject file in `src/guns/`).
- One background everywhere, the last-save scene: DONE (`lua/visceral_title_one_scene.lua`, on the install list; line 241 ticked). Two leftovers still open in line 243.
- A launcher to switch features: NOT STARTED.
- Open doors with the controller: PARTLY: built (`src/hands/doorpush.cpp`, b215; a gun pushes doors, b216), not seen working in RE2 even on the simulator (lines 12, 13).
- Sub weapon from a back holster on RG, RG no longer aims: DONE (as above).
- More zombies: NOT STARTED.
- Every menu over the live world: PARTLY: the inventory shows the world (`src/body/menu_tint.cpp`; lines 91, 95 ticked); the pick-up shows the world but the item sits BEHIND the player (Tefa 2026-11-07); the key-use camera jump, the map and the item box have no row.
- Spent magazines stay where they fall: DROPPED by Tefa 2026-10-15.
- Pumping a live round throws it out: NOT STARTED (`src/guns/pump.cpp` ejects a spent shell only).
- Universal poser / an EMV-style poser for other games: not this project's code; nothing in RE2. Not checked in the poser repos.
- Nail polish that cracks and fades: NOT STARTED (natural nails only, `tools/blender/hd_hands_paint.py`).
- Hands glide to the gun: NOT STARTED in the installed build (no blend in `src/guns/left_hand.cpp`; the opening-hand work of b172 / b173 was gen-1 and is not in the clean tree).
- Fix the broken cutscenes on a flat screen: PARTLY: step 1 built and on (b212, line 17), never run.
- Hands stretch to stay on the gun: NOT STARTED (no code).
- Holsters like our RE8 mod: PARTLY: six spots built (`src/body/holster.cpp`, b208, line 22); Tefa 2026-11-07 "Holsters: work" (the four older spots). Missing: the new HUD, the thumb-rest D-pad (no such code), back-left medicine ("does nothing yet", `holster.cpp` line 132); open fault: right hand at the LEFT hip did not bring out the shotgun (line 39).
- Reloads like our RE8 mod: PARTLY: written clean (`src/guns/rack.cpp`, `magazine.cpp`, `shells.cpp`, `pump.cpp`, b195), simulator only; Tefa's points 2 to 6 unanswered. Revolvers and the GM 79 are NOT in the clean tree.
- Title rain after quitting to the main menu: PARTLY: cured in b178 (`src/body/rainhold.cpp` + `lua/visceral_title_rain.lua`; line 96 "rain is now fixed"; 2026-11-07 "present"). The after-quit case itself has no word since, and quitting hung on black on 2026-11-09 (line 36).

## The board

The OPEN block was 290 lines; by a rough count about 45 are live work. The rest is history that reads as open.

### (a) Ticked, struck, superseded or folded rows still inside OPEN
- Ticked: 16, 20, 26, 50, 65, 67, 76, 85, 88, 89, 102, 142-147, 180, 181-183, 188, 197, 205, 207, 216, 217; ticked but still carrying a gate tag: 91, 105, 139-141, 241, 247, 255.
- Struck through: 23, 159-175.
- "superseded" / "built as" / "worn ->" in italics: 92, 93, 94, 95, 96, 97, 98, 100, 101, 103, 191, 194, 222, 223, 239, 242, 245, 246, 248-254, 256, 257, 258, 290.
- "folded into": 44, 212, 218. "ignore for now": 185. "history" / "fallback" / "Earlier": 210, 213, 214, 259.
- Headings and notes with no work in them: 58, 60, 113, 265, and the whole session log of 2026-09-09 at 293-301 (indented, so it counts as OPEN).

### (b) Rows overtaken by a newer row of the same feature
- Clean rebuild: 46 says "DESIGN WRITTEN, NOTHING BUILT" and 45 "LAYER 0 DONE"; 43 / 42 / 41 show layers 1 to 3 built and installed (b195 to b198). What is really left (layers 4 to 8: GM 79, revolvers, recoil, rumble, the finger clips, timings) has no row of its own.
- Wear rows b194 (44) -> b195 (43) -> b197 (42) -> b198 (41) -> worn 2026-11-07, body done (39). 41, 43, 44 are one test now: points 2 to 7 of line 39.
- Guns by hand, gen-1 builds replaced by the clean rewrite of b195: 51 (pump, 11-02), 68 / 69 / 70 (b182 to b184), 90 (b183), 104 (b174), 106 (b176), 116 (b166), 117 (b165), 187 (b143), 189 (b132).
- GM 79: 56 says "b188 INSTALLED, UNWORN"; 55 is Tefa's wear of it (2026-11-01). And the break action is not in the clean tree: no breech file under `src/` (it is only in `dev-archive/archive/native-gen1`), which answers line 37's question.
- Revolvers by hand: 47 and 57 are the same next job; 31 and 35 are Tefa asking for it again. One row is enough.
- Rounds in the magazine: 118 (2026-10-13, "model some bullets") is answered by 25 and 24 (b201 to b205); list item 34 is the same feature.
- Ladder / switch start: 195 (b086) -> 87 (b185) -> 99 (b188) -> 75 (b194 of 10-29) -> 48 (b192, measured). Tefa 2026-11-07: "Ladders: fixed ... the 5-degree nudge is nothing" (line 39). His word exists; the chain can close on it. His new ask (block or hand-drive the left stick on a ladder) has no row of its own.
- Pick-up screen: 203 (b110), 190, 191 -> 49 (b192). Tefa 2026-11-07: world visible, item behind the player. 49 still says "UNWORN".
- Menus: 198 (b088), 206, 199 (b108) -> 95, 91 (ticked).
- Torch: 292 (roadmap G5), 149-158 (b152 "parked") -> 22 (b208). Holsters: 209 (b076 / b077) -> 22.
- Picture under Single Frame Multipass: 73, 72, 66, 64, 63, 62 -> 61 is the newest word ("still there", next lead named). 74 is done by 67.
- Brightness / head shadow: 83, 82, 81 -> 78 (shadow, "looks right") and 67 (brightness, ticked).
- Body pose and shake hunt: 224-235, 260, 261, 275, 277, 279 are all older than the golden rows 220 / 219.
- Old first-generation plugin (dock v0.6, the grip write, the head hider, bracelets, neck plug): 270, 272, 280, 284, 289 describe a plugin that is archived (`dev-archive/archive/plugin-gen0`); nothing of it is in `src/`.
- Hand tuning sliders and hand shapes: 109, 110, 111, 112, 114, 115, 127, 128-138 are tests of a menu that is no longer installed (`visceral_tuning.lua` is not on the install list; the design dropped it, items 31 and 32). They cannot be worn in b216. They need Tefa's word to close, or one new row "hands per gun, the new way".
- Dates that have passed: 104 and 109 ("RESUME HERE", 2026-10-14), 120-126 ("WEDNESDAY 2026-10-14"), 184 ("RESUME HERE NEXT WEEK", 10-10), 193 ("QUEUED 2026-10-06 ... while Fable is unavailable"), 40 ("NEXT WEEK", 11-07; the first milestone is line 28), 47 ("in about an hour", 11-05).

### (c) What Tefa must test in the headset, by feature (the reader's suggested grouping)
Headset rows come off only on Tefa's word; this is the list to re-cut against the installed build, not a list to delete. The IDs actually given are in `claude-memory/feature-ids.md` (the session gave IDs to the newest wear rows only).
- Hands and guns at walls: 12.
- Doors by hand, body and gun: 13 (and 12's last check); Tefa's stuck door, 32.
- Side step (left stick + right stick click): 15.
- A only interact, Y the inventory: 15.
- Menu button: tap = map, hold = pause: 19.
- Cutscenes on a flat screen: 17.
- Torch in the left hand and the six spots: 22; fault from 39: right hand at the left hip.
- Rounds seen in the magazine and chamber: 25, 24; Tefa's item 34.
- The options book appears: 28.
- Guns by hand (magazine, dry click until racked, LE 5 magazine, W-870 shells and pump, magazine stays out across a swap): points 2 to 6 of 39; rows 43, 54; the Lightning Hawk "slide at the back" fault (39) and "slides, when they stay open" (33); two LT taps (29).
- Hand smoothing sliders: point 7 of 39; rows 41, 53.
- Ladder and switch start: 48 (Tefa's word of 2026-11-07 already given).
- Pick-up screen: 49; the item behind the player (39); the world brightening (38).
- GM 79 by hand: 55's fault list, 37; not testable until it is rebuilt.
- Revolvers by hand: 31, 35; not built.
- The random white flash under Single Frame Multipass: 61.
- Colours and light: the blue in the dark (77), DLSS off (71).
- Head shadow and hair: 78, 80.
- Bullet spread: 215 (pinned).
- Automatics fire while RT is held: 211 (b099, never ticked; the gun list is still in `settings_body.h` line 155).
- REFramework menu text not squished: 148.
- Quit to menu / save change hangs on black: 36, 39.
- Old one-offs to ask about once: the forearm twist on the pump (107), the inventory going wild with the controllers on the table (204), the grenade flip (200, probably closed by 188), HD hands on Claire (283, 285, 286) and "HD hands for the others" (29).

### (d) Gate tags wrong or missing
- 54 `[PD]`: the fix is installed and unworn, so `[VR USER]`.
- 45 `[FLAT]` and 79 `[FLAT]`: both say done; no flat work left.
- 59 `[VR CLAUDE]`, 238, 240 `[VR CLAUDE]`, 244 `[PD]`, 236 `[VR USER]` ("seen next launch", from 2026-10-03): done work under a live tag.
- 30 to 38 (Tefa's list of 2026-11-09) and 29 sit under `[VR USER]` although each first needs a look or a build with no headset: 31, 35 (build), 36 (read the kept log), 37 (answered above), 38 (compare the log) are `[PD]` / `[FLAT]`.
- 56 `[VR USER]` "unworn" though worn (55).
- No tag at all: 113, 176-179 (the "release with button racking" decision), 196, 208, 210, 213, 214, 259, 265, 293-301.
- `[PD @home]` (263, 266, 269): not one of the gate tags, and "home" changed meaning on 2026-11-11.
- `[USER]` rows 118, 262, 273: 118 is done by Blender work already made; 273 (download a speed mod by hand) is overtaken by 197 (running works).

### (e) Facts the code or newer rows contradict
- Line 11 (the OPEN heading) still says "a side-step plan for RE2 waiting for Tefa's yes" (built as b213, line 15) and "b209 ... installed" (b210 replaced it, line 19).
- Line 192: "DLSS STAYS ON FOR RE2" (2026-10-09) against line 71 (off in every RE game, 2026-10-31) and the installed setting `TemporalUpscaler_Enabled=false`.
- Lines 176-179: "release with Arcade Controls style racking" against 117 ("hand racking is the way now") and the clean `rack.cpp`.
- Line 208: "manual reloads wait until after shooting without RG": both are built.
- Lines 181-183: D-pad = hold the right stick click + left stick. The side step now uses the same two controls (`sidestep.h`), and the D-pad profile file is still on the install list. Whether they clash is `[hypothesis]`; worth one look in the log.
- Lines 20 / 21 say the MAP goes at the left back holster; the code has "medicine" there, doing nothing yet (`holster.cpp` lines 12, 44, 132), and the map on the menu tap. Ask Tefa which holds.
- Two build numberings run through the rows (the board's own and the builds store's): "b192" and "b194" each name two different builds (lines 77 / 48 and 75 / 44; line 70 says "builds.py b182 (after repo b194)"). Any re-cut should say which.
- Line 9 (above OPEN) pins the shake hunt page as live; the hunt ended with the golden build (220 / 219). Stale rather than wrong.

### Worth knowing, outside the three jobs
- `lua/visceral_steady_light.lua` is on the install list, and its own header says not to install it beside RE2's native brightness fix (`src/body/exposure.cpp`). Not checked whether it switches itself off in RE2. A possible lead for "the pick-up screen brightens the world" (38) `[hypothesis]`.
- The board file is 376 KB; single rows run to 10,000 characters (199, 260, 288).

## Code shape (`code-shape-scan.py`)
0 files over 1,500 lines, 0 near the Lua locals limit, 2 over 800:
- `dev-archive/native/include/reframework/API.hpp`, 1,090: REFramework's own header, not ours.
- `dev-archive/tools/blender/hd_hands_paint.py`, 1,147: ours, a one-run Blender script (last changed 2026-09-07). It is ten small helpers (lines 58 to 320) and then one long top-to-bottom run of passes that share the same variables. A move-only split would take out about 150 lines and leave it near 1,000, and four of those helpers read the script's own variables (`N`, `rng`), so even that is not purely a move. Getting under 800 would mean wrapping the passes in functions, which is a rewrite. **Verdict (the session's): `keep: a finished one-run tool, not touched since 2026-09-07`; no split made.**
- The mod's own source is well inside the limits: the largest file is `src/body/ladder.cpp` at 470 lines.
- 14 files with 40 or more loose numbers; the ones in the mod itself are `src/game/guns_table.cpp` (217, a data table by design), `lua/visceral_walls.lua` (53) and `lua/visceral_bridge.lua` (45, both mostly bridge slot numbers that already have names).

## What the session did with it (2026-11-19)
- The board: every ticked / struck / superseded row of (a) moved out of the OPEN block under "Done, moved out of the OPEN block on 2026-11-19"; the heading re-dated and its two stale phrases of (e) dropped; feature IDs on the newest wear rows. The older `[VR USER]` rows of (b) stay (they come off on Tefa's word).
- The questions for Tefa from (e) and the fresh ideas are in the session's write-up, by ID.
- Code tidy: nothing to split (above).
