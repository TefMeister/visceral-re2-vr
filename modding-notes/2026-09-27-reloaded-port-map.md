# RE2VRMODRELOADED 1.0.1 → Visceral native port: the map (2026-09-27, home PC, static)

Tefa, 2026-09-27: port **RE2VRMODRELOADED** (Andyalpa) into Visceral, **built from scratch in C++**; the only things
used as they are will be his **hand poses / weapon animations** (slide racking, pump handle and so on) and **the
sounds** that go with them. Permission: Andyalpa, 2026-09-04 (`PREFERENCES.md` → "Visceral source policy"). Rule
unchanged: his Lua is read as the specification and rebuilt; **never copy-pasted**.

This map is a **companion** to `2026-09-05-arcade-controls-port-map.md` (1,251 lines), which already maps most of
this code: Arcade Controls 1.5.0 is, in its own words, "90% of Andyalpa's RE2VRMODRELOADED". Where a mechanism is
already mapped there, this note points at the section instead of repeating it.

Everything below is `[inferred-static 2026-09-27]` (read from the files, nothing run) unless tagged otherwise.

## Where it is

- Package: `D:/RE2 REFramework builds/study - Arcade Controls + Andyalpa (permission 2026-09-04)/RE2VRMODRELOADED1.0.1 2483 1.0.1 2026-07-14T12-37Z ksNn3Uik5.zip`
- Unpacked beside it: `.../RELOADED-1.0.1-extracted/` (local only: his files never go into our public repos).
- `modinfo.ini`: author Andyalpa, v1.0.1, "A bundle of QoL gameplay improvements to Resident Evil 2".

## What is in it

| File | Lines | What it does | Already mapped (AC map §) | Lines differing from AC 1.5.0 |
| --- | --- | --- | --- | --- |
| `re2_vr_reload.lua` | 3,232 | orchestrator: config load/migrate, which weapon reloads how, blocking the game's own reload (input strip, ActionOrderer inhibit, `get_IsReload` spoof), dry-fire sound, UI | B.1, B.5, B.6 | 114 |
| `re2_vr_reload_ext_1.lua` | 4,146 | magazine state machine: mag out/drop/grab from the pouch, carried rounds, chamber count, ammo bookkeeping, pouch anchor + calibration | B.2, B.3, B.4 | 87 |
| `re2_vr_reload_ext_2.lua` | 2,903 | slide dock + rack gesture: hand docks on the slide, pull/push along the slide axis, slide joint moved by hand travel, chamber clear | C.1 (+ slide sections) | 429 |
| `re2_vr_reload_ext_3.lua` | 3,522 | revolvers and single rounds: cylinder open/close gesture, bullet insert preview, chamber bullet follow | B.7 | 27 |
| `re2_vr_reload_ext_4.lua` | 2,669 | pump action: manual pump gesture, scrubbing the game's own pump motion by hand position, pump-shot flow, Wwise suppression | C.2–C.6 | 235 |
| `re2_vr_reload_ext_5.lua` | 1,403 | dropped-mag physics-less fall (freeze, release, fall distance/time) + the **custom sound manager** (discovers `custom_sfx/<folder>/*`, plays by kind) | byte-identical to AC | 0 |
| `re2_vr_holster.lua` | 2,799 | holsters: hip pistol, shoulder long gun, chest slot, head flashlight grab; zone haptics; per-character profiles | (AC changed it most) | 618 |
| `re2_vr_recoil.lua` | 2,604 | recoil: spring model on the wrist (one-hand light/heavy, two-hand scale, sustained fire), suppress native + camera recoil, support hand follows the gun | (AC changed it) | 545 |
| `re2_vr_ik_extention.lua` | 2,094 | arm IK extension: arm stretch toward the controller, clavicle shift, two-hand reach boost, slide-dock left-arm IK, auto standing height | (AC changed it) | 191 |
| `re2_vr_haptics.lua` | 1,286 | haptics: shot rumble per hand, support hand, melee contact rumble via physics ray/capsule probes | — | 6 |
| `re2_vr_melee.lua` | 1,032 | physical knife: blade capsule follows the controller, hijacks the game's melee colliders, swing speed gate, swing sounds | byte-identical to AC | 0 |
| `utility/RE2Character.lua` | 146 | which character is playing (profile key) | — | — |

