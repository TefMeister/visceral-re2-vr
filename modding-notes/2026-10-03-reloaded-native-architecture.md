# RELOADED native port: the architecture (2026-10-03, home PC, static, Fable)

The design session the port map asked for (`2026-09-27-reloaded-port-map.md` → "then architecture, FABLE").
Everything here is `[inferred-static 2026-10-03]` unless tagged otherwise. Read the port map first; this note
decides, it does not re-describe.

## 0. The one-paragraph version

Twelve Lua files become **small C++ files, one per feature**, under `dev-archive/plugin/src/port/`, each fed by
**one shared hand-reading function** and **one per-frame schedule that mirrors Andyalpa's exactly**. The Lua
bridge grows (v2): it writes the controllers **twice a frame**, carries the standing origin and rotation offset
the hand maths needs, carries rumble **out**, and **measures its own timing** so the "stepped hands" risk is
settled by one log line instead of by feel. The first build after the bridge is the sound player + dry-fire,
because it proves the audio pipeline and the `requestFire` hook with nothing else at stake.

## 1. The frame contract (this is the whole jitter question)

### 1.1 What the Lua does, by frame point

Read from `re2_vr_reload.lua` 3049-3125 and `ext_2` 1050-1140 `[inferred-static]`:

| Frame point (RE Engine `via.Application` entry) | What RELOADED does there |
| --- | --- |
| `UpdateHID` pre | (reload.lua 2438) input-cache bookkeeping |
| `UpdateBehavior` pre | sync the reload-input inhibit (blocking layer) |
| `UpdateBehavior` post | refresh the VR input cache, dry-fire sound tick, strip the weapon animation input bits |
| `UpdateScene` pre | magazine update |
| `UpdateMotion` pre | pump: pre-arm-IK + update |
| `LateUpdateBehavior` pre | pump: pre-arm-IK again |
| **`LateUpdateBehavior` post** | **the hand is read here** (`get_controller_game_world_pos`), rack pull, slide rack, hand follow; mag/shell/revolver late updates |
| `PrepareRendering` post | slide: re-sync the rack motion, hand follow again |
| `BeginRendering` post | slide: re-sync + hand follow a third time |
| `UpdateJointExpression` post | slide park, revolver joint expression |

So the hand-driven features **read once (LateUpdateBehavior post) and write the joint three more times** later in
the frame. The repeats exist because the engine's motion and joint-expression passes overwrite joint transforms
after LateUpdate; a single write at the wrong point is overwritten on some frames and not others, and **that
alternation is exactly what Tefa calls "jittery"** (stepped under smooth motion). The port keeps all four points.

### 1.2 Where the hand numbers come from

`get_controller_game_world_pos` (ext_2 1109-1134): `world = cam.pos + cam.rot * (rot_off * (raw - standing_origin))`,
with `raw = vrmod:get_position(left)`, `rot_off = vrmod:get_rotation_offset()`, `standing_origin =
vrmod:get_standing_origin()` (falling back to the HMD position once). Visceral's dock already has a version of this
(`world_from_bridge`, marked "unverified" in `visceral.h`). **Decision: one function, `port_hand_world(side)`, in
`port/rld_input.cpp`, used by every ported feature, and the dock migrates to it in its own step.** Two formulas
for the same hand is the "two writers, one frame" trap from the AC map in a different coat.

### 1.3 The bridge timing, settled by measurement, not by argument

The bridge (v1) writes poses at `UpdateHID` pre. REFramework refreshes its pose snapshot once per frame, at the
rendering end of the frame `[hypothesis; VR.cpp not on disk to check]`. If that is right, a read at `UpdateHID`
pre and a read at `LateUpdateBehavior` post see the **same** snapshot, and there is no lag at all. If it is wrong
(refresh between those two points), the v1 bridge hands the port a hand that is one frame stale against what
Andyalpa's Lua saw, and slide/pump would step.

**Bridge v2 writes the poses at both points and keeps the `UpdateHID` copy of the left hand in spare slots.** The
plugin compares the two copies at `LateUpdateBehavior` post and logs once a second how many frames differed. The
answer is one line in the log after one run with the controllers awake:

- `differ=0` → no mid-frame refresh; the late write is the same data; nothing to fix, keep both writes anyway (cheap).
- `differ>0` → mid-frame refresh exists; the late write is the one the hand features must use (they do by design).

