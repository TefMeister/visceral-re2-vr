# Engine Dossier — Resident Evil 2 Remake (2019) (Capcom RE Engine, via REFramework)

> Distilled current truth about this game's engine, as worked through the
> `PLAYBOOK.md` phases. Carried forward into the Visceral project from our
> Arcade Controls work — same game, same engine, our own document. Blow-by-blow
> history lives in the `-dev-archive` and `-modding-notes` repos of both
> projects; this is the consolidated reference.

**Status:** All engine knowledge below was earned on our shipped, Nexus-released
predecessor ("ARCADE CONTROLS for RE2 VR", final v1.5.0, now frozen) — custom VR
weapon handling, two-handed grips, IK, posture, reload/melee/holster behavior —
layered on top of an existing flat-to-VR base. Visceral rebuilds that
interaction layer from scratch against this same engine surface.
**VR-readiness verdict:** not applicable in the usual sense —
**the VR conversion itself is provided by praydog's REFramework**, which
already delivers stereo rendering, 6DOF, and motion controls for all RE Engine
games. This project's work sits entirely in the gameplay/interaction layer
*above* that, so this dossier documents the **RE Engine object model as seen
through REFramework's Lua reflection API**, not a from-scratch renderer/camera
reverse-engineering effort like our other engine dossiers.

> **How this differs from our other engine dossiers.** For Psychonauts / TEW /
> XIII the PLAYBOOK's North Star (game in a headset with head tracking) is the
> hard-won deliverable. Here REFramework already owns Phases 1–6 (injection,
> renderer, camera, stereo, the VR runtime). What remained — and what this
> dossier is about — is the engine's *managed object model*: how to find and
> drive the game's own gameplay objects (weapons, player, IK, motion) blind,
> through reflection, with no headers and no source.

## 1. Identity
- Resident Evil 2 (2019 Remake), PC (Steam). Capcom RE Engine title.
- Owned copy confirmed. Nexus mod page:
  https://www.nexusmods.com/residentevil22019/mods/2640
- The mod is a set of REFramework `autorun/*.lua` scripts — **files we author**,
  no game assets redistributed.

## 2. Engine lineage
- **Capcom RE Engine** — Capcom's proprietary in-house engine (successor to MT
  Framework, first shipped with Resident Evil 7, 2017). Used across RE2/RE3/RE7/
  RE8/Village, Devil May Cry 5, Monster Hunter Rise/Wilds, Street Fighter 6,
  Dragon's Dogma 2, and more.
- **Managed type system.** RE Engine runs a .NET-like *managed* runtime with a
  **Type Database ("TDB")** — full metadata for classes, fields, methods,
  properties, events. Comparable in role to Unity's IL2CPP metadata, but it is
  **Capcom's own system, not Unity/IL2CPP** (a common and important conflation
  to avoid). Types live under namespaces: engine types under **`via.*`**
  (`via.GameObject`, `via.Transform`, `via.render.Mesh`, `via.motion.*`, …) and
  each game's own code under its game namespace (RE2's is **`app.ropeway.*`**).
- Renderer: **DirectX 11 / DirectX 12** (REFramework supports both).

## 3. Injection foothold & tooling (all via REFramework — we wrote none of it)
- **REFramework** (praydog) is the entire foothold: a `dinput8.dll`-style
  injector + mod loader + scripting platform + generic 6DOF VR for all RE
  Engine games. We ship Lua scripts into `reframework/autorun/`; no proxy DLL,
  no debugger, no manual hooks of our own.
- **Until 2026-09-04 the whole mod was Lua.** From that date, under the standing "reach for the
  deep end" rule, new features are built in **`dev-archive/plugin/` — a REFramework native
  plugin (`visceral_core.dll`, C++, plugin API 1.15)** using the same reflection/hook machinery
  from native code; the shipped v0.1.0 Lua scripts stay until a feature needs the native
  layer. No memory patching, no x64dbg; REFramework's plugin API is the whole foothold (§8c).
- Base VR layer: **RE2VRMODRELOADED** (by Andyalpa), itself on top of
  REFramework — this mod is tuned against that specific base, used with
  permission.

## 4. The reflection model (the crucial section — how you find anything)
With no headers and no autocomplete, essentially all work is **reflection**:
ask a live object what it has, rather than read a spec. A "where does effect X
live" question has **three structurally different answers**, each a different
call:

1. **A named joint on a skeleton** — `transform:call("get_Joints")` →
   `get_elements()` → each `joint:call("get_Name")`. This is the animation rig
   only (character bones, weapon muzzle/socket points). It does **not** show
   parented GameObjects or components.
2. **A child GameObject in the Transform hierarchy** —
   `transform:call("get_ChildCount")` + `get_Child(i)` → `get_GameObject()`.
   For VFX props spawned and parented under something. **Caveat:** "zero
   children" does **not** prove nothing is attached — many native systems
   attach visual props by joint- or manager-based parenting that never appears
   as a Transform child.
3. **A component on the GameObject itself** —
   `gameobject:call("get_Components")` → `get_elements()` → each
   `component:get_type_definition():get_full_name()`. The easiest to forget
   (it's neither in the skeleton nor the scene tree — it's metadata on an
   object you may already hold). Gameplay objects carry **20–100 components**
   mixing render/physics/audio/gameplay; the real driver is often a
   generically-named class (an "effect manager" / "IK controller"), not the
   one with the effect's name in it.

Field enumeration on any instance (no class name needed):
```lua
local td = obj:get_type_definition()
for _, field in ipairs(td:get_fields()) do
    if not field:is_static() then
        log.info(field:get_name().." ("..field:get_type():get_full_name()..")")
    end
end
```
Two field traps, both hit in practice:
- **Type before action.** A "force this field to identity" probe that doesn't
  filter by field type silently no-ops on non-rotation fields (a `pcall`'d
  type-mismatched write fails quietly). A batch of "no effect" results needs a
  second look at *which fields were even the right type*.
- **Legitimately `nil` under some states.** A per-frame IK/correction target
  field may only exist while the character is in a specific pose context (arm
  colliding with geometry, weapon drawn/aiming). `nil` on a dump ≠ irrelevant —
  re-test under the exact game state the effect needs, not just "player exists."

