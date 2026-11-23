# 2026-11-23 (reader, static): left-hand stages and Hand Poser for RE2 / RE3, groundwork

Read-only groundwork for the board row "HAND POSES THROUGH RELOAD AND RACK NEED BUILDING HERE FIRST". Nothing was run, built
or installed. Every claim is `[measured from file]` (the line says so) unless tagged `[inferred-static]`. Paths are under
`~/github-backups/`. RE2 commit read: 1faade5; RE3: 2d48592.

## 0. Three things the brief had differently

1. **Village does not send the gun through slot 84.** In Village slot 84 is `S_SLIDE_HAND_X` (visceral-re8-vr
   `dev-archive/native/src/bridge.h:54`). Hand Poser gets Village's gun from praydog's `re8vr.weapon`: the weapon's GameObject
   name, e.g. `ri3042_Inventory` (hand_poser.lua:48-67). Slot 84 is only read from **`visceral_bridge`** (the RE2 / RE3
   bridge), as a WP number turned into `wp%04d` (hand_poser.lua:69-76).
2. **RE2's bridge.h already has the gun slot: `S_WHO = 84`** ("WRITTEN BY THE PLUGIN: the gun in hand (WP number), then the
   character at +1", visceral-re2-vr `dev-archive/native/src/core/bridge.h:40`). The writer was gen-1's `tune.cpp:58-59`
   (`dev-archive/archive/native-gen1/src/`), dropped in the clean rebuild. Nothing in the clean tree calls
   `bridge::put(S_WHO ...)`: the only `bridge::put` is `left_hand.cpp:71` (S_LEFT_ON_GUN).
3. **Today's script would not give "one pose per gun": it would give `wp0000` for everything.** The Lua array is zero-filled
   (`visceral_bridge.lua:49-51`), slot 84 is never written, and hand_poser.lua:74 accepts any number `>= 0`. So RE2 reports
   "wp0000" (the Matilda) with any gun and with none. RE3 reads slot 57 (hand_poser.lua:47), also always 0 in the rebuilt
   bridge: the same "wp0000". `[inferred-static]`. Also `bridge::put` writes only while `live()` (headset on,
   `bridge.cpp:52,74`): on a flat-screen run the slot stays 0. The Lua should start slot 84 at -1.

## 1. Village: what hand_poser.lua consumes (bridge `visceral_re8_bridge`, array N = 280, `visceral_re8_bridge.lua:15`, `.n` at :473)

| slot | bridge.h name | meaning | read at (hand_poser.lua) |
| --- | --- | --- | --- |
| 3 | S_LTRIG | LT held (docked part stays "ready on <spot>" until LT) | 377-382 |
| 5 | S_RTRIG | RT held ("trigger pulled" spot, the trigger ease) | 404-411 |
| 36 | S_GRIPPING_WEAPON | the game's two-handed dock (re8vr.was_gripping_weapon) | 625 |
| 153 | S_LEFT_SPOT | the left hand's spot number (table below) | 260, 323-354, 624 |
| 184 | S_RIGHT_SPOT | the right hand's spot 1-6 | 400, 412-430 |
| 186 | S_CLIP_PLAYING | a finger clip plays: +1 left, +2 right, that hand is left alone | 735-741 |
| 213 | S_LEFT_AREA | which area a "ready" (14) left hand is in | 280-289 |
| 214 / 215 | S_LEFT_HOLSTER / S_RIGHT_HOLSTER | holster spot 1-4 per hand (written by the holster Lua) | 299-307 |
| 216 | S_RIGHT_AREA | the right hand's gun zone while the left holds the gun | 432 |
| 279 | S_MAG_PULL | 0 = no magazine on the grip's line; 1 + u (u 0 seated .. 1 out) | 458-468 |
| (out) 29 | S_POSER_HELD | the bridge Lua copies `_G.hand_poser_held` here; fingers.cpp stands down | set at 793 |

Not from the bridge: the gun (`re8vr.weapon`), the guard (`re8vr.wants_block`, 315-322), pair names
(`visceral_re8/pairs.json`, 263-274).

Left spot numbers (hand_poser.lua:329-353; writer pairs.cpp:222-271):

