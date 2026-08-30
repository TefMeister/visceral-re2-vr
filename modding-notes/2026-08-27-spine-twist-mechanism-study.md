# Spine twist — mechanism studied and understood (Arcade Controls study, 2026-08-27)

Study session over the frozen Arcade Controls material (the `ACVR_final_unfinished`
package + the `re2-vr-modding-notes` case studies `2026-08-05-claire-torso-twist.md` and
`2026-08-16-subtract-the-offset-not-the-motion.md`). Per the ground-up rule: **knowledge
carries, code never** — this note records the understanding; Visceral's implementation is
written fresh from it.

## The problem it solved

Claire/Ada's weapon-hold animations bake a ~40° twist into the spine bones. Invisible in
flat play; glaring in VR the moment you look down at your own body.

## The five ideas that made the working fix tick

1. **Fix the pose, not the animation.** Animation-file swapping fails (compiled animation
   data is skeleton-specific — silently wrong on another character); weapon-category
   spoofing fails (the same field selects the rendered weapon model). The working layer
   is below animation selection: per frame, rewrite the spine joints' local rotations
   directly (`getJointByName("spine_0"/"spine_1"/"spine_2")` → `set_LocalRotation`),
   blended toward straight by a strength factor.

2. **Write BEFORE the reader.** Arm/weapon IK consumes the spine rotation to place the
   hands. Pre-hook (before the IK solve) = hands land right; the identical write post-IK
   = gun visibly points sideways. Timing was live-tested three ways (pre / post /
   PrepareRendering); only pre works. *"Did I apply the fix" and "did I apply it before
   the thing that reads the value" are different questions.*

3. **The engine rewrites the bone mid-frame — six times.** `IkArmFit.updateIk` fires 6×
   per frame and native code re-writes spine_0 between calls, clobbering any
   once-per-frame correction (also the root of the laser-dot drift). Fix: hook **every**
   `updateIk` call — all overloads (`get_methods()` iterate; `get_method(name)` silently
   returns one overload) — and re-apply just before each, with a stale-read guard: if the
   joint still holds our own previous write (quaternion dot ≈ 1 vs last_written), don't
   re-correct it or the correction compounds.

4. **Soft mode — subtract the OFFSET, not the MOTION (the breakthrough).** Rigid
   straightening fights the gait (body bobs, spine held still → real shake and gun
   bounce — a kinematic mismatch, not a bug); fading the fix off while moving means no
   fix while moving. Soft mode: keep a slowly-adapting average (EMA, τ ≈ 0.4 s) of the
   ANIMATED spine rotation — that average IS the sustained twist — straighten only the
   average, and re-apply the animation's live deviation from it on top at full amplitude.
   Gait sway/breathing/turns pass through untouched; only the near-constant twist offset
   is removed. Player verdict at the time: "works so damn well."

5. **Freeze the baseline while aiming.** While aiming (`SurvivorCondition.get_IsHold`),
   the adaptive baseline is frozen so the correction is a true constant and the laser
   dot cannot drift mid-aim; adaptation resumes on release (no dot rendered then anyway).

Support scaffolding worth keeping in mind: a cutscene gate (shared cinematic-detection
global published by the holster script — cutscene animations are never touched), a 0–1
strength control, and (legacy rigid mode only) EMA speed/turn-rate estimators with an
asymmetric-τ smoothed on/off fade.

## Engine facts confirmed by that work (reusable as facts)

- Upper-torso joint chain: `pelvis → spine_0 → spine_1 → spine_2` (legs/pelvis untouched
  by the fix — locomotion stays fully animated).
- Hook points: `re.on_pre_application_entry("LateUpdateBehavior")` for the frame-top
  write; `IkArmFit.updateIk` (all overloads) for the per-call re-apply.
- Leon's natural spine_0 local rotation is pure-Y (≈ (0, 0.13, 0, 0.99)); Claire's
  twisted one adds real X/Z components (≈ (-0.023, 0.172, -0.129, 0.976)) — the twist
  defect is specifically the X/Z part, Y is normal hold/gait rotation.
- Aim state: `app.ropeway.survivor.SurvivorCondition.get_IsHold`.

## Why this matters now

Roadmap item 1 (torso twist removal + aim/walk pose matching) sits exactly where this
work lived: the spine chain is where the aim pose and the walk animation meet. The
concepts above are the toolkit; the code gets written new.