## 5. Hooks & frame timing
- **`sdk.hook_method`** hooks any TDB method: a **pre-hook** sees/edits the
  args and can `return sdk.PreHookResult.SKIP_ORIGINAL` to suppress the call; a
  post-hook sees the return.
- **`re.on_pre_application_entry` / `re.on_application_entry`** hook named
  engine application steps (frame phases) — the standard "before/after this
  engine stage each frame" callbacks.
- **Same-frame ordering is a real class of bug.** Writing a value once per
  frame (a bone rotation, a field) is only half a fix if something reads it
  *later the same frame* (an IK solve, a derived aim vector). Two callbacks
  that both fire "before rendering" can still fire in the wrong order relative
  to each other. **Measure, don't guess:** sample the same value at an early
  and a late hook in one frame and compare — if they disagree, something
  between them changed it (your ordering bug, now measured); if they agree and
  the effect is still wrong, it's not a timing bug on *this* value — go back to
  "is this even the right value."

## 6. Camera & player-position gotcha (VR-specific, cost real hours)
- **The render camera's `WorldMatrix` is NOT a faithful proxy for the player's
  real physical orientation.** Reading the camera's world matrix and extracting
  position + forward/right/up to project an offset or reconstruct a hand
  position *happens to line up* while the player faces their calibration
  direction, then **silently diverges the instant they physically room-scale
  turn** (camera smoothing / recentering / the VR layer's composition mean it
  reflects where the in-game view points, which includes artificial
  locomotion, not raw play-space tracking).
- **Fix:** for "where is the player really" questions, use the actual tracked
  controller/HMD pose, never the render camera. (Hit twice in this project
  before the pattern was recognized — a helper's own doc comment literally said
  "includes artificial locomotion; not raw play-space tracking.")

## 7. Rendering: per-pass draw flags (first-person head hiding)
- **`via.render.Mesh` has independent per-pass draw flags.** To hide the
  player's head in first person **without** losing its shadow, don't zero the
  head bone (that collapses geometry out of *every* pass — headless shadow).
  Instead: `set_DrawDefault(false)` + `set_DrawShadowCast(true)` (shadow only);
  on RT builds also `set_DrawRaytracing(false)` so the head isn't in ray-traced
  reflections. Reference: praydog's `RE8VR.cpp` `fix_player_shadow()`. Leave
  REFramework's own `HideJointMesh` **off**.
- **Two scan gaps:** `getComponent()` returns only the **first**
  `via.render.Mesh` on a GameObject (eyes/eyelashes are often extra mesh
  components — enumerate **all** components); and face-part name matching must
  cover `face, hair, head, eye, lash, brow, matsuge, beard, mustache, hige,
  tooth, teeth, tongue`.
- **Joint name for the head is `"head"`** — `transform:getJointByName("head")`,
  the string REFramework's own `FirstPerson.cpp` hashes for RE2.
- **Hide Joint Mesh mode is NOT hollow; the head-shadow mode is** `[reported 2026-09-06, user; consistent with
  n=1 live]`. With the head joint scaled to zero the neck tube (part of the FACE mesh, weighted to the neck
  joints) stays and closes the collar; hiding the face mesh by its draw flags takes the neck with it and opens
  the collar. So the neck plug (v0.7) and the draw-flag head hider (v0.8) are one feature — the plug fills the hole
  the hider makes. The 05e static claim that Hide Joint Mesh leaves an open tube is `[disproved 2026-09-06]` as a
  visible hollow (the geometry description stands).
- **Native implementation: `visceral_core.dll` v0.8 `head_update()`** `[compile-verified 2026-09-06, unrun]` —
  `get_Components` off every GameObject under the player's transform (`get_Child`/`get_Next` walk), match by
  GameObject name AND `getMaterialName(i)` (material names are the reliable part identity: Claire's face file is
  `Face_Mat`/`Hair_Mat`/`Eyelashes`/`Tearline`, body `pl3000_Skin_Mat`), reveal on cinematic / grab
  (`app.ropeway.JackDominator.get_Jacked`) / FirstPerson inactive / camera >0.35 m from `head`. The two Lua-only
  facts (cinematic gate, `firstpersonmod:will_be_used()`) travel through bridge slots 30/31.
- **Stale-component trap (from Arcade Controls, live 2026-08-19):** a save load or Death → Continue rebuilds the
  player while the OLD `via.render.Mesh` components stay writable — `set_DrawDefault` succeeds on a head that is
  plainly visible. Read the flag back after clearing it; true means these are not the meshes on screen.

### 7b. Materials & textures: what the MDF exposes, and how to edit it offline (2026-09-06, `/pd`)
- **`.mdf2.21` is fully read/written by RE Mesh Editor's `modules/mdf/file_re_mdf.py`** (NSACloud) — usable
  outside Blender with the add-on folder on `sys.path`; `readMDF()` → `materialList[].textureList[]`
  (`textureType`, `texturePath`) and `propertyList[]` (`propName`, `propValue`); assign and `writeMDF()` — the
  writer rebuilds the string table `[verified-numerically 2026-09-06: re-read matches, other materials identical]`.
  Texture paths are `natives/stm/`-relative, with `.tex` and **no version number**; the loose loader takes
  `natives/stm/<path>.tex.34`.
- **`.tex.34` ⇄ DDS ⇄ PNG through the same add-on** (`modules/tex/re_tex_utils.py`: `convertTexFileToDDS`,
  `ImageListToDDS` (BC7 via DirectXTex `texconv.dll`, GPU), `DDSToTex(ddsList, 34, out)`). Headless it needs
  `ctypes.windll.ole32.CoInitializeEx(None, 0)` first or WIC fails with `80004002`. Formats seen: ALBM BC7 sRGB
  (dxgi 99), NRMR/detail BC7 (98), MSK1 BC4 (80), ATOS BC1 (71).
