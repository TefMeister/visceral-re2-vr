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

### 5b. ⭐⭐ The two 2026-09-10 headset defects are DOCUMENTED REFramework traps (drained from `/gr` 2026-09-11)

Both ⭐⭐ `[PD]` rows from the first VR run have public answers. Neither is a mystery; both are
things REFramework's own shipped RE8 VR script does differently from us.

**1. The grip write almost certainly hit the value-type COPY trap.** The REFramework book states a
value type you read is *"just a local copy"* and that mutating it *"does not change anything
in-game"* `[reported 2026-09-11, official docs]`. Small value types are also **auto-converted on
return** (`via.vec3` → `Vector3f`, `via.mat4` → `Matrix4x4f`), which is what makes the copy *feel*
like a handle. This alone accounts for "the shim never wrote the grip slots" with no second
explanation needed. Routes that actually commit: `set_field` on the **owning** object;
read → modify → **setter** (`re2_vr_melee.lua` does `set_field` then `set_Capsule`); or
`sdk.set_native_field` for native `via.*` structs.

**⭐ But for a JOINT, praydog writes no fields at all.** `scripts/re8_vr.lua` caches
`via.Joint:set_Position` (a `Vector3f`) and `set_Rotation` (a `Quaternion`) **method definitions** at
file scope and calls them per frame. There is **no `set_field` on a joint transform anywhere in that
script** `[inferred-static 2026-09-11]`. Joint lookup in the wild is `getJointByName` /
`getJointByHash` / `get_Joints`. `RETransform` also offers `calculate_base_transform(joint)`, which
returns the joint's **reference/T-pose matrix** — a rest pose to offset from — and
`set_position(pos, no_dirty)`, where **`no_dirty` is documented as necessary when the scene is
locked**.

**2. The ordering half, and it is a stronger claim than "pick the right callback".** That same script
writes the hand pose **three separate times per frame**, at progressively later application entries:
`UpdateMotion` (hand positioning) → `PrepareRendering` (hand IK) → **`LateUpdateBehavior` (final,
authoritative)**. That is what you do when the engine's own motion and IK passes will overwrite an
earlier write.

⚠️ **Two cautions that bound how far to trust that list.** The entry set *and its order* are
discovered at runtime by pattern-scanning and are **game- and build-specific**
(`shared/sdk/Application.cpp`; stride `0xD0` for TDB < 74, `0xC8` for TDB ≥ 74)
`[inferred-static 2026-09-11]`, and **no ordered `via.ModuleEntry` list is published for RE2 Remake** —
so ours must be dumped locally with a logging callback on every candidate name plus a frame counter.
And REFramework's VR scripts had a **timing race fixed 2026-03-05** (*"VR Scripts (RE2/RE7/RE8): Fix
racy behavior in hooks causing jitter"*); reading that diff is probably worth more than more
searching.

**3. The constant camera read: re-fetch per frame, and read JOINT 0.** `sdk.get_primary_camera()`
resolves `via.SceneManager` → `get_MainView()` → `get_PrimaryCamera()`; **method definitions are cached
for speed, the camera object is not** `[inferred-static 2026-09-11]`. `re8_vr.lua` calls it **every
frame, never cached**, then walks `get_GameObject()` → `get_Transform()` → `get_Joints()[0]` and
operates on **joint 0**. Ranked causes of a constant reading: (1) a handle fetched once at script load
— `utility/RE2.lua` re-acquires player/weapon/inventory every frame and clears caches when the player
goes unavailable, precisely because these go stale; (2) reading the wrong node —
`transform:get_position()` can return a rig origin that genuinely never moves while the live pose is
on joint 0, **which fits "constant, not nil, not garbage" better than anything else**; (3) a different
camera from the one gameplay drives `[hypothesis]`; (4) reading too early — weak, that gives a
one-frame-late value, not a constant one; (5) REFramework's own VR camera override already active
`[hypothesis]`.

**⭐ One log line decides between the two leading causes:** print the camera object's `get_address()`
beside the position each frame. **Address constant across a scene transition ⇒ stale handle. Address
moving while the position does not ⇒ wrong node.**

**4. ⚠️ Three negatives that bear on claims this dossier already holds.**
- **Nothing public demonstrates `via.motion.Motion`, `via.motion.IkLeg`, `via.motion.IkArmFit`,
  `RequestSetJointPose` or `setJointPose`** — searched for specifically, zero documentation and zero
  example usage `[reported 2026-09-11]`. Our static work found some of these names and they may be
  perfectly correct; the narrow point is that **public practice cannot be cited as support for them.**
- **`write_valuetype` is not in the book or in the Lua binding source at all** `[inferred-static
  2026-09-11]` — it appears only in loose secondary summaries. Do not build on it. The real primitives
  are `set_field`, `sdk.set_native_field`, and `ValueType`'s offset writers.
- **No public mod disables a motion bank or IK before writing a joint.** praydog's RE7/RE8 VR does not
  — it conditionally *skips* its own hand-IK updates in cutscenes. alphaZomega's EMV-Engine agrees from
  another direction: its "Freeze" feature works by *constantly setting the same value every frame*.
  **The public technique is "write after IK, every frame, repeatedly", not "disable IK, then write
  once."**

**5. One API hazard to rule out on the grip path:** the docs state *"`ByRef` parameters are not
correctly supported by REFramework"* — they behave as `T**`. The workaround is
`sdk.to_valuetype(ptr, "System.UInt64")` and reading its `mValue`, and **for `out` parameters this
only works in a post-hook**, stashing the reference during the pre-hook. If any slot on the grip path
is by-ref, a naive write there fails in exactly the way we observed.

⚠️ **Currency:** commits dated **2026-04-25** read *"REFramework v2 (#1609)"* and *"move scripts to
`dev/`"*, with internal C++ renames. Whether v2 changes the **Lua** surface could not be determined,
and the `scripts/` paths cited here may have moved. Check against the build actually injected.

### 5b.4. ⭐⭐ CORRECTION — "the camera the plugin reads does not move" WAS A CALL-ONCE BUG, NOT VR (2026-09-12, `/lm` + reader)

**Supersedes §5b.3's ranked causes**, which blamed REFramework replacing the primary camera under VR.
That was wrong, and the evidence that kills it is our own: the 2026-09-09 recon log is a **flat run with
no headset** and carries the identical frozen `cam=(-11.50 -3.20 4.20)` while the player's hands are 21 m
away `[verified-numerically 2026-09-12, n=2 lines]`. REFramework's `VR: Failed to get primary camera!` is
an unrelated init-order message.

