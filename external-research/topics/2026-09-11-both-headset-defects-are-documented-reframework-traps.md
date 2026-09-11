# Both 2026-09-10 headset defects are documented REFramework traps, and praydog's own RE8 VR script shows the working shape

**Researched:** 2026-09-11 (`/gr`, estate sweep) · **Project:** `visceral-re2-vr` · **Engine:** RE Engine (Capcom), via praydog's REFramework

## Why this matters here, specifically

The 2026-09-10 headset run produced exactly two ⭐⭐ `[PD]` defects: **the Lua shim never wrote the
grip transform slots**, so the weapon dock never fired, and **the camera read came back constant**,
which killed both view re-basing and the head hider. Neither is a mystery in public sources. The
first is a failure mode the official documentation states in one sentence; the second has a known
cause with a cheap discriminator. And the reference implementation for both is a script shipped in
REFramework itself, for a sibling game on the same engine.

---

## 1. The grip write: a value type you read is a DETACHED COPY, and that is documented

The REFramework book's `ValueType` page says it outright — a `ValueType` is *"just a local copy"*, and
*"this does not change anything in-game. You'll need to pass the ValueType somewhere that would make
use of the changed data"* `[reported 2026-09-11, official docs]`.

So the sequence our shim almost certainly performs — get the field, mutate it, move on — writes to a
temporary and is **silently lost**. Nothing returns an error. This matches the observed symptom
("never wrote the slots at all") without needing any other explanation.

Compounding it: small value types are **auto-converted on return** — `via.vec2/3/4` arrive as Lua
`Vector2f/3f/4f`, `via.mat4` as `Matrix4x4f` `[reported, official docs]`. That makes the copy feel
like a handle, because it is a first-class Lua object you can read and write happily.

### The three routes that actually commit, in ascending order of reliability

1. **`set_field` on the OWNING object** (not on the copy). The Lua binding source has an explicit
   branch that copies value-type bytes straight into the object `[inferred-static 2026-09-11, read
   from `src/mods/bindings/Sdk.cpp`]`.
2. **Read → modify → commit through the setter.** This is what REFramework's own
   `scripts/re2_vr_melee.lua` does for a capsule: `set_field` the members of the copy, then **call
   `set_Capsule(...)` on the owning shape object**. That trailing setter call is the entire
   difference between working and silently lost `[inferred-static 2026-09-11, read from the shipped
   script]`.
3. **For native `via.*` structs, use the native field API** — `sdk.get_native_field` /
   `sdk.set_native_field`, which the docs state is *required* for ValueType fields rather than
   direct access; `sdk.call_native_func` is the native counterpart to `call` `[reported, official
   docs]`. `scripts/re8_vr.lua` uses exactly this to write a look-ray's `from` member.

### ⭐ But for a JOINT, praydog does not write fields at all — he calls methods

This is the highest-value single finding for our dock. `scripts/re8_vr.lua` — the most mature public
"hands and weapon follow the controllers" implementation on this engine — drives every hand, weapon
and camera bone by caching two method definitions once at file scope:

- `sdk.find_type_definition("via.Joint"):get_method("set_Position")` (takes a `Vector3f`)
- `sdk.find_type_definition("via.Joint"):get_method("set_Rotation")` (takes a `Quaternion`)

and then invoking them per frame as `method:call(joint, value)`. **There is no `set_field` on a joint
transform anywhere in that script** `[inferred-static 2026-09-11, read from the shipped script]`.
Joint lookup in the wild is `getJointByName` / `getJointByHash` / `get_Joints`; `re2_vr_melee.lua`
uses the hash form for its per-frame hot path and the name form otherwise. There is no blessed
wrapper for this in `scripts/utility/GameObject.lua`, so everyone rolls their own.