- **Player skin shader = `MasterMaterial/Master/Record_Player.mmtr`**: 15 texture slots, 37 floats. The ones
  that matter for skin fidelity: `DetailMap` (tiling tangent normal, alpha = AO; `Detail_UVScale`,
  `Detail_Normal_Intensity`, `Detail_AO_Intensity`) masked by `DetailMaskMap`; `NormalRoughnessMap`;
  `AlphaTranslucentOcclusionSSSMap` (B varies = SSS/occlusion term); `SSS_ProfileNumber`, `Translucency`.
  **Claire's skin ships with the detail slot NULL and intensity 0** `[measured 2026-09-06]` — pores are one
  MDF edit + one tiling texture away (tier 1a, deployed, unrun).
- **One 1024² body texture set serves skin AND cloth on Claire; the hands are the bottom ~14 % of it** and their
  normal map is flat — the low fidelity in first person is texture budget, not shading `[measured 2026-09-06]`.

## 8. Animation / motion system
- Locomotion is driven by a **motion-bank selector**, not by picking different
  animation files. In RE2 the locomotion layer plays the **same motion ids from
  the same bank id (1000)** whether armed or unarmed — only the resolved
  animation *name* differs (weapon-variant vs. unarmed-variant prefix). The
  weapon grip lives on a separate layer fed by different bank ids (a "hold"
  bank and a "finger" bank).
- The active-bank list is large (~82 entries) and **byte-for-byte identical
  armed vs. unarmed** — every candidate motlist coexists permanently, several
  sharing bank id 1000. Selection among same-id entries comes from per-bank
  state, exposed as a **`TargetBankType`** property on the motion component.
  Flipping that one narrow switch is how "play unarmed walk while armed" was
  shipped — no file swapping, no weapon-type spoofing (which visibly swaps the
  weapon model).


### 8b. The equipped-weapon surface (from public sources, NOT yet verified live)

`[reported, /gr 2026-08-29]` Read from public source (REFramework's `FirstPerson.cpp` and the
RE2R Custom Animation Framework project), not confirmed against our own build. Treat every line
here as a lead to verify, not a fact to build on.

- **The equipped weapon is one component read:** the player's
  `app.ropeway.survivor.Equipment` component → `<EquipWeapon>k__BackingField`. Weapon kind by
  type check against `implement.Gun` / `implement.Melee`. The gun muzzle is the weapon joint
  **`vfx_muzzle1`**.
- **`SurvivorCondition.get_IsReload`** is the reload-state flag, sitting alongside the
  `get_IsHold` we already rely on. Relevant to the queued manual-reload work.
- **A second dormant enum selector:** `app.ropeway.weapon.shell.ShellDefine.FireBulletType`
  chooses **`Camera` vs `AlongMuzzle`** as the fire origin; REFramework flips it to `AlongMuzzle`
  for VR. **This is the same shape as `TargetBankType` above** — a narrow selector the engine
  already honours, shipped with both paths live. See §11's habit note.
- **Playing a custom animation without the FSM stomping it:** the public CAF project registers
  runtime `via.motion.DynamicMotionBank`s for its own motlists, then **pauses and disables
  `via.motion.MotionFsm2`** for the clip's duration. Manual root motion needs
  `transform:set_Position` **plus `CharacterController:warp()`**, or physics snaps the character
  back. Directly relevant if manual reloads ever need a bespoke clip.

Full write-ups: `external-research/topics/2026-08-29-weapon-equipped-state-surface.md` and
`...-caf-custom-animation-framework.md`.

### 8c. The off-hand support surface — the game already has one (TDB dump, 2026-09-04)

