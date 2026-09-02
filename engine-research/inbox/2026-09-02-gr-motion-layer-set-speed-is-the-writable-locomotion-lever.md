# Board risk "does a WRITABLE locomotion speed param exist?": yes — `via.motion.MotionLayer.set_Speed`, shipped in public REFramework scripts

Filed by: `/gr`, 2026-09-02
Topic: `external-research/topics/2026-09-02-a-writable-speed-lever-exists-the-motion-layers-playback-speed.md`
Dossier section: §8 (motion system) — and the board's req-4 row

`[reported 2026-09-02, from public source]` The Requiem "Better Movement Speed" script (ported to RE2 on Nexus by the same author) writes **`set_Speed(k)` on the player's `via.motion.Motion` layer** every `LateUpdateBehavior`, gated by matching `"walk"`/`"run"` in `get_HighestWeightMotionNode():get_MotionName()`. Because RE2 locomotion is root-motion driven (§8), the playback rate scales travel, leg cycle and footstep events together — req 4's "drive footsteps and legs from speed" holds by construction. It is a rate, not a walk/run blend: cap = `set_Speed(cap / nominal)` on the game-chosen motion, with aim/idle motions excluded by name.

Unverified: which layer index carries RE2 locomotion (script uses `getLayer(0)`; the grip is on another layer here), and whether enemy awareness reads a separate speed. Suggest adding a per-layer `get_MotionName()` / `get_Speed()` dump to the queued reqs-1-and-3 probe. praydog's `re2_smooth_movement.lua` (transform writes per `UpdateMotion`) is the fallback for movement root motion cannot express.
