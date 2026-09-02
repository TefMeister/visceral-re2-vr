# Speed lever follow-up: layer index is 0 (unverified for RE2), awareness has no public answer

**Date:** 2026-09-02 · **From:** `/gr` (scoped re-run, this project named explicitly) · **For:**
this repo's modding session to drain (create-only inbox drop; fold in and delete)

**Bears on:** OPEN board item — *"the MAIN technical risk... does a WRITABLE locomotion
movement-speed blend param exist?"* — continuing the same-day pointer already in this inbox
(`2026-09-02-gr-motion-layer-set-speed-is-the-writable-locomotion-lever.md`, left untouched).

Full write-up: `external-research/topics/2026-09-02-a-writable-speed-lever-exists-the-motion-layers-playback-speed.md`
(see the "Addendum" section).

## What's new since the first pointer

1. **The layer index the public precedent uses is 0** — Junh2x's `re9_movement_speed.lua` calls
   `getComponent(via.motion.Motion):getLayer(0)` directly, no enumeration, no layer-count check.
   That's Requiem, not RE2, and REFramework's own API book doesn't document
   `via.motion.Motion`/`MotionLayer` at all (per-game reflected type) — so **0 is a starting guess
   to test, not a confirmed fact for this game.**
2. **Enemies use the identical `getLayer`/`set_Speed` API in that same script**, for their own
   locomotion animation playback — confirms the mechanism is engine-general, but it is **not** an
   awareness/perception system; it doesn't touch how an enemy notices the player.
3. **No public source anywhere ties RE2/RE-Engine enemy awareness to player movement speed.**
   Searched directly; found only cosmetic/audio mods. Suggest treating req 4's awareness half as
   something this project has to establish itself (once an AI-perception component is found by
   reflection), not a lead to keep searching for.
4. **No existing public tool dumps motion layers by index** (checked EMV Engine) — so the cheapest
   path to a real answer is a small first-party probe, not more searching.

## Suggested next probe

On the player's `via.motion.Motion` component: loop layer indices (0..N, or however many the
reflection dump reveals) and log `getLayer(i):get_HighestWeightMotionNode():get_MotionName()` while
walking / running / aiming / idle. Whichever layer's name tracks real movement is the locomotion
layer; if more than one does, that's new information about aim/locomotion blending this dossier
doesn't have yet.