**⭐⭐ 2026-09-05 (morning) — THE DOCK WORKS FLAT, AND THE GETTER IS READ IN A PRE-UPDATE POSE.**
Plugin v0.5 (`dev-archive/plugin/src/Plugin.cpp`) puts `l_arm_wrist` on a moving target with
`|wrist − target| = 0.000` and rotation error 0° at full weight, ramped over 0.2 s, released back
to 0.000, right wrist unmoved (`|r_arm_wrist − muzzle|` 0.085 throughout), HOLD latched with the
dock — on the handgun and on the minigun `[verified-live 2026-09-05, n=1 launch per weapon]`.
Spec v2.3 reqs 1–3 are met flat with a synthetic target; the controller path is untested (headset).
**The trap that cost two launches:** the game's per-frame call of `getIKLeftArmMatrix` /
`get_AidTargetWorldMatrix` returns the aid joint's world matrix **as it is at that point in the
frame — 0.18–0.20 m and ~48° away from the joint's final pose** on the handgun (18° on the
minigun); a read from a `LockScene` pre-hook returns the final pose. The solver carries the
**offset** you add across to the final pose, not the absolute value, so an absolute target in
final space misses by exactly that gap. Measured mapping, row-vector convention, fitted to ≤ 2 mm
over six samples `[verified-numerically 2026-09-05, n=6]`:
`wrist_final_rows = returned_rows · M` and `wrist_final − aid_final = (returned_t − natural_t) · M`,
with `M = natural_rowsᵀ · aid_final_rows` (constant per weapon/stance; compute it live every frame
from the hook's un-hooked value and `via.Joint.get_WorldMatrix` on the aid joint). So: **blend in
final space, then map back through `Mᵀ`** — see the plugin's `update_dock`. Corollaries: the
returned **rotation is consumed** (the wrist turns with it; NUM3 off → 0°); the "no value" reads of
this getter on the minigun were the plugin's own dump-time reads — the game's per-frame call always
carries one. Ledger: `modding-notes/2026-09-05-the-dock-lands-flat.md`; logs
`dev-archive/recon/2026-09-05-dock-v04/`.

**2026-09-05 (afternoon, `/pd`, static) — THE LEFT ARM'S REACH IS 50 cm, AND IT IS MEASURABLE FROM
THE SKELETON.** The `pl1000` left arm chain is `l_arm_clavicle` → `l_arm_humerus` → `l_arm_radius`
→ `l_arm_wrist`, so `|humerus−radius| + |radius−wrist|` is the arm, and the humerus is the pivot a
dock target must stay inside. Recomputed from the game's own joint dump:
**upper 0.2781 m, fore 0.2213 m, arm 0.4994 m**; the right arm gives 0.4996 m, an independent
measurement of the same skeleton agreeing to **0.18 mm**
`[verified-numerically 2026-09-05, n=2 limbs, 1 frame]`. **Two traps.** (1) The resting arm hangs at
**97.4 % of its own length** — nearly straight — so any clamp fraction below ~0.99 has only
millimetres of headroom over an ordinary standing pose. (2) The 12.6 cm clavicle segment is *not*
part of that sum but the shoulder girdle does move, so `0.4994` is a **lower bound** on true reach,
not the reach. Plugin v0.6 clamps a dock target to `0.98 ×` the measured length about the humerus,
behind a **NUM2** toggle for exactly that reason, and reports `reach=` / `clamp=` in the 1 Hz
summary. v0.6 also measures `M` every frame rather than only while docked, so `angM` is live before
a grip is committed; an unreadable aid joint clears `M_valid` rather than leaving a stale mapping to
be read as the current one. **Bone lengths must be re-measured on a player rebind** — a different
playable character is a different skeleton. Ledger:
`modding-notes/2026-09-05b-the-arm-measures-its-own-reach-and-M-is-live-every-frame.md`; evidence
`dev-archive/recon/2026-09-05b-reach-clamp-and-live-M/`.

**⭐ 2026-09-05 (night) — THE DOCK LEVER: the aid target is a hookable managed getter, and the wrist goes
where it says.** `[verified-live 2026-09-05, n=1 weapon (handgun), unaimed + HOLD, 3 modes]`
Post-hooking `Implement.get_AidTargetWorldMatrix` (or `Implement.getIKLeftArmMatrix`) and adding
10 cm to the returned translation moved `l_arm_wrist` 10 cm off `_101` (joint-to-joint read, no
hook in the path); shifting both gave 20 cm (additive); mode off returned it to 0.000; identical
under HOLD with the aim kept up. **The game calls each getter once per frame** (~345/s at ~350 fps)
and the two counts are always equal, so the chain is *wrist solver → `getIKLeftArmMatrix()` →
`get_AidTargetWorldMatrix()` → `AidJoint` world matrix*. **The solver snaps** (0.000 → 0.100 within
one 100 ms trace sample), so the "smooth, not snap" of spec v2.3 req 1 is ours to add by blending
the returned translation — trivial, the hook returns a fresh matrix every frame. Both return
`Nullable<via.mat4>` through a hidden return-buffer pointer: at return `*ret_val` is that buffer,
`u8 HasValue @0`, the matrix at `+0x10`. The dock is therefore: *while LG is held, return the
controller pose (blended in) instead of the joint's, and latch HOLD natively.* Ledger:
`modding-notes/2026-09-05-the-aid-joint-is-an-anchor-and-the-arm-kind-is-dead.md`; log
`dev-archive/recon/2026-09-05-arm-kind-and-reload/run9-aid-target-override.txt`. Not established:
whether the rotation part is consumed; what an unreachable target does.

**Static, same session — `_101` is an ANCHOR, and the joint constraint on `Implement` is the
weapon→right-hand attach, not the support hand.** Three ghidrust decompiles of `re2.exe`
(`setupAidJoint` `0x140ef7a10`, `updateJointConstraint` `0x140f11ad0`, `get_AidTargetWorldMatrix`
`0x140ebf3e0`) `[inferred-static 2026-09-05]`: `updateJointConstraint` reads **`this+0x78` =
`AttachJoint` (`JointConstraintInfo`)** and never `+0x80` = `AidJoint`; `get_AidTargetWorldMatrix`
reads `AidJoint` and returns its **world matrix** (a static null-Nullable at `0x1491842c0` when the
joint is null). Live, the reload test agreed: `|l_arm_wrist − _101|` opened to **0.349 m** during
`HG_Hold_Reload` while `_100`/`_101` stayed 8 mm apart, then closed to 0.000 in one step
`[verified-live 2026-09-05, n=1]`. **Reading rule for every RE2 decompile:** the first register is
the VM context and `this` is the second; the `*(rcx+0x50)->+0x18 != 0 → return` prologue is the VM
interrupt check. Field map of `Implement` (`offset_from_base`): `+0x70 JointConstraintExpressionID`,
`+0x78 AttachJoint`, `+0x80 AidJoint`, `+0x88 AimJoint`, `+0x98 Motion`, `+0xd8 MotionFsm`.

Read from `il2cpp_dump.json` (the game's own type database, dumped 2026-08-29) and then
**confirmed live the same evening** by the native probe on Claire's minigun save
(`[verified-live 2026-09-04, n=1]` — one weapon, one save; ledger
`modding-notes/2026-09-04-first-native-code-…`). This is the surface the left-hand dock design
(spec v2.3) should ride instead of inventing its own anchor and its own hand placement:

- **Live facts, minigun `wp8700`:** `get_AidJoint()` = weapon joint **`_101`**, `AidJointType` =
  Narrow (2); NARROW hash → `_101`, WIDE hash → `_100`, both on the **weapon** skeleton, neither
  on the player. The player's left **wrist** joint **`l_arm_wrist` sits exactly on `_101`**
  (distance 0.000) in idle, walk, jog and aim — the game's own two-hand hold pins the wrist to the
  aid joint. *(Corrected 2026-09-05: the probe's `l_hand` is `l_arm_wrist`, joint[19], which
  precedes `l_weapon`, joint[20], in the skeleton walk — the 2026-09-04 text said "palm
  `l_weapon`"; every distance ever logged is wrist-to-joint, which is what an arm IK's end effector
  would be.)* `AidTargetWorldMatrix` is live; `getIKLeftArmMatrix()` returned no value at the
  minigun reads, **but carries a value equal to the aid position on the handgun in the ready
  stance** (2026-09-05, appearing the same second `AidJoint` did) — it is the outer getter of the
  wrist-target chain above.
  `IkController`: LEG + ARMFIT enabled, ARM/HAND/SPINE/LOOKAT off, `UseIkArm=0`,
  `UseIkWrist=1`, `UseIkArmFitAsWrist=1`, `ArmStatusList` empty, `ControlStatus` 6 entries,
  unchanged between locomotion and aim. Player skeleton (`pl1000`, 190 joints): palms
  `l_weapon`/`r_weapon`, wrists `l_arm_wrist`/`r_arm_wrist`, no `l_hand`.
- **Handgun `wp0200` (WeaponType 3), same evening: identical** `[verified-live 2026-09-04, n=1]` —
  `AidJoint` = `_101` (Narrow), `_100`/`_101` 8 mm apart on the slide, palm on `_101` at 0.000 in
  idle / ready / walk / HOLD, hands 8 cm apart, LEG + ARMFIT on and ARM off in every state,
  `AttachJoint` = `setProp_A_00`. So the aid-joint surface is per-weapon data, not a minigun quirk.
- **`IkController.setArmFitTarget(int, via.vec3, bool)` is NOT the grip lever** `[disproved
  2026-09-04]`: wrist 0 accepts a target 10 cm off the aid joint every frame for ~1100 frames and
  the palm does not move; wrist 1 throws (one wrist entry). `IkArmFit` is the wall-touch solver.
- **Anchor or follower — ANSWERED 2026-09-05: anchor** (the headline block above). The follower
  reading via `Implement`'s joint constraint is `[disproved 2026-09-05]` (that constraint is the
  attach joint). **The `IkController` ARM kind is not the lever either** `[disproved 2026-09-05]`:
  `setEnable(ARM, true, 0.2f)` through the direct-ABI route *and* the invoke route both execute and
  `isEnabled(ARM)` stays 0 on every frame after; `setArmTarget` throws an internal game exception
  on index 0 and 1; `getIkTwoArm()` and `getIkHand()` are **null** on both weapons, at bind and
  with the weapon held (n=2 weapons). What stays enabled is ARMFIT with `UseIkArmFitAsWrist=1` —
  the wrist solver that consumes the getter chain above `[hypothesis]` as to its name, `[verified-
  live]` as to its behaviour.
- **Native aim latch:** `app.ropeway.InputSystem.setForce(64 /*HOLD*/, true)` from the plugin
  raises the full aim state within a frame (`IsHold` 0→1, layer 0 to bank 2
  `GG_Hold_Start_L0` → `GG_Hold_Idle_Loop`, `TargetBankType` 50 → 3145778); `false` drops it
  cleanly. `[verified-live 2026-09-04, n=1]` — the 2026-08-27 Lua result, now native.
  **`ATTACK` is kind 256 and fires natively:** `setForce(256, true)` for 8 frames under forced HOLD
  spent a round (13 → 12) with `pl00_1100_HG_Hold_Shoot` on layer 4 `[verified-live 2026-09-05,
  n=1]`. Layer map so far: 0 locomotion/hold body, 1 arm, 2 fingers, **3 upper-body action**
  (`Hold_Start`, `Hold_Reload`), **4 shoot overlay**, 5 empty.

- **Every weapon carries an "aid joint"** — `app.ropeway.implement.Implement.get_AidJoint()` →
  `via.Joint`, plus `setupAidJoint()`, `get_AidTargetWorldMatrix()` → `Nullable<via.mat4>` and
  **`getIKLeftArmMatrix()`** → `Nullable<via.mat4>`. "Aid" is Capcom's word for the support hand.
  `Equipment.getAidJointType()` returns `AidJointType` (`None=0, ExtraNarrow=1, Narrow=2,
  Wide=3`), and `Implement.get_LEFT_ARM_JOINT_NARROW()` / `get_LEFT_ARM_JOINT_WIDE()` return
  joint-name hashes (resolve with `via.Transform.getJointByHash(u32)`).
- **The player's arms are already IK-driven** — `app.ropeway.IkController` (reachable as
  `SurvivorCondition.get_IkController()`, also `Implement.get_ParentIkController()`) has
  `setArmTarget(int arm_index, via.vec3 pos, bool immediate)`, `setArmFitTarget(...)` (vec3,
  vec3+normal, or mat4 overloads), `setArmAdjustMode(int, ArmAdjustType{NONE,CANCEL,FIT})`,
  `setEnable(IkKind, bool, float t)` with `IkKind{LEG=0,SPINE=1,LOOKAT=2,ARM=3,ARMFIT=4,HAND=5}`,
  and `get_ArmStatusList()` → `IkArmStatus[]` (fields: `Index` @0x10, `ActivateTime` @0x14,
  `ResetTime` @0x18, `AdjustMode` @0x1c, `AdjustedPoint` Nullable<vec3> @0x20).
  *(2026-09-04 wrote "`setArmTarget` with a blend time is exactly the smooth dock of req 1" —
  `[disproved 2026-09-05]`, see above; the blend is done in the getter hook instead.)*