The component-level `RETransform` also exposes `set_position(position, no_dirty)` (a `Vector4f`),
`set_rotation(rotation)`, and — useful to us — `calculate_base_transform(joint)`, which returns the
joint's **reference/T-pose matrix** in the transform's local space, i.e. a rest pose to offset from
`[reported, official docs]`. ⚠️ **The `no_dirty` flag is documented as necessary when the scene is
locked**, so a write issued from a `LockScene`-phase callback is in a different regime from one
issued elsewhere.

---

## 2. The ordering half: write AFTER the engine's motion pass, every frame, and expect to write more than once

`re.on_pre_application_entry(name, fn)` / `re.on_application_entry(name, fn)` fire around named
*application entries* — the engine's own logic-loop points `[reported, official docs]`. What
`re8_vr.lua` registers is effectively the answer to "which one":

| entry | what it does there |
| --- | --- |
| `UpdateBehavior` | gesture handling, pointer refresh |
| `UpdateHID` | controller input |
| `UpdateMotion` | **refreshes hand positioning** |
| `PrepareRendering` | **updates hand IK** |
| `LateUpdateBehavior` | **final hand IK update — the authoritative last word** |
| `LockScene` | muzzle data, crosshair positions |
| `UnlockScene` | roomscale movement |

**The hand pose is written three separate times per frame, at progressively later points**
`[inferred-static 2026-09-11]`. That is not redundancy for its own sake — it is what you do when the
engine's own motion and IK passes will overwrite an earlier write. By contrast `re2_vr_melee.lua`
*reads* late (`BeginRendering`) and *decides* early (pre-`UpdateBehavior`).

⚠️ **Two cautions that change how much of this to trust:**

- **The entry set and its ORDER are discovered at runtime and are build-specific.**
  `shared/sdk/Application.cpp` pattern-scans for the `Application::Function` array (stride `0xD0`
  for TDB < 74, `0xC8` for TDB ≥ 74) with a brute-force fallback, validating against known names
  `[inferred-static 2026-09-11]`. **No published ordered `via.ModuleEntry` list exists for RE2
  Remake** — the researcher looked in the book, three wiki revisions, releases and GitHub search.
  So the table above is a *shape to copy*, not an order to rely on. We can dump the real one locally:
  register a logging callback on every candidate name with a frame counter.
- **REFramework's own VR scripts had a timing race fixed recently** — a commit dated **2026-03-05**,
  *"VR Scripts (RE2/RE7/RE8): Fix racy behavior in hooks causing jitter"* `[reported 2026-09-11]`.
  Reading that diff is likely worth more than any further searching.

### Against our own dossier

This does **not** contradict anything recorded; it fills a gap. Worth noting explicitly, because the
board has wondered about it: **nothing public shows anyone disabling a motion bank or IK component
before writing a joint.** praydog's RE7/RE8 VR does not — it conditionally *skips* its own hand-IK
updates in cutscenes rather than disabling the engine's systems. alphaZomega's **EMV-Engine**
corroborates the philosophy from a different direction: its "Freeze" feature works by *constantly
setting the same value every frame*. **The public technique is "write after IK, every frame,
repeatedly", not "disable IK, then write once."** `[reported 2026-09-11]`

⚠️ And a direct negative result on our own dossier's vocabulary: the researcher searched
specifically for `via.motion.Motion`, `via.motion.IkLeg`, `via.motion.IkArmFit`,
`RequestSetJointPose` and `setJointPose` and found **zero** public documentation or example usage of
any of them. They may well be correct engine type names — our own static work found some of them —
but **no public source demonstrates them**, so the dossier should not cite public practice as
support. Likewise `write_valuetype`, which appears in loose secondary summaries, **is not in the book
or in the Lua binding source at all** `[inferred-static 2026-09-11]` — do not build on it.

---

## 3. The constant camera read: re-fetch per frame, and read JOINT 0

`sdk.get_primary_camera()` is documented as *"the current camera being used by the engine"*, and the
source shows the real chain: `via.SceneManager` singleton → `get_MainView()` → `get_PrimaryCamera()`
→ `RECamera*`, null if the main view is unavailable. **Type definitions and methods are cached for
speed; the camera object itself is not** `[inferred-static 2026-09-11, read from
`shared/sdk/SceneManager.cpp`]`.