**The actual cause:** `update_camera()` had exactly two callers, and the live one was
`head_update()`'s `if (!g.dock.cam_valid) update_camera();`. `update_camera()` sets `cam_valid = true`
on its first success, so from frame two it was **never called again** and every consumer compared
against the camera pose of the session's first frame `[inferred-static 2026-09-12, confirmed live below]`.

**Ruled out with evidence while chasing it:** REFramework's VR API is unreachable from a native plugin —
plugin API 1.15's `include/reframework/API.h` has no VR symbol at all `[verified-numerically 2026-09-12]`;
and our call chain was never wrong — `via.SceneManager → get_MainView → get_PrimaryCamera` is byte-for-byte
REFramework's own (`shared/sdk/SceneManager.cpp:32-42`) `[inferred-static]`.

**The fix is deleting the guard**, and one flat walk proves it `[verified-live 2026-09-12, n=1]`:

| | before | after |
| --- | --- | --- |
| `cam=` while walking | `(-11.50 -3.20 4.20)`, frozen across two level loads | `(-20.80 -9.94 19.42)` and changing every sample |
| head-hider reveal distance `d=` | 19.68 m | **0.11 m** — what the board predicted for standing |
| head hider | `hid=0/5`, revealed forever | `head=1 hid=1/5`, hiding |

⇒ **One line closed the camera row and the reveal-gate half of the head-hider row.** ⚠️ The head hider's
OTHER defect stands (route B, the costume changer, returned 0 objects on 2026-09-12 — see below): mesh discovery still finds only 5 meshes and they are our own injected objects plus
`Transceiver` and `FlashLight`, so what it hides is still the flashlight, not the head.

A diagnostic trio was added to the per-second summary and is worth keeping: `cam2=` (camera GameObject →
Transform → joint 0, REFramework's own route), `camF=` (the old call re-made fresh), `camA=` (the camera
object address). Frozen `cam` with both others moving is the call-once signature; `camF` frozen but `cam2`
moving would mean the wrong node; both frozen with `camA` changing would mean the wrong camera object.

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
  `Face_Mat`/`Hair_Mat`/`Eyelashes`/`Tearline`, body `pl3000_Skin_Mat` — ⚠️ **`pl3000` is SHERRY; Claire's body
  material is `pl1000_Body_Mat` and her face lives under `pl1050`, so v0.8's material match must be re-checked against
  `pl1000`/`pl1050` MDFs before its FLAT read `[measured 2026-09-06]`**), reveal on cinematic / grab
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
  ⚠️ **Character IDs `[verified-live 2026-09-06, access log + 4 restarts]`: `pl1000` = Claire (skin material
  `pl1000_Body_Mat`, sampling the shared `pl1000_Jacket_*` atlas; face `pl1050`, hair `pl1070`); `pl3000` = Sherry
  (chain + pendant), preloaded for cutscenes.** Everything below that said "Claire, pl3000" before 13:15 on 2026-09-06
  was measured on Sherry. Claire's skin ships **with** a detail tile: `SectionRoot/Character/Textures/Detail_Skin.tex`,
  128×128, `Detail_UVScale` 0.5, `Detail_Normal_Intensity` 0.5, `DetailMaskMap` = `NullWhite` `[measured 2026-09-06]`;
  Sherry's is the one with the slot NULL. **A loose 4K `pl1000_Jacket_ALBM.tex.34` is sampled at its top mips** (green
  bands at 9 mm spacing, crisp in the headset) — 4K authoring is worth it; a loose texture for a character not on screen
  is opened by the loader and changes nothing, so the first deploy on any new path is a loud diagnostic.
- **One 1024² atlas serves Claire's skin, jacket, tank top and shirt; the hands are the bottom ~14 % of it**, left hand
  u 0.52–0.99, right u 0.005–0.49, and their normal map is flat — the low fidelity in first person is texture budget, not
  shading `[measured 2026-09-06]`. Same UV strip layout and bone names as Sherry's `pl3000_Body_*`; Claire's bind pose is
  flatter (finger curl +0.48 vs +1.69) with the thumb abducted out of the palm plane, which broke every thumb-derived
  "back of hand" vector — the paint script now uses the knuckle row for the palm plane and ray occlusion for the dorsal
  pick (`modding-notes/2026-09-06-hd-hands-were-on-sherry-claire-is-pl1000.md`).
- **Record system (dirt/blood) resolution**: `Rec_RTT` = `VFX/RecordSystem/RecordTexture/<pl>/<pl>_body.rtex.5`, a
  64-byte asset with the runtime target's size at +0x10/+0x14 (**512×512** for the whole body, sampled through
  `UVMap1` where both hands share ~a quarter of it); mud = `Record_Mad_Map_MSK4` 256² tiling at `Rec_Mud_UVScale` 3;
  injury = 512² ALBA + NRM `[measured 2026-09-06]`. A loose rtex with a larger size is the untested lever `[hypothesis]`.

### 7c. ⚠️ The detail-map parameter names we were using DO NOT EXIST — and the deployed BC4 mask may be inert (drained from `/gr` 2026-09-07)

**Two of the names this project used are not RE2 parameters at all.** The shipped `pl1000_Jacket_Mat`
set is published in NSACloud's RE Mesh Editor presets (master material
`MasterMaterial/Master/Record_Player.mmtr`) `[reported 2026-09-07]`:

| kind | name | shipped value |
| --- | --- | --- |
| texture | **`DetailMap`** (packed — hence separate normal and AO intensities) | `MasterMaterial/Textures/NullDetail.tex` |
| texture | `DetailMaskMap` | `systems/rendering/NullWhite.tex` |
| float | `Detail_UVScale` | **`0.25`** |
| float | **`Detail_Normal_Intensity`** | `0.52` |
| float | **`Detail_AO_Intensity`** | `0.0` |

**There is no `DetailNormalMap` and no `DetailIntensity`.** Anywhere this project wrote those names,
they were wrong.

#### ⭐ The cheap positive control that replaces the whole mask gamble — ONE float

`Detail_Normal_Intensity` and `Detail_AO_Intensity` are **plain floats on the same material**, so
**setting the normal intensity to 0 disables the detail tile for that material with no texture edit
and no channel gamble at all** `[inferred-static 2026-09-07]`. The shipped AO intensity is already
`0.0`, so realistically it is **one number**.

That separates the diagnostic from the fix, which is exactly what the board was hesitating over
(*"it changes skin Tefa has already judged good"*): **band gone with the intensity at 0 → the tile is
the cause and the mask work is worth doing properly; band unchanged → the tile is not the cause and
the mask work is not worth doing at all.**