- Other named joints on a weapon: `get_AttachJoint()` → `JointConstraintInfo` (`Joint` @0x10,
  `OfsetPosition` @0x20, `OfsetRotation` @0x30), `get_AimJoint()` → `VirtualJoint`,
  `Gun.get_MuzzleJoint()` → `ExtraJoint` (`get_Position/get_Rotation/get_WorldMatrix`),
  `Gun.get_MuzzleJointWorldMatrix()`.
- Player lookup, all through properties (no component scans): `app.ropeway.PlayerManager`
  singleton → `get_CurrentPlayer()` (GameObject) / `get_CurrentPlayerCondition()`
  (`PlayerCondition : SurvivorCondition`) → `get_Equipment()` → `get_EquipWeapon()`
  (`implement.Arm`; `Gun : Arm : Implement`).
- Value-type layouts (unboxed, as `invoke` returns them): `via.vec3` = 3 floats; `via.mat4` =
  16 floats row-major, translation in row 3 (`m30..m32`); `Nullable<T>` = `_HasValue` byte at
  +0, `_Value` at +0x10. `System.String` = int32 length @0x10, UTF-16 @0x14. Managed arrays on
  THIS build: **count @0x1c**, elements @0x20 (`+0x18` holds 1, probably the rank) — measured from
  the bridge array's sentinel on 2026-09-04, and re-measured identically on every boot since; the
  plugin derives the offsets at hand-over rather than assuming them.
