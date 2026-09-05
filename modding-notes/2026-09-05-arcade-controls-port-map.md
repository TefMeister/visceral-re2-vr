# Arcade Controls → the native plan: the port map (2026-09-05 afternoon, home PC, `/pd`)

**The game was not launched. Nothing here has been run.** Everything below is a read of source
files already on this machine.

This is the board's second `[PD]` row, delivered: *"port-map Arcade Controls 1.5.0 into the native
plan … read the AC two-hand / manual-reload / slide-racking Lua and write
`modding-notes/<date>-arcade-controls-port-map.md` — every managed method, field, joint name, timing
and trap it relied on, mapped to what the plugin already proves it can call. That map is what the
C++ reload and dock code is written from."*

## What was read

- **Primary:** the frozen `arcade-controls-re2-vr` repo — 39 autorun Lua files, ~1.1 MB, plus its
  `ENGINE-DOSSIER.md`, all 26 `modding-notes/case-studies/`, the dev-archive `INDEX.md` and
  `CREDITS.md`. **This is our own code**, so it may be rebuilt line by line without restriction.
- **The shipped 1.5.0 release zip**, extracted to a scratch directory only — because the JSON it
  ships is what actually runs (§0, caveat 1).
- **Andyalpa's RE2VRMODRELOADED 1.0.1**, third-party, studied with the permission granted
  2026-09-04. It is handled **describe-only throughout: no code is quoted from it**, and nothing
  from it is reproduced here beyond naming which engine surfaces it touches. AC's own `CREDITS.md`
  and ReadMe are explicit that it is the base layer AC builds on.
- **VR Light is excluded**, per the board. It turns out not to be part of AC at all — it is a
  separate Nexus mod, and the only trace of it in the codebase is one line noting it belongs to the
  user, not to us.

## ✅ SOURCE VERSION — asked, checked, and settled (2026-09-05 evening)

Tefa raised a worry after this was written: *"i downloaded the 1.5.0 mod version, but the latest one
is the unpublished one on github, that has many more features in it."* **Checked, and this map is
already built on that newer source.** No re-extraction is needed.

The post-1.5.0 work lives in `arcade-controls-re2-vr/dev-archive/`, committed **after** the v1.5.0
release of 2026-08-16 and never packaged as a Nexus zip — which is exactly why it reads as
"unpublished":

- **2026-08-19** — HMD-relative holsters + ammo pouch, Samurai Edge rack, LT return gate, wrist
  identity cache, ladder body-yaw fix
- **2026-08-19** — inventory/pickup black-void root cause: REFramework VR culls `GuiBack` by name;
  rename bypass built
- **2026-08-20** — head shadow + invisible head, confirmed working in VR

**That is the source this map read.** The extraction recorded it as the "frozen, post-1.5.0 dev
state" at the time, and the features are verifiably in the files it used: `GuiBack`, `head_shadow`,
`ladder_body_yaw`, `samurai_edge_rack_migrate_rev`, `hmd_fx` and `mag_holster` (7 files) all appear
in `dev-archive/reframework/autorun/` `[verified 2026-09-05]`.

**The only 1.5.0-based part is the shipped-JSON comparison in §0, and that is deliberate** — it
documents what the *released* build actually runs, which is the whole point of caveat 1. It should
stay pinned to a release, not follow the dev branch.

Also checked, since the consolidation was the obvious suspect: the 2026-08-30 pass deleted 79 repos,
but every one was a `<prefix>-<suffix>` sub-repo whose content was **imported as a folder first and
verified before deletion** (16/16 folder sets). AC's four went that way. **Nothing was lost.**

## How much of this is known, and how

Everything below is **`[measured 2026-09-05]`** unless marked otherwise — read directly out of the
Lua and the shipped JSON, not inferred. Where the extraction inferred rather than read, the word
`[inferred]` appears next to the claim. Player-facing behaviour taken from the shipped ReadMe is
`[reported]`. **Nothing here is `verified-live`** — none of it has been run against the game in this
session, and a value being correct in AC's Lua is not proof it is correct for the plugin.

**Four cited claims were spot-checked against the source by hand before publishing, and all four
matched to the line:** the `should_spoof_reload_state` forward-reference (function at
`re2_vr_reload.lua:1512`, the three locals it reads declared at 1532/1536/1538 — so four branches of
the reload-state spoof are dead code); the `find("^" .. prefix, 1, true)` plain-match defect at
`:2792`; the clavicle stretch's `get_BaseLocalPosition` idempotence trick at
`re2_vr_ik_extention.lua:962`; and `twohand_l_extra_max_m = 0.08` at `:34` with its clamp at `:932`.
The line numbers in the body can be trusted to the same standard.

## ⚠️ The finding that lands on today's other work

**AC does not clamp an out-of-reach target — it stretches the arm.** `re2_vr_ik_extention.lua`
carries a clavicle stretch (`l_arm_clavicle`, written through `get_BaseLocalPosition` so the write
is idempotent within a frame) with a live budget of `max_m = 0.16` m, plus a **two-hand boost of up
to `twohand_l_extra_max_m = 0.08` m for large weapons, left arm only**, the whole thing clamped to
`0.60` m of total stretch.

Set that beside this session's other row: plugin v0.6 measures Claire's left arm at **0.4994 m** and
clamps a dock target to **0.4894 m** about the humerus. **AC's shipping answer is more permissive
than our new clamp, by design and after live tuning** — it deliberately buys reach by moving the
shoulder, which is exactly the 12.6 cm clavicle segment our measurement excludes as a lower bound.

Two consequences, and neither is theoretical:

1. **The v0.6 reach clamp is the conservative fallback, not the destination.** A hard clamp will
   refuse poses AC has already shipped and tuned. That is the strongest argument yet for keeping it
   behind **NUM2** until a headset run reports `clamp=`.
2. **The eventual native answer is probably a clavicle stretch, not a clamp** — and AC has already
   paid for the hard part: the idempotence trap (re-reading the base after writing it compounds the
   stretch), the per-frame base reset, and the bone-length sanity clamp `[0.18, 0.45]` m are all
   recorded in §A.

This is what a port map is *for*: it answered a question in another row that neither row knew it was
asking.

## Three other things to know before reading the body

- **§0 caveat 1 — the Lua defaults are not what runs.** The JSON in `reframework/data/re2_vr/`
  overrides `DEFAULT_CFG` wholesale at load, and eleven confirmed divergences are tabulated. Port
  against the JSON.
- **§0 caveat 2 — in shipped 1.5.0, both custom two-hand-grip mechanisms are OFF.** What actually
  ships is the native RG-driven support pose plus recoil's IK patch. A port has to choose
  deliberately which of the three systems it is reimplementing; §A.6 gives the attach/detach
  conditions for each.
- **§D is the most valuable section in the document.** It is the verbatim comment record of what was
  tried and failed — the stale world matrix, the two-writers-one-frame flicker, the phantom pull,
  the near-zero-length direction vector, the fix that broke the animation and had to be reverted.
  Every one of those is a launch we do not have to spend again.

## Credit

**Andyalpa**, for RE2VRMODRELOADED, the base layer this all builds on, studied with permission.
**praydog**, for REFramework and the FirstPerson plugin the whole system sits on. **Junh2x**, whose
diagnostic scripts are cited in the dossier's §8d rather than here. If someone should be credited
and is not, email us and it is fixed the same day.

---

## 0. Sources, scope, and two caveats you must read first