What `re8_vr.lua` does, and both halves matter:

1. **Calls `sdk.get_primary_camera()` every frame, inside the callbacks — never cached.**
2. **Walks `get_GameObject()` → `get_Transform()` → `get_Joints()[0]` and operates on JOINT 0 of the
   camera's transform**, reading its pose and writing it back with `via.Joint.set_Position` /
   `set_Rotation` to apply the HMD.

### The ranked causes of a constant reading, and the one-line test that separates them

1. **A handle fetched once at script load.** Strongest candidate. The object valid at autorun time
   belongs to whatever scene existed then (title/loading), and scene changes invalidate it.
   Corroborated by convention: `scripts/utility/RE2.lua` re-acquires player, weapon and inventory
   **every frame** and **clears its caches when the player goes unavailable**; `re8_vr.lua` calls
   `update_pointers()` every frame and re-initialises when pointers are invalid. That defensive shape
   exists precisely because these handles go stale.
2. **Reading the wrong node of the hierarchy.** `transform:get_position()` on the camera transform
   can return a parent/rig origin that genuinely never moves, while the live pose lives on **joint
   0**. **This fits "constant, not nil, not garbage" better than anything else.**
3. **Reading a different camera from the one gameplay drives.** `get_primary_camera()` goes through
   `get_MainView()`, so it is the render/main-view camera, whereas `scripts/utility/RE8.lua` reaches
   the camera indirectly through *postural-camera-motion-controller* hooks — which implies the
   gameplay camera is a different object from the one the scene view hands you. ⚠️ **But no public
   source actually states that RE Engine has distinct gameplay / cutscene / render cameras**, so this
   is `[hypothesis]`, not a reported fact.
4. **Reading too early in the frame** — weak fit: that yields a one-frame-late value, not a constant
   one, unless the camera's own updater never runs.
5. **REFramework's own VR camera override already active** (RE2 is a natively-supported VR title), so
   our read may sample a pre-override slot. No documentation of the interaction found
   `[hypothesis]`.

**⭐ The discriminator, and it costs one log line:** print the camera object's `get_address()` beside
the position every frame. **Address constant across a scene transition ⇒ cause 1. Address changing
while the position does not ⇒ cause 2 or 3.** That single line decides between the two leading
explanations without building anything.

---

## 4. How to make a silently-failed write announce itself

Our shim failed silently, which is the real defect behind the defect. Five techniques, all from
documented primitives:

1. **Resolve by definition, not by string, at file scope.** `sdk.find_type_definition("T")` returns
   nil for a bad type, `:get_method(...)` / `:get_field(...)` nil for a bad member. Caching those at
   load and asserting non-nil turns a typo — which a string-based `obj:call("set_X", v)` can swallow
   — into an immediate, loggable nil. The docs also recommend this caching as a **best practice for
   performance**, so it costs nothing.
2. **Read back through the getter immediately.** The only thing that distinguishes "the call
   succeeded but wrote a copy" from "the call wrote the real object". Nothing in the API reports it.
3. **Read back AGAIN at a later entry point.** If the value verifies immediately but not at
   `LateUpdateBehavior`, the problem is ordering, not copying. **This one test separates our two
   candidate diagnoses.**
4. **Log addresses, not just values** (`get_address()` on managed objects, `address()` on value
   types).
5. **Count the callback.** Given the symptom is "never wrote the slots at all", a frame counter
   incremented inside the grip-write callback answers "did this code run" before anyone argues about
   what it wrote. `re.msg(text)` raises a blocking MessageBox if an unmissable tripwire is wanted.