- Also seen, unexplored: **`app.ropeway.survivor.SurvivorMotionSpeedController`**
  (`: MotionSpeedController`; `TensionSpeed` / `WaterResistanceSpeed` as `RangeLerpFloat`,
  `applyTensionSpeed`) — a game-side motion-speed controller on the player, one level above the
  motion layer, and therefore the deeper candidate for req 4 than `TreeLayer.set_Speed` (§8d).

The native probe that reads all of this live is `dev-archive/plugin/` (`visceral_core.dll`,
REFramework plugin API 1.15 — the version the pinned `76298bd` build exports, confirmed from its
`API.h`). **Plugin API 1.15 has no VR calls**; controller poses reach native code only through
a Lua shim (`visceral_native_bridge.lua`) over a shared `System.Single[64]` handed across by a
hook on `System.GC.KeepAlive` — see the plugin header for the slot map.

### 8d. Motion-layer playback speed — the writable locomotion lever (from `/gr`, 2026-09-02)

`[reported 2026-09-02, public source]` The board's "does a writable movement-speed param exist"
risk is answered in public code: Junh2x's Requiem "Better Movement Speed" (ported to RE2 on
Nexus) writes **`set_Speed(k)` on the player's `via.motion.Motion` layer** every
`LateUpdateBehavior`, gated on `"walk"`/`"run"` in
`get_HighestWeightMotionNode():get_MotionName()`. Because RE2 locomotion is root-motion driven
(§8), a playback-rate clamp scales travel, leg cycle and footstep events together — req 4's
"drive legs and footsteps from speed" holds by construction. It is a rate, not a walk/run blend.

- RE2's types (`[inferred-static 2026-09-04]`, from the dump): `via.motion.Motion.getLayer(u32)` →
  **`via.motion.TreeLayer`** (there is no `MotionLayer` type in RE2), with `get_/set_Speed`,
  `get_HighestWeightMotionNode()` → `via.motion.MotionNodeCtrl` (`get_MotionName`, `get_Weight`,
  `get_MotionID`, `get_MotionBankID`), `get_BlendRate`, `get_LayerNo`, `getLayerCount()`.
- **Settled: locomotion is layer 0** `[verified-live 2026-09-04, n=1]`. Walk =
  `pl10_0190_KFF_GazingWalk_F_Loop`, jog = `pl10_0231_KFF_Jog_Straight_Loop`, idle =
  `pl10_0160_KFF_Gazing_Idle_F_Loop`; under HOLD the same layer plays bank 2
  (`pl10_0160_GG_Hold_Idle_Loop`, `GG_StrafeL_F`). Layer 1 = arm (`pl10_2000_GFC_Arm`), layer 2 =
  fingers (`pl10_02_FIN_GG_LGT`), layers 3–5 empty. `get_Speed` = 1.000 and `get_BlendRate` = 1.00
  on every live layer. **Read floats through the direct call route** (`Method::call<T>(vmctx,
  this, …)`), not the reflection invoke — invoke returned 0 for every float on this build.
- **Native-plugin traps found on the way** `[verified-live 2026-09-04]`: managed arrays on this
  build keep the count at **`+0x1c`** (`+0x18` holds 1), elements at `+0x20` — measure, do not
  assume; `System.GC.KeepAlive` is an internal call with no resolvable body — hooking it fails
  and invoking it crashes the game inside the native invoker, so hook/call only methods whose
  dump `function` address lies inside `re2.exe`; synthetic mouse buttons via `SendInput` do not
  reach the game (aim through `setForce(HOLD)` instead).
- Enemy awareness: no public source ties RE2 enemy perception to player movement speed at all;
  enemies use the same `getLayer/set_Speed` API for their own animation, not for noticing the
  player. Treat req 4's awareness half as ours to establish, not a lead to keep searching for.
- Fallback if root motion cannot express something: praydog's `re2_smooth_movement.lua` writes
  the body transform per `UpdateMotion` instead.

**2026-09-05 (`/gr` drop, drained by `/pd`) — a SECOND speed lever, one level above the layers, and
two cheap probe upgrades.** From `Junh2x/RE9-Movement-Speed-Mod`'s `re9_layer0_diag.lua`, a public
motion-layer property dumper in the repo the 09-02 topic was already reading (study-only: no licence
file on that repo, nothing copied):

- 🎯 **`via.motion.Motion` has a `PlaySpeed` property on the COMPONENT, above every layer**
  `[reported 2026-09-05, from source]`. Engine-level `via.motion`, not `app.ropeway`, so §8d's
  confidence in the reflection route transfers to it, and a global rate cap written there needs
  neither a layer index nor motion-name gating. ⚠️ **Only the getter is witnessed — the dumper never
  writes it.** `set_PlaySpeed` existing is `[hypothesis]` until a reflection dump lists it. **The probe
  now settles it: plugin v0.6's `dump_motion()` hangs off the existing NUM7 dump** and prints the
  `via.motion.Motion` surface filtered on `speed`/`play`/`rate`/`layer`, `get_PlaySpeed` as a value
  (through the direct call route, so `NaN` means the method is absent, not that the value is zero),
  `getLayerCount`, and `get_Weight` per layer. One keypress on any launch, flat or in the headset. This is the better
  first candidate for req 4 than `TreeLayer.set_Speed`, and than
  `app.ropeway.survivor.SurvivorMotionSpeedController` (§8c), which no public source uses at all
  `[checked 2026-09-05]` — an absence that means nobody mapped it for us, **not** that it is a bad
  lever, since Nexus 403s automated fetch and the type is RE2-only and game-side.
- **Identify the driving layer by `get_Weight`, not by string-matching motion names**
  `[inferred-static 2026-09-05]`. The shipping mod hard-codes layer 0; the string test is how it
  decides *walk vs run*, not *which layer*. ⚠️ The `get_Weight` already listed above is on
  `MotionNodeCtrl`; the one being suggested is on the **layer** — verify it exists on `TreeLayer`
  before relying on it. Our own live read (layer 0 = locomotion) already agrees, so this is
  corroboration by a second method rather than a new answer.