#### 🚨 And the mask we already deployed may read as zero everywhere

`pl1000_Jacket_MSK1.tex.34` (deployed 2026-09-06 15:06) is **2048 BC4**, and sampling a
**BC4_UNORM** texture returns `(R, 0, 0, 1)` — **green and blue read as 0**. **Which channel
`DetailMaskMap` is read from is not documented anywhere public** — a genuine negative from a corpus
that *does* document channel packing for ALBM/NRMR/ATOS and NRRC/ATOC, so the shape of the answer
exists and this map simply is not covered `[checked 2026-09-07]`.

**So if the shader reads `.g` or `.b`, our mask reads 0 across the whole material and kills the detail
tile everywhere — and that failure looks EXACTLY like success:** palms smoother as intended, a
regression everywhere nobody is looking. `[hypothesis 2026-09-07]` — a D3D inference about BC4, not an
observation of RE Engine's shader.

Three supporting points: there is **no channel-selector property** for this mask (where RE Engine
wants a runtime channel pick it ships an explicit vec4, e.g. `Rec_RTTChannelControl`), so the channel
is hard-coded; `NullWhite` is white in all four channels and gives nothing away; and the engine's own
masks are four-channel **`_MSK4`** (`ImperfectDetail_MSK4`, `NullGray_MSK4`) while ours is `_MSK1`.