**Primary (frozen, post-1.5.0 dev state):** `C:\Users\TD3KX\github-backups-pd\arcade-controls-re2-vr\dev-archive\reframework\autorun\` — 39 Lua files, ~1.1 MB. Relevant: `re2_vr_reload.lua` (136 KB, orchestrator), `re2_vr_reload_ext_1..5.lua` (151/151/130/105/43 KB), `re2_vr_ik_extention.lua` (87 KB), `re2_vr_recoil.lua` (122 KB), `re2_vr_cosmetic_dock.lua` (37 KB), `re2_vr_suppress_supporthold.lua`, `re2_vr_delayed_shell_eject.lua`, `utility/RE2.lua`, `utility/RE2Character.lua`, `utility/GameObject.lua`, `utility/Statics.lua`, `vr/VRControllerManager.lua`.
**Also read in full:** `engine-research/ENGINE-DOSSIER.md`, all 26 `modding-notes/case-studies/*.md`, `dev-archive/_RE2VR_dev_archive/INDEX.md` (1271 lines), `CREDITS.md`.
**Release zip** extracted to scratchpad only (`.../scratchpad/ac150/`), plus Andyalpa's for comparison. Nothing was written into any repo.

### ⚠️ Caveat 1 — the Lua defaults are NOT what runs
`re2_vr_reload.json`, `re2_vr_ik_extention.json`, `re2_vr_cosmetic_dock.json`, `re2_vr_recoil.json` in `reframework/data/re2_vr/` **override `DEFAULT_CFG` wholesale at load**, and the repo's own case study (`2026-08-07-three-position-systems-one-weapon.md`) records this trap biting twice on one weapon. Confirmed divergences:

| key | Lua `DEFAULT_CFG` | shipped + repo JSON (the live value) |
|---|---|---|
| `slide_dock.slide_node_by_wp.wp0100` | `"_02"` | **`"_01"`** |
| `slide_dock.slide_node_by_wp.wp2200` | absent | **`"_02"`** |
| `slide_dock.dock_dist_default` | `0.20` | **`0.15`** |
| `slide_dock.dock_blend_speed` | `0.10` | **`0.04`** |
| `slide_dock.pull_smooth_alpha` / `pump_reach_mult` / `slide_return_sec` / `arm_ik_blend_start` | absent | **`0.92` / `2.75` / `0.31` / `0.0`** (UI-created keys) |
| `manual_pump.pump_node_by_wp.wp0300` | absent | **`"_02"`** |
| `cosmetic_dock.grip_back_from_muzzle_m` | `0.14` | **`0.038`** |
| `cosmetic_dock.enabled` | `true` | **`false`** |
| `weapons.wp0800.chamber_grab_dist` | `0.5` | absent → falls back to `0.28` |
| `weapons` enabled set | 5 true | **16 true** (see §E) |
| `mag_node_by_wp` | 7 entries | **17 entries** |

**Port against the JSON, treat `DEFAULT_CFG` as seed data.**

### ⚠️ Caveat 2 — in shipped 1.5.0, BOTH two-hand-grip mechanisms are OFF by default
- `re2_vr_cosmetic_dock.json` → `"enabled": false`
- `re2_vr_recoil.json` → `support_hand_latch_enabled` **absent** → Lua default `false`

So the only two-handing that actually ships is the **native RG-driven support-hand pose**, plus recoil's `should_support_hand_follow_weapon()` IK patch. Both custom mechanisms exist, are complete, and are gated off because of one unresolved bug (§A.5). A port has to decide deliberately which of the three it is reimplementing.

### Ancestry (third-party handling)
`CREDITS.md` and the shipped ReadMe are explicit: **RE2VRMODRELOADED (Andyalpa) is the base layer AC builds on, used with permission.** The zip confirms it — `re2_vr_reload_ext_5.lua` and `re2_vr_melee.lua` are byte-identical between the two; ext_1/2/3/4, `reload.lua`, `ik_extention.lua`, `holster.lua`, `haptics.lua` are AC's later extensions of Andyalpa's files. The ReadMe says it plainly: *"IT USES 90% OF ANDYALPA'S 'RE2VRMODRELOADED' MOD AND ONLY REPLACES THE SOMEWHAT WIP'ISH FEELING BITS."* Mechanistically Andyalpa's baseline provides: the mag/pump/slide/revolver reload skeleton, arm-IK extension, holsters, melee, recoil, and — uniquely — the `reframework/data/custom_sfx/<weapon-folder>/*.ogg|wav` sound pack (11 folders: handgun, magnum, shotgun_w870, smg_mp5, smg_mq11, spark_shot, flamethrower, grenade_launcher, minigun, rocket_launcher, knife) that AC's `ext_5` audio manager discovers by globbing. It touches `app.ropeway.implement.Gun`, `updateIk`, `requestFire`, `getBulletNumber`, `getMainWeaponRemainingBullet`, `lateUpdate`, `survivor.Equipment`, `via.motion.Motion`, `via.render.Mesh`, `app.CharacterManager`. **AC 1.5.0 ships REAudio.dll but no `custom_sfx` assets — it depends on Andyalpa's install for them.** No code quoted; describe-only per brief.

**VR Light:** not part of AC. It is a separate Nexus mod (`nexusmods.com/residentevil22019/mods/2493`), and the only trace in this codebase is `dev-archive/_RE2VR_dev_archive/INDEX.md` noting `VRLight.lua` is *"separate user-owned Fluffy mod ... legitimate/harmless, not leftover cruft."* Excluded, nothing to skip.

---

## The two universal conventions

`NS(x)` = `sdk.game_namespace(x)` = **`app.ropeway.` + x**. (Some sites hardcode `"app.CharacterManager"` and `"via.VRControllerManager"` without the ropeway segment — reproduce exactly.)

`sc(obj, "method", ...)` (`re2_vr_reload.lua:482`) = `pcall(obj:call(method, ...))`, **returns nil on any failure**. Every managed call in the reload stack swallows errors. A C++ port must decide per call site whether nil is tolerable — the dossier is explicit that this is deliberate (*"Wrap every reflection call in pcall. Native calls into an engine never designed for this introspection fail unpredictably"*).

**Input binding contract** (`_interaction_profiles_oculus_touch_controller.json`, shipped at game root):

| action | left path | right path |
|---|---|---|
| `grip` | `/user/hand/left/input/squeeze` | `/user/hand/right/input/squeeze` |
| `trigger` | `/user/hand/left/input/trigger/value` | `/user/hand/right/input/trigger/value` |
| `bbutton` | `/user/hand/left/input/y/click` | **`/user/hand/right/input/b/click`** |
| `abutton` | `.../x/click` | `.../a/click` |
| `pose` | `.../grip/pose` | `.../grip/pose` |
| `haptic` | `.../output/haptic` | `.../output/haptic` |

Everything is read as a **digital** action through `vrmod:is_action_active(handle, joystick)`. There is no analog path anywhere in A/B/C — which is why the `2026-08-08-edge-trigger-no-retry-and-a-controller-blip.md` case study bottoms out at *"a pre-digitized true/false already resolved by the OpenXR runtime, not a raw analog value the mod has access to. There's no hysteresis to add on top of a boolean that's already lost the underlying signal."*

**Player-facing gesture map** (shipped ReadMe, ground truth):
- Manual reload, handguns: **B (right)** drops the mag → **LG** at the HMD-anchored ammo pouch grabs a fresh one → bring it to the magwell → then *"hold LG to put your hand on the slider and press LT to pull it back, let go of LT to release the slider."*
- Shotgun: *"RG + LG to 2-hand the shotgun, then LT to pull the pump handle down, release LT to push it up. works any time now, live round chambered or completely empty ... only actually cycles/ejects a shell when the game needs it to, otherwise it's just for feel."*
- *"RG held suppresses LG and LT"*; *"LG held suppresses LT"*; LG+RG throws grenades; LT is the flashlight.

---

# A. Two-hand grip / off-hand on the weapon

Three independent systems coexist, arbitrated only through `_G` globals.

| system | owner | trigger | writes | shipped state |
|---|---|---|---|---|
| **(a) Real grip** | `re2_vr_cosmetic_dock.lua` | LG **rising edge** + proximity to a muzzle-relative foregrip anchor | publishes `__vr_real_grip_active`, `__vr_grip_anchor_world_pos/_rot`, `__vr_muzzle_world_pos/_fwd/_up` | engagement live, **no longer drives the hand**; `enabled=false` in JSON so even the anchors go nil |
| **(b) RG+LG latch** | `re2_vr_recoil.lua` | RG **and** LG held together arms a latch; RG release keeps it; LG release for 0.15 s clears it | `app.ropeway.IkArmFit` left-arm `<TargetMatrix>k__BackingField` | **`support_hand_latch_enabled = false`** |
| **(c) Slide/pump dock** | `ext_2` / `ext_4` publish, `ik_extention` solves | reload gesture state machine | `__vr_slide_dock_blend_factor`, `__vr_slide_hand_world_pos/_rot`, `__vr_slide_dock_ik_pole/_ik_twist` | **live and shipping** — this is what the custom bend-IK actually serves |

## A.1 Managed methods

| what | exact name | how it is used | notes/trap |
|---|---|---|---|
| player | `sdk.get_managed_singleton("app.ropeway.PlayerManager")` → `:call("get_CurrentPlayer")` | every entry point | cached in `RE2.lua`'s own `on_pre_application_entry("UpdateBehavior")` — **only valid from that entry onward**; earlier entries see last frame's value |
| component | `<GameObject>:call("getComponent(System.Type)", <typeof>)` | Equipment, SurvivorCondition, `via.motion.Motion`, `via.render.Mesh` | the `(System.Type)` overload suffix is required |
| transform | `player:call("get_Transform")` (ik_extention) / `go:call("get_Transform")` (recoil) | root of every joint lookup | two different paths, deliberately |
| **joint by name** | `via.Transform:getJointByName(System.String)` | the hottest call in the feature | |
| joint by hash | `via.Transform:getJointByHash(<uint32>)` | muzzle fallback | hash from `via.murmur_hash` |
| joint pose read | `via.Joint:get_Position`, `get_Rotation`, `get_LocalPosition`, **`get_BaseLocalPosition`**, `get_AxisX/Y/Z`, `get_WorldMatrix`, `get_Parent`, `get_NameHash`, `get_Name`, `get_Valid` | measurement, pole vectors, bind-pose base | `get_BaseLocalPosition` is **preferred** for the clavicle stretch so the write is idempotent within a frame; only if nil does it cache `get_LocalPosition` per frame (`stretch_base_frame`) |
| joint pose write | `via.Joint:set_LocalPosition`, `set_LocalRotation`, `set_Rotation`, `set_Position` | clavicle stretch, dock twist, hand rotation | `apply_slide_dock_shoulder_follow` iterates `set_Position` **twice** per call (`for _ = 1, 2 do`) |
| **hashing** | `sdk.find_type_definition("via.murmur_hash"):get_method("calc32")` → `:call(nil, "<string>", 0)` | `l_arm_wrist`, `r_arm_wrist`, `vfx_muzzle1`, `vfx_muzzle2` | static → first arg `nil`, seed `0` |
| **the IK hook** | `sdk.find_type_definition("app.ropeway.IkArmFit")` → iterate `:get_methods()` for name `"updateIk"` → `sdk.hook(m, pre, post)` | **hooked independently by BOTH `ik_extention` and `recoil`** | ik_extention hooks *every* overload; falls back to `:get_method("updateIk")` if the loop found none. Relative order between the two mods is undefined — a single C++ plugin must merge them |
| arg → IkArmFit | `sdk.to_managed_object(args[i])` for `i = 2..10`, then `obj:get_type_definition():is_a(ik_arm_fit_type)` | identical in both files | brute-force scan; do not assume a fixed arg index |
| motion joint pose | `sdk.find_type_definition("via.motion.Motion")` → `getJointIndexByNameHash`, `getWorldPosition`, `getWorldRotation`, invoked as `method:call(motion, ...)` | recoil's **bone-distance teacher** | genuine world space, immune to head-yaw drift — the only trustworthy arm-identity signal |
| aim / reload state | `app.ropeway.survivor.SurvivorCondition:get_IsHold`, `:get_IsReload` | gating | `get_IsReload` is short-circuited to false whenever `__vr_manual_reload_active` |
| camera | `sdk.get_primary_camera()` → `:call("get_WorldMatrix")` | `get_fp_style_hand_world_pos` | the **only** way to get a controller into world space |
| weapon | `Equipment:get_field("<EquipWeapon>k__BackingField")` → `weapon:call("get_GameObject")` → `:call("get_Name")` | the `wpNNNN` id keying every table | |
| main-weapon arm | `Equipment:call("get_MainWeapon")` | suppress_supporthold | returns an "Arm", not the weapon |
| identity | `arm_fit:get_address()` | recoil's per-object side cache | cache flushed at **8** entries (scene-reload address churn) |

**FirstPerson C++ plugin API (global `firstpersonmod`, not managed):** `will_be_used()` (gates `vr_active()` in ik_extention *and* cosmetic_dock — if FP is off, all of A no-ops), `was_gripping_weapon()`, `set_block_left_hand_ik(bool)` (edge-guarded via `__vr_fp_left_hand_blocked`), `get_block_left_hand_ik()`.

## A.2 Fields

| exact name | lives on | direction | notes/trap |
|---|---|---|---|
| `<EquipWeapon>k__BackingField` | `app.ropeway.survivor.Equipment` | read | |
| `<ForceEquipType>k__BackingField` | `app.ropeway.survivor.Equipment` | **written every frame while suppressing** | a `Nullable<WeaponType>` struct: set inner `_HasValue=true` + `_Value`, write the whole struct back |
| `<ForceEquipParts>k__BackingField` | same | written alongside | **forcing type alone strips weapon attachments** — `setForceEquipType`'s real signature is `(Nullable<WeaponType>, Nullable<WeaponParts>, bool)` |
| `_HasValue`, `_Value` | `System.Nullable<…>` | written | clear = `_HasValue=false, _Value=0` |
| `_WeaponType`, `_WeaponParts` | the object from `Equipment:get_MainWeapon()` | read | |
| `<SolverList>k__BackingField` → `:get_field("data")` | `app.ropeway.IkArmFit` | read | two-level: backing field is a wrapper, the array is its `data` |
| `<ApplyJoint>k__BackingField` (fallback `ApplyJoint`) | the solver element | read | **the only structural arm identity** |
| `ArmFitList` → `:get_element(0)` / `:get_elements()[1]` | `app.ropeway.IkArmFit` | read | always element 0 |
| **`<TargetMatrix>k__BackingField`** (fallback `TargetMatrix`) | the ArmFitList element | **read + written** | see raw-offset technique below |
| `LeftHand` / `<LeftHand>k__BackingField` | ArmFitList element | read | `false` ⇒ force side `"right"` |
| `<MuzzleJoint>k__BackingField` → `:get_field("_Parent")` | the weapon | read | note the extra `_Parent` hop |

**TargetMatrix raw access** (identical in both files):
```
offset = td:get_field("<TargetMatrix>k__BackingField"):get_offset_from_base()   -- cached once
read : arm_fit_data:read_float(offset+48/+52/+56)                -- row 3 = translation
write: arm_fit_data:write_float(offset+48/+52/+56, …)
recoil writes the FULL matrix: for i=0..3, base = offset + i*16, four floats each
```
Both then *also* `set_field(name, matrix)` as belt-and-braces; the return value reports the **native** write's success.

## A.3 Joint / bone names (exact)

`l_arm_clavicle`, `l_arm_humerus`, `l_arm_radius`, `l_arm_wrist` · `r_arm_clavicle`, `r_arm_humerus`, `r_arm_radius`, `r_arm_wrist` · `spine_2` → `spine_1` → `spine_0` (first that resolves) · `vfx_muzzle1`, `vfx_muzzle2` · `head` (from the dossier, praydog's FirstPerson.cpp hash) · left-wrist search order `LEFT_WRIST_JOINT_NAMES = { "l_arm_wrist", "L_Arm_Wrist", "L_Arm_Hand", "l_hand_palm", "L_Hand_Palm" }` · `L_Arm_Hand` is additionally rotated by `apply_slide_dock_hand_rotation` **only if it resolves to a different object than the wrist**.

## A.4 Constants

**`re2_vr_ik_extention.lua` file-level:** `SUPPORT_HAND_DISTANCE = 0.5` m · `SLIDE_ARM_SLACK = 0.02` m · `SLIDE_IK_REACH_SLACK = 0.018` m · `SLIDE_DOCK_SHOULDER_MAX_PUSH = 0.50` m.

**Live `CFG` (from the shipped JSON):** `max_m 0.16` · `prestart_m 0.025` · `tau_s 0.10` · `slack 0.018` · `forward_gate_min 0.0` · `use_side_forward_gate true` · `apply_fp_hand_offset true` · `twohand_boost_enabled true` · `twohand_l_extra_max_m 0.08` · `twohand_l_extra_prestart_m 0.0` · `use_ik_target_matrix false` · `fp_hand_use_cached_yaw false` · `auto_standing_height_openxr true` / `_grace_s 3.0` / `_frame_delay 60` · `slide_dock_fixes.{apply_after_update_motion, apply_l_arm_hand, apply_late_passes, ik_early_joint_pos, ik_force_full_snap, ik_hash_side_detection, ik_repatch_post_hook} = all true`.

`load_cfg()` ends with `CFG.use_ik_target_matrix = false` **unconditionally** — the key exists but can never be enabled. Comment: *"IkArmFit TargetMatrix is pre-clamped; never use for stretch distance."*

**Clamps:** `prestart_m` → `[0, 0.12]` · `max_m` → `[0, 0.40]` · `forward_gate_min` → `[0, 1]`, **forced to 0.0 for L whenever the slide dock owns the arm** · `slack` → `[0, 0.12]` · `tau_s` → `max(0.001, _)` · bone lengths → `[0.18, 0.45]` m (default lower `0.24`, upper `0.28`) · two-hand boost: `max_stretch = clamp(max + clamp(twohand_l_extra_max_m,0,0.40), 0, 0.60)`, **left side only**, only when `(docked or slide_left_arm_ik_locked()) and large_weapon` · `dt` clamped to `0.20`, `alpha = 1 - exp(-dt/tau)` · overflow epsilon `1e-4`, apply epsilon `1e-5`.

**FP hand offsets (metres, camera-relative):** `left = (-0.052, 0.084, 0.02)`, `right = (0.052, 0.084, 0.02)`.

**IK solver:** elbow-pole blend `char_right*0.42 + char_down*0.42 + char_back*0.14` normalized (fallback `char_down`) · weapon-pole mix when rack inactive `0.20/0.80` · pole smoothing alpha `0.07` while `__vr_slide_rack_active` else `0.16` **per frame** · elbow-flip heuristic: flip if `hand.y < shoulder.y - 0.06` **and** `elbow.y > shoulder.y - 0.03` · `snap_frac = clamp((rot_blend - 0.55)/0.45, 0, 1)` · degenerate-axis guards `1e-5` · twist defaults `bend_sign -1.0`, `elbow_pole_mix 0.58`, `upper_pole_mix 0.0`, `bone_axis_flip 1.0`.

**Blend thresholds:** engaged `> 0.001`, complete `>= 0.999`. `sync_fp_left_hand_block` uses **different** ones: `pump_support and blend > 0.02`, or `arm_blend > 0.15`. The IK pre-hook arms the override at `slide_blend > 0.02`.

**`re2_vr_cosmetic_dock.lua`:** `grip_zone_radius_m 0.22` (clamped `[0.05, 0.30]`, slider tightened from `0.05–0.40` after the accident) · `grip_back_from_muzzle_m` live `0.038` · back clamp `[-0.30, 1.0]`, right/up `[-0.20, 0.20]`, pitch/yaw/roll `[-180, 180]`.

**`re2_vr_suppress_supporthold.lua`:** `CLEAR_DELAY_FRAMES = 3`.

**`re2_vr_recoil.lua`:** `SUPPORT_HAND_DISTANCE 0.5` m · `WEAPON_NEAR_RIGHT_CONTROLLER_DIST 0.30` m · **LG-release debounce `0.15` s** · side-ambiguity margins `0.03` m (controller distance) and `0.05` m (bone teacher) · identity cache flush at 8 · latch log throttle `0.5` s, grip debug `0.03` s · `two_hand_scale 0.55`, `two_hand_aim_blend 0.5` · roll-correction abort if `axis_len2 < 0.2 and raw_angle2 > π/2` · diagnostic fires at `angle_deg > 90` or `jump_deg > 45`.

## A.5 Order-of-operations traps

**`ik_extention` application entries, in file order** — the dock pose is re-applied **six times per frame at escalating priority**:

| entry | pre/post | what runs |
|---|---|---|
| `UpdateBehavior` | pre | VRControllerManager update |
| `UpdateMotion` | pre | `sim_frame++` **and `on_stretch_pass(true)` — the ONLY smoothing pass** |
| `UpdateMotion` | post | `on_stretch_pass(false)` |
| `LateUpdateBehavior` | pre / post | `on_stretch_pass(false)` |
| `LockScene` | pre | auto-height + `on_stretch_pass(false)` |
| `LockScene` | post | `on_stretch_pass(false)` + `apply_slide_dock_to_left_arm(2)` |
| `UpdateJointExpression` | post | `apply_slide_dock_to_left_arm(4)` + stretch pass |
| `PrepareRendering` | post | `apply_slide_dock_to_left_arm(6)` |
| `BeginRendering` | post | `call_slide_hand_follow()` + `apply_slide_dock_to_left_arm(90)` |
| `EndRendering` | post | `apply_slide_dock_to_left_arm(300)` |
| IK post-hook | — | priority `50 + post_ik_count` |

Guard: `if slide.dock_apply_frame == frame_id and priority < slide.dock_apply_prio then return end` — **strictly `<`**. Tightening it to `<=` was tried and **broke the visible hand-reach animation entirely** (§D).

**Two independent writers of the same pose.** The IK pre-hook writes `TargetMatrix` (native solver does the bend); `apply_slide_dock_to_left_arm` writes joint rotations directly (bypasses the solver). Both gated on the same `slide_blend > 0.001`. Resolved by `... and slide.dock_apply_frame ~= get_frame_id()` in the pre-hook so the custom solver, once it claims a frame, is sole owner. Without it: a two-state per-frame flicker with the real hand stationary.

**Per-frame stretch base reset:** `if frame_id ~= stretch_base_frame then stretch_base_local.L/R = nil` — the stretch writes an *absolute* local position derived from a base; re-reading the base after a write compounds it.

**recoil's arm-side ladder, in order** — this is the whole ballgame for the latch:
1. `ApplyJoint` hash/name (structural, `via.murmur_hash.calc32("l_arm_wrist"/"r_arm_wrist", 0)`)
2. `LeftHand == false` ⇒ right
3. **per-object-address identity cache**
4. target-matrix vs reconstructed hand distance
5. **bone-distance teacher** via `via.motion.Motion` (only answers at margin ≥ 0.05 m, and *teaches* the cache)
6. `dr < 0.30` ⇒ right
7. one-handed heuristics (`ik_call % 2 == 0` ⇒ right, ambiguous ⇒ right, odd ⇒ left)
8. call order

Tiers 4/6/7 use camera-relative reconstructions and **are what flipped identity when the player looked far off-axis — the sole unsolved blocker on the latch.** ik_extention's own fallback is far cruder: *call 1 = left, call 2 = right, anything else = nil.*

`update_support_hand_latch()` is called **unconditionally at the top of recoil's IK pre-hook**, before the `has_recoil` gate, because the gate reads its result. The gate itself is `if not frame_apply.has_recoil and not frame_ik.support_hand_latch then return` — without the latch clause the docked hand would only hold for a few frames after firing.

**suppress_supporthold** runs on `re.on_frame` and corrects the **result** every frame rather than clearing the input bit. LG+RG ambiguity is decided **once, at RG's rising edge**: `suppress_this_rg_hold = not left_grip`. RG-first-then-LG = two-handing (suppress sub-weapon); LG-already-held = deliberate grenade throw (must not interrupt). Both triggers fold into **one** `suppress_active = (right_grip and suppress_this_rg_hold) or real_grip_active` — deliberately one flag, not two state machines racing over `pending_clear_*`.

**Input stripping** (`re2_vr_holster.lua`, same technique in `reload.lua`): `sdk.get_managed_singleton(NS("InputSystem"))` → `:call("get_ButtonBits")` → mask `Down`/`On`/`Up` u64 fields. `BUTTON_FIELD_OFFSET = { Down = 0x10, On = 0x18, Up = 0x20 }` with `get_field` tried first and `read_uint64(offset)` as fallback; writes do **both**. Masks from `statics.generate(NS("InputDefine.Kind"))`: `STRIP_LEFT = SUPPORT_HOLD(128) | UI_SHIFT_LEFT(268435456)`, `STRIP_RIGHT = HOLD(64) | UI_SHIFT_RIGHT(536870912)`, `ATTACK = 4`. Applied on three entries: `on_application_entry("UpdateHID")`, `on_pre_application_entry("UpdateBehavior")`, `on_application_entry("LateUpdateBehavior")`.

**⚠️ Genuine inconsistency, ships as-is:** `strip_input_mask` reads `Down/On/Up` off `InputSystem:get_ButtonBits()`, while `strip_weapon_anim_input_bits` reads the same names **off the `InputSystem` object itself**. One of the two is reading the wrong object.

**Dead code — do not port:** `__vr_cosmetic_dock_blend_factor` / `_hand_world_pos` / `_hand_world_rot` are *read* by `slide.get_dock_state()` (ik_extention:159-163) but **set nowhere in the entire autorun tree** (verified by grep). The cosmetic fallback branch of `get_dock_state()` is unreachable.

## A.6 Attach/detach — the concise answer

**(a) real grip** attaches when ALL of: `vr_active()` (FP will-be-used + HMD active + controllers in use); weapon id ∈ `COSMETIC_DOCK_WEAPONS`; `blocked_by_other_system()` false (none of `__vr_slide_dock_blend_factor > 0.001`, `__vr_mag_in_left_hand`, `__vr_shell_in_left_hand`, `__vr_in_holster_zone`, `__vr_in_head_flashlight_zone`, `__vr_block_left_support_in_mag_holster_zone`); **LG rising edge**; and at that instant `dist(anchor, __vr_lh_world) <= clamp(grip_zone_radius_m, 0.05, 0.30)`. Detaches on LG release, weapon change mid-hold, any blocker true for one frame (which also resets `prev_lg`, so re-engaging needs a fresh press with the hand back inside the radius), or `vr_active()` false.

**(b) latch** attaches when `support_hand_latch_enabled` AND LG AND RG (raw button reads). Stays through RG release. Detaches after LG reads released for a continuous **0.15 s**, or if `firstpersonmod:get_block_left_hand_ik()`. LG alone never arms it.

**(c) slide/pump dock** attaches when the gesture machine publishes `blend > 0.001` + a valid `__vr_slide_hand_world_pos`; blends in at `0.04`/frame (slide, live JSON) or `2.0`/s (pump), smoothstepped `b*b*(3-2b)`. **Always wins** over (a).

**Note the units mismatch:** the slide dock blends per *frame*, the pump support per *second*. Do not unify them blindly — they will feel different at different framerates by design/accident.

---

# B. Manual reload

`re2_vr_reload.lua` is a router, not an implementer. **What actually makes the game reload lives in `ext_1`**, exposed to ext_2/3/4 as `deps`.

## B.1 Gesture and dispatch

**Trigger:** rising edge of **right-controller B**. `is_vr_reload_button_down()` = `vrmod:get_action_b_button()` + `vrmod:get_right_joystick()` + `is_action_active` — **the action handle is re-fetched every call**, unlike trigger/grip which are cached (and whose cache invalidates on `nil` **or `0`**). No hold duration, no double-tap, no cooldown.

**Dispatch, verbatim structure** (`re.on_frame` step 13):
```
if not menu_block and not native_change_bullet_suspend_active()
   and b_edge and manual_reload_context_active() then
    suppress.until_t = os.clock() + 0.35
    if   revolver.is_revolver_weapon_active() -> revolver.handle_b_edge()
    elseif shell.is_shell_weapon_active()     -> shell.handle_b_edge()
    else                                      -> reload_mag.handle_b_edge()
    if not revolver_active                    -> reload_slide.arm_rack_for_reload()
```
Priority **revolver > shell > mag**; the slide rack is armed for everything except a revolver.

**Secondary condition** `empty_mag_locomotion_trigger_active`: weapon empty AND right trigger down AND player locomoting (`LOCOMOTION_MOVE_THRESH = 0.06` m XZ delta between `on_frame` samples, latched `LOCOMOTION_HOLD_SEC = 0.15` s). Only *extends* the suppression window; not a reload trigger on its own.

## B.2 The magazine state machine (`ext_1`)

States: `attached` → `dropping` → `out` → `in_hand` → `inserting` → `attached`.

| transition | trigger | gates | side effects |
|---|---|---|---|
| ATTACHED → DROPPING | B edge | `mag.target` + `mag.has_rest`; not blocked by shell/revolver; context active | `capture_mag_carried_rounds()` = max(chamber, `MainSlot:get_Number`); phase `"slide"`; SFX `mag_drop` |
| DROPPING slide → fall | t≥1 | `anim.drop_sec` **0.18 s**, smoothstep `t²(3-2t)` | snapshot world matrix, switch to `world` freeze |
| fall → OUT | t≥1 | `anim.fall_sec` **0.5 s**, drop `fall_distance * t²` = **1.2 m** | `hide_mag_mesh()`, `__vr_mag_dropped = true`, chamber cached to `mag.carried_rounds`, display zeroed; SFX `mag_floor` |
| OUT → IN_HAND | `tick_mag_holster_grip_grab()` | state OUT; not `mag_hand.active`; not `release_fall_active`; supply available; **in pouch zone**; **fresh LG rising edge**; `os.clock() - last_grab_t >= 0.6 s`; not menu-blocking | `show_mag_mesh()`, `__vr_mag_in_left_hand = true`, SFX `mag_grab`, haptic |
| OUT → INSERTING | B edge | **only when `mag_holster_enabled()` is false** | B-insert exists only if the pouch system is off |
| IN_HAND → INSERTING | `try_mag_insert_dock()` | `mag_hand.active`; not anim; not manual-shell; **grip still held**; `dock_dist <= get_mag_dock_dist(wp)` (**0.20 m** default); `>= 0.5 s` since last dock | `begin_mag_insert()` |
| IN_HAND → DROPPING | LG **falling edge** | | freeze `world_full` at the hand pose |
| release-drop → OUT | t≥1 | **0.5 s**, `1.2 * t²` | hide, SFX `mag_floor` |
| INSERTING → ATTACHED | t≥1 | `anim.insert_sec` **0.18 s** | `restore_mag_rest_pose()`, **`commit_reload_ammo()`**, `arm_left_support_suppress_grace()`, `on_mag_insert_complete(rack_pending)` |

Insert-start geometry: `(mag.rest_x, mag.exit_y, mag.exit_z)` — *"Seated X rail; Y+Z from magwell exit (matches drop pitch) into rest pose."*

**There is no magazine spawning.** The mag is an existing weapon joint (`mag_node_by_wp[wp]`). Hiding is `write_target_local_scale(target, kind, Vector3f(0,0,0))` (`visual_hide.mode == "scale"`, the default); the legacy path walks the subtree with `set_DrawSelf(false)` / `set_Enabled(false)` / `mesh:setPartsEnable(i,false)` for `i in 0..min(n-1,63)` plus `Gun:setPartsEnable(false, {idx})`. `enforce_mag_mesh_hidden()` re-applies the hide every `on_prepare_rendering` — **skipped while `mag_hand.active`**.

Freeze modes (`mag.freeze_mode`), applied by `apply_mag_frozen_pose()`: `"local"` (local position only), `"local_full"` (local pos + rot, used in hand), `"world"` (world position only, fall), `"world_full"` (world pos + rot, hand-release drop). `apply_mag_frozen_pose()` early-returns if `mag.mesh_hidden` — a hidden mag never gets posed.

## B.3 The ammo-pouch anchor (HMD-yaw basis)

`resolve_mag_holster_anchor()` returns origin + three axes:
- origin = `sdk.get_primary_camera():get_WorldMatrix()[3]`
- forward = `(wm[2].x, wm[2].z)` normalized on the horizontal plane, **length guard ≥ 0.05**; below it the last-good `hmd_fx/hmd_fz` is reused; ultimate fallback `(0,1)`
- `ax = (fz, 0, -fx)`, `ay = (0,1,0)` (world up, always), `az = (fx, 0, fz)`

Pouch = `origin + ax*off_right + ay*off_up + az*off_forward`.

| constant | value |
|---|---|
| `off_right` | **-0.15** m (player's left) |
| `off_up` | **-0.60** m (below eye level) |
| `off_forward` | **+0.18** m |
| `trigger_dist` | **0.25** m (zone entry) |
| `release_dist` | **0.38** m (zone exit, hysteresis) |
| `cooldown` | **0.6** s |
| `calibrate_delay_sec` | **5.0** s |
| `MAG_PREVIEW_SEC` | **5.0** s |
| `anchor_version` | **2** — one-shot migration that resets pre-2026-08-18 joint-basis offsets |
| `sub_weapon_grace_sec` | **1.0** s |

Per-profile storage `CFG.mag_holster.per_profile[key]`, keyed by `RE2Character.get_active_profile_key(player)` with a parent fallback via `get_pose_fallback_profile(key)`. Known keys: `leon`, `claire`, `ada`, `hunk`, `tofu`.

Calibration (`start_mag_holster_calibrate` / `tick_mag_holster_calibrate`) captures `decompose_hand_offset(hand_pos, origin, ax, ay, az)` after the countdown; the capture basis is deliberately the same function (`mag_holster_capture_pose = resolve_mag_holster_anchor`).

**Haptics:** zone `0.15 s / 58.883 Hz / 0.05` every 3 frames when continuous · grab `0.057 / 169.385 / 0.99` · denial triple pulse at `+0.00 / +0.13 / +0.26` s (`0.05 s`, `60/190/60 Hz`, amp `1.0`) · calibrate fail `0.12/80/1.0`, success `0.25/250/1.0`. All via `vrmod:trigger_haptic_vibration(0.0, duration, frequency, amplitude, joystick)`.

**Left-hand position sources — three functions with DIFFERENT fallback chains, not interchangeable:**
- `get_mag_holster_hand_pos()` (zone test, calibration): `l_arm_wrist` joint → `__vr_lh_joint_pos` (only if `__vr_lh_slide_ik_override == true`) → `__vr_lh_world` → `getJointWorldPos(tf,"l_arm_wrist")` — **joint first**
- `get_left_hand_position()` (exported to ext_2): `__vr_lh_joint_pos` (gated) → `__vr_lh_world` → raw VR controller → cached `__vr_lh_joint` → `l_arm_wrist` — **globals first**
- `get_track_pos_for_rack()` (ext_1): `get_mag_holster_hand_pos()` → tag `"hand_skeleton"`, else raw controller → tag `"vr_raw"`, else nil
- `get_track_pos_for_rack()` (ext_2, separate impl): deps track-with-source → deps track → `__vr_lh_joint_pos` (gated) → `__vr_lh_world` → `get_left_hand_position` → `get_controller_game_world_pos()` — six deep

## B.4 What actually changes the ammo — the exact engine calls

**Reads:** `Gun:getBulletNumber` · `Gun:getReloadableBulletNumber` · `Equipment:getReloadableBulletNumber(WeaponType)` · `InventoryManager:getMainWeaponRemainingBullet` (=loaded) / `getMainWeaponReloadableBullet` (=carried) / `getItemNumber(id)` · `Inventory:get_MainSlot` → `get_Number` / `get_MaxNumber` / `get_BulletID` / `get_VacancyNumber` · `Inventory:getReloadableBulletMainSlot(false)` · `Inventory:get_MainSlotSurplusBulletID` / `getSlotNumber(id)` · `Gun:get_ReloadTrackHandle` → `get_Track` **or** `getTrack`.

**Writes — the +1 shell cascade (`apply_single_shell_round`), five rungs in order:**
1. `Inventory:reloadMainSlot(1)` (only if `carried_before > 0`) — tag `"reloadMainSlot"`
2. `Equipment:reloadMainWeapon(1)` → `Equipment:executeReload(wt, 1)` → `Gun:executeReload(1)` — **all three fired unconditionally in sequence**, tag `"executeReload"`
3. `InventoryManager:reduceItem(bullet_id, 1)` then `Gun:executeReload(1)` — tag `"reduceItem_executeReload"`
4. `Inventory:reduceSlot(bullet_id, 1)` then `Gun:executeReload(1)` — tag `"reduceSlot_executeReload"`
5. **last resort, free ammo:** `Inventory:set_MainSlotSurplusBulletNumber(0)` then `Slot:set_Number(loaded_before + 1)` — tag `"slot_set_number"`, consumes nothing

After **every** rung: `Gun:executeEndReload()` then `Gun:endChamberClear()` (`sync_shotgun_chamber()`). Without that pair the round is in the slot but the gun refuses to fire.

**Direct chamber set (no reserve consumed), `apply_direct_chamber_count`:**
1. `Inventory:set_MainSlotSurplusBulletNumber(0)`; `Slot:set_Number(count)`
2. `weapon:set_field(f, count)` for `f ∈ {"BulletNumber", "_BulletNumber", "<BulletNumber>k__BackingField"}`, **verified by re-reading `read_raw_weapon_chamber_bullet_count() >= n` after each**
3. `track:set_field(f, count)` for `f ∈ {"Number", "_Number"}`

The three-name ladder is written out in full in **four** places (`native_bullet_n`, `zero_weapon_chamber_without_reserve_touch`, `sync_weapon_chamber_display_zero`, `clear_weapon_chamber_ammo`, `apply_direct_chamber_count`). Factor it once in the port.

**Pull reserve into chamber, `apply_reserve_to_chamber`:** `set_MainSlotSurplusBulletNumber(0)` → `addSlotNumber(bullet_id, need)` → `reloadMainSlot(need)` (method `"inject_reload"`); alt route `set_MainSlotSurplusBulletID(id)` → `set_MainSlotSurplusBulletNumber(count)` → `changeBulletMainSlotWithoutReload()` → `reloadMainSlot(pull)` (method `"surplus_reload"`). **Then the claw-back:** `reduceSlot(bullet_id, max(0, reserve_after - reserve_before))` + `set_MainSlotSurplusBulletNumber(0)`. Skipping the claw-back hands the player free ammo.

**Prime the pool, `prime_main_weapon_reloadable_pool`:** `set_MainSlotSurplusBulletID(id)` → `set_MainSlotSurplusBulletNumber(spare)` → `changeBulletMainSlotWithoutReload()`. Early-outs if `get_inventory_spare_bullet_count() <= 0`. Required before the first insert — *"Spare rounds in HUD not yet in reloadable pool after save load."*

**Reserve top-up after mag insert, `commit_reserve_top_up`:** `Inventory:reloadMainSlot(add)` where `add = min(vacancy, reserve)`. Deferred by **2 frames** (`pending_reserve_top_up.frames_left`).

**Clear chamber, tactical (rounds stashed by the mod, `get_mag_carried_rounds() > 0`):** `track:set_field("Number", 0)` else the three `BulletNumber` spellings. **Must not touch reserve.**
**Clear chamber, non-tactical:** `Equipment:useMainWeapon(before)` → `Equipment:use(wt, before)` → `Equipment:executeEndEject(wt)` → `Gun:executeEndEject()`. Only `useMainWeapon` credits reserve — hence the warning `"clear_weapon_chamber_ammo touched reserve %d -> %d"`.

**Chamber display zero, `sync_weapon_chamber_display_zero`:** `set_MainSlotSurplusBulletNumber(0)`; `Slot:set_Number(0)`; `weapon:call("setBulletNumber", 0)`; all three `set_field` spellings; `track:set_field("Number", 0)`.

**Make the gun fire-ready, `sync_chamber_shoot_ready`:** `Gun:executeEndReload()`; if `native_bullet_n() > 0` then `Gun:endChamberClear()`. Called on cylinder close, not on insert. Also clears `mag.weapon_ammo_cleared` and the reserve cache first.

**Return chamber to reserve:** `Equipment:useMainWeapon(loaded)`, then if under-credited `Inventory:addSlotNumber(bullet_id, loaded - credited)`.

**`setBulletNumber` is NOT the reload primitive.** The only call is `weapon:call("setBulletNumber", 0)` inside `sync_weapon_chamber_display_zero()`. `[inferred]` reason: it sets the count without updating the reload track or chamber state, so the gun still refuses to fire — which is why `executeEndReload` + `endChamberClear` are paired everywhere.

**There is no `app.ropeway.InventoryManager`.** It is `app.ropeway.gamemastering.InventoryManager`. The inventory *object* (`re2.inventory`) is not a singleton — it is an `app.ropeway.survivor.Inventory` instance from the `utility/RE2` helper. Getting this wrong is an easy port bug.

## B.5 Suppressing the vanilla reload — four layers

| layer | mechanism | gate |
|---|---|---|
| 1 — order inhibit | `PlayerActionOrderer:setInhibitPrecede(true, RELOAD)`; `setInhibit(true, CHANGE_BULLET)` — applied in the **post** of `doSurvivorActionOrdererUpdate` | `manual_reload_context_active()` / `should_suppress_empty_native_reload()` |
| 2 — method skip | `SKIP_ORIGINAL` **pre**-hooks on 13 methods | `should_block_native_reload_pipeline` / `should_block_native_change_bullet` |
| 3 — state spoof | `SurvivorCondition:get_IsReload` **post** returns `false` | `should_spoof_reload_state()` — suppresses the canned reload IK/animation |
| 4 — fire lockout | mask `ATTACK` out of `Down/On/Up`; `InputSystem:setForce(ATTACK, false)`; `requestFire` pre `SKIP_ORIGINAL` | `should_hard_block_empty_fire()` |

**Every hooked method, with pre/post:**

| type | methods | pre | post |
|---|---|---|---|
| `app.ropeway.survivor.Equipment` | `requestReload`, `reloadMainWeapon`, `executeReload*` | SKIP_ORIGINAL | identity |
| `app.ropeway.survivor.Equipment` | `requestChangeBullet`, `changeBulletMainWeapon`, `executeChangeBullet*` | SKIP_ORIGINAL | identity |
| `app.ropeway.implement.Gun` | `executeReload*`, `createCartridge`, `executeChangeBullet*` | SKIP_ORIGINAL | identity |
| `app.ropeway.survivor.Inventory` | `reloadMainSlot`, `changeBulletMainSlotWithoutReload`, `changeBulletMainSlot` | SKIP_ORIGINAL | identity |
| `app.ropeway.survivor.SurvivorCondition` | `get_IsReload` | empty | **logic here** — returns `false` when spoofing |
| `app.ropeway.survivor.player.PlayerActionOrderer` | `doSurvivorActionOrdererUpdate` | captures `sdk.to_managed_object(args[2])` only | **all logic here** |
| `app.ropeway.implement.Gun` **and** `app.ropeway.survivor.Equipment` | `requestFire` | `on_pre_fire` | `on_post_fire` |
| `app.ropeway.implement.Gun` | `getBulletNumber` | — (nil) | **post** returns `0` when spoofing |
| `app.ropeway.gamemastering.InventoryManager` | `getMainWeaponRemainingBullet` | — (nil) | **post** returns `0` when spoofing |
| `app.ropeway.weapon.shell.ShellCartridgeController` | `generate` | SKIP_ORIGINAL for manual-pump shotguns | identity |

**`on_pre_fire` order of operations (verbatim sequence):**
1. `fire_request_blocked = false`
2. `if should_hard_block_empty_fire()` → `play_blocked_fire_sfx_immediate()`, `fire_request_blocked = true`, **`return sdk.PreHookResult.SKIP_ORIGINAL`**
3. `reload_pump.on_pre_weapon_fired()`
4. `if should_play_empty_dry_fire()` → `reload_util.play("dry_fire")`
5. `reload_pump.seal_pump_shot_in_flight()`

**`on_post_fire`:** `should_play_pump_fire_sfx(fire_request_blocked)` → `reload_util.play("pump_fire", { wp = wp })`; if not blocked → `reload_pump.on_weapon_fired()`; if `reload_slide.is_slide_close_latched()` and `getBulletNumber() > 0` → `reload_slide.on_weapon_fired()`.

**Hook-installation mechanics (port these):** `hook_method_once(method, label, gate)` de-dupes on `label .. "|" .. method:get_signature()` in `hooked_signatures`. `hook_methods_by_prefix` enumerates `td:get_methods()` and matches `mname == prefix or mname:find("^"..prefix,1,true) == 1`. Both the prefix scan and `make_suppress_prehook` skip any method whose name contains `"Sub"` (`is_sub_weapon_method_label`) — **sub-weapon reloads are never suppressed**. `install_reload_hooks()` bails and retries next frame if `total <= 0` (`tick_reload_hooks_install`), so idempotence is load-bearing.

Enum values (with fallbacks used when the type won't resolve): `INPUT_KIND_ATTACK` 4 · `HOLD` 64 · `SUPPORT_HOLD` 128 · `UI_SHIFT_LEFT` 268435456 · `UI_SHIFT_RIGHT` 536870912 · `PATIENT_ORDER_HOLD_WALK` 64 · `PATIENT_ORDER_JOG` 256 · `PRECEDE_ORDER_RELOAD` 64 · `PRECEDE_ORDER_CHANGE_BULLET` **nil** (the change-bullet inhibit is silently skipped when unresolvable).
Masks: `INPUT_SUPPRESS_LEFT_SUPPORT_MASK = SUPPORT_HOLD | UI_SHIFT_LEFT`, `INPUT_SUPPRESS_RIGHT_HOLD_MASK = HOLD | UI_SHIFT_RIGHT`.

**The engine's own typo is load-bearing: `setInhibitPetient`, and the enum `SurvivorDefine.ActionOrder.Petient`. Reproduce verbatim.**

Orderer acquisition: `sdk.get_managed_singleton("app.ropeway.PlayerManager")` → `get_CurrentPlayerCondition` → `cond:get_field("<ActionOrderer>k__BackingField")`; fallback `player:call("getComponent(System.Type)", sdk.typeof("app.ropeway.survivor.player.PlayerActionOrderer"))`.

**Gate predicates, exact logic:**
- `manual_reload_context_active()` = `CFG.enabled == true` **and** `is_manual_reload_weapon(get_weapon_go_name())` (any of `enabled` / `needs_manual_shell_reload` / `needs_manual_pump` / `needs_manual_cylinder_reload` / `needs_manual_revolver_reload`)
- `should_suppress_empty_native_reload()` = not suspended, context active, and (revolver blocks empty native reload **or** `weapon_needs_reload()`)
- `should_block_native_reload_pipeline()` = not suspended, not `ammo.internal_commit`, neither bypass global, then true if `manual_reload_session_active()` **or** `should_suppress_empty_native_reload()` **or** `manual_reload_context_active()`
- `should_hard_block_empty_fire()` = not suspended, then true if revolver cylinder blocks fire **or** pump `should_block_fire()` **or** (context active and (mag out of gun **or** slide `needs_rack()` **or** weapon empty))

**⚠️ The re-entrancy escape hatch.** The mod's own commits call the very methods it blocks. Three flags open the gate:
```
if ammo.internal_commit then return false end
if rawget(_G, "__vr_rack_chamber_commit_bypass") == true then return false end
if rawget(_G, "__vr_mag_ammo_commit_bypass") == true then return false end
```
Every ammo write is wrapped in `ammo_commit_wrap` / `with_commit`, which set **both** the global and `deps.ammo.internal_commit`, and clear both after. `with_commit` additionally sets the rack bypass and calls `clear_native_reload_precede_inhibit_now()` **before** the call. **It is not refcounted — a nested commit clears the flag early.** A C++ port needs the same token or it will block its own writes.

**The kill switch:** `suppress.tick_change_bullet_suspend()` polls `PlayerManager.CurrentPlayerCondition:get_IsChangeBullet()`. Rising edge ⇒ `suppress.change_bullet.active = true`, `__vr_manual_reload_suspended = true`, `__vr_native_change_bullet_active = true`, `reload_revolver.begin_native_change_bullet_suspend()`, `until_t = os.clock() + 6.0`. Ends immediately when `IsChangeBullet` goes false and `saw_native` was set; otherwise times out after 6 s. While active every suppression gate returns false and pump/shell/revolver/mag/slide `on_frame` are skipped.

**Reload-stack reset** wipes all **63** `RELOAD_STACK_GLOBAL_KEYS` to nil under `__vr_reload_stack_reset_in_progress`, then calls `reset_stack_state()` / `clear_globals()` / `on_weapon_swap()` on every module inside one `pcall`, then `clear_native_reload_precede_inhibit_now()`, refreshes the weapon cache twice, `mag.refresh_weapon_cache`, `slide.refresh_weapon_cache`, and finally `slide.apply_slide_park("stack_reset")`. Reasons: `session_exit`, `session_exit_deferred`, `player_unloaded`, `player_ready_after_save_load`, `player_death_respawn`, `load_busy_cleared`, `load_wait_cleared`, `main_flow_load_cleared` (`is_load_cleared_reset_reason()` marks the last four as load-signal, letting them run even while save/load is busy). Detectors: `SaveDataManager:get_IsLoadBusy`, `saveLoadStep == 3` (`SAVE_LOAD_STEP_LOAD_WAIT`; read via `get_saveLoadStep()` then fields `<saveLoadStep>k__BackingField` / `_saveLoadStep` / `saveLoadStep`), `MainFlowManager:get_IsInLoadGameData` / `get_IsLoadGame` / `get_GameGUID`. Deferred until a player exists and load isn't busy, with a **120-frame** escape hatch.

## B.6 Application entries (`re2_vr_reload.lua`)

| entry | pre/post | what runs |
|---|---|---|
| `UpdateHID` | pre | `_G.__vr_left_support_strip_frame++` only |
| `UpdateHID` | post | `apply_vr_input_suppress()` |
| `UpdateBehavior` | pre ×2 | `apply_vr_input_suppress()`; and separately `sync_reload_input_inhibit()` → `setForce(ATTACK,false)` |
| `UpdateBehavior` | post | `refresh_vr_input_cache()`, `tick_reload_fire_sfx()`, `strip_weapon_anim_input_bits()` |
| `UpdateScene` | pre | `reload_mag.on_update_scene()` |
| `UpdateMotion` | pre | `pump.on_pre_arm_ik()`, `pump.on_update_motion()`, **`slide.tick_trigger_rack(false)`** |
| `LateUpdateBehavior` | pre | `pump.on_pre_arm_ik()` again, **`slide.tick_trigger_rack(true)`** |
| `LateUpdateBehavior` | post | menu-block → silence + return; else `mag/shell/revolver/util/pump.on_late_update()`, then `slide.update_rack_pull()`, `update_slide_rack()`, `sync_rack_motion("LU")`, `tick_hand_follow()` |
| `PrepareRendering` | post | `mag/shell/revolver/util.on_prepare_rendering()`, `slide.sync_rack_motion("PR")`, `tick_hand_follow()`, `update_manual_reload_globals()` |
| `BeginRendering` | post | `slide.sync_rack_motion("BR")`, `tick_hand_follow()` |
| `UpdateJointExpression` | post | `revolver.on_joint_expression()`, `slide.apply_slide_park("UJE")` |
| `re.on_draw_ui` | — | master dispatcher, sorts `_G.__vr_ui_callbacks` by `order`, `pcall`s each (reload is `order = 41`) |

`sync_rack_motion` is called with three distinct tags — **`"LU"`, `"PR"`, `"BR"`** — so the slide module knows which pipeline stage it is in. `apply_slide_park` is called with **`"UJE"`** and **`"stack_reset"`**.

**`re.on_frame` main tick, in order:** 1 `tick_reload_hooks_install()` · 2 `refresh_vr_input_cache()` · 3 `is_menu_blocking()` via `_G.__vr_is_menu_blocking` · 4 `silence_manual_reload_for_menu()` on menu enter **and on the frame after menu exit** · 5 `apply_vr_input_suppress()` · 6 `update_manual_reload_globals()` · 7 `tick_player_locomotion()` · 8 `tick_reload_stack_reset()` · 9 weapon-change detect (`get_weapon_go_name() ~= prev_weapon_wp`; fans `on_weapon_swap` out, only if `prev_weapon_wp ~= nil`; **revolver alone is passed the previous wp**) · 10 `pump/shell/revolver.on_frame()` · 11 `suppress.tick_change_bullet_suspend()` · 12 disabled/inactive fan-out · **13 B-edge dispatch** · 14 suppression-window extension · 15 `mag.on_frame()`, `slide.on_frame()` · 16 `util.update_listener()`, `tick_reload_fire_sfx()`, `update_manual_reload_globals()` · 17 mag-holster left-support sync · 18 dry-fire edge (`prev_bullet_count > 0 and n <= 0` and carried ≤ 0 and slot ≤ 0 ⇒ `reload_slide.on_dry_fire()`) · 19 `update_manual_reload_globals()` (third time) · 20 `try_execute_pending_reload_stack_reset()`.

**Re-entrancy / idempotence guards:** `hooks_installed`, `isreload_hook_installed`, `action_orderer_strip_hook_installed`, `vr_input_suppress_hooks_installed`, `fire_hook_installed`, `hooked_signatures[label|signature]`, `_G.__vr_ui_master_installed`, `_G.__vr_reload_stack_reset_in_progress`, `pending_reload_stack_reset` (+ frame counter), `mag_sub_suppress.active`, `blocked_fire_sfx_prev.{trigger,grip_trigger}`, `prev_frame_vr_reload_b`, `prev_weapon_wp`, `prev_bullet_count`, `play_session_death_latched`, `abort_blocked_native_reload_last_t` (0.15 s debounce), `menu_block_manual_reload_prev`.

**Module load order is fixed and load-bearing: ext_1 → ext_5 → ext_2 → ext_4 → ext_3**, with `deps` filled in *progressively* — ext_1 receives the table and 24 more keys are added to it **after** its `init` returns (`play_reload_sfx`, `on_mag_insert_complete`, `clear_tactical_rack_state`, and the 21 `shell_*` / `revolver_*` predicates). A C++ port must use function pointers / `std::function` slots, not a value-copied struct.

**`deps` handed to ext_1 at construction:** `CFG`, `sc`, `re2`, `suppress` (mutable: `.until_t`, `.precede_active`, `.change_bullet{active,saw_native,wp,until_t,hold_sec}`, `.end_change_bullet_suspend()`, `.tick_change_bullet_suspend()`, `.refresh_precede_inhibit()`), `ammo` (mutable `{internal_commit=false}`), `frame` (mutable `{vr_reload_b, weapon_needs_reload, vr_grip_trigger, vr_right_trigger}`), `save_cfg`, `manual_reload_context_active`, `is_weapon_enabled`, `get_weapon_go_name`, `weapon_label_for`, `weapon_display_name`, `mark_tuning_dirty`, `refresh_tuning_snapshot`, `on_mag_holster_suppress_change`, `clear_native_reload_precede_inhibit_now`, `native_change_bullet_suspend_active`.

**ext_5 init:** `CFG`, `sc`, `re2`, `get_mag_hand`, `get_weapon_go_name`, `weapon_display_name`, `is_weapon_enabled`, `current_profile_key`, `mark_tuning_dirty`, `get_vr_controller_world_pos(hand)`, `shell_pose_active`, `revolver_pose_active`.

**ext_2 init:** `CFG`, `sc`, `re2`, `manual_reload_context_active`, `manual_reload_session_active` (**ships as nil — see §D bug 2**), `is_weapon_enabled`, `get_weapon_go_name`, `weapon_display_name`, `get_left_hand_position`, `get_left_track_position`, `get_left_track_pos_with_source`, `get_vr_controller_world_pos(hand)`, `is_left_grip_pressed`, `is_left_trigger_pressed`, `get_bullet_number`, `haptic_pulse(lj,dur,freq,amp)`, `get_haptic_left_joystick`, `get_mag_carried_rounds`, `mark_tuning_dirty`, `play_reload_sfx`, `arm_left_support_grace(sec)`, `weapon_uses_manual_cylinder_reload`, `capture_cylinder_rest`, `weapon_uses_chamber_bullet_follow(wp)`, `get_chamber_bullet_open_bind_display(wp)`, `sync_chamber_bullet_pose`, `apply_chamber_bullet_follow_open_t(open_t)`, `revolver_invalidate_cylinder_joint_cache`. **The whole `reload_slide.init(...)` is wrapped in `pcall`** and logs `"[re2_vr_reload] slide_ext init failed: ..."` — the slide/rack module can silently fail to initialise and the mod keeps running.

**ext_4 init:** `CFG`, `sc`, `re2`, `reload_drop`, `get_weapon_go_name`, `manual_reload_context_active`, `is_weapon_enabled`, `weapon_display_name`, `mark_tuning_dirty`, `play_reload_sfx`, `haptic_pulse`, `get_haptic_left_joystick`, `is_left_grip_pressed`, `is_left_trigger_pressed`, `get_left_track_position`, `get_left_track_pos_with_source`, `get_left_hand_position`, `get_left_hand_joint`, `get_mag_hand_hold_entry`, `get_mag_dock_dist(wp)`, `detach_mag_hand_for_shell`, `get_weapon_chamber_bullet_count`, `get_main_weapon_reserve_ammo_count`, `prime_main_weapon_reloadable_pool`, `get_weapon_reloadable_count`, `get_imgr_weapon_loaded`, `get_weapon_mag_slot_round_count`, `get_shell_hud_ammo`, `apply_carried_mag_ammo(count, opts)`, `apply_single_shell_round`, `reload_slide` (whole module), `extend_suppress_window`, `arm_left_support_grace(sec)`.

**ext_3 init (34 keys):** `CFG`, `sc`, `re2`, `reload_drop`, `get_weapon_go_name`, `manual_reload_context_active`, `weapon_display_name`, `mark_tuning_dirty`, `play_reload_sfx`, `get_left_hand_position`, `get_left_hand_joint`, `get_left_track_position`, `get_left_track_pos_with_source`, `get_mag_hand_hold_entry`, `get_mag_dock_dist`, `is_left_grip_pressed`, `detach_mag_hand_for_shell`, `get_weapon_chamber_bullet_count`, `get_main_weapon_reserve_ammo_count`, `get_inventory_spare_bullet_count`, `prime_main_weapon_reloadable_pool`, `get_shell_hud_ammo`, `apply_carried_mag_ammo`, `apply_single_shell_round`, `extend_suppress_window`, `arm_left_support_grace` (**no-arg wrapper here, unlike ext_2/ext_4's `(sec)` version**), `publish_sub_weapon_suppress`, `clear_weapon_chamber_ammo`, `sync_chamber_shoot_ready`, `on_chamber_ammo_to_carried(stash_n)`, `commit_reload_ammo`, `weapon_uses_canister_mag_reload`, `get_mag_carried_rounds`, `get_weapon_mag_slot_round_count`, `haptic_pulse`, `native_change_bullet_suspend_active`.

**Callbacks pushed back INTO ext_1:** `reload_mag.set_bullet_insert_preview_handlers({ extend(wp, sec), cancel(restore_pose), is_active(), get_remaining() })` — all four route to `reload_revolver`.

**ext_2 publishes** `_G.__vr_reload_slide_dock` — a table of `tick`, `tick_hand_follow`, `update_slide_rack`, `update_rack_pull`, `sync_rack_motion`, `apply_slide_park`, `on_gesture_start`, `on_rack_released`, `set_needs_rack`, `slide_rack_context_ok`, `is_weapon_enabled`, `get_dock_dist`, `is_track_near_dock`, `on_weapon_swap`, `clear`, plus the shared `gesture_*` helpers. This is the handoff to ext_4's pump gesture.

## B.7 Revolver / single-shell specifics (ext_3)

`REVOLVER_CAPACITY = { wp0300 = 6, wp3200 = 5, wp0800 = 5, wp4100 = 1 }` (fallback 6). Note wp0800 says 5 here while `bullet_chamber_nodes_by_wp.wp0800` lists six nodes — `get_next_chamber_slot_index` uses `#nodes`, so the 5 is effectively unused.

`revolver_reload` constants: `open_lerp_rate 6.0` (travel/s) · `dock_dist_default 0.20` m · `chamber_grab_dist_default 0.28` m · `dock_cooldown 0.45` s · close-gesture: `swipe_speed 0.55` m/s, `swipe_dist 0.025` m, `lateral_ratio 0.45`, `max_vertical_ratio 0.45`, `window_min 0.035` s, `window_max 0.17` s, `up_swipe_speed 0.55`, `up_swipe_dist 0.025`, `up_ratio 0.5`, `delay_sec 0.1`, `cooldown_sec 0.8`, `insert_preview_sec 6.0`.
Hardcoded in ext_3: post-insert grace **0.65 s** (and `insert_dur + 0.65`) · detach detection `0.35` m world / `0.08` m local / `0.08` m rest-mismatch · `dt` hitch clamp `0.1` s · `quat_slerp` switchover `0.9995` · open-enough-to-reload `open_t > 0.01` · `open_t` closed epsilon `0.001` · vector epsilon `1e-6` · transform depth cap `14` · `rv.pending_insert.frames_left = 999999` (never expires) · grab sentinel `99.0`, `vec3_dist` nil sentinel `1e9`.
Chamber-grab haptic `0.057 / 169.385 / 0.99`; close-gesture haptic `0.06 / 200.0 / 0.7`.

`resolve_bullet_target(weapon_xform, node_name)` tries, for both `node_name` **and its underscore-toggled twin** (`"_04"` ↔ `"04"`): `getJointByName`, then a recursive child-transform search by GameObject name. Returns `(target, "joint")` or `(target, "xform")`. `resolve_dock_anchor` falls through `bullet_dock_joint_by_wp` → literal `"Chamber"`, `"Bullet"`, `"Cylinder"` → default `"Chamber"`.

`get_cylinder_joint_name(wp)` checks **`CFG.slide_dock.slide_node_by_wp[wp]` before `CFG.cylinder_joint_by_wp[wp]`**; `get_cylinder_open_bind(wp)` reads `slide_dock.slide_bind_by_wp` / `slide_bind_default` (mapping `open_x/open_y/open_z`, falling back to `parked_z`); `get_chamber_bullet_open_bind` reads `bullet_open_x/y/z/rot_pitch/yaw/roll` from the same per-wp slide-bind entry. The revolver deliberately shares the slide-rack config namespace. Variable names `rv.slide_lx0/ly0/lz0/lx1/ly1/lz1` and `rv.slide_rw0..rz1` are the *bullet insert* animation endpoints, **not** slide state — pure naming carry-over, a real porting hazard.

Three archetypes: **per-chamber-joint revolver** (wp0300/wp3200/wp0800 — six joints scaled to `(0,0,0)` to hide, `uses_joint_scale_bullet_visual` true which **disables** `uses_chamber_bullet_cylinder_follow`, insert animation lands on `rv.rest_x/y/z`, grab target = `get_next_chamber_slot_index` = `loaded + 1` clamped to `#nodes`); **single-chamber follow** (wp4100 — one bullet joint lerping with the cylinder open via `apply_chamber_bullet_cylinder_follow(open_t)`, `keep_spent_chamber_bullet = true` so the spent casing stays visible and must be manually extracted); **canister mag** (`canister_mag_reload`, JSON-only, code complete, no default weapon — `chamber_extracted` state, per-weapon `canister_stash_by_wp[wp]` surviving weapon swaps, commits via whole-mag `commit_reload_ammo()` rather than `+1`, and uses `release_local_fall = true` so the drop animates in *parent-local* space).

`Drop` helper (ext_5) fallback chain for the drop joint: `reload_drop_joint_by_wp` → `bullet_joint_by_wp` → `shell_joint_by_wp` → `mag_node_by_wp` → `"_04"`. `Drop.fall_duration` 0.5 s / `Drop.fall_distance` 1.2 m fallbacks; `Drop.reload_drop_slots` default 6.

**ext_5 SFX manager:** `SOUND_KINDS = { "mag_drop", "mag_floor", "mag_grab", "mag_insert", "slide_rack_pull", "slide_rack_release", "pump_fire", "dry_fire" }`; `DEBOUNCE_S = 0.10` s per kind; folder scan `CACHE_TTL = 2.0` s; `KIND_VOLUME_MIN/DEFAULT/MAX = 0.0/1.0/2.0`, master clamped `0..1`; files globbed from `custom_sfx/<folder>/*.ogg|wav` (folder `"common"` excluded from the per-weapon map); `weapon_sfx.default_fallback_wp = "wp1000"`.
**ext_5 finger pose:** 17 left-hand bones — `l_hand_index_0/_1/_2`, `l_hand_middle_0/_1/_2`, `l_hand_ring_0/_1/_2/_3`, `l_hand_little_0/_1/_2/_3`, `l_hand_thumb_0/_1/_2` — driven via `get_LocalEulerAngle` / `set_LocalEulerAngle`. `blend_sec 0.12`, `preview_sec 5.0`, dt clamp `0.25` s (fallback `1/60`), blend epsilons `0.0001` / `0.9999` / `0.001`, angle wrap `180/360`. Context priority: **`bullet_hold` > `shell_hold` > `mag_hold` > `slide_rack`**.

---

# C. Slide racking / pump

**There is no bolt-action code anywhere.** Two gesture families sharing one core: **pump** (`ext_4`, shotguns) and **slide rack** (`ext_2`, pistols).

## C.1 Anchor resolution (identical shape in both)

1. `sc(weapon_xform, "getJointByName", node_name)` → `(joint, "joint")`
2. else `find_transform_child_by_name(...)` — recursive `get_Child`/`get_Next` sibling walk, **depth cap 14** — then retries `getJointByName` (a quirk), else `(child_tf, "xform")`
3. else `(weapon_xform, "xform")` — **it never returns nil.** An unresolved node moves the whole gun.

## C.2 Pump gesture state machine (`update_manual_pump_gesture`)

Called from `Pump.on_pre_arm_ik()` on **both** `UpdateMotion`-pre and `LateUpdateBehavior`-pre — **twice per rendered frame**. Travel easing is per-call (`speed * dt`), so effective easing is ~2× the nominal constant.

Entry guards in order: `manual_pump_equipped()` → `__vr_shell_in_left_hand` (shell outranks pump) → `slide_gesture.gesture_update_pull` exists → weapon-id change nulls anchor/spans → resolve anchor → `pump_await_grip_release and not grip` clears the latch.

**Grip held, not yet active:** fresh grip edge clears `mo_hold_off`; `pump_is_real = (needs_pump == true)`; if cosmetic, set `grab_bind_z = bp.parked_z`, write the joint, `prepare_pump_axis_for_weapon(wp)` — **but do NOT set `needs_pump`**; then `active = true` and reset `pull_done / pull_now / pump_travel / pump_committed / trigger_prev / mo_init / mo_ratio`.

**While active:**
- `trig_pressed_edge` ⇒ `pump_committed = true`; if cosmetic, **arm `needs_pump` here** (which is what blocks fire)
- target = `not pull_done ? (committed ? 1 : 0) : (trig ? 1 : 0)`
- `pump_travel` → target at `pull_travel_speed` **7.0**/s up, `push_travel_speed` **5.0**/s down, × `re.get_delta_time()`
- motion drive: `mo = gesture_motion_ratio(gesture, wp, ctx, motion_pull_scale = 1.0, motion_deadzone_m = 0.015)`; **`if mo > pump_travel then pump_travel = mo`** (max, never min)
- `eased = smoothstep01(pump_travel) = x*x*(3-2x)`; **`gesture.pull_now = eased * pull_d`** — converted back to metres because downstream gates treat it as a distance
- joint write: pulling ⇒ `z = grab_bind_z + eased*(back_z - grab_bind_z)`; returning ⇒ `z = rest_z + eased*(back_z - rest_z)`, via `sj:call("set_LocalPosition", Vector3f.new(x, y, travel))` (X/Y preserved from `get_LocalPosition`)
- **latch:** `not pull_done and (pump_committed or mo >= 1.0-1e-4) and pump_travel >= 1.0-1e-4` ⇒ `pull_done = true`, SFX `slide_rack_pull`, haptic `(0.06, 220.0, 0.7)`, **`_G.__vr_pump_pulled_down_wp = wp`**. A motion-only pull does **not** arm `needs_pump`
- **complete:** `pull_done and not trig and pump_travel <= 1e-4` ⇒ `complete_pump_cycle(wp)`, publish, set `last_grip`, **return early**

