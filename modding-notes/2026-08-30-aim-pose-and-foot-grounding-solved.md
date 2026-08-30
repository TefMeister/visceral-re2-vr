# The aim-pose saga: what didn't work, and the fix that did (2026-08-29→30)

Long investigation into making the VR weapon-ready pose acceptable. Result: the
**body pose + movement are done** — for both Claire and Leon.

## The symptoms
The braced weapon-hold/aim pose: torso twist, butt-out/shoulders-down brace, slow
aim-walk, and — the worst part — the whole body **floating ~10 cm off the ground**.

## What did NOT work (all confirmed dead, don't retry)
- **Animation swap / motlist swap / weapon-category spoof** — the game rejects it
  (also failed in AC historically).
- **Bank poison** (poison weapon *_MOVE / *_HOLD banks so bank resolution falls
  through to CMN unarmed): the poison APPLIES and the resolved motion changes
  (HG_→CMN_/KFF_ confirmed in the layer dump), BUT the legs are bound to the
  weapon-HOLD bank (2000) whenever the weapon is up, and the unarmed fallbacks
  (CMN_HOLD / KFF) still floated and shuffled. Wrong layer.
- **Forcing TargetBankType=0 (fully unarmed)** — switches the anim to the unarmed
  set BUT loosens the hand grip to bare-hand (game thinks unarmed) AND the feet
  still floated. Ruled out: the float is NOT an animation problem.
- **Forcing TargetBankType=9 (weapon-out-not-aiming)** — kept the grip, but the
  brace/float persisted because they come from the AIM STATE (IsHold), not the
  bank type.
- **Spine straighten + pelvis/waist straighten** — helps the torso twist (keep the
  spine one), but did NOTHING for the float.
- **Escaping the aim state** — can't: firing is gated behind the aim state
  (decided player-side, un-overridable). "Ready to fire" and "combat pose" are one
  bundle. And docked/auto-fire NEED the sustained aim state anyway.

## The key insight (from the user's headset test)
Lowering the physical HMD made the feet touch the floor and the knees bend. So the
float is a **VR height effect**: the braced pose lowers the head relative to the
body, and since VR anchors the body to the HMD, the game lifts the whole body to
keep the head at the headset → feet float. The game's own leg IK WILL plant the
feet + bend the knees if the body is low enough.

## THE FIX (shipped): pelvis-drop foot grounding
`visceral_foot_ground.lua`. While aiming (IsHold), lower the **pelvis bone** by a
fixed amount each frame (at pre-LateUpdateBehavior, composing with the game's leg
IK). The feet come down to the floor and the knees bend — the automatic version of
lowering the HMD. The braced pose stays, but it sits on the ground.
- **User-tuned value: drop = 0.175 m.** Feet touch while walking + looking down.
- **Skeleton-only** (pelvis + children). Hands/gun/arms are VR-controller-pinned,
  so the muzzle — and where bullets land — is UNAFFECTED (confirmed: bullets true).
- **No view penalty:** the body pulls back so you only see arms while aiming; the
  body lowering isn't visible. Works for Claire AND Leon.
- Enabled by default, only-while-aiming, tunable slider + NUM2 toggle.

## Net state of the body/pose layer
- **Torso twist:** spine straighten (soft-baseline, spine_0/1/2), on.
- **Float:** foot grounding (pelvis-drop 0.175 while aiming), on.
- **Speed (aim-walk):** collision-safe amplify available (`visceral_locomotion.lua`,
  aim-only) — separate, tune when wiring the fire/dock.
- **Cutscene/grab safety:** cinematic gate switches body scripts off.
- Accepted residual: the leg brace itself (butt-out) isn't fully removed, but it's
  grounded and out of view while aiming, so it reads fine.

## Next (unchanged)
Trigger-latch fire (RT → micro-latch, relaxed pose between shots; sustained while
held for auto/burst), then the LG dock (two-handed), then manual reloads from
scratch (study Andyalpa, beat the slide-rack edge jitter with the forward-projection
trick). See [[2026-08-29-lefthand-dock-design-spec-v2]].
