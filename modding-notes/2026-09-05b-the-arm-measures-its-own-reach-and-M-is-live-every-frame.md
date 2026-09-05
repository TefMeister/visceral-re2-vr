# The arm measures its own reach, and M is live every frame (2026-09-05 afternoon, home PC, `/pd`)

**The game was not launched. Nothing here has been run.** Everything below is either a compile
result or arithmetic done on log files the game produced this morning.

This closes the small half of the board's first `[PD]` row: *"recompute `M` every frame so the
summary's `angM` is not stale while undocked; add a reach clamp for a target the arm cannot
reach."* Plugin v0.5 → **v0.6**, `dev-archive/plugin/src/Plugin.cpp`.

⚠️ Housekeeping note for whoever reads `/pd`'s own command file: §2 lists `visceral-re2-vr` as
permanently out of scope, on the grounds that a REFramework Lua project has no static-only progress
to make. **That premise is stale** — this project grew a C++ native plugin on 2026-09-04, exactly as
`re-village-scope-vr` did, and a plugin is ordinary compile-verifiable code. Tefa named the game, so
the exclusion was overridden; it should probably be rewritten rather than overridden each time.

## 1. `M` is now measured every frame, not only while docked

`M` is the constant rotation that carries a value written into the `getIKLeftArmMatrix` return
buffer across to the wrist's final pose — measured this morning as `M = Nᵀ·A` (the game's natural
getter value against the aid joint's final world matrix), constant per weapon and stance, 48° on
the handgun and 18° on the minigun `[verified-numerically 2026-09-05, n=6 samples]`.

`update_dock` computed it *after* the "not docked, nothing to do" early return, so `angM` in the
1 Hz summary held whatever the last dock had left behind — including, on a fresh session, nothing
at all. Harmless to the dock itself, which recomputes `M` on the frame it uses it, but actively
misleading to a **headset run**, where the first thing worth knowing is what `M` looks like *before
committing to a grip*.

v0.6 measures it every frame, ahead of the idle return, at the cost of one extra
`via.Joint.get_WorldMatrix` per frame. Two details are deliberate:

- An unreadable aid joint now **clears** `M_valid` instead of leaving the previous frame's mapping
  in place. A stale `M` is worse than none: it reads as this frame's answer.
- The hook is unaffected in the idle case — `target_valid` is still false, so nothing is written.

## 2. The reach clamp, measured from the skeleton rather than assumed

A dock target further from the shoulder than the arm is long leaves the engine's two-bone solver
straining at full extension while the trace reports a distance that can never fall to zero. That
reads as a broken mapping when it is nothing of the kind — it is an out-of-range target.

The clamp pulls such a target back along the line from the shoulder until it sits at the arm's own
length, and leaves the rotation alone (a wrist orientation is always reachable; only the position
runs out of arm). It is applied **before** the target is published, so `|Lw-tgt|` still goes to
0.000 when the mapping is right.

**The reach is measured, not a constant.** RE2's `pl1000` skeleton names the left arm chain
`l_arm_clavicle` → `l_arm_humerus` → `l_arm_radius` → `l_arm_wrist` `[verified-live 2026-09-04]`.
The plugin binds the humerus and radius in the same joint walk that already binds the wrist, and
each frame takes `|humerus−radius| + |radius−wrist|`, keeping the running maximum over frames whose
segments both fall in a 5 cm–1 m plausibility band. Bone lengths are rigid, so the maximum *is* the
length; the band stops one garbage frame setting the clamp for the rest of the session.

### What the numbers say

Recomputed independently from this morning's own joint dump — `dev-archive/recon/2026-09-05b-reach-clamp-and-live-M/reach_check.py`,
reading `run12-dock-v05-final-space-mapping.txt`, touching none of the plugin's arithmetic:

| | left | right |
| --- | --- | --- |
| clavicle → humerus | 0.1264 | 0.1262 |
| humerus → radius | 0.2781 | 0.2786 |
| radius → wrist | 0.2213 | 0.2209 |
| **arm length (upper + fore)** | **0.4994** | **0.4996** |
| clamp radius (× 0.98) | 0.4894 | 0.4896 |
| shoulder → wrist at rest | 0.4863 (**97.4 %**) | 0.4747 (95.0 %) |

The two arms are independent measurements of one skeleton and agree to **0.18 mm (0.035 %)**
`[verified-numerically 2026-09-05, n=2 limbs, 1 frame]`. Claire's left arm is **≈ 50 cm** from
shoulder to wrist.

### Two ways the clamp is conservative, and why that matters

- **The clavicle is not counted.** 12.6 cm of shoulder girdle sits above the humerus and does move,
  so `0.4994` is a **lower bound** on true reach, not the reach.
- **× 0.98 on top of that.** A target at exactly 1.0 puts the elbow at full lock, which is both an
  ugly pose and where a two-bone solver is least stable.

Conservative twice over means the clamp **can fire on a target the real arm could have held**. And
the margin is thin: the resting left arm already hangs at **97.4 %** of the measured length, i.e.
3.1 mm inside the clamp radius.

### What is NOT established

**Whether the clamp would have fired on this morning's verified runs is unknown.** The bound that
*can* be stated: at full weight the wrist reached the target exactly (`|Lw-tgt| = 0.000`, 14 samples
over 5 s, both weapons), so `|target − humerus| = |wrist − humerus| ≤ 0.4994` — inside the **raw**
reach. Whether it was inside the **clamped** 0.4894 cannot be recovered, because the humerus
position was never logged at dock time.

