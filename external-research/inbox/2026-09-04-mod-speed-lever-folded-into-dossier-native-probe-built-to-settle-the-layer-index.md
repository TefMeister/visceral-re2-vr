# Modding verdict: the `set_Speed` lever is folded in as dossier §8d; the layer index is now a probe question, not a search question

**Date:** 2026-09-04 · **From:** modding (`/lm visceral`, home PC) · **For:** `/gr` to drain and
flip the INDEX status of the 2026-09-02 topic *"A writable speed lever exists: the motion layer's
playback speed"* from 🆕 new to 👀 reviewed.

## What happened to the lead

- Both 2026-09-02 inbox drops were read together (no `Supersedes:` lines), folded into
  `engine-research/ENGINE-DOSSIER.md` as a new **§8d**, and deleted by name.
- The board's "MAIN technical risk" row (does a writable locomotion speed param exist) is
  **retired** on the strength of the lead — tagged `[reported]` in the dossier, exactly as filed.
- RE2's type names differ from the Requiem script: there is **no `via.motion.MotionLayer`** in RE2's
  type database; `Motion.getLayer(u32)` returns **`via.motion.TreeLayer`** (which does have
  `get_/set_Speed`, `get_HighestWeightMotionNode()` → `via.motion.MotionNodeCtrl` with
  `get_MotionName`). `[inferred-static 2026-09-04]` from `il2cpp_dump.json`. Worth a line in the
  topic so nobody greps for `MotionLayer` in RE2.
- The **layer-index caveat is now a probe question**: the native recon probe built today
  (`dev-archive/plugin/`, a REFramework C++ plugin) logs every layer's highest-weight motion name
  on change, so the first flat run names the locomotion layer. Nothing further to search for on
  that point.
- One deeper candidate surfaced from the same dump that the public sources did not mention:
  **`app.ropeway.survivor.SurvivorMotionSpeedController`** (`: app.ropeway.MotionSpeedController`),
  a game-side speed controller on the player with `TensionSpeed` / `WaterResistanceSpeed`
  (`RangeLerpFloat`). If `/gr` finds any public use of `MotionSpeedController` in RE Engine
  titles, that is the one lead still worth a search — it sits above the layer, which is where
  the "reach for the deep end" rule says req 4 should live.

## Not established

Nothing here has been run. Every claim above is static reading of the type database.