- **`getLayerCount` bounds the enumeration** — no-underscore spelling first, matching `getLayer` /
  `getComponent` on this component; `get_LayerCount` is the dumper's fallback
  `[reported 2026-09-05, from source]`. Already listed above from our own dump.

Still blocked by the same wall: **"Better Movement Speed RE2"** (Nexus 2391) is the port that had to
solve the `app.ropeway` translation, its page 403s to automated fetch and it is not on GitHub. A
launch-side session that can read that file locally would likely get RE2's move-speed accessor named
outright. Credit: **Junh2x**.

Sources: `external-research/topics/2026-09-02-a-writable-speed-lever-exists-the-motion-layers-playback-speed.md`,
`external-research/topics/2026-09-05-the-public-layer-dumper-we-said-did-not-exist-is-in-the-repo-we-already-read.md`.

### 8e. What Arcade Controls already paid for (port map, 2026-09-05, `/pd`, static)

Full map: `modding-notes/2026-09-05-arcade-controls-port-map.md` — every managed method, field,
joint name, constant and recorded trap behind AC's two-hand grip, manual reload and slide/pump rack,
read out of our own frozen source. All `[measured 2026-09-05]` unless the body says otherwise; four
cited claims were spot-checked against the source by hand and matched to the line. The durable
engine facts:

- **⚠️ Reach is bought by STRETCHING THE CLAVICLE, not by clamping the target.**
  `l_arm_clavicle` is written through **`get_BaseLocalPosition`** — deliberately, so the write is
  idempotent within a frame; re-reading the base after writing it compounds the stretch, and the
  base is reset per frame for that reason. Live budget `max_m = 0.16` m, **plus up to `0.08` m more
  for large weapons, left arm only**, total clamped to `0.60` m, bone lengths sanity-clamped to
  `[0.18, 0.45]` m. **This is more permissive than §8c's measured 0.4994 m arm**, so a hard reach
  clamp will refuse poses AC already ships and has tuned live. Treat a clamp as the fallback and the
  stretch as the destination.
- **Structural arm identity, and the single biggest thing a native port buys.** Hook
  `app.ropeway.IkArmFit::updateIk` **once**, read `<ApplyJoint>k__BackingField` → `get_NameHash()`,
  and compare against `via.murmur_hash::calc32("l_arm_wrist" / "r_arm_wrist", 0)` (a **static**
  method — first argument `nil`, seed `0`); cache by `get_address()`. AC's Lua needs an eight-tier
  ladder because it hooks `updateIk` from two files whose relative order is undefined and because
  `ApplyJoint` sometimes fails to resolve; its lower tiers guess from camera-relative hand positions,
  and **that guessing is the sole unfixed bug that keeps AC's two-hand latch shipped OFF**. One hook
  plus a persistent map deletes the problem.
- **⚠️ A joint's cached `WorldMatrix` is STALE if read in the same frame as a write to that joint.**
  AC reads it fresh at `LateUpdateBehavior` and stale at `UpdateMotion`, and the resulting alternating
  pose *was* a hand-teleport bug. Our own plugin reads `via.Joint.get_WorldMatrix` at `LockScene`-pre
  and writes no joints, so it is unaffected — but any future joint write makes this immediate.
- **Nothing chambers a round without the finalize pair.** After every ammo write:
  `Gun::executeEndReload()` then `Gun::endChamberClear()` (the pump path adds `executeEndEject()`).
  Skipping it leaves the round in the slot with the gun refusing to fire — recorded as a live
  finding, not a guess. `setBulletNumber` is **not** the reload primitive; it updates the count
  without the reload track and the gun still will not fire.
- **A mod that hooks the reload pipeline must carry a re-entrancy token**, because its own commits
  call the methods it blocks. AC uses `ammo.internal_commit` plus two globals, and **it is not
  refcounted** — a nested commit clears the flag early. In C++ this wants RAII with a depth count.
- **The engine's own misspelling is load-bearing:** `setInhibitPetient`, enum
  `SurvivorDefine.ActionOrder.Petient`. So are the overload suffixes
  `getComponent(System.Type)` and `setPartsEnable(System.UInt64, System.Boolean)`.
- **`app.ropeway.InventoryManager` does not exist** — it is `app.ropeway.gamemastering.InventoryManager`,
  and the inventory *object* is an `app.ropeway.survivor.Inventory` instance, not a singleton.
- **Config shape trap:** the shipped JSON in `reframework/data/re2_vr/` overrides the Lua defaults
  **wholesale at load**, and `merge_cfg` deep-merges only a named handful of tables — `slide_dock`
  and `manual_pump` are replaced entirely by a nested JSON table. Eleven confirmed divergences are
  tabulated in the note; port against the JSON.

Credit: **Andyalpa** (RE2VRMODRELOADED, the base layer, studied with permission — described only,
no code taken), **praydog** (REFramework, FirstPerson).