Either way the port reads **buttons at `UpdateHID` time** (so input stripping lands before `UpdateBehavior`
consumes it) and **poses at `LateUpdateBehavior` time** (the freshest the game itself has). The plugin also logs
the real order of the application entries once (frame 300), so the schedule above is `[measured]` after the
first run instead of inferred.

### 1.4 Rumble goes the other way through the same array

`vrmod:trigger_haptic_vibration` is Lua-only. The plugin writes `amplitude, seconds` per hand into four slots at
`LateUpdateBehavior` post; the shim fires it at the next `UpdateHID` pre and clears the slots. Worst case one frame
late, which no hand can feel. Frequency is a named setting, not per call.

## 2. Bridge v2 slot map (slots 0-31 unchanged, v1 plugins keep working)

| Slot | Name | Written by | Meaning |
| --- | --- | --- | --- |
| 32 | `S_WRITE_PT` | Lua | 1 = last write was `UpdateHID` pre, 2 = `LateUpdateBehavior` post |
| 33 | `S_WRITE_SEQ` | Lua | +1 per write (the plugin can see a missed write) |
| 34-36 | `S_STAND` | Lua | standing origin (`get_standing_origin`), 0 if unavailable + slot 57 = 0 |
| 37-40 | `S_ROTOFF` | Lua | rotation offset quaternion (`get_rotation_offset`), identity if unavailable |
| 41-42 | `S_RSTICK` | Lua | right stick x, y |
| 43-46 | `S_LA S_LB S_RA S_RB` | Lua | A/X and B/Y buttons per hand (`get_action_a_button/b_button`) |
| 47-48 | `S_LCLICK S_RCLICK` | Lua | stick clicks |
| 49-50 | `S_RUMBLE_L_AMP/_SEC` | **plugin** | left rumble request; Lua fires and zeroes |
| 51-52 | `S_RUMBLE_R_AMP/_SEC` | **plugin** | right rumble request |
| 53-55 | `S_LPOS_HID` | Lua | the left position as written at `UpdateHID` pre (the timing probe) |
| 56 | `S_LATE_SEEN` | plugin | 1.0 once the plugin has run its late tick (a v2 handshake, for the UI) |
| 57 | `S_STAND_OK` | Lua | 1.0 when `S_STAND` is real |
| 61 | `S_BRIDGE_VER` | Lua | 2.0; the plugin logs a mismatch once |

## 3. Files (new, under `dev-archive/plugin/src/port/`; each ≤ 800 lines, numbers in one place)

| File | Replaces (RELOADED) | Job |
| --- | --- | --- |
| `port_settings.h` | the `config` tables | **every number, named**: blend times, distances, pouch anchor, rumble frequency, per-weapon tables' defaults |
| `rld_data.cpp` | `re2_vr_reload.json` loading | reads his JSON **as data** (poses + per-weapon numbers) from `reframework/data/visceral/reloaded/` with a tiny JSON reader (`third_party/json.hpp`, nlohmann, MIT) |
| `rld_input.cpp` | ext_2 1050-1140 + the button caches | `port_hand_world(side)`, button edges (pressed this frame / released), the one place that reads the bridge |
| `rld_sfx.cpp` | ext_5 sound manager + REAudio.dll | miniaudio engine (public domain, same library REAudio uses), `sfx_play(kind, weapon)`, folder discovery by weapon id, volume by kind |
| `rld_hands.cpp` | ext_5 `lhp_runtime` + pose apply | finger poses: blend the 17 joint rotations per hand over `blend_sec`, one writer per joint |
| `rld_block.cpp` | reload.lua B.5 | the four layers that keep the game's own reload off: input bits, ActionOrderer inhibit, `get_IsReload` spoof, `requestFire` block |
| `rld_mag.cpp` | ext_1 (B.2-B.4) | magazine state machine + the exact ammo calls |
| `rld_slide.cpp` | ext_2 (C.7, C.8) | slide dock + rack, 4-point schedule |
| `rld_pump.cpp` | ext_4 (C.2-C.6) | pump gesture, motion-layer scrubbing, Wwise suppression |
| `rld_revolver.cpp` | ext_3 (B.7) | cylinder + single rounds |
| `rld_drop.cpp` | ext_5 fall | the physics-less magazine fall |
| `rld_holster.cpp` | holster.lua | hip / shoulder / chest / head-light, per-character profiles |
| `rld_recoil.cpp` | recoil.lua | wrist spring in the `IkArmFit.updateIk` pre-hook, native + camera recoil off |
| `rld_knife.cpp`, `rld_haptics.cpp` | melee + haptics | last |
| `rld_ik_ext.cpp` | ik_extention.lua | arm stretch, clavicle shift, two-hand reach boost (fed into the dock), auto standing height |
| `rld_ui.cpp` | the imgui pages | the in-game settings page for every setting, drawn natively via `on_imgui_draw_ui` |