That gap is the whole reason the clamp ships **behind a toggle**: **NUM2** turns it off, restoring
the v0.5 target exactly. It is the one v0.6 change that can alter a pose already verified live, and
one keypress now isolates it instead of a rebuild.

The 1 Hz summary gained `reach=<m> clamp=<m>`, with `(OFF)` appended when NUM2 has disarmed it, and
the clamp logs a line on each engaged/released edge. `reach=none` means the two arm joints were
never bound — a different failure from `clamp=0.000`, which means the target was in range.

## 3. The interaction that had to be handled: a different character is a different arm

The dock's state deliberately **survives** a player rebind — its settings and latches must. A
measured arm length must not: RE2 has more than one playable character, and a kept reach would clamp
Leon's arm to Claire's length, or Sherry's to Leon's. `rebind_player` now clears the humerus and
radius bindings and zeroes `reach` / `reach_raw` / `reach_valid` / `clamp_on` alongside the wrist
bindings it already dropped. `[inferred-static]` that the skeletons differ — plausible for distinct
characters but not measured here, and the clearing is correct either way.

The other interaction checked: `dump_joints` is called for the **weapon** skeleton too. The two new
bindings are guarded on the same out-parameter that marks the player call, so a weapon joint named
`l_arm_radius` could never be picked up as an arm bone.

## 4. Build

Clean Release x64 build, VS2022 Build Tools 14.44.35207, **zero warnings and zero errors** at `/W4`
`[compile-verified 2026-09-05]`. `visceral_core.dll` 99,328 B, `sha256` prefix `5494e5f23782cb53`.
Build log preserved at `dev-archive/recon/2026-09-05b-reach-clamp-and-live-M/build-v06-release.log`.

**Deployed** to `reframework/plugins/` on the home PC, so the headset run costs no rebuild. The v0.5
DLL was backed up twice before the copy: `visceral_core.dll.bak-2026-09-05-v05` (dated, kept) and
`visceral_core.dll.prev` (rolling, written by `build.sh`). Reverting is one file copy.

## 5. What this changes for the headset run

Nothing about the plan, and two things about the reading:

- `angM` is now meaningful **before** the first dock. On the handgun expect ≈ 48°; a wildly
  different figure while merely standing still, before any grip, says the aid joint or the natural
  value is not what we think — and it says so without spending a dock to find out.
- `clamp=` is the new tell. `clamp=0.000` throughout means reach never bound the target and the
  clamp changed nothing. A **persistently non-zero** `clamp` while docking normally means the
  controller is being held outside the character's arm — either genuinely (reposition), or because
  the lower-bound reach is too tight, in which case the fix is to add the clavicle segment rather
  than to loosen the fraction. Press **NUM2** to settle which: if the hand's behaviour changes with
  the clamp off, the clamp was doing something.

## 6. A `/gr` drop landed mid-session, and it is now one keypress from being settled

`/gr` pushed to this repo at 15:25 while this session was working — a create-only drop into
`engine-research/inbox/`, which is the modding lane's to drain. It was drained into dossier §8d and
deleted by name.

Its headline: **`via.motion.Motion` carries a `PlaySpeed` property on the component, above every
layer** `[reported 2026-09-05, from public source]` — engine-level `via.motion`, so a global rate cap
written there would need neither a layer index nor motion-name gating. That is a better first
candidate for spec req 4 than `TreeLayer.set_Speed`. **But only the getter is witnessed**: the public
dumper it came from never writes it, so `set_PlaySpeed` existing is a `[hypothesis]`.

A hypothesis that a single method enumeration can settle should not sit in a document waiting for
someone to think of it, so v0.6 adds `dump_motion()` and hangs it off the **existing NUM7 dump** —
no new hotkey. It prints:

- the `via.motion.Motion` method and field surface filtered on `speed` / `play` / `rate` / `layer`
  — **`set_PlaySpeed` appearing in that list is the whole answer**;
- `get_PlaySpeed` as a value, through the direct call route (§8d: the reflection invoke returns 0 for
  every float on this build, so **`NaN` here means the method is absent**, not that the value is zero);
- `getLayerCount`, to confirm the no-underscore spelling against the fallback;
- **`get_Weight` per layer**, which the drop asks for because it identifies the layer driving the pose
  by measurement rather than by string-matching `walk`/`run` in a motion name. Our own live read
  already says locomotion is layer 0 `[verified-live 2026-09-04, n=1]`, so a weight of ~1.0 on layer 0
  is corroboration by a second, independent method — and a weight elsewhere would be a real finding.

`dump_type_surface` grew a hunt-word parameter to make that possible; its two existing callers keep
the old word list by default and are unchanged.

**Nothing here has been run.** Press NUM7 on any launch — flat or in the headset, it costs nothing
and needs no dock — and the answer is in the log.

## 7. Deployed build

Final DLL of this session: **102,912 B, `sha256` prefix `7065a6c9cfb824d9`**, clean `/W4` Release
build, zero warnings and zero errors `[compile-verified 2026-09-05]`. Deployed to
`reframework/plugins/`. Rollback copies in the same folder: `visceral_core.dll.prev` (99,328 B, v0.6
without the motion dump) and `visceral_core.dll.bak-2026-09-05-v05` (98,304 B, this morning's
verified v0.5).
