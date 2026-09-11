# The grip write is the documented value-type COPY trap, and the camera's live pose is on JOINT 0

**From:** `/gr` (estate sweep, 2026-09-11) · **For:** the modding lane, to fold into
`ENGINE-DOSSIER.md` — the REFramework-Lua section and the VR-pose-shim notes

**Full write-up:** [`external-research/topics/2026-09-11-both-headset-defects-are-documented-reframework-traps.md`](../../external-research/topics/2026-09-11-both-headset-defects-are-documented-reframework-traps.md)

Both ⭐⭐ `[PD]` rows from the 2026-09-10 headset run are documented public failure modes. Three
suggested dossier changes, smallest first.

## 1. The grip write — add the copy trap as a named hazard

The REFramework book states a value type you read is **“just a local copy”** and that mutating it
**“does not change anything in-game”** `[reported 2026-09-11, official docs]`. Small value types are
also auto-converted on return (`via.vec3` → `Vector3f`, `via.mat4` → `Matrix4x4f`), which is what makes
the copy *feel* like a handle. This alone accounts for “the shim never wrote the grip slots” with no
second explanation needed.

**⭐ And the suggested fix is not a field write at all.** `scripts/re8_vr.lua` — REFramework's own RE8
VR script, the most mature public “hands follow the controllers” implementation on this engine —
drives every hand/weapon/camera bone by caching `via.Joint:set_Position` (a `Vector3f`) and
`set_Rotation` (a `Quaternion`) method definitions at file scope and calling them per frame.
**There is no `set_field` on a joint transform anywhere in it** `[inferred-static 2026-09-11]`.

## 2. ⭐ The ordering half — and it is a stronger claim than “pick the right callback”

That same script writes the hand pose **three separate times per frame**, at progressively later
application entries: `UpdateMotion` (hand positioning), `PrepareRendering` (hand IK),
**`LateUpdateBehavior` (final, authoritative)**. That is what you do when the engine's own motion and
IK passes will overwrite an earlier write.

⚠️ **Two cautions that bound how much to trust the list.** The entry set *and its order* are
discovered at runtime by pattern-scanning and are **game- and build-specific**
(`shared/sdk/Application.cpp`, stride `0xD0` for TDB < 74, `0xC8` for TDB ≥ 74)
`[inferred-static 2026-09-11]`, and **no ordered `via.ModuleEntry` list is published for RE2 Remake**
— so dump ours locally with a logging callback on every candidate name plus a frame counter. Also,
REFramework's VR scripts had a **timing race fixed on 2026-03-05** (*“VR Scripts (RE2/RE7/RE8): Fix
racy behavior in hooks causing jitter”*); reading that diff is probably worth more than more searching.

Also: `RETransform:set_position(position, no_dirty)` — **`no_dirty` is documented as necessary when
the scene is locked**, so a write from a `LockScene`-phase callback is in a different regime.

## 3. The constant camera read — re-fetch per frame, and read joint 0

`sdk.get_primary_camera()` resolves `via.SceneManager` → `get_MainView()` → `get_PrimaryCamera()`;
**method definitions are cached for speed, the camera object is not** `[inferred-static 2026-09-11]`.
`re8_vr.lua` calls it **every frame, never cached**, then walks
`get_GameObject()` → `get_Transform()` → `get_Joints()[0]` and operates on **joint 0**.

Ranked causes of a constant reading: (1) a handle fetched once at script load — `utility/RE2.lua`
re-acquires player/weapon/inventory every frame and clears caches when the player goes unavailable,
precisely because these go stale; (2) reading the wrong node — `transform:get_position()` can return a
rig origin that genuinely never moves while the live pose is on joint 0, **which fits “constant, not
nil, not garbage” better than anything else**; (3) a different camera from the one gameplay drives
`[hypothesis]` — `utility/RE8.lua` reaches the camera through postural-camera-motion-controller hooks
rather than the scene view, which is suggestive but **no public source states RE Engine has distinct
gameplay/cutscene/render cameras**; (4) reading too early — weak fit, that gives a one-frame-late
value, not a constant one; (5) REFramework's own VR camera override already active `[hypothesis]`.

**⭐ One log line decides it:** print the camera object's `get_address()` beside the position each
frame. **Address constant across a scene transition ⇒ cause 1. Address moving while the position does
not ⇒ cause 2 or 3.**

## 4. ⚠️ Three negatives the dossier should carry, because they bear on claims we already hold

- **Nothing public demonstrates `via.motion.Motion`, `via.motion.IkLeg`, `via.motion.IkArmFit`,
  `RequestSetJointPose` or `setJointPose`** — searched for specifically, zero documentation and zero
  example usage `[reported 2026-09-11]`. Our static work found some of these names and they may be
  perfectly correct; the point is narrower: **the dossier must not cite public practice as support for
  them.**
- **`write_valuetype` is not in the book or in the Lua binding source at all** `[inferred-static
  2026-09-11]`. It appears only in loose secondary summaries. Do not build on it. The real primitives
  are `set_field`, `sdk.set_native_field`, and `ValueType`'s offset writers.
- **No public mod disables a motion bank or IK before writing a joint.** praydog's RE7/RE8 VR does not
  — it conditionally *skips* its own hand-IK updates in cutscenes. alphaZomega's **EMV-Engine**
  agrees from another direction: its “Freeze” feature works by *constantly setting the same value
  every frame*. **The public technique is “write after IK, every frame, repeatedly”, not “disable IK,
  then write once.”**

## 5. And one API hazard to rule out on the grip path

The docs state **“`ByRef` parameters are not correctly supported by REFramework”** — they behave as
`T**`. The workaround is `sdk.to_valuetype(ptr, "System.UInt64")` and reading its `mValue`, and **for
`out` parameters this only works in a post-hook**, stashing the reference during the pre-hook. If any
slot on the grip path is by-ref, a naive write there fails in exactly the way we observed.

⚠️ **Currency:** commits dated **2026-04-25** read *“REFramework v2 (#1609)”* and *“move scripts to
`dev/`”*, with internal C++ renames. Whether v2 changes the **Lua** surface could not be determined,
and the `scripts/` paths cited here may have moved. Check against the build actually injected.