### 8f. The RT `.motlist.524` container, decoded enough to splice (2026-09-06, `/pd`, static)
- **Layout** `[measured 2026-09-06, n=5 files: pl10/pl00 × hold/move originals + a modder's RT specimen]`:
  header `0x00` u32 524 · `0x04` `mlst` · `0x10` u64 pointer table (0x50) · `0x18` u64 collection offset ·
  `0x20` u64 name offset (0x34) · `0x30` u32 **slot** count. One u64 per slot; **two slots may share an
  entry** (the original does it for Hold_Idle_Loop and Shoot_NoAmmo). Entries contiguous, 16-byte
  aligned, up to the collection offset. v492 entry header: name offset u64 at **+0x58** (entry-relative),
  frames f32 +0x60, bones/clips u16 +0x70, fps u16 +0x78, `motSize` u32 +0x0C (= blob minus padding, and
  **0 on every file's first entry**, specimen included).
- **No entry-relative header offset points outside its own blob** (0 of 10 fields × 165 entries), so an
  entry moves between containers whole. CAF's "entry-relative" claim holds on RT.
- **The collection block is 72 bytes per slot; only u32 at +8 varies and it is the motion NUMBER**
  (`0x6e` = 110 for `pl10_0110`, `0xa0` = 160 for `pl10_0160`). Byte-identical between Claire's original
  and the Jill replacement whose entries all differ in size → it holds no offsets or sizes. It is the
  slot → number table the motion FSM asks by `[inferred-static; the splice's flat run is the proof]`.
- **Splice recipe** (`dev-archive/tools/re-engine/motlist_splice.py`): header + rebuilt pointer table +
  re-laid blobs + the original collection block verbatim; blob names left alone (renaming in place has
  no room). Claire's and Leon's aim-walk banks with the twelve aim-walk slots replaced by their own
  `KFF_GazingWalk_*` loops build and self-verify `[verified-numerically 2026-09-06]`, unrun.
- **Open until one flat run:** lookup by number vs name hash; whether first-entry `motSize` must be 0;
  whether the aim-walk blend drives phase by frame or normalised time (walk loops are 3–6× longer).

## 9. "Several lookalike systems, one is live" (a recurring RE2 trap)
- A single weapon can carry **multiple similarly-purposed config tables** for
  what looks like one feature, and tuning the wrong one throws no error and no
  crash — just silence. Concrete case: a revolver's VR reload had **three**
  position systems — the spent-shell *extraction* joint/offset system is
  entirely separate from the new-round *insertion* grab point; meticulously
  debugging the extraction math (correct to four decimals) did nothing to the
  insertion feature being tested. Lesson: before deep-debugging a value, prove
  it's the one the feature you're testing actually reads. Also: some per-weapon
  tables have **missing entries that silently fall back to a hardcoded default
  joint** that's only correct for the *other* weapons sharing the table.

## 10. Logging / probe conventions (kept from the start)
- Every diagnostic script logs under one consistent bracketed tag
  (`[my_script_name]`) so a log with tens of thousands of lines from every
  loaded script greps down to just the relevant ones.
- **Wrap every reflection call in `pcall`.** Native calls into an engine never
  designed for this introspection fail unpredictably (wrong arg count, wrong
  overload, method not supported) — a caught, logged failure beats a script
  that dies silently on line one with only a missing log line as the symptom.
- A live UI status readout reflects only the **last** thing that ran; with
  several buttons/status lines, "it says X" is ambiguous — a screenshot beats
  another round of text description.

## 11. Dead ends & false leads (save future time)

- **🧭 HABIT, earned the expensive way: check for a dormant enum before building a mechanism.**
  Twice now RE2 has turned out to ship **both** behaviours behind a narrow selector the engine
  already honours — `TargetBankType` for armed/unarmed locomotion (§8), and
  `ShellDefine.FireBulletType` for `Camera` vs `AlongMuzzle` fire origin (§8b). In both cases the
  engine-supported switch beats anything hand-built. The 2026-08-29/30 animation battle burned a
  full day on bank poisoning, motlist swapping and bone correction before the answer turned out
  to be a state flag. **Before writing a mechanism, spend ten minutes looking for the enum.**

- **🧭 HABIT: when a HUD-suppression script "does nothing", date-check the REFramework
  revision before doubting the Lua.** Returning `false` from `re.on_pre_gui_draw_element`
  (the documented way to hide a GUI element, used by `visceral_crosshair.lua`) was silently
  broken in REFramework master between **2026-08-19 (PR #1503) and 2026-08-28 (PR #1809)** —
  no error, the element just kept drawing. **Checked 2026-08-31 (home PC): our pinned rev
  `76298bd` is dated 2026-03-11, months BEFORE the broken window — v0.1.0's crosshair hiding
  is NOT affected.** But any future REFramework upgrade should land on a build from
  2026-08-28 or later, and this class of silent no-op is worth remembering: the framework
  under the script can break the script's documented contract without a single log line.
  (Found by the 2026-08-31 `/sr` sweep; details in the cross-engine library's
  "silent no-ops" technique page.)

- **Placing the support hand through `IkController`** (2026-09-04/05): `setArmFitTarget` accepts and
  does nothing (the game rewrites the target from the aid joint every frame); `setEnable(ARM)`
  never sticks by either call route; `setArmTarget` throws; `getIkTwoArm`/`getIkHand` are null.
  The lever is the **getter the solver reads** (§8c), not the solver's own setters.
- **Reading `Implement.updateJointConstraint` as the support-hand constraint** — it is the
  weapon→right-hand *attach* (`AttachJoint` @+0x78), decompiled 2026-09-05. `AidJoint` @+0x80 is
  never touched by it.
- **A `/lm` menu route chained without a capture between steps** (2026-09-05, launch 2): an
  "autosave feature" notice preceded the title on that boot and ate the ENTER; the run sat at the
  title with the whole chain one step behind. Verify the title by capture before ENTER.
- **Swapping compiled animation files on disk** to change locomotion —
  skeleton-specific binary data, and a file-level hammer for a runtime problem.
  Use the motion-bank selector (§8) instead.
- **Spoofing the equipped-weapon type** to change animations — visibly swaps
  the weapon model too. Wrong altitude.
- **Camera world-matrix as player pose** (§6) — lines up at calibration,
  drifts on physical turning. Use tracked controller/HMD pose.
- **`SurvivorCondition.get_IsDamage` as a "left first person" signal** for
  head-reveal — true for *any* hit (an ordinary punch that never leaves first
  person), so it pops the head on with the camera inside the skull.
- **Camera-to-head distance as a "left first person" signal** — measured
  0.111 m at rest vs. 0.112 m during an actual zombie grab; the camera never
  leaves the head, so no threshold could ever work.
- **Tuning a per-weapon joint table entry that the game silently ignores** (§9)
  — no error, no effect; confirm the value is actually read first.

## 12. External resources
See **[`EXTERNAL-RESOURCES.md`](EXTERNAL-RESOURCES.md)** for the annotated link
list — REFramework, its Lua/API documentation, the EMV Engine toolkit, and
general RE Engine references — that this project's engine-side knowledge draws
on.