`Plugin.cpp` only registers the schedule and calls `port_*_tick()` functions; it never holds feature logic.

## 4. Overlaps with what Visceral already has: the decisions (the three ⭐ answered by Tefa, 2026-10-03 01:55: port ALL of them)

| RELOADED piece | Visceral has | Decision |
| --- | --- | --- |
| two-hand reach boost, arm stretch, clavicle shift (`ik_extention`) | its own left-hand dock + the REFramework grip-socket freeze (worn 2026-10-02) | ⭐ **PORT HIS** (Tefa: without the stretch the left hand shows as coming off the weapon while it is still technically gripping). Goes into the dock as the dock's reach rule, not as a second IK writer |
| slide-dock left-arm IK (`ik_extention`) | the dock writes `getIKLeftArmMatrix` | **fold into the dock as a second target ("slide")**: one writer per joint per frame |
| support hand follows the gun (`recoil`) | the dock does this | **ours** |
| auto standing height (`ik_extention`) | nothing | ⭐ **PORT HIS** (Tefa: loves the feature) |
| holsters | nothing | **port his 1.0.1** (not AC's rework) |
| recoil spring + native/camera recoil off | nothing | **port his**, after the reloads |
| knife + haptics | nothing | **port his**, last |
| sound player | nothing | **miniaudio in our plugin**; his sound pack shipped as data, credited |
| settings UI (imgui from Lua) | NUM hotkeys + log | ⭐ **PORT HIS, IN-GAME, EVERY SETTING** (Tefa). The plugin API draws ImGui natively (`on_imgui_draw_ui`, API.h 1.15), so the page is C++ in `rld_ui.cpp`, backed by `port_settings.h` defaults + a saved settings file |

## 5. Build order, each step a worn test, nothing moves on until the step before is confirmed

| # | Step | What proves it | Gate | Model |
| --- | --- | --- | --- | --- |
| 0 | merge the split branch | one flat run old vs new DLL, same `[visceral]` milestone lines | FLAT | OPUS |
| 1 | **bridge v2 + probes** (this session) | log: entry order line, `differ=` line, buttons seen, a test rumble on NUM key | VR CLAUDE (controllers awake; nobody wearing it) | OPUS |
| 2 | sound player + dry-fire on empty trigger | hear the click, log the hook | VR USER | OPUS |
| 3 | blocking layer, NUM toggle | the game no longer reloads on its own; toggle restores it | VR USER | OPUS |
| 4 | magazine reload, pistols | pouch grab → insert → rounds counted right | VR USER | OPUS, FABLE if the state machine misbehaves |
| 5 | slide rack | pull/release moves the slide, chambers | VR USER | OPUS |
| 6 | pump | | VR USER | OPUS |
| 7 | revolver + single rounds | | VR USER | OPUS |
| 8 | holsters | | VR USER | OPUS |
| 8b | arm stretch + reach boost + auto standing height (`rld_ik_ext`) | the left hand stays on the gun at full reach; the body height matches standing | VR USER | FABLE for folding the stretch into the dock without two writers, then OPUS |
| 8c | the settings page | every setting changeable in game and saved | FLAT | OPUS |
| 9 | recoil | | VR USER | FABLE for the spring + IK matrix maths, then OPUS |
| 10 | knife + haptics | | VR USER | OPUS |

## 6. Rules that carry into every file of the port

- **One writer per joint per frame.** Before a file writes a joint, say which other file could (dock, head
  hider, hands) and take it out of the loop, never blend two writers.
- **Read his Lua as the spec, write ours.** Names, numbers and schedule come from his files; the code does not.
- **Every number lives in `port_settings.h` or in his JSON.** No bare constants in feature files.
- **Every feature logs its own effect** (measured before, acted, measured after) so a run tells "no effect"
  from "wrong choice" without asking Tefa to judge it.
- **A step that fails comes out of the test copy the same session**, archived with a hash, never deleted.