Logging available: `log.info/warn/debug/error`; `log.debug` needs DebugView or ScriptRunner's *Spawn
Debug Console*. Callback errors go to the debug log, and in newer nightlies appear in the
ScriptRunner window; startup errors raise a MessageBox `[reported, official docs + wiki]`. ⚠️ **Where
`log.info`/`warn`/`error` actually land is undocumented** — a gap worth knowing before relying on it.

**One API hazard to rule out on our grip path:** the docs state *"`ByRef` parameters are not correctly
supported by REFramework"* — they behave as `T**`. The workaround is to wrap the pointer with
`sdk.to_valuetype(ptr, "System.UInt64")` and read its `mValue` field, and **for `out` parameters this
only works in a post-hook**, stashing the reference during the pre-hook. If any slot on the grip path
is by-ref, a naive write there fails in exactly the way we observed.

---

## 5. ⚠️ A currency warning that applies to all of the above

Commits dated **2026-04-25** read *"REFramework v2 (#1609)"* and *"cleanup: remove AI slop docs and
move scripts to dev/"*, and v2 branch messages show internal C++ renames (`call_method` becoming a
member method, `get_name` relocating) `[reported 2026-09-11]`. **Whether v2 changes the Lua surface
could not be determined**, and the `scripts/` paths cited here may have moved to `dev/`. Check
against the build actually being injected. Separately, **the official docs carry no version stamp or
date on any page**, so per-claim currency cannot be established beyond repo commit history.

## 6. What this unlocks, concretely

Two changes, both small, both testable on one launch:

1. **Move the grip write to post-`LateUpdateBehavior`, drive it through cached
   `via.Joint:set_Position` / `set_Rotation` method definitions rather than any field set, and add a
   frame counter plus an immediate read-back at that point.** That is praydog's exact shape.
2. **Re-fetch `sdk.get_primary_camera()` every frame and read the pose off the camera transform's
   joint 0, not off the transform — logging the object address beside the position.**

## Sources

All read online; no code copied. Full credit list in `CREDITS.md`.

- **praydog** — *REFramework* (`github.com/praydog/REFramework`), master head at time of reading;
  `src/mods/bindings/Sdk.cpp`, `shared/sdk/SceneManager.cpp`, `shared/sdk/Application.cpp`;
  shipped scripts `scripts/re8_vr.lua`, `scripts/re2_vr_melee.lua`,
  `scripts/vr/VRControllerManager.lua`, `scripts/utility/RE2.lua`, `scripts/utility/RE8.lua`,
  `scripts/utility/GameObject.lua`. Releases v1.5.9.1 (2025-03-05) … v1.5.7 (2024-07-04); `scripts/`
  commit history 2022-09-22 → 2026-04-25.
- **praydog / cursey** — *REFramework book* (`cursey.github.io/reframework-book`, mirror
  `refdocs.praydog.com`), repo `cursey/reframework-book`, latest commits 2026-03-07 (actively
  maintained): `api/sdk.html`, `api/re.html`, `api/log.html`, `api/vrmod.html`,
  `api/types/ValueType.html`, `api/types/REManagedObject.html`, `api/types/RETransform.html`,
  `api/general/best-practices.html`, `api/general/Notes-on-Method-Arguments.html`,
  `api/general/Notes-on-Return-Types.html`, `examples/Example-Snippets.html`.
- **praydog** — *REFramework wiki* Home, last revised 2022-01-29 (stale, superseded by the book).
- **alphaZomega (alphazolam)** — *EMV-Engine* (`github.com/alphazolam/EMV-Engine`): per-frame
  "Freeze" writes, the Poser's per-joint `LocalPosition`/`LocalRotation`, Show Joints / Print Bones
  Enum. Fork by **SilverEzredes** (*EMV-Engine-SILVER*). Also *RE_RSZ* (file-level editing, not used
  here).
- Deliberately **not** relied on, recorded for honesty: third-party Nexus "REFramework Lua API"
  wrapper pages (authorship not established), `reframework.dev` (undated, unofficial), and
  machine-generated DeepWiki pages — used only to cross-check leads, with no claim resting on them.