**Grip released while active:** cosmetic ⇒ full `clear_manual_pump_state()` + park joint; real ⇒ `active=false`, `pump_travel=0`, `pump_committed=false`, `clear_gesture_pull_state()`, park joint, `apply_pump_bind`.

`complete_pump_cycle` (frame-deduped on `pump_complete_frame == get_frame_id()`): SFX `slide_rack_release` → **`try_end_chamber_clear()` always, cosmetic or not** → `set_pump_joint_local_z(bp.rest_z, bp)` → `clear_manual_pump_state()` → **`mo_hold_off = true` (after the clear, which resets it false)** → `await_pump_on_insert = false`, refresh `prev_loaded` → haptic `(0.06, 200.0, 0.8)`.

## C.3 What chambers the round

```
weapon:call("endChamberClear")             -- ok flag set here
_G.__vr_rack_chamber_commit_bypass = true
weapon:call("executeEndReload")
weapon:call("executeEndEject")             -- ext_4 (pump) only
_G.__vr_rack_chamber_commit_bypass = false
```
ext_2's slide version omits `executeEndEject` and adds a fallback: `if get_IsChamberCleared ~= true then weapon:call("executeReload", 1)`.

## C.4 `needs_pump` truth table (verbatim)
```
-- Tube empty / no insert yet: false
-- Shell top-up while needs_pump from a shot: stays true until manual pump
-- Fired with round in chamber AND another shell still in gun (prev_in_gun > 1): true
-- Last shot (only round in gun): false on fire; await_pump_on_insert until shells inserted
-- Dry fire on empty tube: false
-- After manual shell insert when gun was emptied: true (even if native sync chambers)
```
Implemented as `prev_chamber > 0 and prev_in_gun > 1` ⇒ `needs_pump = true, pump_is_real = true`; else false and `prev_chamber > 0 and prev_in_gun <= 1` ⇒ `await_pump_on_insert = true`. `prev_chamber` falls back to `prev_loaded`, else `now_chamber + 1` (*"requestFire post-hook: chamber count is already post-shot"*).