Also: `plugins/REAudio.dll` (a sound player built on **miniaudio**, public domain), and it relies on praydog's
shipped VR helpers (`utility/RE2.lua`, `Statics.lua`, `GameObject.lua`, `vr/VRControllerManager.lua`).
The scripts share state through `_G.__vr_*` globals and `M.init(deps)` tables, not through files.

## The parts that are used as they are

### Hand poses (the "hand animations") — `re2_vr_reload.json`

They are **stored finger shapes**, not animation clips: per pose, per weapon, per character, a local rotation
(3 numbers, Euler, radians by the look of the values) for each finger joint (17 joints per hand, e.g.
`r_hand_index_0..2`, `r_hand_little_0..3`, thumbs), blended in over `blend_sec`.

| Pose | Hand | Weapons covered |
| --- | --- | --- |
| `mag_hold` | left | wp0000 0100 0200 0300 0400 0600 0700 0800 1000 2000 2200 3000 4100 4200 4300 4500 6300 7000 |
| `slide_rack` | left | wp0000 0100 0200 0400 0600 0700 1000 2000 2200 3000 4200 4300 4500 6300 7000 |
| `shell_hold`, `bullet_hold` | left | switches only (no shapes in 1.0.1) |
| `shoot_ready_hold` | right | e.g. wp1000 per character |

Characters: `leon`, `claire`, `hunk` (and profiles for ada/carlos/jill/tofu in the holster file).

### Weapon "animations" — code plus numbers, not clips

- **Slide racking:** the slide joint (`slide_node_by_wp`, e.g. `_01`/`_02`) is moved along the weapon's axis by how
  far the left hand pulls; numbers per weapon in `slide_dock` (`slide_bind_by_wp`, `slide_dock_by_wp`,
  `slide_ik_twist_by_wp`, pull/push distances, return time). Rebuilt as code; the per-weapon numbers are data we read.
- **Pump:** `manual_pump` (pump joint per weapon, rest/back positions) moves the fore-end by hand travel, and
  `native_pump` / `weapon_pump_scrub_layers` **scrub the game's own pump motion** (set frame by hand progress)
  instead of letting it play.
- **Magazine drop/insert:** `anim` block (drop/fall/insert times and distances) + `mag_exit_by_wp`, `mag_dock_by_wp`,
  `mag_hand_hold` (where the mag sits in the hand).
- **Revolver:** `cylinder_joint_by_wp`, `cylinder_open_angle_by_wp`, `revolver_reload` gesture numbers.

### Sounds — `reframework/data/custom_sfx/`

79 files in 11 folders: `handgun` (16, incl. revolver + SW649 cylinder), `shotgun_w870` (9), `spark_shot` (8),
`flamethrower`, `magnum`, `smg_mp5`, `smg_mq11` (7 each), `grenade_launcher` (6), `minigun` (5),
`rocket_launcher` (4), `knife` (3 swings). Kinds: `dry_fire`, `mag_drop`, `mag_floor`, `mag_grab`, `mag_insert`,
`slide_rack_pull`, `slide_rack_release`, `pump_fire`, `shotgun_fire`, `revolver_*`, `swing_1..3`.
Which weapon uses which folder and volume: `weapon_sfx.by_wp` (+ `volume_by_kind`, `master_volume`, `spatial`).

✅ Shipping his sounds and pose data in a Visceral release is **confirmed** (Tefa, 2026-09-27, after asking);
credit him in `CREDITS.md` and the release notes.

## Every place it touches the game