| n | name | n | name |
| --- | --- | --- | --- |
| 0 | "" (the gun's own pose) | 12 | slide from the left at the back |
| 3 | slide | 13 | magazine in hand |
| 4 | magazine | 14 | ready / ready at <area> (slot 213) |
| 5 | handle | 15 | free |
| 6 | drum | 16 | two magazines in hand |
| 7 | insert point | 17 | spare in hand, docked on the magazine |
| 8 | slide from the left | 18 | spare in hand, pulling the magazine out |
| 9 | rounds from the pouch | 19 | spare in hand, magazine still in the gun |
| 10 | hammer | 1000+N | land N <pair name> |
| 11 | slide at the back | 2000+N | start N <pair name> |

Area codes for slot 213 / 216 (280-282): 1 pouch, 10 magazine, 11 well, 12 slide, 14 front grip, 15 handle, 16 drum,
17 insert point, 18 slide from the left. Holsters (300): 1 right shoulder, 2 right hip, 3 left hip, 4 left shoulder.
Right spots (424-429): 1 on the cover, 2 on the bolt, 3 over the hammer, 4 on the bolt at the back, 5 thumb on the latch,
6 the latch flicked open; with 0, a gun and RT held 1 s: "trigger pulled".

⚠️ A clash in Village itself: pairs.cpp:40-41 gives the shell-port dock (`zones::WELL`) spot **8**, which the Lua names
"slide from the left" (336). A shotgun's hand at the port therefore loads the "slide from the left" pose name. Do not copy
that to RE2: give the shell port its own number.

Pose files (444, 510, 359): `reframework/data/hand_poser/guns/<gun>[ @ <spot>].json`, `{"joints": {name: [x, y, z] degrees}}`,
key = gun ("no gun" when none) + " @ " + spot; the two hands' spots join as "left + right". Characters `<>:"/\|?*` become
`_`. The app keeps its own copy per game under its data folder (`app/Pose.cs:85,324`) and mirrors into the game folder
(`Pose.cs:306-318`). The script and the app talk through `hand_poser/state.json`, `command.json`; the robot's way in is
`hand_poser/remote.json` `{"seq": n, "set": {joint: [x, y, z]}}`, answered in `remote_done.json` (`app/Remote.cs:7-13`),
read by the WINDOW, not by the script.

Joint names: the script is name-agnostic: it lists the skeleton (150-168), holds whatever names the file or the window gives,
and sorts hands by the first two letters, lower-cased (`l_` / `r_`, 226, 501, 763). The wrist names already include
`l_arm_wrist` / `r_arm_wrist` (224). The app already knows RE2's names (`app/JointNames.cs:4-23`, `PoserUi.cs:440-449`).
Village-only by name: the sway (`^(L_%a+)F[12]$`, 722) and the built trigger pose (644, switched off at 646).

### The right hand in Village's plugin (righthand.cpp), to port

- Joints: `R_IndexF1..3, R_MiddleF1..3, R_RingF1..3, R_PinkyF1..3, R_Thumb1..3` (28-32); the index three come first.
- **trigger_pull() (199-227)**: want = TRIGGER_PULL_ON, in play, a gun in hand, RT held, not on the bolt / cover. `k` eases
  0..1 over TRIGGER_PULL_S 0.07 s (`settings.h:495`), smoothstep. Each index joint: read `get_LocalRotation`, multiply by a
  turn about its own Z of `TRIGGER_PULL_DEG = {4, 20, 10}` degrees x u (`settings.h:494`), `set_LocalRotation`. The base is
  read once per frame number (S_FRAME): BeginRendering comes twice a frame on Two Frame Sequential and bending the already
  bent finger doubled the pull (203-204, "measured").
- **Where**: `begin_render()` (229), called at **BeginRendering pre**, the last of our writes (`plugin.cpp:132`). The comment
  at 194-198 says Hand Poser's own "trigger pulled" pose never showed in Village because its write was not the last.
- **The closed grip ("Firm", 106-169)**: FIRM_CAPTURE_S 0.12 s after a live shot (`recoil::last_shot_s()`), all 15 right
  finger rotations are read and kept per gun id (in memory, until the game closes); from then on, whenever that gun is out
  and the right hand has no other job, the fingers are eased to them (nlerp, FIRM_EASE_S 0.15) at PrepareRendering post and
  written again at BeginRendering pre. While RT is down and 0.25 s after (FIRM_TRIGGER_FREE_S) the three index joints are
  neither held nor captured. Hand Poser, which writes after it at PrepareRendering, wins on any joint it holds.
- "Still" (45-100): no weapon out: the right fingers eased to `get_BaseLocalRotation` (rest pose) plus a small sway; the
  left the same when free.

## 2. RE2: the left-hand states the plugin has, and the Village spot for each

All in `visceral-re2-vr/dev-archive/native/src/guns/` (shared with RE3).

| state | where | today's accessor | spot |
| --- | --- | --- | --- |
| hand on the slide / bolt / pump, not yet at the back | rack.cpp:39 `g_hand_on` | `rack::active()` (264) | 3 |
| the same, the part at the back | rack.cpp:42 `g_pulled` (set 149, cleared 158 / 177) | none: add `rack::at_back()` | 11 for pumps and bolts; pistols keep 3 (Village's rule, pairs.cpp:232) |
| pump fore-end gripped with LG, no LT yet (the two-handed hold) | rack.cpp:40 `g_grip` (170) | none: add `rack::gripping()` | 5 "handle" (proposal) |
| hand on the seated magazine | magazine.cpp:31 `g_hand_on_mag` (106-113), `St::SEATED` | `magazine::hand_docked()` (258-261) | 4 |
| magazine sliding out into the hand | `St::SLIDE_OUT` + `g_out_to_hand` (88, 187-190) | inside hand_docked() | 13, pull u = ease(t / 0.19 s) |
| magazine in the hand | `St::IN_HAND` (magazine_internal.h:9) | `hand_socket()` returns 0 (267) | 13 |
| magazine going in | `St::INSERT` (176, 200-206) | inside hand_docked() | 4, pull u = 1 - ease(t / 0.19 s) |
| shells / rounds in the hand, away from the port | shells.cpp:29 `g_n > 0`, `!g_docked` | `shells::in_hand()` (157) | 9 |
| shells docked at the port | shells.cpp:31 `g_docked` (124) | `hand_socket()` returns 1 (159-164) | NEW 20 "shell port" (not Village's 8) |
| drum: LG held with the wrist at the drum | cylinder.cpp:33-40 `hand_at_drum()` (a test, not a state) | none: add `cylinder::hand_on_drum()` | 6 |
| drum open | cylinder.cpp:26-27 `g_want_open`, `g_deg`; `is_open()` (70) | yes | no left spot in Village; keep 6 |
| hammer | not in the clean tree (the revolvers are not by hand, board row "LAYERS 4 TO 8") | - | none (10 later) |
| spare magazine stages | no second magazine in RE2 / RE3 | - | none (16-19 later) |
| two-handed grip on the gun's own aid joint | the LOADER docks it; the plugin only guesses: LG held within 0.45 m of the gun hand (left_hand.cpp:67-70, `LEFT_ON_GUN_NEAR_M`, tagged hypothesis in settings_guns.h:82) | S_LEFT_ON_GUN | 0 + "on the gun" = the gun's own pose |
| the torch in the left hand | hand_state.h:7 | sets S_LEFT_ON_GUN (left_hand.cpp:66) | undecided: today it would get the gun's pose; a new spot ("torch") is cleaner |
| free near a part ("ready") / free | not computed | `rack.cpp:104 hand_near_slide`, `magazine_pose.cpp:113 hand_to_dock` are private | 14 + area, 15: a second step |

Proposed code (new file `src/guns/stage.cpp` + `.h`, shared):

```cpp
// 0 none, else Village's spot numbers (hand_poser.lua current_spot) + 20 shell port
int left_stage() {
    if (gf().wp < 0 || !bridge::live()) return 0;
    if (shells::docked()) return 20;                                   // new accessor: g_docked
    if (shells::in_hand()) return 9;
    switch (magazine::left_stage()) {                                  // new accessor over detail::g_st
        case 4: return 4;                                              // SEATED && g_hand_on_mag, or INSERT
        case 13: return 13;                                            // SLIDE_OUT && g_out_to_hand, or IN_HAND
    }
    if (rack::active()) return (rack::at_back() && rack::stage_at_back()) ? 11 : 3;   // stage_at_back: is_pump(wp) or a long gun
    if (rack::gripping()) return 5;
    if (cylinder::hand_on_drum()) return 6;                            // ours() && LG held && hand_at_drum() && !shells::in_hand()
    return 0;                                                          // step 2: 14 (+ area) / 15
}
float mag_pull() {   // 0 = not on the line, else 1 + u
    // SLIDE_OUT && g_out_to_hand: 1 + ease(t / RELOAD_SLIDE_SEC); INSERT: 1 + (1 - ease(t / RELOAD_INSERT_SEC)); else 0
}
void publish() {
    put(S_WHO, gf().wp >= 0 ? gf().wp : -1);  put(S_WHO + 1, gf().claire ? 1 : 0);
    put(S_LEFT_STAGE, left_stage());  put(S_MAG_PULL, mag_pull());
}
```

The order follows left_hand.cpp:35-52 (the reload has the last word over the rack) and Village's pairs.cpp:232-245.

Where to call it: at the end of `left_hand::frame()` (UpdateBehavior pre, guns.cpp:50) **and** at the end of
`guns::late_point()` (guns.cpp:53), because the magazine's `hand_checks()` (IN_HAND -> INSERT / dropped, magazine.cpp:150-183,
run from `late_point` 313-318) and the shells' `checks()` (shells.cpp:189-194) change state at LateUpdateBehavior post. Both
call sites are in shared files, so RE3 gets them by the sync. The script polls the spot every 3 frames (hand_poser.lua:813).

## 3. RE2: slots, array, files

- Slot 84 = `S_WHO`, reserved for exactly this, unwritten. 85 = the character.
- Dead slots inside the array (declared, no reader or writer left in `src/` or `lua/`): 56-83 (the b156-b159 sliders,
  `visceral_bridge.lua:33` "unused since b164"), 86-535 (`S_HT_ON`, `S_HT`: the tuning menu, not installed, not in
  `tools/install-list.txt`), 536 `S_FREEZE`, 540-603 `S_HT2`. So **86 and 87 are free without growing anything**.
  Cleaner, and safe against an old tuning script on some install: append **649 = S_LEFT_STAGE, 650 = S_MAG_PULL**
  (651 left area, 652 right spot for step 2) and grow `N` from 649 (`lua/visceral_bridge.lua:22`) to 653.
- `_G.visceral_bridge` has no `n` field (visceral_bridge.lua:248-252); Village's has (`visceral_re8_bridge.lua:473`) and
  hand_poser.lua:464 needs it for the pull slot. Add `n = N`.
- Gun id: RE2's equivalent of Village's item name is the WP number, `gf().wp` = `weapons::current_id()` =
  `wp_of_enum(Equipment.get_EquipType)` (gun_frame.cpp:28, game/weapons.cpp:58-90). The script already formats it `wp%04d`
  (hand_poser.lua:74), so files are `wp0000.json`, `wp1000 @ slide at the back.json`. RE3 reuses the same numbers for other
  guns (RE3 guns_table.cpp:4-5), harmless: each game has its own data folder.
- Files to change. Plugin (all shared, RE2's repo owns them): `src/core/bridge.h` (the new slot names), new
  `src/guns/stage.cpp/.h`, `src/guns/guns.cpp` (call in late_point), `src/guns/left_hand.cpp` (call; keep S_LEFT_ON_GUN,
  `hands/sidestep.cpp:66` reads it), accessors in `rack.h/.cpp`, `magazine.h/.cpp`, `shells.h/.cpp`, `cylinder.h/.cpp`,
  `CMakeLists.txt` if it lists sources. Lua: `lua/visceral_bridge.lua` (N, `n = N`, slot 84 started at -1). Install lists:
  `tools/install-list.txt` in EACH repo (not shared) needs a line for `hand_poser.lua` and its data folder.
  For the trigger finger and the grip: a new shared `src/hands/righthand.cpp`, called from `guns::begin_render()`
  (guns.cpp:55, reached from each game's `plugin.cpp:97` BeginRendering pre) so neither plugin.cpp needs touching.

## 4. hand_poser.lua: what must change

1. **Line 46-47**: delete the RE3 line (`VB_GUN, VB_LEFT = 57, 58`): the rebuilt RE3 uses the same bridge as RE2 (84 / 539).
   Slot 57 is now a dead slider slot that reads 0.
2. **One bridge helper** instead of nine `rawget(_G, "visceral_re8_bridge")` blocks (284, 302, 324, 378, 407, 413, 463, 622,
   735): pick `visceral_re8_bridge` or `visceral_bridge`, plus a per-game slot table. LT = 3 and RT = 5 are the same number
   in both bridges (RE2 bridge.h:9), so `lt_held` / `rt_held` only need the helper. Per game: spot 153 -> RE2's stage slot,
   pull 279 -> RE2's, 213 / 184 / 214-216 / 186 -> none on RE2 yet (must read as 0, not as slot 153 of the RE2 array, which
   belongs to the old tuning block).
3. **current_spot() (323-354)**: add `20 = "shell port"`; on RE2 skip the pair names file.
4. **hands_on_gun() (616-632)**: the RE2 branch returns at 620 on slot 539 alone, skipping `posed_left`, `tuning` and
   `spot_unposed`. With stages, a magazine or shells in the hand have NO socket (left_hand.cpp:43-52 sets `active` 0), so 539
   is 0 unless the hand is within 0.45 m: the fingers would be dropped exactly then. New rule for RE2: left = stage > 0 or
   slot 539, then the same `posed_left` / `tuning` / `spot_unposed` lines as Village.
5. **mag_pull() (462-468)**: through the helper; needs the bridge's `n`.
6. **current_gun() (48-78)**: works as is once slot 84 is written and starts at -1.
7. Game detection: none needed beyond the helper; `find_player()` already takes `PlayerManager` under `sdk.game_namespace`
   (113-118). Wrists: already listed (224). `guard_on()` is Village-only and harmless (nil global).
8. Village-only names to leave or generalise: the sway pattern (722), `PULLED_AUTO_DEG` (644, off).
9. The app: `HandSpots.cs` needs no change for an unknown spot (falls to "docked on the <spot>", the last line of `LeftPlace`); the dock /
   magazine boxes hide themselves without Village's data folder (`DockBox.cs:171`).

RE2 / RE3 finger names (from gen-1 `archive/native-gen1/src/handpose_data.h:11-27`): `l_hand_index_0..2`,
`l_hand_middle_0..2`, `l_hand_ring_0..3`, `l_hand_little_0..3`, `l_hand_thumb_0..2` (17 per hand; ring and little have a
fourth joint); wrist `l_arm_wrist`. The right hand's `r_hand_*` names are `[inferred-static]` (mirrored), not read from a
skeleton here.

## 5. RE3

- `src/core`, `src/guns`, `src/hands`, `src/tools`, `game.h`, `lua/visceral_bridge.lua` are byte-identical to RE2 today
  (`diff -rq`), and `tools/sync_shared.py:18-23` carries all of them. So the stage code, the slots, the bridge Lua and a
  shared righthand file arrive by the sync.
- Per game, by hand: `tools/install-list.txt` (the hand_poser line), `src/game/plugin.cpp` only if a call is added there,
  `src/game/guns_table.cpp` (what `is_pump`, `long_mag`, `round_gun` answer).
- RE3-only states: the MGL's drum (`ROUND_GUNS` 4100, crane `_01`; RE2's `round_gun()` returns nullptr, guns_table.cpp:69),
  so spot 6 and "rounds in hand at an open drum" exist only there. RE3 has one pump (1000, the M3), the CQBR 2000 / 2100 as
  long-magazine guns, no bolt gun.
- The pose files are per game (WP numbers name other guns in RE3).

## 6. Risks

- **No other finger writer in the clean RE2 / RE3 plugin**: `grep` for finger joints and `set_LocalRotation` in `src/` finds
  only gun joints (magazine, slide, shell, crane). `handpose` and `tune` were dropped (plugin.cpp:5). The Lua in `lua/`
  writes no joints. So the game's own animation is the only other writer `[inferred-static]`.
- **Not determined**: whether the patched REFramework (FirstPerson's arm IK, the dock, `visceral_hands_frozen`) or the
  backup-only scripts (`re2_vr_melee.lua`, `re2_vr_grenade.lua`, the `vr` folder; `install-list.txt` layer 1, not on GitHub)
  touch finger joints, and whether a write at PrepareRendering survives to the picture in RE2. Gen-1's handpose wrote
  `set_LocalEulerAngle` at LateUpdateBehavior and PrepareRendering and logged a read-back (`handpose.cpp:173-178`,
  `handpose.h:4`), which is the same timing hand_poser.lua uses (835-836): `[reported]`, not re-verified.
- **The bend axis and sign of RE2's right index finger are unknown.** Village bends about +Z. Gen-1 treats `.z` of the left
  finger's Euler as the curl (`handpose.cpp:178`); the right hand's sign must be measured before porting `TRIGGER_PULL_DEG`.
- **Two Frame Sequential** doubles BeginRendering (Village measured it): keep the S_FRAME guard. RE2's pictures run on it.
- **The magazine pull is timed in RE2** (0.19 s), not by distance: the blend will be a quick ease, not Village's
  finger-follows-the-pull. The script's own 0.15 s blend (713) may hide it entirely.
- **Firm grip** needs a "last shot" time: RE2 has `rack::on_shot()` (rack.cpp:259) and the shot hooks in dryfire / pump /
  fire, no clock like Village's `recoil::last_shot_s()`.
- **The knife and grenades have WP numbers** (`gf().wp >= 0` with `gf().gun == nullptr`): slot 84 would name them as guns.
  Decide: send them (a grip pose per knife) or -1.
- `bridge::put` is a no-op without the headset (bridge.cpp:74): flat-screen tests of the stage slots will read 0.