**Cheap disambiguation if the mask route is ever resumed:** ship a **uniform mid-grey** mask first.
Whole-material grain halves → the shader reads a channel BC4 populates, and a black region is then a
trustworthy off switch. Nothing changes → it reads G or B and the mask needs a 4-channel format
before any of this means anything. (That is the cross-engine library's own *"never read back against
the neutral value"* rule applied to a texture.)

⚠️ **Two discrepancies to settle LOCALLY, not from the preset.** The preset says `Detail_UVScale`
**0.25** where this project's notes say 0.5, and its `DetailMap` is a **null** texture. But the preset
row is `pl1000_Jacket_**Mat**` while our MDF edit targets `pl1000_Body_**Mat**` — plausibly two
different materials, given how RE2 crosses these names over. **Reading our own dumped
`pl1000.mdf2.21` settles both and is free.** A third party's snapshot of a shipped material is a
strong lead and a poor authority. ⚠️ And do not assume `Detail_UVScale` is a multiplier rather than a
divisor — nothing public states the direction and every shipped value seen is sub-1, which fits
either reading.

### 7d. ⭐ The zombie "montage" system — the face pool is DATA, and a new face LOADS (2026-09-12, home PC, static + one flat run)

Drained from `inbox/2026-09-12-mod-zombie-montage-system.md` the same day it was written, then
extended with the live result. Built for the ZOMBIE VARIETY board rows (Tefa, 2026-09-12: "at least
20 heads/faces; clothing secondary; more police / sewer / scientist outfits").

**Classes** (`app.ropeway.enemy.em0000`, three kinds: em0000 male, em0100 female, em0200 police):
`Em0000MontageCatalogRegister` holds per kind a `*PartsContainer` (part key → `via.Prefab`), a
`*MontageTableData` (named outfits: face/body/shirt/pants keys + accessory keys) and a
`*CombinationRule`. `MontageManagerBase<T>` (static `get_Instance` on the
`RopewaySingletonBehaviorRoot`1<…>` base — `sdk.find_type_definition(...):get_method("get_Instance"):call(nil)`
works from Lua `[verified-live 2026-09-12, n=1]`) resolves an outfit ID + fashion seed to key names and
picks part/material variants (`getFaceRandomVariation`, `getFaceMaterialRandomVariation`, …). Runtime
works on **strings**; the `EM0000_MONTAGE_PARTS_*` enums are editor-side. `Em0000SimpleMontageBase`
is the per-zombie component (`FaceMesh/BodyMesh/ShirtMesh/PantsMesh/HatMesh`, `attachedMontageMesh`).

**Files.** `natives/STM/SectionRoot/UserData/Character/Enemy/em0000/Montage/{em0000,em0100,em0200}{PartsContainer,MontageTableData,CombinationRule}.user.2`
plus `em0000*_Zombie_AfterChapter2.user.2`. Faces live in
`natives/STM/SectionRoot/Character/Enemy/em0000/Face/FaceNN/em0050_FaceNN.{pfb.17,mesh.2109108288,mdf2.21}` +
`_ALBM/_NRMR/_ATOS.tex.34` (512² BC7/BC7/BC1). The **prefab is nothing but its name**: Face00's and
Face10's `.pfb.17` are byte-identical after a UTF-16 rename `[verified-numerically 2026-09-12]`.

**Numbers** `[measured 2026-09-12]`: male 15 face prefabs on disk (00–07, 10, 11, 14, 70–73), 14 in the
base container, 65 outfits (+3 AfterChapter2) using 00×5 01×8 02×7 03×9 04×8 05×5 06×10 07×4 11×1 14×4
and 70–73 once each (ID900–903, unique zombies); female 5 faces / 27 outfits, even; police 5 faces /
14 outfits. Enum slots with no prefab: **FACE08, 09, 12, 13, 74, 75, 76**. Bodies/shirts/pants carry
`_mm_nn` variants (BODY00_00_00…_14 = 15 skins of one body); **no face has any variant**.
`CombinationRule` pairs Face03 with BODY00_00_01 and the rest with BODY00_00_00.

**The `.user.2` (RSZ) layout, enough to write it** — `dev-archive/tools/zombies/montage_rsz.py`,
byte-identical round-trip on all nine shipped files `[verified-numerically 2026-09-12, n=9]`: USR header
(0x30, no resource/userdata tables), `RSZ` header, object table, instance infos (type id + crc), data.
Strings = u32 count incl. NUL + UTF-16LE, **empty string = count 1 + NUL**; bool = 1 byte align 1;
object arrays = u32 count + u32 ids; `via.Prefab` is its own instance (u32 0 + path) referenced by id.
Field order = il2cpp offset order. New instances go before the root; refs ≥ the insert point shift.

**Live, 2026-09-12 (two flat launches, Continue save = B1F save room by the morgue):**
- All three edited em0000 tables were **served as loose files** (`reframework_loose_files.txt`), the
  game reached gameplay, no error `[verified-live 2026-09-12, n=2 launches]`.
- `visceral_zombie_probe.lua` read the live manager: 68 outfits known, `getMontageData("ID004")` →
  **FACE08** (our re-deal), `getFacePrefab("FACE08")` → our prefab path, `set_Standby(true)` →
  `get_Ready()==true` and **all six Face08 files served loose** (pfb, mesh, mdf2, ALBM, ATOS, NRMR)
  `[verified-live 2026-09-12, n=1]`. So a face in an EMPTY enum slot, added only in data, loads
  through the engine's own path. Not yet SEEN on a rendered zombie (the save room has none in view).
- VR init without a headset present: `XR_ERROR_FORM_FACTOR_UNAVAILABLE` → clean flat fallback.

**Open:** whether `getFaceMaterialRandomVariation` prefix-matches `FACEnn_mm_kk` for faces as it does
bodies `[hypothesis]`; whether a spawn's outfit ID comes from level data (so "more police" = edit
rows, not add rows) `[inferred-static]`; Face08 has no `Em0000DirtyPreset_Face_Face08.user.2` (nor do
06/07/11/14) — expected harmless, unverified.

### 7e. ⭐⭐⭐ THE ZOMBIE FACE POOL IS NOT CAPPED BY THE ENUM — 25 NEW FACES RUN (2026-09-12, `/lm`)

§7d established the montage system and that a face in an **empty enum slot** (FACE08) loads. The
open question was whether the pool could grow past the enum at all. It can, and that is what makes
"as many faces as we like" true rather than "seven more".

**Why it works:** the runtime is all strings. `Em0000MontageData.FaceKeyName` is a `System.String`
and `MontageManagerBase.getFacePrefab(KeyName)` looks that string up in the container's
`FacePrefabs` list. `EM0000_MONTAGE_PARTS_FACE` is editor-side, used by the `EM0000_MontageData`
struct the shipped tables never go through. `[inferred-static 2026-09-12]` → **confirmed live:**
`getFacePrefab("FACE20")`, `("FACE29")`, `("FACE37")` — none of which exist in the enum — each
returned our prefab, went `standby=true`, and had all six of their files served from the loose
folder `[verified-live 2026-09-12, n=1 launch, 3 beyond-enum keys]`. The manager also reported the
re-dealt outfits using them: `ID004 → FACE30`, `ID009 → FACE29`, `ID201 → FACE75`, `ID305 → FACE21`.

**What now ships in the pack** (`dev-archive/tools/zombies/`): **25 new faces, pool of 36**, built
from the player's own archive at build time — `face_roster.py` is the recipe list, `make_faces.py`
the builder, `montage_rsz.py` the byte-faithful table writer. 153 files, 112.8 MB, deployed with a
manifest and reversible with `--undeploy`.

**Four things that cost a run each and are worth not re-learning:**
- **Only Face00–Face07 can be source heads.** `Face11` and `Face14` have a prefab but **no mdf2 and
  no mesh of their own** in the archive — they are assembled from another head's parts
  `[measured 2026-09-12]`.
- **Material 0 is not always the skin.** Face01 lists `Hair` first; every head also carries `Teeth`
  and `insidehead_Mat`. Pick by name, then by albedo path `[measured 2026-09-12]`.
- **A head's mesh is whatever its PREFAB names**, not `em0050_<Face>.mesh` by convention.
- **The container must list every key the table deals.** `FACE10` exists on disk but only the
  *AfterChapter2* container lists it, so dealing it from the base table without adding it hands the
  picker a key it cannot resolve.
- Folder and file names use Title case (`Face20`), the container key is upper (`FACE20`).

⚠️ **Still unseen on a rendered zombie.** Everything above is the loader and the manager. `ready=false`
on one of six prefabs was a sampling artefact — all six of that face's files were served. The
remaining check is a look, and it needs a save with zombies in the room.

### 7f. ⭐⭐ THE FACES ARE ON REAL ZOMBIES — and one head can hang a level load (2026-09-12, `/lm`, RPD)

§7e proved the pool is uncapped at the loader and the manager. This is the same thing on **real
spawned zombies**, and the failure found on the way is the part worth carrying.

**Live in the police-station main hall** `[verified-live 2026-09-12, n=1 launch]`, from
`visceral_zombie_census.lua`: five distinct zombies, five distinct faces, and four of the five are
ours — `GateZombiesM_Dead01 → FACE20`, `GateZombiesM_Eaten01 → FACE25`,
`GateZombiesM_Dead02 → FACE34`, plus `FACE30 / FACE36 / FACE23 / FACE28` on the first load. Every
one reports `complete=true` with a real face-mesh material list, so the montage completed and the
mesh bound. **The variety the feature exists for is on screen.**

**🚨 THE TRAP: `FACE11` and `FACE14` HANG THE RPD LOAD IF YOU DEAL THEM.** Both have a prefab but
own **no mdf2 and no mesh** (§7e) — the shipped tables only ever give them to two special outfits,
where they resolve through another head's parts. Putting them into the everyday deal stalled the
police-station load at **90 % forever**: the world streamed in (the census listed ten zombies) but
the loader never finished `[verified-live 2026-09-12, n=1]`. A control with every one of our files
removed loaded the same save into gameplay, and re-deploying the full 25-face pack with those two
keys dropped from the deal **also loaded normally** `[verified-live 2026-09-12, n=1 each]`. So the
cause is specific and proven by both directions, not by suspicion.

⚠️ **The general lesson, worth more than the two names:** the underground save had far fewer zombies
and never revealed this. **A face pool is only as sound as its worst head, and only a room that
spawns many zombies at once will show it.** Test face work in the RPD, not in a save room.

**Also measured:** the desktop window goes black whenever a headset is connected (RE2 launches into
VR), so a look-with-your-own-eyes test needs `openxr_loader.dll` parked; and the census double-counts
because it asks for the derived types *and* the base type, which `findComponents` answers separately.
Harmless, worth fixing when the file is next touched.

### 7g. ⚠️ `scene.findComponents(via.render.Mesh)` UNDER-REPORTS — get meshes from the component that owns them (2026-09-12, `/lm`)

Chasing the head hider's mesh-discovery defect produced an engine fact worth more than the row it
came from, because it invalidates a technique this project kept reaching for.

**Measured, one flat run in the RPD main hall** `[verified-live 2026-09-12, n=1]`:

```
head: walkA 25 tf / 5 mesh | walkB 0 mesh via cond.get_CostumeChanger
    | walkC 8 scene mesh / 0 player via scene.findComponents(via.render.Mesh) | walkD 0 tf / 0 mesh
head:   walkC: (none)
```

At that same moment the zombie census reported **10 live zombies**, each holding a face mesh it could
read material names off. So the scene sweep saw **8 meshes in the entire scene** while at least ten
character face meshes existed. It is not a derived-type problem either: a live zombie's
`get_FaceMesh()` reports its concrete type as **exactly `via.render.Mesh`**, parent `via.Component`
`[verified-live 2026-09-12, n=1]`.

⇒ **`findComponents` does not reach meshes inside instantiated prefabs.** Two consequences:
- **Never enumerate the scene to find a character's meshes.** It returns a plausible-looking small
  number and no error, which is the worst shape of wrong.
- **The working pattern is the one the zombie work proved: ask the component that OWNS the mesh.**
  `Em0000SimpleMontageBase.get_FaceMesh()` hands it over directly, and
  `attachedMontageMesh(Face, Body, Shirt, Pants)` is the moment it is assigned. The survivor needs its
  own analogue, found by shape.

Related and still true: `findComponents(System.Type)` matches the **exact** type, so a base class
finds nothing (§7f). Two different traps, same function, both silent.

**⭐ The survivor's owning component, found by shape (reader, drained 2026-09-12).** Of every
`app.ropeway` type in RE2's dump, exactly three hand out a `via.render.Mesh`: the player condition,
`SurvivorCostumeChanger` (both measured null outside a costume change) and
**`app.ropeway.survivor.SurvivorMeshPartsController`** (generic base `app.ropeway.MeshPartsController\`1<…>`),
which exposes **`get_Mesh()`** and **`setPartsEnable(int, bool)`** `[inferred-static 2026-09-12]`.
That is RE2's structural equivalent of `Em0000SimpleMontageBase` — it sits on the object whose mesh
it controls and names that mesh. `setPartsEnable` is also a possible future lever for hiding a part
without touching draw flags `[hypothesis]`. ⚠️ It must be reached FROM the player, not by sweeping,
for the reason this whole section exists.

### ⭐⭐⭐ AND THE HOOK ROUTE SOLVED IT: the costume changer's SETTERS carry the player's meshes

The getters read null, but `SurvivorCostumeChanger`'s **setters** hand the meshes over, and a pre-hook
on them catches the lot. One flat run in the RPD `[verified-live 2026-09-12, n=1]`:

```
head: MESH CAUGHT by CC.set_Face -> "Face" n=5: pl1050_Eyelash_Mat, pl1050_Face_Mat,
                                                pl1050_Tearline_Mat, pl1050_Eyes_In_Mat, pl1050_Eyes_Out_Mat
head: MESH CAUGHT by CC.set_Hair -> "Hair" n=3: pl1070_Hair_Mat, pl1070_Hair2_Mat, pl1070_Hair3_Mat
head: MESH CAUGHT by CC.set_Body -> "Body" n=12: pl1000_Boots_Mat, pl1000_Jacket_Mat, pl1000_Trousers_Mat,
                                                pl1000_Body_Mat, pl1000_Chain_Mat, pl1000_Holster_Mat
```

**That is Claire's head, hair and body, named.** `caught=8` in one bind — the other five catches are
other characters (`pl5700`/`pl5750` Sherry, `pl7800`/`pl7850`/`pl7870` an NPC), so **the hooks are
global and must be filtered to the player's own changer instance.**

⇒ **The rule for this engine, now proven twice:** a character's meshes are reached from the component
that owns them — and if its getters are empty, hook the moment they are handed over. Zombies:
`Em0000SimpleMontageBase.get_FaceMesh()` / `attachedMontageMesh`. Survivor:
`SurvivorCostumeChanger.set_Face` / `set_Hair` / `set_Body`. Sweeping never works.

**⭐ A second lever noticed on the way, not yet used** (reader, drained 2026-09-12):
`SurvivorCostumeChanger.setPartsEnable(int index, bool)` alongside `get_DefaultPartsEnable()` and
`get_EditIndexList()` — a **per-sub-mesh switch**, which could drop just the head parts without
touching per-pass draw flags at all `[hypothesis]`. Worth a look if the draw-flag route ever costs
us the shadow. Note `setPartsEnable` also appears in the hand-over list as receiving a mesh during
play, so it may be a second place the meshes pass through.

⚠️ **Correction carried in the same note:** the earlier claim that a scene sweep was "the ground
truth that cannot come back empty" is **withdrawn** — it can, and it did.

### ✅ THE HEAD HIDER IS DONE (2026-09-12, `/lm`, five flat runs)

Route E now feeds the hider, and the live result is the one the row has wanted since 2026-08-26
`[verified-live 2026-09-12, n=1 launch]`:

```
head:   routeE routeE:CC.set_Hair n=3: pl1070_Hair_Mat, pl1070_Hair2_Mat, pl1070_Hair3_Mat -> HIDE
head:   routeE routeE:CC.set_Face n=5: pl1050_Eyelash_Mat, pl1050_Face_Mat, pl1050_Tearline_Mat,
                                       pl1050_Eyes_In_Mat, pl1050_Eyes_Out_Mat -> HIDE
head: 2 mesh(es) hidden, shadow kept          head=1 hid=2/8 d=0.11
head: REVEAL — not first person (d=14.99 m) → REVEAL — camera off the head → HIDE again (d=0.11 m)
```

Face and hair hidden, **body kept**, **shadow kept**, and the reveal triggers still fire and clear.

**Two things cost a run each and are the reusable part:**

1. **⚠️ THE CATCHES ARRIVE AFTER THE SCAN.** The one scan per player bind ran at `16:15:17.829`; Claire's
   face was handed over at `16:15:17.943` — **114 ms later** `[verified-numerically 2026-09-12]`. A route
   that only reads the catch list at scan time finds it empty and *silently hides nothing*. The hider now
   takes new catches as they land, and re-takes on every bind.
2. **⚠️ `argv[0]` IS NOT THE INSTANCE on these hooks.** Claire's Body, Hair and Face catches carried three
   DIFFERENT `argv[0]` pointers (`…1B0D3B60`, `…1AFB8760`, `…1B59F4E0`), none of them the player's own
   costume changer (`…0C761520`) `[verified-numerically 2026-09-12, n=3]`. So the hooks give no usable owner,
   and **the catch list holds every character** — Claire, Sherry, an NPC.

   **The fix turns the original problem inside out:** walking DOWN from the player never reached the meshes
   (§7g), but walking **UP** from a caught mesh is a short, certain climb — `mesh → GameObject → Transform`,
   then parents until one is the player's transform. That is identity, not a material-name guess, which
   matters because Claire is `pl1000/pl1050/pl1070` and Leon and the alternate costumes are not. Hiding an
   NPC's face would be far worse than failing to hide the player's.

**Left as a knob:** hair hides with the face by default (a floating hairstyle is as wrong as a floating
head); `g_head_hide_hair` turns it off.

**Head-hider state:** DONE flat. The reveal gate is correct and discovery is solved. Three routes failed first
and are recorded so nobody retries them — the transform walk (finds only our own injected objects plus
`Transceiver` and `FlashLight`), the costume-changer getters (resolve, return null), and the scene
sweep (under-reports). Remaining work is wiring the hide to the caught face (and hair) mesh, filtered
to the player, keeping the shadow, and clearing the cache on re-bind.

### 7h. ⚠️ THE FACE PACK STALLS THE RPD IN VR, AND THE HEAD'S SHADOW IS NOT KEPT (2026-09-12 evening, Tefa wearing it)

Two hard results from Tefa's own session, both of which flat testing had missed.

**1. The 25-face pack stalls the police-station load in VR, and only in VR.** Controlled both ways on
the same save, in the headset `[verified-live 2026-09-12, n=2 loads each]`: with the pack deployed the
loader freezes at **90 %** (no new file requested for minutes, the world visible behind the notice);
with every face file removed the same save **loads normally**. ⚠️ The same save loaded fine FLAT with
the same pack several times the same afternoon, so **flat is not a valid test for this**.
Reshaped heads made it worse, but they are not the cause — the shapes-off pack stalls too.
Face pack currently **REMOVED**; everything else (hands, bracelets, plug, head hider) left in place.

⚠️ **And a process lesson worth more than the bug:** an earlier stall that evening was caused by
**deploying 153 files into the game while Tefa was mid-load**. Never deploy into a running game.

**2. ⚠️ CORRECTED THE SAME EVENING — THE SHADOW *IS* KEPT ON A CLEAN LOAD. What follows was read too
quickly.** Tefa's own follow-up is the thing that decides it: *"first load after launching the game the
head shadow is there, when i load a game then it's gone, even if i load the same save i loaded when the
game first launched"* `[verified-live 2026-09-12, n=1 wearer]`. **The save is not the variable; whether
a level has been loaded before in this process is.** So the per-pass draw-flag technique works, and what
breaks it is our own stale state across a re-bind (§7i). The 2026-08-26 kill condition is **not** met,
and the row below stands only as the symptom that led here.

**2b. The symptom as first written (superseded by 2 above):** The plugin
logs `2 mesh(es) hidden, shadow kept` (DrawDefault off, DrawShadowCast left on) and Tefa reports **no
head shadow** on the same loads `[verified-live 2026-09-12, n=2 saves]`. So clearing the default draw
flag drops the shadow with it on this build, whatever `DrawShadowCast` says. The 2026-08-26 row's kill
condition was exactly "head gone but no shadow", and until today the head had never actually been
hidden, so it could never be tested. ⭐ The alternative is already identified: §7g's
`SurvivorCostumeChanger.setPartsEnable(int, bool)`, a per-sub-mesh switch that might drop the head
parts without touching draw flags at all `[hypothesis]`.

**3. Other observations from the same session, all `[reported 2026-09-12, n=1 wearer]`:**
- **The bracelets do not follow the wrist's twist**, while Claire's own watch does — ours are built in
  the radius's local frame (`visceral_bracelet_*_radiuslocal.mesh`), so this is an attachment-bone
  question, not a mesh one.
- **Bracelets appear on some loads and not others, and the pattern is not the level** — absent on a
  clean-skin save, present after loading a dirty-skin save and then returning to the same clean save.
  State is carrying across loads. The plugin's own `brac=on` is not evidence they are visible.
- **The HD hand textures were vanilla on the later save**: only 2 requests for `pl1000_Jacket_ALBM` in
  the whole session. Same shape as the 2026-09-10 report.
- **"Can see through the body" on one load only.** Not our hider: the log shows only `set_Face` and
  `set_Hair` were ever marked HIDE, never `set_Body`, on every load that session.
- The **neck plug is created and drawing** (`PLUG CREATED`, `DrawDefault=1`, 12 materials from
  `pl1000.mdf2`) yet is not visible where the head was — so it is a placement or scale problem, not a
  missing object.

### 7i. ⭐⭐ "FIRST LOAD vs EVERY LOAD AFTER" IS OUR OWN STATE, NOT THE GAME'S (2026-09-12, `/pd`, static)

Tefa separated the variable that had been confusing three different symptoms all day: **it is not which
save, it is whether a level has already been loaded in this process** `[verified-live 2026-09-12, n=1 wearer]`.

| | first load after launching | every load after |
| --- | --- | --- |
| head shadow | **present** | gone |
| bracelets | **absent** | present |

Two independent bugs in our own code produce exactly that, and both are fixed statically
`[compile-verified 2026-09-12]` — neither has been run.

**a. The caught-mesh list was never cleared.** `g_catch` is global; `rebind_player()` reset the head
state but not the catch list, so every load after the first inherited the PREVIOUS level's mesh
pointers — dead objects that route E would then read and write draw flags on. Worse, `g.head` was
**wiped before** anything was restored, so the only record of which meshes we had altered, and what
their flags were, was thrown away at the moment it was needed. Now: restore first, then clear the catch
list, and a generation counter keys the per-frame bookkeeping so a new life cannot inherit the old
one's indices.

**b. The bracelets got exactly one creation attempt per bind.** `bracelet_create()` set `tried = true`
on entry, and the caller skipped forever after. Fired the instant the player binds, that first attempt
lands before the arm's joints are ready — and there was no second chance until the next level load,
which is precisely why they appear on the SECOND load and not the first. Now six attempts, half a
second apart, then give up with a log line saying so. The neck plug has the same one-shot shape and is
the obvious next candidate.

⭐ **And the bracelet twist is a knob, not a bug.** Tefa: they do not turn with the wrist while Claire's
own watch does. By design — the bracelet is pinned to the **radius** joint because the wrist joint
carries the hand's flexion, which a bracelet must not follow, and `k`, the fraction of wrist twist fed
back in, ships at **0** as the safe baseline. `NUM-` cycles it. It has never been judged because until
now the bracelets were never reliably visible in a run anyone was watching.

### 7j. Head MESH deformation is lossless, and 15 shapes are measured — but the game will not load them (2026-09-12)

Drained from `inbox/2026-09-12-reader-head-mesh-roundtrip-and-shapes.md` the same day. Tooling:
`dev-archive/tools/blender/head_roundtrip.py` (RE Mesh Editor V0.66 in headless Blender 5.2, the same
pattern as `tex2png.py`) and `dev-archive/tools/zombies/head_shapes.py` (16 recipes).

**The round trip is free.** Import a head, re-export to the same mesh version, re-import and diff:
same objects, same vertex count, same faces, both UV layers identical, **every bone weight identical**,
the whole skeleton (63–77 bones) identical, file size within 16 bytes, worst vertex drift 0.0003 mm
`[verified-numerically 2026-09-12, n=8 heads]`. So editing only positions cannot damage the skin, the
rig or the material split.

**⚠️ The obvious seam rule fails on these heads.** A head's skin is one open shell whose boundary runs
unbroken from the neck over the top of the skull, so "do not move the bottom" has no well-defined
bottom. The recipes instead freeze everything at and below the **neck bone** and fade to full effect at
the **head bone**, using the face bones that sit in the same places on all eight heads.

**15 shapes** (broad, gaunt, heavy_jaw, long_skull, big_nose, snub_nose, brow_ridge, hollow, bloated,
lantern, pug, weak_jaw, flat_skull, small_head, big_head) on seven dials. Measured across 56
head/recipe runs and again when wired into the pack: **max vertex move 9–29 mm** — enough to read as a
different person — with **0.000000 mm at the neck ring and everywhere below it**, and vertex count,
bone-weight count, UV layers and bone count unchanged `[verified-numerically 2026-09-12]`. Costs no
pack size: each face already ships its own copy of a head mesh.

**🚨 AND THE GAME STILL WILL NOT LOAD THEM.** Deployed with shapes, the RPD save froze at 90 % in VR;
with shapes off it froze too, so the shapes are **not** the cause of that stall (§7h) — but they were
never seen working either, and the shapes-off pack is the only version that has ever loaded. So
everything above is a **capability**, not a shipped feature: the mesh writer produces a file that
passes every check we can make, and no run has yet shown the game accepting one. Until §7h's stall is
understood, do not read "the round trip is lossless" as "the game takes our meshes".

⭐ One lever noticed and unused: `setPartsEnable(int, bool)` alongside `get_DefaultPartsEnable()` /
`get_EditIndexList()` — a per-sub-mesh switch `[hypothesis]`.

### 7k. ⭐⭐ OUR SPAWNED MESHES NEVER GET A MATERIAL — `set_Material` is accepted and does nothing (2026-09-12, `/lm`, flat)

This is one bug under three symptoms the board has carried separately for a week: the grey flickering
bracelets, the bracelets "absent on the first load", and the neck plug that draws but cannot be seen.

**Measured, one flat load** `[verified-live 2026-09-12, n=1 launch, 3 objects]`. v0.17 re-creates the
material holder and re-applies it whenever the mesh reports no materials — eight times, half a second
apart. All three objects:

```
plug:       material STILL empty after 8 attempts (set_Material ok) — it will draw nothing
bracelet l: material STILL empty after 8 attempts (set_Material ok) — it will draw nothing
bracelet r: material STILL empty after 8 attempts (set_Material ok) — it will draw nothing
```

⇒ **`set_Material` returns OK every time and `get_MaterialNum()` stays 0 for four seconds. It is not a
timing problem, and a retry does not fix it.** ⚠️ That withdraws the working theory from earlier the
same day, that the material simply had not finished loading.

**What is ruled out:**
- **Not the reading method** — `get_MaterialNum`/`getMaterialName` return 12 real names on Claire's own
  body mesh in the same session (§7g).
- **Not a missing file, and not the loose loader** — `visceral_bracelets.mdf2.21` is present under
  `natives/STM/visceral/` and the log shows it **served as a loose file** in the same run.
- **Not the holder route** — the code A/Bs the two of them in one launch (left bracelet "manual +0x10",
  right the older `create_holder`), and **both report 0** `[verified-live 2026-09-12]`.
- **Not the mesh** — `setMesh` clearly takes: the bracelets are visible as **grey** geometry when they
  appear at all, and grey is exactly what a mesh with no material draws. That reframes the old
  "bracelets went grey and flickered" row as this same defect, not a separate one.

**The live hypothesis, and the next check.** RE Engine binds a `.mdf2`'s materials to a mesh **by
material name**, so a mesh whose internal material slots are not named exactly as the MDF's materials
binds nothing and reports zero `[hypothesis]`. Our MDF declares `visceral_bracelet_leather`,
`visceral_bracelet_metal`, `visceral_bracelet_leather_red`, `visceral_bracelet_leather_purple` (plus
the pl1000 set it was cloned from). What our own `.mesh` files call their slots is **not yet read** — a
crude ASCII scan of the mesh finds no name table, so it needs RE Mesh Editor. That comparison is the
next step and it needs no game.

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
  the wrist solver that consumes the getter chain above `[hypothesis]` as to its name, `[verified-live 2026-09-05, n=2 weapons]` as to its behaviour.
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
`get_HighestWeightMotionNode():get_MotionName()`. ⚠️ **CORRECTED 2026-09-09** (`/gr` drop drained). This
paragraph used to read: *"Because RE2 locomotion is root-motion driven (§8), a playback-rate clamp
scales travel, leg cycle and footstep events together — req 4's 'drive legs and footsteps from
speed' holds by construction."* **"By construction" does not survive the public source.**

**The lever is a PAIR.** Junh2x's shipping mod hooks `app.MovementDriver:getMoveSpeed` as a
first-class part of the feature alongside the layer-0 `set_Speed` write, applying the same factor
to both; `Namsku/re-engine-trainer` does the identical pairing **independently** for its Requiem
"Player Speed" feature, with separate walk/run factors and a multi-frame restore of the layer rate
on disable `[reported 2026-09-09, from source]`. Two authors would not both scale the driver's own
returned speed if clamping the layer rate already moved the character. So the **animation rate and
the travel rate look separately driven**, and keeping them in sync is the mod's job. Plan req 4 as a
pair until RE2 is measured otherwise. (The same drop also corrects this lane's 2026-09-02
characterisation of the `getMoveSpeed` hook as "Requiem-specific" — it is not, it is half the
mechanism.) ⚠️ This is evidence about the technique's **authors**, not a measurement of RE2: it
downgrades "holds by construction" to `[hypothesis]`, it does not disprove it for RE2.

It is a rate, not a walk/run blend.

**✅ THE FIRST HALF IS NOW MEASURED LIVE (2026-09-09, `/lm`, one flat launch).** The NUM7 probe's
`via.motion.Motion` surface carries, on the inherited **`via.motion.Animation`** base:

```
via.motion.Animation :: System.Single get_PlaySpeed()
via.motion.Animation :: System.Void   set_PlaySpeed(System.Single value)
via.motion.Animation :: System.Single get_SecondaryPlaySpeed()
via.motion.Animation :: System.Void   set_SecondaryPlaySpeed(System.Single value)
via.motion.Animation :: System.Single get_CurrentPlaySpeed()
```

with a live read of **`get_PlaySpeed = 1.0000`** — a real float, not the `NaN` the probe prints for an
absent method. `[verified-live 2026-09-09, n=1 launch]` So req 4 **does** have a component-wide speed
lever above every layer: one setter, no layer index, no motion-name gating. `getLayerCount = 6` and
all six layers independently read `get_Speed = 1.0000`, so the per-layer levers exist too and are
currently neutral.

⚠️ **The SECOND half is still unmeasured.** The probe dumps the motion component only and never
touches `app.MovementDriver`, so nothing here confirms or denies the pairing above. Extending the
probe to dump `app.MovementDriver` is one `[PD]` edit and should happen before req 4 is designed.

⚠️ Incidental, recorded because it will bite the first blend-weight work: **`get_Weight` returned
`nan` on all six layers** — either absent under that spelling or not a float. `[verified-live 2026-09-09]`

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
- **Open until one flat run** (narrowed 2026-09-11 by draining the `/gr` 2026-09-07 drop — **two of the three clauses are now answered from public sources and have been removed**; what is left is):
  whether the aim-walk blend drives phase by frame or normalised time (walk loops are 3–6× longer).

#### ⭐ 8f drain, 2026-09-11: motions are addressed by NUMBER, and `motSize` is vestigial

From the `/gr` 2026-09-07 drop. **This answers two of §8f's three "open until one flat run"
questions without a launch, and it makes item 22's expected outcome the predicted one rather than a
coin flip.**

- **Lookup is by `(bankID, motionID)` — a numeric pair — not by a motion-name hash.** Four independent
  public sources agree `[reported 2026-09-07]`: the motion's UTF-16 name is carried in the file but is
  not a lookup key, and there is **no motion-name hash field** in the documented structure. The murmur3
  hashing in these files is for **bone** names, and the one hash-keyed lookup that exists —
  `findMotionBankByNameHash` — hashes the **motlist/bank** name, i.e. is file-level. Sources:
  alphaZomega's MMDK and his 010 Editor motlist template, plus the RE2R Custom Animation Framework's
  `actor_motion_systems.md` and `motlist_format_guide.md`.
  ⇒ **Item 22's outcome (a) is the predicted one**, and "frozen / T-posed legs ⇒ the game keys by name
  hash" becomes the *surprising* branch. ⚠️ Not a proof: no public source names every dword of the
  72-byte collection entry, so a per-slot hash in an unnamed field is not formally excluded
  `[hypothesis]` — though §8f's own byte-identical comparison argues hard against it.
- **`motSize` is vestigial in v524, so the question dissolves rather than being answered.** §8f measured
  a real value with 0 on each file's first entry `[measured 2026-09-06, n=5]`; two public writers
  (alphaZomega's Motlist-Tool, CAF's mot writer) emit **0 for every entry** in this generation with no
  first-entry special case, and CAF's spec says the field is only populated in the older RE2 **v65**.
  Both are true at once, and the reading is that **the engine does not read it in v524** — tolerated at
  Capcom's genuine value and at zero `[hypothesis 2026-09-07]`. Deliberately not stronger: "tools that
  zero it are in general use and their mods work" is inference from their public use, not a controlled
  test. ⭐ **Either way our splice tool is already correct**, because it preserves each blob's own value
  — right if the field is ignored and right if it is read. **So the `motSize` clause is dropped from
  item 22's outcome (c), leaving that outcome as "loose file not taken".**
- **⭐ `motNumber` is a u16 at +0x08 and `Switch` a u16 at +0x0A** — the public template names the second
  half of the u32 that §8f recorded as merely "varying" `[reported 2026-09-07]`. It also independently
  confirms the **72-byte stride for version ≥ 486** and one collection entry per slot. **A future splice
  that changes a motion number is writing a u16, not a u32.**
- **One field §8f does not list, worth reading before the launch:** the mot **entry header** carries a
  **`blending`** field the public template annotates as *"set to 0 to enable repeating"*
  `[inferred-static 2026-09-07]`. Relevant to the aim-walk loop question directly.
- ⚠️ **Do NOT re-raise the collection block's 15 unnamed dwords.** The public template shows 15 dwords
  of float/uint payload per entry after `motNumber`, which invites the worry that per-slot frame or
  blend data is kept verbatim while entry lengths change 3–6×. **§8f already disproved it**: the block
  is byte-identical between Claire's original and the Jill replacement whose entries all differ in size
  `[measured 2026-09-06]`. Recorded so the next reader of the template does not spend time on it.

#### 8g. The `.rtex.5` descriptor — decoded, and a larger authored size is no longer a hypothesis

Drained 2026-09-11 from the `/gr` 2026-09-07 drop **together with its own 09-07b correction**, so the
withdrawn field names are not recorded here at all.

- **⭐ The "decode the format first" branch of the grime row is already done**, on the sibling
  `re-village-scope-vr` project, byte-for-byte verified on six shipped RE8 files — and **there is no
  size-dependent field in a v5 descriptor**, which is the part that mattered. The format is also
  **documented publicly**: kagenocookie's RE-Engine-Lib, `REE-Lib/OtherFiles/RTexFile.cs`
  `[reported 2026-09-07]`. Agreed across both: `0x0C` DXGI format, `0x10`/`0x14` width/height.
- ⚠️ **But the field names at `0x18`–`0x24` are a GUESS THAT FITS, not a measurement**, and the
  earlier wording "mip count is a flat `1`" is withdrawn. Every observed value in that range is `1`,
  which is consistent with several different readings, so the observed bytes cannot distinguish them
  `[hypothesis 2026-09-07]`. Treat `0x18` as **depth** on kagenocookie's authority rather than ours.
- **⭐ And the sibling project proved the lever live:** an authored, non-shipped larger target size is
  **accepted by the engine** — so §7b's old `[hypothesis]` that "a loose rtex with a larger size is the
  untested lever" is **upgraded**, on a sibling result on the same engine rather than on our own run
  `[verified-live 2026-09-06, on re-village-scope-vr]`.

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