| Game method (hooked) | Used by | For |
| --- | --- | --- |
| `app.ropeway.implement.Gun.requestFire` | reload, recoil, haptics | block fire while reloading/empty, recoil kick, shot rumble |
| `...doSurvivorActionOrdererUpdate` | reload | strip reload/change-bullet input bits |
| `...get_IsReload` | reload, pump | spoof "is reloading" so the game keeps its hands off |
| `...updateIk` (arm-fit IK) | recoil, ik_extention | patch the IK target matrix: recoil offset, arm stretch, slide-dock left arm |
| `...updateOnFrameHead` | holster | head-flashlight / timing |
| `...getMainWeaponRemainingBullet`, `getBulletNumber` | ext_1 | chamber display, ammo bookkeeping |
| `implement.Melee.lateUpdate / checkActiveMeleeAttack / onCheckHitAttack / onCheckHitTerrain`, `Implement.set_Status`, `EnemyController.update` | melee, haptics | physical knife hits, contact rumble |
| camera recoil methods (via a name list) | recoil | suppress the camera kick |

Direct calls that change the game: `executeReload`, `reloadMainSlot`, `reloadMainWeapon`, `executeEndReload`,
`executeEndEject`, `endChamberClear`, `changeBulletMainSlotWithoutReload`, `set_MainSlotSurplusBulletNumber/ID`,
`reduceItem`, `reduceSlot`, `addSlotNumber`, `useMainWeapon`, `setPartsEnable` (hide mag/bullet mesh parts),
`set_DrawSelf`, ActionOrderer `setInhibit`/`setInhibitPrecede`/`setForce`, Motion layer `set_Frame/Speed/Weight`,
holster `requestHolster`/`changeWeapon`/`equipMainSlot`/`unequipEquipedWeapon`, flashlight `set_ManuallyLight`.
Singletons: `app.CharacterManager`, `gamemastering.InventoryManager`, `CutSceneManager`, `camera.CameraSystem`,
`gamemastering.TimelineEventManager`, `gui.GUIMaster`, `IlluminationManager`, `via.physics.System`.

## Can C++ do all of it? Yes, with one known detour

| Needed | C++ route |
| --- | --- |
| hooks, managed calls, fields, joints, motion layers, mesh parts | REFramework plugin API (`add_hook`, invoke, fields): what `Plugin.cpp` already does |
| controller poses, buttons, sticks, standing origin | **not in the plugin API** (1.15 has no VR calls, dossier §"VR API unreachable"). Route: extend the existing Lua shim `visceral_native_bridge.lua` + shared `System.Single[64]` |
| rumble | same bridge, other direction: C++ writes a request slot, the shim calls `vrmod:trigger_haptic_vibration` |
| sounds | **miniaudio** inside our plugin (the same public-domain library REAudio uses) |
| physics probes (melee) | `via.physics` managed API (`CastRayQuery`, capsule shapes), all invokable |
| settings UI | REFramework UI from the plugin, or a settings file + a tiny Lua page (decide in the design) |

⚠️ **The one real risk:** the bridge hands controller data over once per frame, from Lua. If the C++ reads it at a
different point in the frame than Andyalpa's Lua read `vrmod` directly, hand-driven things (slide pull, pump) can
lag a frame and look **stepped** (Tefa's "jittery"). The design has to place the bridge write before the reads,
and prove it. → **FABLE.**

## Overlap with what Visceral already has

Visceral already has its own two-hand grip, left-hand IK and first-person handling, and a policy of right-hand-only
aiming for pistols. RELOADED's `ik_extention` (arm stretch, two-hand boost), `recoil` (support hand follows the gun)
and parts of `holster` overlap it. Each overlap needs a decision (take his, keep ours, merge). → **FABLE**, with Tefa.

## Suggested order (for the design session to confirm)

1. Sound player (miniaudio) + his sound pack, fired from existing events (dry fire). Small, proves the pipeline.
2. Bridge extension (both controllers, buttons, rumble out), with its timing proved.
3. Hand poses (read his pose data, blend onto finger joints).
4. Magazine reload for pistols (ext_1 + blocking from reload.lua).
5. Slide rack (ext_2) · 6. Pump (ext_4) · 7. Revolver and single rounds (ext_3) · 8. Mag drop fall (ext_5)
9. Holsters · 10. Recoil · 11. Physical knife + haptics.

## Before any C++ goes in

`dev-archive/plugin/src/Plugin.cpp` is **2,639 lines**: over the 1,500 hard limit (code-shape rule). It has to be
split move-only first, and the port goes in as new, small files (one per feature), with every number in one
settings table.