`get_in_gun_shell_count()` chain: `get_shell_hud_ammo()` → `get_imgr_weapon_loaded()` → `get_weapon_mag_slot_round_count()` (only if > 0) → `get_weapon_chamber_bullet_count()`.

## C.5 Motion-layer scrubbing (blocking the native pump animation)

`scrub_pump_motions()` from `Pump.on_frame()` (when `block_native_pump ~= false`) and `Pump.on_update_motion()` (`UpdateMotion`-pre).

`sdk.typeof("via.motion.Motion")` (resolved **once at file load** — if nil, all scrubbing silently no-ops) → `go:call("getComponent(System.Type)", motion_type)` → `sc(mc,"getLayer",i)` → `sc(layer,"get_HighestWeightMotionNode")` → `get_MotionName()` / `get_Weight()` / `get_Frame()` / `get_EndFrame()`. Kill = node `set_Frame(EndFrame)` + `set_Weight(0)`; layer `set_Frame(EndFrame)` + `set_Weight(0)` + **`set_Speed(100.0)`**.

Layers `{0..15}` for both weapon and player (code fallback `{0..8}`). Weapon motion cache TTL **0.5 s**, player **1.0 s**; `collect_motion_components` recursion depth **6** (function default 4), child index cap `min(n-1, 32)`; `sc(re2.weapon,"get_Motion")` appended to the weapon list if not already collected.

Player motion lookup chain: `re2.get_localplayer()` → `sdk.get_managed_singleton("app.CharacterManager")` → `get_PlayerContextFast` → `get_GameObject` → `sdk.get_managed_singleton(NS("PlayerManager"))` → `get_CurrentPlayerCondition` → `get_Controller` → `get_GameObject`.

**Decision order inside `scrub_motion_layer(mc, layer_index, source)`:** get layer → get top node → get name → **`if not pump_suppress_wanted() then return end`** (note: *after* the name read, not before) → `get_Weight() < 0.05` ⇒ return → classify → min-progress gate → kill.

**Name classifiers (verbatim logic):**
- `motion_name_is_re2_shotgun_weapon_cycle`: reject `idle`/`ready`; require one of `wp1000`, `wp1001`, `wp1100`, `wp1200`, `wp1300`, `wp1500`, `sg02`, `_sg0`; then require one of `blowback`, `pump`, `cycle`, `reload`, `eject`, `rack`
- `motion_name_is_re2_spark_weapon_hold_cycle`: require `wp4300`, reject `idle`, require `hold` or `pump`
- `motion_name_is_re2_player_pump_cycle`: require `pump`; reject `idle` **unless** `hold_pump` also present

**Min-progress gate — let the first N% play, then kill (avoids a hard visual pop):** default `pump_cycle_min_progress` **0.20**; `player_pump` → `re2_player_pump_min_progress` **0.0**; a shotgun cycle whose name contains `"blowback"` → `re2_blowback_min_progress` **0.08**; spark hold → **0.08**. Node weight floor **0.05**.

`pump_suppress_wanted()` = `pump_weapon_equipped()` and, if `when_equipped == false`, additionally `manual_reload_context_active()`. Default `when_equipped = true` ⇒ suppression is on the whole time the weapon is out.
`pump_window_active()` = `os.clock() < pump_suppress_until`, armed at `os.clock() + pump_window_sec` (**2.5 s**) from `Pump.on_weapon_fired()`. Zeroed by `reset_stack_state` and `on_weapon_swap`.

**`wp1001` appears only here** — a motion-name mesh token, not a weapon entry.

## C.6 Wwise suppression

Hooks on `app.WwiseContainerApp` (`sdk.typeof(NS("WwiseContainerApp"))`, resolved once at load): `trigger(System.UInt32)` — pre `on_wwise_trigger_u32` (`args[2]` = this, `args[3]` = u32 id), and `triggerByFsm(System.UInt32, via.wwise.RequestInfo)` — pre `on_wwise_trigger_fsm` (`args[3]` = fsm index, resolved via `mo:call("getTriggerIdByFsm", idx)`; if that fails **and** the pump window is active **and** `block_wwise_in_pump_window ~= false`, it still returns `SKIP_ORIGINAL`). Both posts are pass-through. Name resolved by `container:call("getName(System.UInt32)", id)` then `getTriggerName(System.UInt32)` — first non-empty string wins. Container obtained by `getComponent(wwise_app_type)` with `sc(re2.weapon,"get_WwiseContainerApp")` as fallback.

```lua
local PUMP_WWISE_TOKENS = {
    "pump", "reload", "shell", "cycle", "load", "chamber", "cartridge",
    "shotgun", "rack", "cock", "spark", "electric", "blowback",
}
```
Matched with plain `string.find(..., 1, true)` on the lower-cased name. `"load"` is a substring of `"reload"`/`"unload"` — deliberately broad.

Decision (`should_block_wwise_trigger`): not `pump_suppress_wanted()` ⇒ false; `block_wwise_triggers ~= false` and token match ⇒ **block**; `block_wwise_in_pump_window ~= false` and `pump_window_active()` and the name contains neither `"fire"` nor `"shot"` ⇒ **block**. So inside the 2.5 s post-shot window everything except fire/shot sounds is muted; outside it, only token matches are.

Ownership check `wwise_hook_matches_weapon(mo)` = `pump_weapon_equipped()` and `mo == get_weapon_wwise_container()` — object-identity compare against the equipped weapon's own container, so other actors' audio is untouched. Duplicate guard: `hooked_signatures["wwise|"..type_name.."|"..signature]` plus module-level `pump_hooks_installed`; logs `[re2_vr_reload] installed %d Wwise hook(s)`.

## C.7 The motion drive (`M.gesture_motion_ratio`, ext_2)

Measures `dot(P_left - P_right, pull_dir)` against a baseline captured at grab time — so left-back, right-forward, or both register identically, and the weapon translating with the right hand cancels out of the subtraction.

Both hands via `get_controller_game_world_pos`:
```
rel        = vrmod:get_position(idx) - standing_origin
rel        = vrmod:get_rotation_offset() * rel
ctrl_world = camera.position + (camera.rotation * rel)
```
(`camera.position` = `get_primary_camera():get_WorldMatrix()[3]`; rotation from `_G.__vr_camera_stored_rot`, else `get_GameObject`→`get_Transform`→`get_Rotation`. **`controllers[1] = LEFT`, `controllers[2] = RIGHT`; `vrmod:get_position(0)` = HMD.** `standing_origin` from `vrmod:get_standing_origin()`, fallback `vrmod:get_position(0)` latched once.)

Direction: **the joint's LIVE travel axis in world, taken from the PARENT's axis column (`sc(sj,"get_Parent")` → `get_WorldMatrix`), times the LOCAL sign of `back_z - parked_z`.** Never sampled by writing the joint (stale-matrix trap, §D).

Constants: EMA `ratio = prev + (ratio-prev)*0.4` · snap `>0.985 ⇒ 1.0`, `<0.01 ⇒ 0.0` (so the `>= 1.0-1e-4` latch and `<= 1e-4` completion are reachable through the asymptote) · minimum span **0.005** m · span validity epsilon `1e-4` · axis length epsilon `1e-6` · identical-hands guard `(hx²+hy²+hz²) < 1e-6` ⇒ bail loudly · `motion_pull_scale` **1.0** pump / **2.0** slide · `motion_deadzone_m` **0.015** pump / **0.012** slide · log throttles 0.05 / 0.25 / 2.0 s, camera-GameObject backoff 0.5 s.

## C.8 Slide-rack constants (ext_2)

`dock_dist_default` **0.20** (Lua) / **0.15** (live JSON) · dock exit hysteresis = enter **+0.03** m · `dock_blend_speed` **0.10** per tick (Lua) / **0.04** (live JSON), then smoothstepped `b*b*(3-2b)` · `pull_dist_default` **0.05** m · `push_dist_default` **0.03** m · `pull_deadzone` **0.004** m (**off-threshold is half = 0.002**) · `pull_axis_sign` **1.0** · `pull_smooth_alpha` **0.92** · `trigger_pull_travel_speed` **7.0**/s · `trigger_push_travel_speed` **5.0**/s · `grip_release_debounce_sec` **0.1** s (flat, unconditional) · `grip_release_hard_cap_sec` **3.0** s (keeps absorbing while `rack.in_range`) · finish-on-release ratio **0.55** · push-complete `push_delta >= push_d * 0.98` · `preview_sec` **6.0** s · `slide_return_sec` **0.31** (JSON only) · `pump_reach_mult` **2.75** (JSON only) · `arm_ik_blend_start` **0.0** (JSON only) · arm-IK apply argument `pcall(apply_fn, 5)` — the literal `5` is unexplained in-source · `dt` fallback `1/60` · blend latch epsilons `0.001` / `0.999` · pull-limit haptic `(0.06, 220.0, 0.7)`, cycle-complete haptic `(0.06, 200.0, 0.8)`.

`slide_ik_twist_default`: `bend_sign -1.0`, `elbow_pole_mix 0.58`, `upper_pole_mix 0.0`, all `pole_off_*` / `pole_rot_*` / `hand_off_*` zero, and four per-bone blocks (`clavicle`/`upper`/`lower`/`wrist`, each `{pos_x,pos_y,pos_z,rot_pitch,rot_yaw,rot_roll}`) all zero.

**Two independent grip samples, two independent debounces:** `update_slide_rack_trigger` debounces for the state machine (`rack.grip_release_at`); `M.tick()` separately debounces for the **visual** blend (`rack.visual_grip_release_at`). Same 0.1 s window, different variables. The second was added because the first didn't cover the visual path (§D).

**The pump has NO equivalent debounce** — it reads `is_left_grip_pressed()` raw every tick, so a one-frame false release aborts the cycle (cosmetic ⇒ full clear; real ⇒ park). A known asymmetry and a likely source of intermittent pump aborts.

**ext_2 entry points:** `M.tick()`/`on_frame` from `re.on_frame`; `M.tick_trigger_rack(false)` from **pre**-`UpdateMotion`; `M.tick_trigger_rack(true)` from **pre**-`LateUpdateBehavior`; `update_rack_pull` / `update_slide_rack` / `sync_rack_motion("LU")` / `tick_hand_follow` from **post**-`LateUpdateBehavior`; `sync_rack_motion("PR")` from post-`PrepareRendering`; `sync_rack_motion("BR")` from post-`BeginRendering`. `M.tick_hand_follow()` guards on `hand_follow_frame == get_frame_id()` (`re.get_frame_count()` else `floor(os.clock()*60)`).

**ext_1 entry points:** `M.on_frame` (`re.on_frame`), `M.on_update_scene` (**pre**-`UpdateScene`), `M.on_late_update` (**post**-`LateUpdateBehavior`), `M.on_prepare_rendering` (**post**-`PrepareRendering`). Animation ticks live in `on_update_scene`; pose *application* in **both** `on_late_update` and `on_prepare_rendering` — writing the same pose twice per frame is deliberate.

## C.9 Delayed shell eject

Hooks `app.ropeway.weapon.shell.ShellCartridgeController:generate` — **pre**, `SKIP_ORIGINAL` for `MANUAL_PUMP_SHOTGUNS`, recording `pending_wp` (the weapon id, **not** the controller instance). If `generate` is missing the whole script returns with `log.warn("...Could not find ShellCartridgeController.generate, aborting")`. Replay chain re-fetched fresh every time: `GameObject.get_component(player, NS("survivor.Equipment"))` → `Equipment:get_MainWeapon()` → `arm:get_ShellCartridgeController()` → `scc:call("request")` — **`request()` only**, never `generate()`. `REPLAY_GUARD_FRAMES = 10`. Consumes and clears `_G.__vr_pump_pulled_down_wp` on `re.on_frame`. Observed timings: request→generate ~13 ms (~1 frame) natural spacing; the second generate that motivated the guard came ~25 ms later.

## C.10 Order-of-operations traps (feature C)

| what | detail |
|---|---|
| **Twice-per-frame call** | `Pump.on_pre_arm_ik()` from both `UpdateMotion`-pre and `LateUpdateBehavior`-pre. Travel easing is per-call, so effective easing is ~2× nominal. |
| **Stale world matrix** | ext_2's `publish_visual` parameter: `false` at `UpdateMotion`, `true` at `LateUpdateBehavior`. The engine does not recompute a joint's cached `WorldMatrix` between our `set_LocalPosition` write and the `UpdateMotion`-timed read. **The pump module has no equivalent guard** — it publishes a scalar offset rather than re-reading the joint, which is why it is immune. |
| **Frame dedupe** | `pump_complete_frame == get_frame_id()` in `complete_pump_cycle`; without it the twice-per-frame call would double-fire chamber finalize, SFX and haptic. |
| **Duplicate hook install** | `hooked_signatures` (both the Wwise keys and reload.lua's `label\|signature` keys) plus `pump_hooks_installed` / `hooks_installed` / `isreload_hook_installed` / `fire_hook_installed` / `action_orderer_strip_hook_installed`. `install_reload_hooks` is retried every frame until it succeeds. |
| **`requestFire` hooked on two types** | Same pre/post pair on `implement.Gun` and `survivor.Equipment`. `on_pre_weapon_fired` re-snapshots each time; `on_weapon_fired` clears the snapshot so a second call falls into the `prev_chamber == nil` recovery path. |
| **`get_IsReload` must be post** | Pre-hook returns are for skipping; the spoof rewrites the return value. |
| **`mo_hold_off` set AFTER the clear** | Reordering silently disables the phantom-pull lockout. |
| **`needs_pump` armed late, on purpose** | At `trig_pressed_edge`, not at grip — the grip button doubles as two-handed aim/support. |
| **Motion-only cosmetic pull never arms `needs_pump`** | Only `pump_committed` (an LT press) does. A phantom can animate the forend but can never block firing. |
| **`apply_slide_park` must be gated per weapon type** | `if rack.active and weapon_uses_trigger_rack(wp) then return end`. Called from three `sync_rack_motion` sites plus `UpdateJointExpression`; without the gate they fight the trigger-driven joint write. |
| **Weapon change** | `Pump.on_weapon_swap()` invalidates motion caches, zeroes `pump_suppress_until`, clears state + support dock, re-reads `prev_loaded`. The delayed-eject script separately drops a stale `pending_wp`. ext_1's `update_mag_weapon_cache()` keys on `(weapon_go_name, gameobject:get_address())` and on change: cancel preview → `finalize_mag_state_for_weapon_swap()` → snapshot old weapon into `mag_snapshots[wp]` `{mag_dropped, carried_rounds}` → clear every target/joint field → resolve new joint → restore that weapon's snapshot. |
| **Stack reset** | `__vr_reload_stack_reset_in_progress == true` makes `should_skip_mag_snapshot_io()` true — snapshots neither captured nor restored, `M.on_weapon_swap` returns immediately. |
| **Menu** | every entry except `UpdateBehavior` guards on `is_menu_blocking()` (`_G.__vr_is_menu_blocking`, a **function** installed by holster). `silence_manual_reload_for_menu()` runs on menu enter *and* on the first frame after leaving. |
| **Camera throws in menus** | `sc(cam,"get_GameObject")` throws internally while the item box has the camera in a transitional state; `sc` swallows it but REFramework logs each throw — a 300+/sec on-screen error storm. Mitigated by `rack.cam_go_fail_until = now + 0.5`. |
| **Locomotion abort** | `block_empty_locomotion_mag_anim()` resets the whole mag visual state when: context active, chamber empty, `__vr_player_locomoting`, `__vr_vr_right_trigger`, and an anim/drop in flight. |
| **Save-load reconcile** | `reconcile_mag_with_loaded_game()` — if state is OUT, nothing in flight, no snapshot says `mag_dropped`, and `getBulletNumber() > 0`, force the mag back to ATTACHED. `reconcile_canister_with_loaded_game()` likewise for canister weapons. |
| **Publish dedupe** | `publish_mag_holster_sub_weapon_suppress()` dedupes on `(frame, want)` via `__vr_left_support_strip_frame`; `arm_left_support_suppress_grace` forces `publish_frame = -1` to break it. Called three times inside `M.on_frame` and again in `on_late_update`/`update_globals`. |
| **Hook install ordering** | `install_chamber_display_hooks()` is called from `M.on_frame` — the display spoof does not exist until the first frame tick after init. **There is no unhook**; the hooks live for the process lifetime and are neutered only by the gate. |

## C.11 Support-hold suppression (the mag system's contribution to two-handing)

Output: `_G.__vr_block_left_support_in_mag_holster_zone` (bool) plus `deps.on_mag_holster_suppress_change(want)` on edge only.
`mag_holster_sub_weapon_suppress_wanted()` is true if: revolver aux suppress wanted; OR left grip pressed while in zone with a sub-weapon; OR inside the grace window; OR shell/revolver sub-weapon suppress; OR the mag session suppresses shoot-ready; OR `mag_hand.active`; OR state DROPPING/INSERTING or `anim_active`; OR in zone (with sub-weapon carve-outs); OR `__vr_needs_rack` / `__vr_slide_rack_active`; OR the sticky `sub_weapon_grip_block`.
**Support grace:** `arm_left_support_suppress_grace(sec)`, default **1.0 s**. Sets `left_support_grace_until = os.clock() + sec` (only extends, never shortens), forces `publish_frame = -1`, re-publishes immediately. Fired at the end of the insert animation and by ext_2's `complete_rack_cycle`. Cancelled if `manual_reload_context_active()` goes false. Inside the window it does **not** suppress if a pump forend hold is active (`__vr_needs_pump` / `__vr_pump_active` / `__vr_pump_slide_support`) unless also in-zone or shell-suppressing.
ext_2's parallel gate: `__vr_block_support_dock` = mag in hand OR mag inserting OR bullet in hand OR (holster-zone block AND not pumping) OR `needs_rack` OR `rack.active`.

---

# D. Traps and hard-won knowledge (verbatim)

These are the most valuable lines in the codebase. Grouped by what they'd cost you to rediscover.

### The stale world matrix — the single most important porting fact
> `-- This function is called twice per frame (UpdateMotion pre-hook, then LateUpdateBehavior pre-hook). Debug logging proved the UpdateMotion-timed call always reads back a STALE cached world matrix for rack.anchor (the slide joint) -- the engine hasn't recomputed it since our set_slide_joint_local_z write above -- while the LateUpdateBehavior-timed call reliably reads a fresh one. Publishing from both alternated the rendered hand between a frozen stale pose and the correct eased one every frame -- that WAS the teleport. Only publish/apply from the call that's confirmed fresh; still advance trig_travel/local_z every call for the higher-frequency easing.`

> `-- Pull direction is derived WITHOUT moving the joint: earlier versions sampled the parked/back world poses (write local_z, read world matrix back), but the engine serves a STALE cached world matrix for a joint written the same frame ... the sample reads a zero span and the whole drive silently never arms. Instead: the joint's LIVE travel axis in world (a plain read, always fresh enough for a direction) times the LOCAL sign of back_z - parked_z from the bind pose. That sign is per-weapon ground truth for which way along the axis "racked back" lies -- no guessed convention, no sampling.`

> `-- The rack/pump animation writes the node's LOCAL POSITION, which moves it in the PARENT's frame -- so the world direction of that travel is the PARENT's axis column, not the node's own (they only coincide when the node's local rotation is identity, which weapon slide/pump nodes are not guaranteed to have). Projecting on the node's own axis is the prime suspect for every "axis projection reads ~zero pull" failure in this system's history, this feature's first version included.`

> `-- Publishing the target position alone wasn't enough -- the actual arm-IK application (bending the arm bones to reach it) only ran from M.tick()/M.tick_hand_follow()'s own once-per-frame-ish ticks, not from this higher-frequency one, so the rendered arm was still only catching up in occasional large jumps. Apply it directly here too.`

### Coordinate spaces — the mistake made and reverted twice
> `-- 2026-08-13: two attempts were made and REVERTED this session ... based on the theory that raw controller position (vrmod:get_position or vrc_manager.controllers_list[i].position) is the "true" world position and __vr_lh_world's camera-relative reconstruction merely "drifts". That theory was WRONG: confirmed live both times that raw controller position is in VR tracking/room space (near-origin values, e.g. -0.3,-0.1,-0.5) while the anchor is in game-world space (e.g. 5.3,2.3,-5.3) -- completely different coordinate systems, not comparable at all. The camera-relative reconstruction isn't an approximation of world position, it IS the (only) way to GET world position here -- it anchors the tracking-space offset to the camera's real world position/orientation. Global-first is back to being correct. Any head-orientation sensitivity in the grip check is a real, separate, harder problem (the reconstruction's yaw-only approximation) -- not something to "fix" by bypassing it.`

> `-- 2026-08-15, EXPERIMENTAL, default OFF: get_fp_style_hand_world_pos() re-rotates the controller's tracking-space offset by the camera's LIVE yaw every frame -- confirmed live (see re2_vr_hand_head_yaw_coupling_status.md memory) to visibly swing the computed hand position when the player just turns their head, even with the controller held perfectly still (12,356-frame capture, 6,192 controller-motionless-frame-pairs, hand_pos still moved ~0.012 units per degree of yaw change). When true, the yaw used for that rotation is captured ONCE (first successful resolution after script load) and reused for the rest of the session ... vrmod:get_standing_origin()'s w component was checked and ruled out -- confirmed fixed regardless of head yaw, but its value (w=1.0, x/y/z small) looks like a standard homogeneous-coordinate position marker, not a yaw angle.`

> `-- Both hands are read via get_controller_game_world_pos -- the RAW controllers transformed into game world -- NEVER __vr_lh_world, which the slide-dock override feeds the DOCKED joint position while racking (the hand is glued to the slide there: reading it back would measure our own output, not the player's input).`

> `-- 2026-08-14: found live -- player reported the loop being relative to HMD look direction (stable if looking the same way the whole hold, loops when turning the head left/right with the real hand stationary). __vr_lh_world ... is published via get_fp_style_hand_world_pos -- a tracking-space offset rotated by the CAMERA'S CURRENT YAW every frame, correct for matching what the camera sees when rendering the FP hand mesh, but NOT a stable world-space signal: the same physical hand position produces a DIFFERENT computed value as soon as the head turns. Unlike the other candidates here (genuine noise outliers averaged out by the "take the minimum" design), this one has a real, sustained bias, not transient noise -- letting it win the minimum comparison reintroduces exactly the visual-loop symptom the 2026-08-08 fix targeted, just via head yaw instead of distance-threshold noise. Only trust it as a fallback when NOT already docked via the confirmed-stable gated joint below.`

### The blend-origin taint (the longest and most valuable comment in ik_extention)
> `-- 2026-08-14: found live -- __vr_slide_dock_blend_from_pos is NEVER SET (capture_slide_dock_blend_origin, above, is dead code, unreferenced anywhere in the mod) -- so this always falls through to __vr_lh_world, meaning "from_pos" is a LIVE, every-frame-changing read, not a fixed blend origin. That's normally fine (an exponential ease, __vr_lh_world mostly mirrors this same function's own previous output via __vr_lh_joint_pos while docked) -- but __vr_lh_slide_ik_override flickers false for isolated frames even with the near/grip debounce fixes above (confirmed live via slide_rack_dist_probe logging), and on those frames __vr_lh_world falls back to publish_vr_hand_globals's OWN camera-relative reconstruction. Reading that tainted value here as "from_pos" kicks this blend's trajectory away from the dock for a moment, then the exponential ease pulls it back -- a visible snap-and-recover, worse the more the head has turned. ... __vr_lh_world itself is only guaranteed to be the stable joint while the override gate is true ... checking the gate directly here, not just whether the global happens to be populated, since a tainted value still reads as "valid".`
*(Note: the "NEVER SET" claim is now stale — `ext_2:990` and `ext_4` do set `__vr_slide_dock_blend_from_pos`, and `ext_2`'s `capture_hand_dock_blend_origin` calls the exported `__vr_capture_slide_dock_blend_origin`. Treat the comment as historical.)*

### Arm identity must be structural
> `-- Persistent per-arm identity (2026-08-18): the native engine can never misassign arms because each IkArmFit component IS one specific arm -- identity is structural, not derived. Mirror that: whenever a ground-truth signal identifies this solver object's side (apply-joint name/hash, the native LeftHand field, or the skeletal bone-distance teacher below), remember it by object address and never fall back to positional guessing for that object again. The positional fallbacks (camera-relative hand reconstructions) are what flipped identity when the player looked far off-axis -- the sole unsolved blocker on the RG+LG latch ("aiming backwards" / both hands glitching on head turn).`

> `-- ... confirmed live this reconstruction drifts enough under head/body orientation divergence (player looking far off-axis from the character's facing) to flip which arm_fit_data this function decides is "right", which snapped the weapon's aim to the support hand's pose ("aiming backwards") once the RG+LG latch made a genuinely continuous docked left hand possible for the first time.`

> `-- Bone-distance teacher: compare the IK target against the player's ACTUAL skeletal wrist joints (genuine world space via the motion system -- head orientation cannot shift these, unlike the camera-relative reconstructions the tiers below use). Only answers with a confident margin, which is exactly when it's allowed to teach the identity cache; with both hands together on a weapon it stays silent and the cache (taught earlier, hands apart) carries it.`

### The fix that broke the animation (do not "improve" this)
> `-- 2026-08-14: tried tightening this to <= (same-priority repeats within a frame also skipped, not just strictly-lower ones) to stop on_post_update_ik's escalating-priority repeat calls from re-running the full custom bend-IK solve multiple times per frame ... CONFIRMED LIVE this broke something else: the visual hand-reach-onto-the-slide animation stopped appearing entirely (grip+trigger racking still worked functionally, just no visible hand travel, and the player had to already be close to the weapon for it to engage at all). Root cause of THAT not chased down. Reverted to the original < ... (player confirmed: "felt better than before"). The escalating-priority repeat-call pattern this leaves in place is a known, deliberately deferred follow-up.`

### Two writers, one frame
> `-- 2026-08-14: found live -- player reproduced a rapid two-state hand loop (real hand stationary, raw controller distance flat, but __vr_lh_joint_pos alternated between two fixed values nearly every frame) at a specific hand position relative to the weapon. Root cause: this branch and apply_slide_dock_to_left_arm (above) are TWO INDEPENDENT writers of the same __vr_lh_joint_pos/arm pose -- this one writes the native IkArmFit target matrix directly, the other runs a full custom bend-IK solve (elbow pole hints, clavicle stretch) and writes joint rotations directly, bypassing the native solver entirely. Both are gated on essentially the same slide_blend>0 condition and can both fire the same frame from different hooks ... whichever happens to write last that frame wins, and if that alternates frame to frame ... that alone reproduces exactly the observed two-state flicker.`

### The phantom pull
> `-- Motion-pump lockout: set when a pump cycle completes, cleared by a fresh grip press or a real (game-required) pump arming. Without it the cosmetic gesture that auto-reactivates right after completion (grip still held) captures a rest-posture motion baseline, and ordinary aiming-hand drift then reads as a phantom full pull that arms needs_pump and blocks firing (seen live 2026-08-17: s drifted +14cm to ratio 1.00 within 1.3s of a completed cycle).`

> `-- LT-committed cosmetic racks still arm needs_pump (an LT press is deliberate). A MOTION-only cosmetic pull never does: that arming is exactly how a drifted-baseline phantom pull became a fire block (live log 2026-08-18 00:34, ratio pinned 0.75-1.00 for 6s while just holding the gun). Without it a phantom costs a spurious forend animation at worst -- firing is never blocked, and the pull-arm IK dock (needs_pump-gated) never yanks the arm.`

> `-- Motion pumping is ALWAYS available (player's explicit call, 2026-08-18: "i really really don't wanna give that up" -- reverts the one-session real-pumps-only experiment). Phantom protection is layered elsewhere instead: mo_hold_off after each completed cycle (re-squeeze LG to free-pump again -- the mechanism the player confirmed working), and a motion-only cosmetic pull never arms needs_pump (below), so a phantom can animate the forend at worst -- it can never block firing.`

### Chamber finalize is not optional
> `-- Always call this, cosmetic rack or not -- it's what tells the native weapon it's fire-ready again (see sync_chamber_shoot_ready() in re2_vr_reload_ext_1.lua for the same pattern), not something that only matters when ammo actually changed. Skipping it for cosmetic racks left the weapon stuck refusing to fire afterward.`

### The hand must stay on the forend
> `-- needs_pump OR active: the hand stays attached to the forend the whole time the grip is held. Requiring needs_pump alone dropped the hand the instant a cycle completed ("still lets go after pump handle goes back up", live 2026-08-18) -- with the cosmetic gesture auto-active while gripping, the no-pull FP-passthrough branch below hands the visual to the native support snap instead of releasing.`

> `-- 'not gesture.active': never block the FP left hand while the player is actually holding the forend (grip held => active). Without this, finishing a pump INSIDE the 2.5s post-shot suppress window (pump_window_sec) blocked FP hand IK the moment needs_pump went false -- the hand visibly floated off the forend for the window's remainder ("still loses that grip", live 2026-08-18; fast motion pumps finish well inside the window, slow LT pumps mostly ate it, which is why this never stood out before).`

### `needs_pump` is armed at the trigger edge, not at grip
> `-- Free/cosmetic grip: prime the bind pose/pull-axis setup (same as arm_pump_gesture() would do) so the pump joint and hand-follow IK are ready, but do NOT set needs_pump yet -- this grip button doubles as the normal two-handed aim/support hold, so merely gripping (without ever pulling the trigger to rack) must not block firing. needs_pump is armed later, at the trig_pressed_edge commit point below, only once the player actually starts pulling.`

### Eject on the pull-down, not the return
> `-- Signal for re2_vr_delayed_shell_eject.lua: the pump handle has just been fully pulled down (distinct from complete_pump_cycle(), which only fires after the handle is released and returns to rest). The spent shell should eject on the pull-down, not the return.`

### The `pull_now` unit trap
> `-- pull_now is treated elsewhere as a real distance in meters (e.g. the arm-IK support gate), so scale by the configured pull distance rather than using the raw 0..1 travel value.`

### State that only resets on one of two exit paths
> `-- trig_travel/trig_committed/trigger_prev are trigger-rack-only state that nothing else resets between cycles (the two places that clear them inside update_slide_rack_trigger are dead code or only reachable on an aborted grab, never a normal completion) -- left stale-true here made the SECOND+ trigger-rack reload in a session auto-commit to a full pull instantly on grab, with no LT press at all, since 'target = rack.trig_committed and 1.0 or 0.0' doesn't care what LT is doing. Always start a fresh grab clean regardless of how the last cycle ended.`

> *(dev-archive write-up of the same bug)* `rack.trig_travel/trig_committed/trigger_prev are never reset between cycles by anything actually reachable — the only two reset points live inside update_slide_rack_trigger itself, and one is dead code (blocked by the wrapper's own rack.active gate), the other only fires on an aborted grab, never a normal completion. So after the *first* successful trigger-rack reload in a session, trig_committed stays true forever.`

> *(the reusable lesson, verbatim from the dev archive)* `Reusable lesson for any future trigger-driven gesture conversion (e.g. porting this pattern to another weapon or to RE3): any function that writes set_slide_joint_local_z or reads/writes _G.__vr_slide_rack_pull_signed/rack.pull_now needs an explicit weapon_uses_trigger_rack(wp) guard, or it will silently fight the trigger-driven write — both root causes here were exactly that class of bug (one via a missing publish-side guard across duplicate hook call sites, one via a genuinely missing weapon-type guard in a third, unrelated function).`

### Grip sensor noise, two layers
> `-- Real controllers occasionally report a brief false grip read even under a sustained, deliberate squeeze (analog sensor noise / runtime polling hiccup). This flat window applies unconditionally to EVERY release (even an obvious one, hand already moving away), so it's kept short -- just enough to absorb a couple of frames of noise. Longer sustained false reads are handled by the grip_release_hard_cap_sec layer below instead, which only keeps waiting while the hand is corroborated as still at the dock, so it doesn't cost genuine releases any extra delay.`

> `-- Past the debounce, keep absorbing a false grip read as long as the hand is still at the dock (rack.in_range) -- a real release is almost always followed by the hand moving away, a sensor glitch isn't. Hard ceiling so a genuine release with the hand left resting nearby still eventually aborts.`

> `-- 2026-08-14: player deliberately held LG for ~2s past RG's release, twice, and the log both times showed is_left_grip_active() reading false the instant RG let go -- real data, not a guess, so treat it as real: likely a brief genuine dip in the LG reading (analog grip easing slightly, or an OpenXR/binding quirk) exactly during the RG-release motion, even though the hold felt continuous. Debounce: only actually clear the latch after LG has read released for a sustained 0.15s, not on a single frame's reading.`

### The undebounced visual gate (a SECOND instance of the same bug class)
> `-- 2026-08-14: found live -- same bug CLASS as the 2026-08-08 slide-rack visual-loop fix ..., on a DIFFERENT signal that fix never covered. Player held LG continuously for ~10s and still saw the hand looping. update_slide_rack_trigger() reads its OWN separate grip sample and debounces it (grip_release_debounce_sec/hard_cap) before letting a release abort rack.active -- but tick_hand_dock_blend() below (via should_hand_dock()) was being fed this RAW, per-frame 'grip' local with no debounce at all, so a single noisy false frame eased the visual dock blend out and immediately back in, looping the hand even while rack.active stayed perfectly stable.`

> `-- Hysteresis: use a wider exit threshold than the entry threshold once already docked. should_hand_dock() gates the visual hand-dock blend directly on rack.in_range with no debounce of its own, so a hard single threshold let ordinary boundary noise flip it rapidly, easing the hand toward the dock and back over and over even while rack.active (the actual grab cycle) stayed perfectly stable.`

### The pose-based signal that read back its own output
> `-- 2026-08-14: raw RG button read, mirrors is_left_grip_active() exactly but for the right controller -- needed because is_weapon_grip_active() (used for the RG+LG latch's "is RG really held" check) turned out to never actually go false across an entire held-LG test that included a real RG release (confirmed live: method=frozen_latch_offset never once appeared in SUPPORT_HAND_DEBUG despite multiple ARM/hold/release cycles). Suspected cause: is_weapon_grip_active()'s underlying signals (is_support_hand_active/is_two_handed) likely reflect the character's CURRENT pose rather than the RG button directly -- and since this mod's own cosmetic system is what's now posing the left arm as two-handed, that pose-based check may be reading its own output back as "still two-handed" even after RG lets go. A raw button read sidesteps that entirely.`

### The near-zero-length direction vector
> `-- 2026-08-13: was measured relative to muzzle_pos (either direction tried) -- wrong reference point entirely. During a real grip the support hand sits only a few cm from the muzzle by design (e.g. wp1000's grip_back_from_muzzle_m persisted at 0.038m), so normalizing (hand - muzzle_pos) divides by a near-zero-length vector -- any sub-cm tracking jitter then dominates the result, which is what a player reported as the weapon "repelling" from the hand, snapping away the instant grip engaged, and an inverted hand-to-muzzle mapping (all symptoms of a noise-dominated direction, not a sign error). Fixed: measure from the rotation's actual pivot instead -- rh_post's own position ... is the right/trigger hand's grip point, always tens of cm from the left hand, so the vector is stable regardless of tracking noise. This also matches the real-world model: the barrel should point from the grip pivot through wherever the support hand actually is.`

### The spin bug and its guard
> `-- 2026-08-13: guard against the exact same ill-conditioned-axis failure class that caused the original spin bug ... if up_after_fwd_fix and desired_up end up nearly anti-parallel (large angle, small axis_len), the rotation axis is noise-dominated and can flip between two near-opposite results frame to frame. Skip the roll correction entirely that frame rather than risk it -- near-parallel (small angle) is always safe and unaffected by this guard, only the near-180-degree case is excluded.`

### The frozen rigid transform
> `-- 2026-08-14: while RG is released but the latch still holds (LG alone), use the WEAPON's own anchor point directly (re2_vr_cosmetic_dock.lua's __vr_grip_anchor_world_pos/_rot -- muzzle bone position + a fixed per-weapon offset, republished every frame a covered weapon is equipped, independent of this file's own now-superseded engagement logic) as the hand's target pose, instead of a frozen hand-relative RIGID transform (tried first, reverted here). The rigid-transform approach assumes the support hand rotates exactly like a rod fixed to the trigger hand's wrist -- only true very near the angle it was captured at; a real arm's elbow/shoulder reposition as the aim swings, so reapplying a frozen rotation across a large aim change increasingly diverges from a natural pose -- confirmed live: "the further right I aim, the more my left hand retracts toward my hip." The weapon anchor has no such problem: it's derived fresh every frame from the gun's own bone transform, so it rotates correctly with the gun at any aim angle.`

### Why hand tracking was abandoned for a latch
> `-- 2026-08-14: new approach, replaces the real-hand-tracked grip system entirely (re2_vr_cosmetic_dock.lua's proximity/anchor-based real grip, now disabled via its own enabled=false config) after the player found no amount of tuning real-world hand tracking made it feel non-janky or "really held" -- decided the position-tracked approach is fundamentally not going to feel good in this game. New idea: don't track hand position at all. Instead, LATCH the ALREADY-PROVEN native RG-driven cosmetic follow-weapon state (should_support_hand_follow_weapon, used for RG this whole time) so it survives RG being released, as long as LG stays held -- i.e. RG+LG held together "catches" the hand on the weapon, then letting go of RG alone keeps it there until LG itself is released. No position/distance math anywhere in this mechanism -- sidesteps the whole HMD-position-affects-detection problem that real hand tracking could never get past.`

> `-- A hand-tracked distance check can only ever measure proximity, never intent -- the same reason this mod's actual weapon gestures (pump-action, slide-rack) were long since converted from tracked-distance to discrete trigger presses.`

> `-- Gated on LG alone, not LG+RG -- RG is normally already held just for regular two-handed aiming, so requiring both would make this active almost the entire time the player aims rather than a distinct deliberate action.`

> `-- This is a ONE-TIME decision at the press edge, not continuous re-evaluation, which is what makes it reliable.`

> `-- 2026-08-14: real grip ... used to reverse the usual relationship here -- WEAPON's aim blended toward the real tracked left hand, left hand itself shown untouched or soft-snapped. Player reported that felt wrong (grip zone effectively tracked HMD position/gaze rather than the pump handle, no "really held" feel like RG, still janked while moving) and asked to try zero real-world-position dependency once gripped. Removed entirely ... blend_aim_toward_left_hand and the visual-snap code are left defined but unused (not deleted) in case this doesn't pan out and needs reverting.`

> *(why the latch ships off)* `-- 2026-08-14: RG+LG support-hand latch. Defaults to DISABLED -- engagement and hold-through-movement are confirmed working, but one bug never got resolved: the weapon's aim can briefly point the wrong way (up to fully backwards) if the player looks far off-axis while gripped. Opt-in for testing only until that's fixed and confirmed non-janky.`

> *(the diagnostic that set two_hand_aim_blend to 0.5)* `-- 2026-08-13: temporarily lowered from 1.0 to test whether forcing a full-commit rotation correction every single frame is what's destabilizing the native IkArmFit solver's own position solve (real grip's raw_target position jumps ~0.4m/frame despite the real hand being stationary and recoil.lua never touching position itself...). If lowering this calms the position jitter, that confirms rotation-forcing is feeding back into position; if not, this isn't the mechanism.`

### Why the raw input bit could not be cleared
> `-- the native "SUPPORT_HOLD" input flag (InputSystem's ButtonBits) triggers off LG's press and persists/re-asserts the sub-weapon in hand for as long as LG stays held. An earlier version of this script tried to clear that raw input bit directly every frame, but that turned out to be a losing race against native per-frame recomputation (confirmed live: our clear landed, but the native ready-state still flipped true moments later on the very same interaction). This version corrects the RESULT instead of trying to preempt the input.`

> `-- LG+RG is ambiguous on its own -- it's both "add support grip to my main weapon" AND the native grenade-throw gesture (arm/throw whatever's already readied in the off-hand via LG). The two are told apart by ENGAGEMENT ORDER: RG-first-then-LG-joins means two-handing the main weapon (suppress SW); LG-already-held-when-RG-engages means the player deliberately readied a throwable and is now interacting with it (must NOT be interrupted). This is decided once, at the moment RG's rising edge occurs, and stays fixed for that whole RG hold regardless of what LG does afterward.`

### The attachment-stripping bug
> `-- Found via dump_equipment_parts_fields: Equipment has a paired <ForceEquipParts>k__BackingField alongside ForceEquipType (matches setForceEquipType's real signature taking both a WeaponType and a WeaponParts). Forcing WeaponType alone was what stripped attachments; forcing both together should preserve them.`

> `-- forcing ForceEquipType alone strips attachments, since WeaponType only identifies the base weapon, not its parts (the muzzle-stripping bug from before -- tolerable there since it was a brief pulse, not tolerable here since we hold it for the whole RG-held duration).`

### The mag-pouch rework
> `-- 2026-08-18: the hip/shoulder/chest anchor entries are gone -- the ammo pouch is HMD-anchored now (see resolve_mag_holster_anchor), matching re2_vr_holster.lua's weapon-holster rework. Only the wrist joints remain (hand-position sources).`

> `--- HMD anchor with a YAW-ONLY basis (2026-08-18) -- same design as re2_vr_holster.lua's get_hmd_pose_yaw, replacing the old r_Prop_Hip_A/spine_0 joint anchor (the skeleton twists with animation, which made body-anchored zones swing around the player's real body). Position = VR camera world position; axes = camera forward projected onto the horizontal plane (up = world up), so head pitch/roll can't rotate the offsets. ... Degenerate headings (looking straight up/down) hold the last-good heading via the mag_holster_st.hmd_fx/fz cache instead of flipping.`

> `-- Capture basis MUST match the runtime basis exactly, or a calibrated offset means something different at capture time vs. in play.`

> `-- One-shot migration: persisted ammo-pouch offsets from the joint-anchored system are per-skeleton values in a different basis -- reset them (and only them) to the new HMD-basis defaults. Trigger/release distances are arm-reach based and kept.`

> `-- Require a genuinely FRESH grip press while already in the zone, not merely "ready" transitioning true because zone-entry coincided with a grip that was ALREADY held (from two-handing the shotgun's foregrip with the same left-grip button). Without this, sweeping that already-gripping hand through the holster zone auto-grabbed a shell unintentionally (player report: two-handing + moving the left hand near the ammo holster auto-picks-up a shell). Seed the tracker with the current grip state on the very first in-zone frame so an already-held grip is correctly treated as "not fresh".`

### Delayed shell eject — full root cause
> `-- Root cause found via re2_vr_shell_eject_probe.lua (live hook tracing): firing calls Equipment.requestFire -> shortly after, ShellCartridgeController.request() -> generate(). generate() is the actual casing spawn -- confirmed by blocking it entirely: ammo still decremented, sound/recoil still played normally, only the casing prop stopped appearing. So generate() is purely cosmetic and safe to gate.`

> `-- Approach: pre-hook generate(), and for manual-pump shotguns only, return SKIP_ORIGINAL and just remember that a shell is owed for this weapon (NOT the ShellCartridgeController instance itself -- holding a reference across the delay until the pump-pull risks it going stale).`

> `-- ShellCartridgeController extends via.Behavior (has startCoroutine/registerCoroutine). request()+generate() are ~13ms (about 1 frame) apart in the natural fire sequence, which matches request() kicking off a coroutine that calls generate() on a LATER frame rather than doing so synchronously. First attempt (calling generate() alone) produced no visible casing; second attempt (calling request() then immediately also calling generate() ourselves, resetting the guard right after) produced a second "Delaying shell eject" log ~25ms later -- almost certainly request()'s own coroutine calling generate() on a subsequent frame, after our guard had already reset, so our hook caught it again as if it were a new fire event. Fix: call ONLY request() (let its own coroutine drive generate() naturally, exactly like the real fire sequence), and hold the guard open for several frames instead of resetting it synchronously.`

> `-- Bug fixed here: this used to be gated behind an early return when pulled_down_wp was nil (i.e. almost every frame), so the countdown below never actually decremented past its first tick -- it got stuck permanently above zero, which permanently disabled the generate() hook's suppression after the very first pump cycle (every later shot ejected immediately again). The countdown must run unconditionally, every frame, regardless of whether a pump-pull happened this frame.`

### The lookalike-systems trap (three separate incarnations)
> `-- This is the ACTUAL bullet-insertion dock point (measured against in measure_bullet_insert_local_distance) -- not bullet_joint_by_wp/bullet_dock_offset_by_wp, which only affect extracting a spent shell, a separate feature. wp0800 had no entry here at all before, so it was using mag_exit_default verbatim. Starting from that default and nudging left/up/toward-muzzle per the player's ask, reusing the same sign convention already confirmed for this weapon (+x = left, +z = toward muzzle) -- not yet confirmed this local space shares that convention, needs a live test.`

> `-- Missing entirely before this -- fell back to the hardcoded "_04" default (see get_bullet_joint's fallback chain in re2_vr_reload_ext_3.lua), which isn't the cylinder joint on this weapon's mesh, unlike wp0300/wp3200 where "_04" happens to be correct for their own meshes. -- Attempt 1: "_01" (cylinder_joint_by_wp's value, the pivot joint) -- tested, still landed toward the back of the gun. Makes sense: a cylinder's pivot/crane attaches at the rear of the frame, not the drum face. -- Attempt 2: one of the individual chamber-slot joints instead (bullet_chamber_nodes_by_wp.wp0800 = "_10".."_15") -- these should be the actual bullet positions on the drum face itself.`

> `-- REVERTED (2026-08-07): this whole detour was tuning a code path SLS60 doesn't actually use for bullet insertion -- get_bullet_chamber_nodes(wp0800) being non-nil means rv.target resolves via get_hand_bullet_node_name's chamber-node branch, never touching bullet_joint_by_wp at all. Worse, since this offset applied unconditionally to rv.rest_x/y/z regardless of which resolution path set rv.target, it was corrupting the TRUE chamber joint position that the insert animation fix below now relies on. Left at zero going forward; the real fix for both the grab-point and the visual-landing-spot issues lives in mag_exit_by_wp (JSON, tuned live) and begin_bullet_insert_anim's chamber-aware end target.`

> `-- Weapons with per-chamber joints (SLS60-style, get_bullet_chamber_nodes ~= nil) need the animation to land ON the actual chamber -- rv.rest_x/y/z, the joint's true rest position captured at refresh time -- not at mag_exit, which is the ergonomic hand-reach/grab-detection point (tuned separately) and generally isn't anywhere near the chamber hole itself. Other weapons (mag-fed, no chamber nodes) keep using mag_exit, since for those it's already at/near the real insertion point.`

### The identical-hands source bug
> `-- vrmod:get_controllers() + get_position FIRST: the proven per-hand path (ext_1 and re8_vr both use it successfully). The VRControllerManager list was tried as the primary source and produced IDENTICAL positions for both entries in live 2026-08-17 logs (relative-hands s == 0.0000 exactly), so it's demoted to a fallback with a zero-vector guard.`

> `-- Self-diagnosing guard for the exact failure seen twice on 2026-08-17: a position source returning the SAME point for both hands makes the relative pull identically zero -- flag it loudly instead of silently measuring nothing.`

> `-- Must forward the hand arg: ext_1's export defaults nil to "left", so a no-arg wrapper here silently serves the LEFT controller for BOTH hands of ext_2's relative-hands gesture.`

### The skeleton-joint outlier
> `-- The animated hand_skeleton joint (track_pos above) can glitch to an outlier value on individual frames (confirmed via track_probe logging: spikes to ~3x neighboring samples while the raw controller position stayed smooth). Including the raw, properly world-transformed controller position here means a single bad skeleton-joint frame can't block a grab when raw tracking agrees the hand is actually at the dock.`

### LT owns the return stroke / hand detaches at the release edge
> `-- LT owns the return stroke it commanded: once an LT-committed pull is released, the spring must run home unopposed. The left hand is still physically back at the slide (LG held), so without this gate the motion ratio -- noisy EMA hovering near full pull -- re-raises the decaying travel every time tracking noise crosses it, and the slide visibly stalls or backtracks on its way home (player-reported on Matilda, 2026-08-18). Motion-committed pulls (trig_committed false) keep the hold-and-ride-home behavior.`

> `-- Let go of the slide at the bottom of the pull, right here, instead of waiting for the slide to finish easing back to rest. Real behavior: player releases LT, hand comes off immediately, the slide snaps forward on its own (like a recoil spring) rather than looking like the hand is still carrying it forward. Only the VISUAL hand-dock detaches early -- rack.active/trig_travel keep running exactly as before, so the completion SFX/haptic/chamber finalize in complete_rack_cycle() still fire once the slide actually reaches rest, just without the hand attached for it.`

> `-- Latched pull: detach the hand visual the INSTANT the grip reads released -- ahead of the debounce, zero latency. Same cure the LT path already got. A spurious one-frame grip blip at full pull costs only the hand visual, nothing mechanical.`

> `-- update_slide_rack_trigger() is the sole owner of the joint's local_z for the duration of an active trigger-rack cycle -- this function's hand-tracked/push-delta math below (which reads real controller position once rack.pull_done is true) was still running unconditionally for these weapons too, fighting the trigger-driven writes every frame (confirmed live: caused both the pull snapping instead of easing, and the slide moving with raw hand motion while LT was held).`

### Menu / camera crash storm
> `-- get_GameObject on the camera throws internally while menus (item box etc.) have it in a transitional state; sc() swallows the Lua error but REFramework logs each throw -- polled every motion tick that produced a 300+/sec on-screen error storm. Back off after a failure instead of retrying every call.`

### Stale globals
> `-- __vr_lh_joint_pos is only kept fresh while __vr_lh_slide_ik_override is true (the slide-dock system never clears it afterward), so it's only trustworthy while that flag says docking is actually live.`

### Ordering
> `-- Must clear before chamber restore (mag insert also clears; canister insert does not).`
> `-- Do NOT reset last_physics_frame here: UpdateMotion may have already run process_recoil_physics for this frame. Resetting would bypass the gate and call update_recoil a second time, over-advancing spring physics every frame.`
> `-- Grace/sub-weapon suppress must not block pump forend arm IK while needs_pump.`
> `-- Empty-chamber rack workflow: slide must stay open until manual rack completes.`
> `-- Block native reload only; do not end-reload after block (clears manual chamber commits).`
> `-- Shell/revolver holster: proximity alone must not break shoot-ready.`
> `-- Zero Gun.getBulletNumber while rounds live in mag.carried_rounds.`
> `-- Restore virtual mag rounds to chamber without consuming reserve.`
> `-- Spare rounds in HUD not yet in reloadable pool after save load.`
> `-- HUD loaded / carried (shotgun reserve box): IMgr remaining=loaded, reloadable=carried.`
> `-- Seated X rail; Y+Z from magwell exit (matches drop pitch) into rest pose.`
> `-- After save/session reset the game may report chamber loaded while mod state still has chamber_extracted.`
> `-- Per-chamber joint scale hiding (wp0800-style); takes priority over mesh-part indices.`
> `-- Keep insert_chamber_slot during insert anim so other rounds stay visible.`

### Runtime warnings that encode failure modes worth reproducing
> `"[re2_vr_reload] Revolver bullet node '%s' not found on %s"`
> `"[re2_vr_reload] clear_weapon_chamber_ammo touched reserve %d -> %d"`
> `"[re2_vr_reload] Could not clear weapon chamber (%d rounds)"` · `"Could not apply %d rounds to chamber"`
> `"[re2_vr_reload] Shell +1 commit failed loaded %d->%d carried %d->%d bullet_id=%s"`
> `"[re2_vr_reload] carried restore failed; skipping reserve top-up"`
> `"[re2_vr_reload] Reload suppression hooks not ready yet"` · `"SurvivorCondition type missing — reload IK spoof unavailable"`
> `"[re2_vr_delayed_shell_eject] Overwriting a still-pending shell eject (never got pumped) for wp=..."`
> `"[re2_vr_delayed_shell_eject] Could not fetch fresh ShellCartridgeController, shell eject lost"`

### Bugs the port should FIX rather than reproduce
These are real defects verified in source, none of them commented:
1. **`should_spoof_reload_state()` (line 1512) references `reload_pump`, `reload_revolver`, `reload_mag` — declared `local` at lines 1532–1538, *after* it.** Inside that function they compile as globals, which are never assigned. So the `get_IsReload` spoof only ever fires via `frame.vr_reload_b`; the pump-spoof, revolver-blocks-empty, mag-out-of-gun and mag-anim-active branches are **all dead**. The function also lacks a final `return false` and falls through returning `nil`. (Verified directly: function body 1512–1530, declarations 1532–1538.)
2. `init_reload_stack` passes `manual_reload_session_active` (line 1723) but the local is defined at 2250 — **ext_2 receives `nil`**.
3. `draw_weapon_mode_toggle` (1482) calls `update_manual_reload_globals()` defined at 2445 — toggling any per-weapon checkbox in ImGui errors, the master dispatcher's `pcall` swallows it, and every UI callback ordered after 41 is skipped that frame.
4. `tick_reload_hooks_install` (2854) calls `install_isreload_hook()` defined at 2858 — first `on_frame` throws if hooks weren't installable at load. (The top-level call at 3047 works because it is textually after the definition.)
5. `hook_methods_by_prefix` uses `mname:find("^"..prefix, 1, true) == 1` — `plain=true` makes `^` literal, so **only the exact-name arm ever fires**. The "prefix" hooks are exact-name hooks.
6. `strip_weapon_anim_input_bits` vs `strip_input_mask` read `Down/On/Up` off **different objects** (see §A.5).
7. Both `Gun:requestFire` and `Equipment:requestFire` share one `on_pre_fire`/`on_post_fire` closure pair and one `fire_request_blocked` upvalue — sequential calls clobber it.
8. `sync_canister_bullet_visual` (ext_3:2121) is an accidental **global** — no `local` anywhere — called 700 lines earlier at 1403. Works only via `_G`, and leaks into the global namespace.
9. `weapon_entry` is defined **twice** in ext_3 (lines 190 and 1310); the second shadows the first and drops its `if not wp` guard.
10. Gesture windows and cooldowns use `os.clock()` (wall clock) throughout — **pausing the game does not pause them.**
11. `ensure_weapon_setup` (1174) forward-references `weapon_needs_manual_cylinder_reload` (1345) and `weapon_reload_drop_slots` (1333) — dormant only because `ensure_weapon_setup` is never called.
12. `mag_anim_duration("slide")` has **no `"slide"` branch** and falls through to the `drop_sec` default (0.18 s). `[inferred]` intentional; make it explicit in the port.
13. `sync_mag_holster_left_support_suppress(want)` only sets `mag_sub_suppress.active`, which nothing reads except `force_clear_*` — the `on_mag_holster_suppress_change` dep is effectively a no-op sink.

**Dead code to skip:** `mesh_type`, `get_world_state_fingerprint`, `apply_tuning_snapshot`, `setup_ui`, `reload_activity_active`, `should_block_native_ammo_slot`, `should_play_shell_empty_dry_fire`, `ensure_weapon_setup`, `tuning_snapshot` (written, never read), `abort_blocked_native_reload` (body is comments only), `cylinder_open_angle_by_wp` (declared, never read — open pose comes from `slide_dock.slide_bind_by_wp`), `anim.fall_local_y` (read only by `mag_fall_local_offset_y()`, which is never called), `tick_dbg_joint_stick_check` (detection body is empty: `if gun_move > 0.02 and joint_move < 0.005 then end`), `manual_pump.pump_bind_by_wp` (the pump reads `slide_dock.slide_bind_by_wp` instead), `PLAYER_JOINT.right_wrist` in ext_1 (declared, never read), `bullet_insert_preview_remaining` (stored into `deps`, never called), `__vr_pump_pull_axis_x/y/z` (cleared, never set).

### Lua-only constraints — do NOT port
Several files sit at Lua's **200-top-level-locals-per-chunk ceiling** (`re2_vr_recoil.lua` reached 193/200), which is why state is folded into tables (`frame_ik`, `slide`, `rack`, `gesture`, `pump_ease`, `trig_rack_ease`, `MAG_HOLSTER_DEF`). In C++ the constraint vanishes — but the grouping it produced is a reasonable struct layout to keep.
> `-- Grouped into one table (instead of 4 separate top-level locals) to stay under this file's 200 local-variable ceiling.`

### Case-study lessons that generalise (from `modding-notes/case-studies/`)
- **`2026-07-26-pump-trigger-reload-conversion.md`** — split a VR gesture into "needs real tracking" and "binary state dressed up as a gesture"; replace only the unreliable half with a discrete input, and **write into the same state fields the existing downstream code already consumes**. That reuse is what made the pattern portable to the slide rack.
- **`2026-07-31-slide-rack-teleport-duplicate-hooks.md`** — three unrelated root causes of one visual teleport: duplicate per-frame hook sites racing on the same output; an older sibling function missing a guard its siblings had; state reset on only one of two cycle-end paths. *"when porting a working pattern to reuse existing lower-level plumbing, audit every other place that plumbing gets written to or reset, not just the places that read it."*
- **`2026-08-07-three-position-systems-one-weapon.md`** — one UI feature backed by three independent position systems (grab-from / insert-to / visual-landing). *"A log statement that never fires is data."* And: *"A config value with no entry for the thing you're testing might be silently using a shared default that was only ever validated for something else."*
- **`2026-08-08-edge-trigger-no-retry-and-a-controller-blip.md`** — the grab check required grip rising-edge AND in-range in the *same* frame, with no retry. Found by grepping a flag for **read** sites (ten writes, zero reads = decoration, not logic). Fix: level-triggered while the gesture hasn't started.
- **`2026-08-09-inhibit-is-not-request.md`** — *"'Permit/block' and 'request/cause' are different capabilities on the same underlying state."* `setInhibitPetient` cannot manufacture a request. And: *"Finding 'a' caller of the thing you want to control is not the same as finding 'the only' caller, or 'the last' one — intercept the setter itself."*
- **`2026-08-09-a-fix-that-kept-reverting-itself.md`** — a character-profile lookup silently routed Claire's saved data into Leon's block; bit-for-bit identical reverts are the tell for a stale in-memory copy being re-serialised, not a new corruption event.
- **`2026-08-12-from-proximity-to-a-real-two-handed-grip.md`** — the full arc of Feature A, ending in: *"when a feature keeps generating a new plausible-sounding bug every time the last one gets fixed, and each individual fix is clean and well-reasoned, that pattern is itself a signal ... Reusing an existing, already-correct system by extending when it applies can beat rebuilding an equivalent system from lower-level primitives."*
- **`2026-08-15-deleting-a-feature-without-breaking-its-neighbor.md`** — shared hooks acquire second jobs; read the whole hook body before deleting anything.
- **ENGINE-DOSSIER §5** — *"Same-frame ordering is a real class of bug ... Measure, don't guess: sample the same value at an early and a late hook in one frame and compare."*
- **ENGINE-DOSSIER §9** — *"A single weapon can carry multiple similarly-purposed config tables for what looks like one feature, and tuning the wrong one throws no error and no crash — just silence."*

---

# E. Per-weapon variation

## E.1 The live enabled set (repo + shipped JSON are identical here)

| wp | label | live `enabled` | mode flags |
|---|---|---|---|
| wp0000 | Matilda | ✔ | `trigger_slide_rack` |
| wp0100 | M19 | ✔ | |
| wp0200 | JMB Hp3 | ✔ | |
| wp0300 | Quickdraw Army | ✘ | `needs_manual_cylinder_reload`, `needs_manual_pump`, `no_slide_rack_required`, `cylinder_close_gesture=false` |
| wp0400 | Glock 17 | ✔ | |
| wp0600 | MUP | ✔ | |
| wp0700 | Broom Hc (Ada) | ✔ | `trigger_slide_rack` |
| wp0800 | SLS 60 | ✔ | `needs_manual_cylinder_reload`, `no_slide_rack_required`, `cylinder_close_gesture_right=true`, `_up=false` |
| wp1000 | W-870 | ✘ | `block_native_pump`, `needs_manual_pump`, `needs_manual_shell_reload`, `no_slide_rack_required` |
| wp1100 | Remington 870 | ✘ | same |
| wp1200 | M3 Shotgun | ✘ | same |
| wp1300 | GM 79 | ✘ | `block_native_pump`, `needs_manual_pump`, `no_slide_rack_required` — **no shell reload** |
| wp1500 | Lightning Hawk (shotgun) | ✘ | same as wp1000 |
| wp2000 | MQ 11 | ✔ | |
| wp2200 | LE 5 | ✔ | |
| wp3000 | Lightning Hawk | ✔ | `trigger_slide_rack` |
| wp3200 | Chief Irons Revolver | ✘ | `no_slide_rack_required` |
| wp4100 | GM 79 (launcher) | ✘ | `reload_drop_slots=1`, `bullet_follows_cylinder_open`, `keep_spent_chamber_bullet`, `needs_manual_cylinder_reload`, `cylinder_close_gesture`, `_up=true` |
| wp4200 | Chemical Flamethrower | ✔ | `canister_mag_reload`, `needs_manual_cylinder_reload`, `keep_spent_chamber_bullet` |
| wp4300 | Spark Shot | ✔ | `block_native_pump`, `needs_manual_pump`, `no_slide_rack_required` |
| wp4400/4600/4700 | ATM-4 / AT Rocket / Minigun | ✘ | label only, no manual reload |
| wp7000 | Samurai Edge | ✔ | |
| wp7010/7020/7030 | Samurai Edge Chris/Jill/Albert | ✘ | mirrored from wp7000 by `migrate_cfg`, once, stamped `samurai_edge_rack_migrate_rev = 1` |

**The Lua `DEFAULT_CFG.weapons` table, verbatim** (seed data — the JSON above overrides it):
```lua
wp0000 = { enabled = true,  label = "Matilda", trigger_slide_rack = true },
wp0100 = { enabled = true,  label = "M19", trigger_slide_rack = true },
wp0200 = { enabled = true,  label = "JMB Hp3", trigger_slide_rack = true },
wp0300 = { enabled = false, needs_manual_cylinder_reload = false, no_slide_rack_required = true, label = "Quickdraw Army" },
wp0400 = { enabled = false, label = "Glock 17", trigger_slide_rack = true },
wp0600 = { enabled = false, label = "MUP", trigger_slide_rack = true },
wp0700 = { enabled = true,  label = "Broom Hc", trigger_slide_rack = true },
wp0800 = { enabled = false, label = "SLS 60", chamber_grab_dist = 0.5 }, -- default is 0.28; roughly doubled as a forgiving-radius test
wp1000 = { enabled = false, label = "W-870", block_native_pump = true, needs_manual_pump = true, needs_manual_shell_reload = true, no_slide_rack_required = true },
wp1100 = { enabled = false, label = "Remington 870", block_native_pump = true, needs_manual_pump = true, needs_manual_shell_reload = true, no_slide_rack_required = true },
wp1200 = { enabled = false, label = "M3 Shotgun", block_native_pump = true, needs_manual_pump = true, needs_manual_shell_reload = true, no_slide_rack_required = true },
wp1300 = { enabled = false, label = "GM 79", block_native_pump = true, needs_manual_pump = true, no_slide_rack_required = true },
wp1500 = { enabled = false, label = "Lightning Hawk (shotgun)", block_native_pump = true, needs_manual_pump = true, needs_manual_shell_reload = true, no_slide_rack_required = true },
wp2000 = { enabled = false, label = "MQ 11", trigger_slide_rack = true },
wp2200 = { enabled = false, label = "LE 5", trigger_slide_rack = true },
wp3000 = { enabled = true,  label = "Lightning Hawk", trigger_slide_rack = true },
wp3200 = { enabled = false, needs_manual_cylinder_reload = false, no_slide_rack_required = true, label = "Chief Irons Revolver" },
wp4100 = { enabled = false, label = "GM 79", reload_drop_slots = 1, bullet_follows_cylinder_open = true, keep_spent_chamber_bullet = true },
wp4200 = { enabled = false, label = "Chemical Flamethrower" },
wp4300 = { enabled = false, label = "Spark Shot" },
wp4400 = { enabled = false, label = "ATM-4" },
wp4600 = { enabled = false, label = "Anti-Tank Rocket" },
wp4700 = { enabled = false, label = "Minigun" },
wp7000 = { enabled = false, label = "Samurai Edge", trigger_slide_rack = true },
wp7010 = { enabled = false, label = "Samurai Edge (Chris)", trigger_slide_rack = true },
wp7020 = { enabled = false, label = "Samurai Edge (Jill)", trigger_slide_rack = true },
wp7030 = { enabled = false, label = "Samurai Edge (Albert)", trigger_slide_rack = true },
```

**Flag semantics:** `enabled` = mag-holster + slide-rack manual reload (the "top checkbox") · `trigger_slide_rack` = slide rack driven by the left **trigger** (the confirmed-working path) · `no_slide_rack_required` = no rack step after inserting · `needs_manual_cylinder_reload` = revolver path (also switches close SFX to `pump_fire`) · `block_native_pump` = suppress the vanilla pump animation/audio · `needs_manual_pump` = player must pump manually · `needs_manual_shell_reload` = shell-tube loading · `chamber_grab_dist` = per-weapon override of `revolver_reload.chamber_grab_dist_default` (0.28) · `reload_drop_slots` ≤1 ⇒ "single_shot" mode · `bullet_follows_cylinder_open` = chamber bullet tracks the cylinder swing · `keep_spent_chamber_bullet` = spent case stays in the chamber · `canister_mag_reload` = whole-canister reload (JSON-only) · `needs_manual_revolver_reload` = legacy alias, migrated by `migrate_cfg` · `cylinder_close_gesture` / `_right` (default on) / `_up` (default off) · `use_chamber_joint_hiding`.

Note the two `"GM 79"` labels: **wp1300** (pump shotgun family) and **wp4100** (break-action launcher). `wp1500` is `"Lightning Hawk (shotgun)"`, `wp3000` is `"Lightning Hawk"`.

`wp8400` (ATM-4 Unlimited) and `wp8700` (Minigun Unlimited) exist **only** in `re2_vr_recoil.lua`'s `WEAPON_CATALOG` and `re2_vr_cosmetic_dock.lua`'s weapon list. **No pump, no slide, no bind pose, no node name, no reload entry.**

## E.2 Joint-name tables (LIVE JSON values — the ones that run)

```
slide_node_by_wp   = { wp0000:_01  wp0100:_01  wp0200:_01  wp0300:_01  wp0400:_01
                       wp0600:_01  wp0700:_01  wp0800:_01  wp1000:_01  wp2000:_01
                       wp2200:_02  wp3000:_01  wp4100:_01  wp4200:_05  wp4300:_01  wp7000:_01 }
mag_node_by_wp     = { wp0000:_04 wp0100:_04 wp0200:_04 wp0300:_03 wp0400:_01 wp0600:_04
                       wp0700:_04 wp0800:_17 wp1000:_04 wp2000:_04 wp2200:_04 wp3000:_04
                       wp3200:_04 wp4100:_04 wp4200:_04 wp4300:_04 wp7000:_04 }
pump_node_by_wp    = { wp0300:_02  wp1000:_01 wp1100:_01 wp1200:_01 wp1300:_01 wp1500:_01 }
cylinder_joint_by_wp = { wp0300:_04  wp0800:_01  wp3200:_04  wp4100:_01 }
bullet_joint_by_wp   = { wp0300:_04  wp3200:_04  wp4100:_04  wp4200:_04 }   (Lua default has wp0800:_10)
shell_joint_by_wp    = { wp1000:_04 wp1100:_04 wp1200:_04 wp1500:_04 }
bullet_barrel_node_by_wp   = { wp0300:_16  wp0800:_17 }
bullet_chamber_nodes_by_wp = { wp0300 / wp0800 / wp3200 : ["_10","_11","_12","_13","_14","_15"] }
cylinder_open_angle_by_wp  = { wp0300 / wp3200 : { chamber:-30.0, swing:-90.0 } }   ← DEAD, never read
mesh_parts_by_wp   = { wp0200:[2]  wp2000:[1] }
shell_mesh_parts_by_wp = { wp1000/1100/1200/1500 : [31] }
bullet_mesh_parts_by_wp = { wp3200 : [10,11,12,13,14,15] }
rack_kind_by_wp    = { wp1000/1100/1200/1300/1500 : "pump" }
```

**Lua `DEFAULT_CFG` versions, verbatim** (differ where noted in §0):
```lua
mag_node_by_wp = { wp0000="_04", wp0100="_01", wp0200="_04", wp0300="_04",
                   wp0400="_01", wp0600="_01", wp3200="_04" }
slide_dock.slide_node_by_wp = { wp0100="_02", wp0000="_01", wp0200="_01",
                                wp0300="_04", wp3200="_04",
                                wp1000="_01", wp1100="_01", wp1200="_01",
                                wp1300="_01", wp1500="_01" }
manual_pump.pump_node_by_wp = { wp1000="_01", wp1100="_01", wp1200="_01",
                                wp1300="_01", wp1500="_01" }
cylinder_joint_by_wp = { wp0300="_04", wp3200="_04", wp0800="_01" }
bullet_joint_by_wp   = { wp0300="_04", wp3200="_04", wp0800="_10" }
bullet_dock_offset_by_wp = { wp0800 = { x=0.0, y=0.0, z=0.0 } }   -- zeroed after the 2026-08-07 revert
shell_joint_by_wp      = { wp1000="_04", wp1100="_04", wp1200="_04", wp1500="_04" }
shell_mesh_parts_by_wp = { wp1000={31}, wp1100={31}, wp1200={31}, wp1500={31} }
shell_spawn_by_wp      = {}
mesh_parts_by_wp = { wp0200 = { 2 }, wp2000 = { 1 } }
```

## E.3 Travel geometry — the sign inversion

```
slide_bind_default = { x:0, y:0, rest_z:0.06, parked_z:0.04, back_z:0.015,
                       open_rot_pitch:0.0, open_rot_yaw:0.0, open_rot_roll:0.0 }
  wp0100 = { y:0.0, rest_z:0.06, parked_z:0.04,  back_z:0.015 }
  wp0000 = { y:0.0, rest_z:0.06, parked_z:0.038, back_z:0.013 }
  wp0200 = { y:0.0, rest_z:0.06, parked_z:0.043, back_z:0.018 }
  wp1000/1100/1200/1300/1500 = { rest_z:0.0, parked_z:0.0, back_z:-0.08 }
pump_bind_default  = { rest_z:0.0, parked_z:0.0, back_z:-0.08 }
pump_bind_by_wp    = {}   -- EMPTY and effectively dead; the pump reads slide_bind_by_wp
```
Pistol slides travel **+Z** (rest 0.06 → back 0.015, ~25 mm throw / 45 mm return); shotgun pumps travel **−Z** (0.0 → −0.08, 80 mm both ways). `M.gesture_motion_ratio` derives `s_sign = sign(back_z - parked_z)` so no per-weapon convention is hardcoded — **port the derivation, not the sign.** (Both families happen to come out negative here; the mechanism is per-weapon ground truth regardless.) `bind_travel_axis_from_entry` supports `entry.travel_axis` / `entry.bind_travel_axis` (`"y"` selects Y, anything else Z); **no default weapon sets it.**

**Critical porting note:** `get_pump_bind_pose(wp)` in ext_4 delegates to `slide_gesture.gesture_get_bind_pose` = ext_2's `get_slide_bind_pose` — so the pump reads the **`slide_dock`** bind table, not `manual_pump.pump_bind_by_wp`. `manual_pump.pump_bind_default` is only reachable through ext_4's hardcoded local fallback `{x=0, y=0, rest_z=0, parked_z=0, back_z=-0.08}` when `slide_gesture` is nil. The two happen to agree.

`slide_dock_default = { dock_off_x/y/z = 0.0, hand_pos_x/y/z = 0.0, hand_rot_pitch/yaw/roll = 0.0 }`. `slide_dock_by_wp`, `no_rack_required_by_wp`, `dock_dist_by_wp`, `slide_ik_twist_by_wp` all **ship empty in Lua** and are populated entirely from JSON by live tuning. Sample live values: `slide_dock_by_wp.wp0000 = { dock_off_x 0.086, dock_off_y 0.037, dock_off_z -0.049, hand_pos 0/0/0, hand_rot_pitch 8, hand_rot_yaw 180, hand_rot_roll 0 }`; `wp0300` adds `hand_pos -0.012/0.056/0.037`, `pitch -9.0`, `yaw 167.0`, `pull_dist 0.018`, `push_dist 0.012`; `wp0700` `hand_pos -0.002/0.0/0.05`, `roll 1.0`, `yaw 165.0`; `wp1000` `dock_off -0.059/0.041/-0.043`, `hand_pos 0.124/-0.068/0.007`.

## E.4 Other per-weapon values

`dock_dist_default` **0.15** (live JSON); overrides `{wp0600:0.2, wp0700:0.15, wp2000:0.2, wp2200:0.2, wp3000:0.2, wp4200:0.2, wp4300:0.2, wp7000:0.2}`.
`no_rack_required_by_wp` (live JSON) = `{wp0300, wp0800, wp1000, wp1100, wp1200, wp1300, wp1500, wp4100, wp4200, wp4300}` all true.
`mag_dock_default` **0.20**; `mag_dock_by_wp = { wp0700 = 0.06 }` — *"Ada's Broom Hc (wp0700) mag/hand model is smaller than the other characters' -- tuned live down from 0.10 through 0.08 to 0.06 (see re2_vr_ada_chapter_status memory), but only the first guess (0.10) had ever made it into this default. Corrected here to match the value actually live in re2_vr_reload.json."*
`mag_exit_default = { x:0.0, y:-0.103, z:-0.035, pitch:0, yaw:0, roll:0 }`; `mag_exit_by_wp = { wp0000:{x:0.0, y:-0.11, z:-0.04}, wp0800:{x:0.03, y:-0.083, z:-0.015} }`. **Inconsistency:** `get_mag_exit_pos()` defaults `y` to `mag.rest_y + drop_local_y (-0.12)` when unset, but `ensure_mag_exit_entry()` seeds a new entry with the literal `-0.103`.
`shotgun_shell = { enabled true, dock_dist_default 0.22 m, dock_cooldown 0.45 s, shell_hand {ox,oy,oz,yaw,pitch,roll all 0} }`; shell dock-refresh retry 0.5 s.
`SHELL_CAPACITY_FALLBACK = { wp1000:5, wp1100:6, wp1200:7, wp1500:2 }` (default 5).
`REVOLVER_CAPACITY = { wp0300:6, wp3200:5, wp0800:5, wp4100:1 }` (fallback 6).
`MANUAL_PUMP_SHOTGUNS = { wp1000, wp1100, wp1200, wp1500 }` — **`wp1300` is deliberately absent**, so the GM 79 pumps like a shotgun but its casing ejects on fire, not on the pull-down. It is also absent from `shell_joint_by_wp`, `shell_mesh_parts_by_wp` and `SHELL_CAPACITY_FALLBACK` — consistent, since a break-action grenade launcher has no tube.
`SHOTGUN_WP_DEFAULT_PUMP = { wp1000, wp1100, wp1200, wp1300, wp1500 }`.
`DEFAULT_LARGE_WEAPONS` (gates the left-arm two-hand stretch boost) = `wp1000, wp1100, wp3000, wp3200, wp4100, wp4400, wp4600, wp4610, wp4700` — merged, never replaced, by a JSON `large_weapons` array.
`COSMETIC_DOCK_WEAPONS` (live JSON) = `wp1000, wp1100, wp2000, wp2200, wp4100, wp4200, wp4300, wp4400, wp4600, wp4700, wp8400, wp8700`. The Lua defaults name each: `wp1000/wp1100` W-870 · `wp4100` GM 79 · `wp4400/wp8400` ATM-4 (+unlimited) · `wp4600` Anti-Tank Rocket · `wp4700/wp8700` Minigun (+unlimited) · `wp2000` MQ 11 · `wp2200` LE 5 · `wp4200` Chemical Flamethrower · `wp4300` Spark Shot. Fully **replaced** (not merged) if JSON supplies a `weapons` array.
`WEAPON_NO_RECOIL_IDS` = `wp0001, wp0500, wp1001, wp4000, wp4110, wp4310, wp4500, wp4510, wp4530, wp4701, wp4901, wp6200, wp6202, wp6300, wp6301`.
`weapon_sfx = { enabled true, master_volume 1.0, spatial true, default_fallback_wp "wp1000", volume_by_kind {}, by_wp {} }`.

## E.5 Per-weapon selection logic

| function | rule |
|---|---|
| `Pump.is_weapon_pump_capable(wp)` | entry has `needs_manual_pump` or `block_native_pump` set (either value) ⇒ true; else `SHOTGUN_WP_DEFAULT_PUMP[wp]` |
| `Pump.is_weapon_pump_enabled(wp)` (suppression) | `CFG.enabled` true and `CFG.block_native_pump` not false; entry `block_native_pump == false` ⇒ false, `== true` ⇒ true; else `native_pump.default_shotgun_pump == false` ⇒ false; else `SHOTGUN_WP_DEFAULT_PUMP[wp]` |
| `Pump.is_weapon_manual_pump_enabled(wp)` (gesture) | `CFG.enabled` true and `manual_pump.enabled ~= false`; entry `needs_manual_pump == false` ⇒ false, `== true` ⇒ true; else `manual_pump.default_shotgun_pump == false` ⇒ false; else `SHOTGUN_WP_DEFAULT_PUMP[wp]` |
| `weapon_uses_trigger_rack(wp)` | `CFG.weapons[wp].trigger_slide_rack == true` — nothing else |
| `weapon_no_rack_required(wp)` | entry `no_slide_rack_required == true` **or** `slide_dock.no_rack_required_by_wp[wp] == true` |
| `Shell.is_shell_weapon(wp)` | `needs_manual_shell_reload == true` ⇒ true; else `(needs_manual_pump or block_native_pump)` **and** `shell_joint_by_wp[wp]` exists ⇒ true |
| `mag_visuals_configured(wp)` | false if `mag_node_by_wp[wp]` is nil, or the weapon is manual-shell / manual-cylinder |
| `is_manual_reload_weapon(wp)` | any of `enabled` / `needs_manual_shell_reload` / `needs_manual_pump` / `needs_manual_cylinder_reload` / `needs_manual_revolver_reload` |

**Three distinct reload archetypes:** per-chamber-joint revolver (`get_bullet_chamber_nodes(wp) ~= nil`: wp0300/wp3200/wp0800); single-chamber follow (`bullet_follows_cylinder_open` or `reload_drop_slots <= 1` and no chamber nodes: wp4100); canister mag (`canister_mag_reload`: JSON-only, no default weapon).

## E.6 Character profiles

`utility/RE2Character.lua`: keys `leon, claire, ada, hunk, tofu, carlos, jill`. `RE2_PL_MAP = { pl0000→leon, pl1000→claire, pl2000→ada, pl4000→hunk, pl4100→tofu }`, any `pl10*` → claire, default leon. `POSE_FALLBACK = { ada→claire, hunk→leon, tofu→leon, carlos→leon, jill→claire }`. Resolution order: `_G.__vr_active_char` if set, else `player:call("get_Name")` matched by substring alias, then `pl%d%d%d%d` extraction. The `2026-08-09-a-fix-that-kept-reverting-itself.md` case study is entirely about this lookup silently routing Claire's data into Leon's block.

Six per-weapon grip axes in cosmetic_dock each use a **3-tier lookup**: `[profile][wp_id]` → `[wp_id]` → CFG default. Tables `GRIP_BACK_BY_CHAR_WP`/`GRIP_BACK_BY_WP`, and identically `GRIP_RIGHT_*`, `GRIP_UP_*`, `GRIP_ROT_PITCH_*`, `GRIP_ROT_YAW_*`, `GRIP_ROT_ROLL_*`. **All six default tables ship empty** — every value comes from JSON at runtime. Live tuning **always** writes the per-character tier. Rationale: *"different characters' rigs (arm length, hand size) can want a different fit on the exact same gun."* Only concrete tuned value in the repo: `grip_back_by_char_wp.leon.wp1000 = 0.162`, `grip_right 0.076`, `grip_up -0.029`, `pitch 11.6`, `yaw -23.2`, `roll -144.3`; `grip_back_by_wp.wp1000 = 0.175`, `wp1100 = -0.13`.

`mag_holster.per_profile` ships `{leon, claire, ada, hunk, tofu}` each `{off_right 0, off_up 0, off_forward 0}`. `SETUP_TEMPLATE_WP = "wp0100"`, `SETUP_TEMPLATE_PROFILE = "leon"`.

## E.7 Recoil weapon classes (Feature A consumer)

`WEAPON_GRIP_PROFILE` keys `handgun / revolver / smg / rifle / shotgun / magnum / launcher / flamethrower / electric / rpg / minigun`, each with `weight` (`light`/`medium`/`heavy`) and separate `one` / `two` modifier sets `{kick, spring, auto, sustain}`. **Every `two` entry is `{1.00, 1.00, 1.00, 1.00}`** — two-handed is the baseline; one-handed adds the penalty. Examples: handgun `one {1.08, 1.10, 1.00, 1.00}`, revolver `{1.14, 1.15, …}`, smg `{1.10, 1.12, 1.55, 1.18}`, rifle `{1.18, 1.20, 1.25, 1.10}`, shotgun `{1.22, 1.48, 1.00, 1.05}`, magnum `{1.26, 1.40, …}`, minigun `{1.15, 1.22, 1.65, 1.25}`. Global two-hand kick scale `CFG.two_hand_scale = 0.55`; one-hand global kick by weight: heavy → `one_hand_heavy` (live 1.867), medium → `light + (heavy-light)*0.42`, light → `one_hand_light` (live 1.18).
`WEAPON_AUTO_RECOIL_IDS = { wp2000, wp2200, wp4200, wp4700, wp8700 }`.
Live `re2_vr_recoil.json`: `intensity 2.0`, `attack_duration 0.005`, `spring_stiffness 50.0`, `spring_damping 5.0`, `sustained_damping 10.0`, `sustained_window 0.125`, `spring_settle_mult 4.5`, `substep_dt 0.008`, `stack_cap 2.0`, `randomness 0.347`, `rotation_intensity 0.01`, `position_intensity 0.006`, `position_up_ratio 1.0`, `position_back_ratio 0.0`, `yaw_intensity 0.171`, `auto_scale 0.25`, `mult_exponent 0.1`, `wrist_pitch_sign 1.0`, `wrist_yaw_sign 1.0`, `suppress_native_recoil true`, `suppress_camera_recoil true`, `use_native_recoil false`.

## E.8 Config migrations that rewrite per-weapon data (port these or the JSON reads wrong)

1. `repair_shotgun_shell_reload_flags`: for every wp in `shell_joint_by_wp`, if `needs_manual_pump == true` **and** `needs_manual_shell_reload == false`, force `needs_manual_shell_reload = true`.
2. `needs_manual_revolver_reload == true` ⇒ set `needs_manual_cylinder_reload = true`.
3. **Samurai Edge mirror** — `mirror_wp7000(...)` applied to `slide_node_by_wp`, `slide_bind_by_wp` and the `weapon_sfx.by_wp` table, gated on `slide_dock.slide_bind_by_wp.wp7000 ~= nil`, run once, stamped `samurai_edge_rack_migrate_rev`. Verbatim: *"wp7010/7020/7030 (Chris/Jill/Albert) are the same gun model as wp7000 with different finishes, but every per-weapon tuning table only ever got wp7000 entries, so the variants had no working manual reload or slide rack. Mirror wp7000's tuning onto any variant slot that's still absent and match the variants' enabled state to wp7000's. Waits until wp7000 is actually tuned (slide bind present), then runs exactly once (stored rev), so a player can still turn an individual variant off afterwards."*
4. `revolver_reload.close_gesture_*` bump (`close_gesture_migrate_rev` 0/1 → 2), via `bump_num(key, new, old)` which overwrites only if the current value is nil or within `1e-6` of the *old* default: `swipe_speed 0.75→0.55`, `swipe_dist 0.035→0.025`, `lateral_ratio 0.55→0.45`, `max_vertical_ratio 0.4→0.45`, `window_min 0.045→0.035`, `window_max 0.14→0.17`, `up_swipe_speed 0.75→0.55`, `up_swipe_dist 0.035→0.025`, `up_ratio 0.6→0.5`.

**`merge_cfg` deep-merges only** `weapons`, `mag_node_by_wp`, `mag_exit_by_wp`, `mag_dock_by_wp`, `mesh_parts_by_wp`, `left_hand_pose` (four levels: context → `by_wp` → profile → `pose` → bone), `mag_hand_hold` (`by_wp` → profile). **Everything else — including `slide_dock` and `manual_pump` — merges one level deep, so a nested table in JSON replaces the default wholesale.** That is what the motion-toggle comment warns about: *"Read nil-tolerant (absent key = enabled): these keys aren't in older saved JSONs, and merge_cfg replaces the slide_dock/manual_pump tables wholesale."*

`TUNING_CFG_KEYS` (what `snapshot_tuning` captures): `mag_holster`, `mag_hand_hold`, `mag_dock_default`, `mag_dock_by_wp`, `mag_exit_default`, `mag_exit_by_wp`, `slide_dock`, `left_hand_pose`, `anim`, `shotgun_shell`, `shell_spawn_by_wp`, `shell_joint_by_wp`, `shell_mesh_parts_by_wp`.

Keys used at runtime but **not** in `DEFAULT_CFG` (created only by the UI or `migrate_cfg`, all read nil-tolerant where absent = enabled): `slide_dock.motion_rack_enabled`, `slide_dock.motion_pull_scale` (default 2.0, slider 0.3–3.0), `manual_pump.motion_pump_enabled`, `manual_pump.motion_pull_scale` (default 1.0, slider 0.3–3.0), `samurai_edge_rack_migrate_rev`, `revolver_reload.close_gesture_migrate_rev`, `reload_drop_slots_by_wp`, per-weapon `canister_mag_reload`, `shell_dock_joint_by_wp` (→ effectively the constant `"Elevator"`; resolver tries `dock_name`, `"Elevator"`, `"elevator"`, `"C_Attach_Hand_A"`), `bullet_dock_joint_by_wp`, `reload_drop_joint_by_wp`, `revolver_reload.chamber_grab_dist_by_wp`, `slide_bind` bullet-open keys (`bullet_open_x/y/z`, `bullet_open_rot_pitch/yaw/roll`).

## E.9 ImGui / config surface

Tree node `"Manual Reload"`, `order = 41`. Colours accent `rgb(136,204,255)`, enable `rgb(0,255,0)`, disable `rgb(255,64,64)`, muted `rgb(136,136,136)`, packed ABGR by `imgui_col`. Controls in draw order: master `CFG.enabled`; then per-weapon (only when `weapon_reload_configured(wp)` and `weapon_reload_applicable(wp)`): `entry.enabled` ("Mag manual reload"), `entry.needs_manual_pump` ("Manual pump" — turning it **on** defaults `block_native_pump = true` if unset; turning it **off** forces `block_native_pump = false`), `entry.needs_manual_shell_reload`, `entry.needs_manual_cylinder_reload` (and nils the legacy `needs_manual_revolver_reload`), then the two motion toggles + scale sliders.
Live readouts useful as port telemetry: `reload_slide.get_rack()` → `{active, mo_status, mo_init, mo_ratio}`; `_G.__vr_reload_pump.get_gesture()` → same shape. Strings: `"Motion rack: grab active, %s, pull %.0f%%"`, `"Motion rack: waiting for slide grab (LG on slide during a rack-needed reload)"`, `"Motion pump: waiting for forend grip (hold LG)"` — confirming **left grip** is the motion-drive grab for both.
Exported for other scripts: `_G.__vr_mag_holster_get_cfg()`, `_G.__vr_mag_holster_set_cfg_field(key, value)`, `_G.__vr_mag_holster_save_cfg`.

---

# F. Cross-module IPC bus (the real ABI a C++ port collapses)

118 `__vr_*` globals form the entire inter-script contract. Complete writer/reader map derived by parsing every `rawset` / `rawget` / `_G.` site across all 39 files. The load-bearing ones for A/B/C:

| global | written by | read by |
|---|---|---|
| `__vr_real_grip_active` | cosmetic_dock | recoil, suppress_supporthold |
| `__vr_grip_anchor_world_pos` / `_rot` | cosmetic_dock | recoil (the latch's `weapon_anchor_direct` path) |
| `__vr_muzzle_world_fwd` / `_up` | cosmetic_dock | recoil |
| `__vr_lh_world`, `__vr_rh_world` | ik_extention | cosmetic_dock, ik_extention, recoil, holster, ext_1, ext_2, ext_3 |
| `__vr_lh_joint_pos` | ik_extention | haptics, ik_extention, ext_1, ext_2, ext_3 |
| `__vr_lh_joint_rot` | ik_extention | ik_extention, ext_3, ext_4 |
| `__vr_lh_joint` | ext_1 | ext_1, ext_4 |
| `__vr_lh_slide_ik_override` | ik_extention, ext_4 | ik_extention, ext_1, ext_2 — **the freshness gate for `__vr_lh_joint_pos`** |
| `__vr_apply_slide_dock_left_arm` (**function**) | ik_extention | ext_2 (`apply_fn(5)`) |
| `__vr_clear_slide_dock_arm_snap` (**function**) | ik_extention | ext_2 |
| `__vr_capture_slide_dock_blend_origin` (**function**) | ik_extention | ext_2 |
| `__vr_get_live_left_hand_for_slide_blend` / `_rot` (**functions**) | ik_extention | ext_2, ext_4 |
| `__vr_slide_dock_blend_factor` | ext_2, ext_4 | cosmetic_dock, ik_extention |
| `__vr_slide_hand_world_pos` / `_rot` | ext_2, ext_4 | ik_extention |
| `__vr_slide_dock_ik_pole` / `_ik_twist` | ext_2, ext_4 | ik_extention |
| `__vr_slide_dock_blend_from_pos` / `_rot` | ik_extention, ext_2, ext_4 | ik_extention |
| `__vr_slide_dock_arm_snap` | ik_extention | ik_extention |
| `__vr_slide_rack_active` | ext_2 | ik_extention, ext_1, ext_4, ext_5 |
| `__vr_needs_rack` | ext_2 | recoil, ext_1, ext_5 |
| `__vr_reload_slide_dock` (**module table**) | ext_2 | ik_extention |
| `__vr_needs_pump` | ext_4 | ik_extention, recoil, reload, ext_1, ext_2 |
| `__vr_pump_active` | ext_4 | ik_extention, reload, ext_1, ext_2 |
| `__vr_pump_slide_support` | ext_4 | ik_extention, reload, ext_1, ext_2 |
| `__vr_pump_fp_passthrough` | ext_4 | ik_extention (`effective_slide_dock_arm_blend` returns **0.0** when true) |
| `__vr_pump_hand_world_offset` | ext_4 | ik_extention (added to the left-hand IK target **only when `__vr_slide_dock_blend_factor <= 0.001`**) |
| `__vr_block_empty_pump_reload_motion` | ext_4 | ik_extention (`sync_fp_left_hand_block`) |
| `__vr_pump_pulled_down_wp` | ext_4, delayed_shell_eject | delayed_shell_eject (consumes + clears) |
| `__vr_reload_pump` (**module table**) | ext_4 | reload |
| `__vr_shell_reload_blocks_sub_weapon` | ext_4 | reload |
| `__vr_mag_ammo_commit_bypass` | ext_1 | reload.lua hook gates |
| `__vr_rack_chamber_commit_bypass` | ext_1, ext_2, ext_4 | reload.lua hook gates |
| `__vr_manual_reload_active` | reload | haptics, ik_extention |
| `__vr_manual_reload_enabled` | reload | recoil |
| `__vr_block_fire_when_empty` | reload | recoil |
| `__vr_player_locomoting` | reload | reload, ext_1 |
| `__vr_vr_right_trigger` | reload | ext_1 |
| `__vr_left_support_strip_frame` | reload | reload, ext_1 |
| `__vr_reload_stack_reset_in_progress` | reload | ext_1, ext_2, ext_3 |
| `__vr_is_menu_blocking` (**function**) | holster | melee, reload, ext_1 |
| `__vr_is_cinematic_blocking` | holster | head_shadow, posture_spine_straighten_override |
| `__vr_in_holster_zone` | holster | cosmetic_dock, haptics, ik_extention, recoil |
| `__vr_in_head_flashlight_zone` | holster | cosmetic_dock, haptics, ik_extention, recoil |
| `__vr_block_hold_in_holster_zone` | holster | holster, reload |
| `__vr_block_left_support_in_head_flash_zone` | holster | reload |
| `__vr_in_mag_holster_zone` | reload, ext_1 | reload, ext_2, ext_3 |
| `__vr_block_left_support_in_mag_holster_zone` | reload, ext_1 | cosmetic_dock, reload, ext_2, ext_4 |
| `__vr_block_support_dock` | ext_2 | ik_extention |
| `__vr_mag_in_left_hand` | ext_1 | cosmetic_dock, holster, ext_1, ext_2, ext_4 |
| `__vr_mag_insert_active` | ext_1 | ext_2, ext_4 |
| `__vr_mag_dropped` | ext_1 | recoil, ext_2 |
| `__vr_mag_blocks_weapon_holster` | ext_1 | holster |
| `__vr_shell_in_left_hand` | ext_1, ext_4 | cosmetic_dock, ext_1, ext_4, ext_5 |
| `__vr_bullet_in_left_hand` | ext_3 | ext_1, ext_2, ext_5 |
| `__vr_left_hand_pose_slide_active` | ext_5 | ik_extention |
| `__vr_fp_left_hand_blocked` | ik_extention | ik_extention |
| `__vr_mag_holster_get_cfg` / `_set_cfg_field` / `_save_cfg` (**functions**) | reload | holster |
| `__vr_mag_holster_tick_calibrate` (**function**) | ext_1 | holster |
| `__vr_mag_holster_get_capture_remaining` (**function**) | ext_1 | holster |
| `__vr_ui_callbacks` / `__vr_ui_master_installed` | haptics, holster, recoil, reload | the `on_draw_ui` dispatcher |
| `__vr_active_char` | (external) | RE2Character |
| `__vr_menu_player_hidden` | menu_hide_player | head_shadow |
| `__vr_melee_haptics_enabled` | haptics | melee |

**Confirmed dead (written, never read):** `__vr_block_shoot_ready`, `__vr_block_shoot_ready_empty`, `__vr_native_change_bullet_active`, `__vr_manual_reload_suspended`, `__vr_recoil_ik_patching`, `__vr_pump_pull_axis_x/y/z`, `__vr_pump_pull_dist`, `__vr_pump_push_dist`, `__vr_pump_fp_base_pos/_rot`, `__vr_slide_rack_pull_axis_x/y/z`, `__vr_slide_rack_pull_signed`, `__vr_slide_pull_dist`, `__vr_slide_push_dist`, `__vr_slide_dock_arm_delta`, `__vr_slide_dock_in_range`, `__vr_slide_hand_dock_target`, `__vr_slide_rack_ik_done`, `__vr_slide_left_pose_active`, `__vr_left_hand_pose_mag_active`, `__vr_left_hand_pose_shell_active`, `__vr_left_hand_pose_bullet_active`, `__vr_revolver_cylinder_open`, `__vr_shotgun_pump_suppress_active`, `__vr_pending_shotgun_insert`, `__vr_mag_holster_ready`, `__vr_mag_holster_start_calibrate`, `__vr_real_grip_wp`, `__vr_muzzle_world_pos`, `__vr_restore_fp_left_hand_block`, `__vr_spine_correction_enabled`, `__vr_weapon_haptics_enabled`, `__vr_session_visual_enforce_until`, `__vr_pending_session_restore`.

**Read but never written anywhere (always nil):** `__vr_block_weapon_inputs`, `__vr_camera_stored_rot`, `__vr_cosmetic_dock_blend_factor`, `__vr_cosmetic_dock_hand_world_pos`, `__vr_cosmetic_dock_hand_world_rot`, `__vr_rh_joint`, `__vr_rh_joint_pos`.

**63-key reload-stack reset list** (wiped to nil under `__vr_reload_stack_reset_in_progress`) — the slide/pump subset: `__vr_slide_rack_active`, `__vr_slide_dock_in_range`, `__vr_slide_dock_blend_factor`, `__vr_slide_hand_dock_target`, `__vr_slide_hand_world_pos`, `__vr_slide_hand_world_rot`, `__vr_slide_dock_blend_from_pos`, `__vr_slide_dock_blend_from_rot`, `__vr_slide_dock_ik_pole`, `__vr_slide_dock_ik_twist`, `__vr_slide_dock_arm_delta`, `__vr_slide_rack_pull_signed`, `__vr_slide_rack_ik_done`, `__vr_slide_pull_dist`, `__vr_slide_push_dist`, `__vr_slide_rack_pull_axis_x/y/z`, `__vr_rack_chamber_commit_bypass`, `__vr_shotgun_pump_suppress_active`, `__vr_block_empty_pump_reload_motion`, `__vr_needs_pump`, `__vr_pump_active`, `__vr_pump_pull_signed`, `__vr_pump_pull_dist`, `__vr_pump_push_dist`, `__vr_pump_hand_world_offset`, `__vr_pump_pull_axis_x/y/z`, `__vr_pump_slide_support`, `__vr_pump_fp_passthrough`.

---

# G. Port-order recommendation

1. **`RE2` + `RE2Character` + input layer first** — 5 singletons (`app.ropeway.PlayerManager`, `app.ropeway.InputSystem`, `app.ropeway.gamemastering.SaveDataManager`, `app.ropeway.gamemastering.MainFlowManager`, `app.ropeway.gamemastering.InventoryManager`, plus `app.CharacterManager`, `via.VRControllerManager` and `sdk.get_primary_camera()`), 4 `vrmod` action handles, the caching rules (the `nil`-**or**-`0` invalidation quirk), and the `sc()` nil-tolerance policy. Everything else sits on this.
2. **The bypass token before any ammo code.** `ammo.internal_commit` + `__vr_mag_ammo_commit_bypass` + `__vr_rack_chamber_commit_bypass` are not optional and are not refcounted; get the RAII right up front or every write you make will be eaten by your own hooks.
3. **Feature C (pump/slide) next, not B.** It is the most self-contained, has the clearest state machine, and the `try_end_chamber_clear` sequence (`endChamberClear` → bypass on → `executeEndReload` → `executeEndEject` → bypass off) is the single call chain everything else in B eventually reuses.
4. **Feature B's ammo cascade third** — implement all five rungs including the free-ammo fallback and the reserve claw-back; the fallbacks exist because each earlier rung genuinely fails on some weapon. Pair `executeEndReload` + `endChamberClear` after every rung.
5. **Feature A last**, and decide explicitly which of (a)/(b)/(c) you are building. (c) is the only one shipping. (b) is complete but blocked on one bug whose root cause is *documented and understood* — positional arm-side disambiguation. In C++ that bug is straightforwardly fixable: hook `IkArmFit.updateIk` **once** (not twice, as the Lua does), read `<ApplyJoint>k__BackingField` → `get_NameHash()`, compare against `murmur_hash.calc32("l_arm_wrist"/"r_arm_wrist", 0)`, and cache by object address in a persistent map. The Lua only falls back to camera-relative guessing because `ApplyJoint` sometimes fails to resolve — with a single merged hook and a persistent per-instance map, tiers 4/6/7 can be deleted entirely. **That is the highest-value single thing a native port buys.**

**Also worth fixing in the port rather than reproducing:**
- The thirteen defects in §D (especially #1 — the forward-reference that has silently disabled four branches of the reload-IK spoof).
- The per-frame vs per-second blend-rate mismatch between the slide dock (`0.04`/frame) and the pump support (`2.0`/s).
- The pump's missing grip debounce (ext_2 has `grip_release_debounce_sec 0.1` + `grip_release_hard_cap_sec 3.0`; ext_4 has neither).
- `os.clock()` as the gesture clock — use game time so pausing pauses gestures.
- The single `updateIk` hook, replacing the two independent ones whose relative order is undefined.
- The `<`-vs-`<=` priority guard and the six-pass escalating re-application: in a single native plugin the whole pattern collapses to one authoritative write per frame at a known point, which is what the Lua was trying to approximate.
- Factor the three-name `BulletNumber` field ladder (`"BulletNumber"`, `"_BulletNumber"`, `"<BulletNumber>k__BackingField"`) written out in five places.

**Two things to preserve exactly, not "clean up":**
- The engine's own misspelling `setInhibitPetient` / `SurvivorDefine.ActionOrder.Petient`.
- The `getComponent(System.Type)` and `setPartsEnable(System.UInt64, System.Boolean)` overload-suffix strings.

**Open question for a live session:** whether `slide_node_by_wp.wp0100` should be `"_01"` (live JSON) or `"_02"` (Lua default). One of the two is wrong for the M19 and only a headset can say which.
