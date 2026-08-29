# RE2R Custom Animation Framework (CAF): working custom animations + a shipped RE3-style dodge, with docs

**Status of every claim here: `[reported]`** — read from the project's public repo and Nexus/search
descriptions; nothing exercised by us yet.

## What this is

**"NonRTX - RE2R Custom Animation Framework (CAF)"** (Nexus mod 2056, author godlock2000-eng /
NonRTX) is an event-driven custom-animation system for RE2 remake built entirely on REFramework
Lua, MIT-style licensed with full source and — unusually — a `docs/` folder of genuine
reverse-engineering write-ups. Its bundled example is **RE3 Jill's four-directional dodge roll
playing in RE2**, with configurable speed, roll distance, blend frames, cooldown, rebindable key
and root-motion toggles. That example is, almost verbatim, **Visceral roadmap item 15 ("RE3
dodge")** — existing, working, public prior art. (Trivia: its author states it was built almost
entirely by a team of Claude agents with the author as orchestrator/QA.)

Repo: https://github.com/godlock2000-eng/ResidentEvil2_CustomAnimationFramework_NonRTX

## The mechanism (dodge, `CAF_NativeDodge.lua`)

- **Player**: `sdk.game_namespace("PlayerManager")` singleton → `get_CurrentPlayer()`; components
  via `getComponent(System.Type, sdk.typeof(...))` — `via.motion.Motion`, `via.motion.MotionFsm2`,
  and the CharacterController (with a fallback through
  `app.ropeway.survivor.SurvivorCharacterController`'s backing field and child GameObjects).
- **Playing a custom animation**: custom `.motlist` files are registered at runtime as
  **`via.motion.DynamicMotionBank`** entries (chain: `MotionBankResource` →
  `MotionBankResourceHolder` → DynamicMotionBank → register on the Motion component) under
  private bank IDs (900–903 for the four dodge directions), then played with
  `layer:changeMotion(bankID, motionID, start, blend_frames, …)` + `set_Frame(0)` + `set_Speed`.
  This is the load-side complement to our dossier §8 selector knowledge (which switches among
  banks the game already loaded).
- **Stopping the game from overriding it**: the player's motion FSM re-drives the layers every
  frame, so a bare `changeMotion` gets stomped. CAF's answer is blunt but proven: **pause AND
  disable `MotionFsm2`** (`set_Paused(true)` + `set_Enabled(false)`) for the duration, restore
  both afterward. Their `docs/actor_motion_systems.md` states the general rule: hook the FSM or
  work within it — custom animations risk immediate override. (The FSM is reachable as
  `app.ropeway.MotionFsm` via a `<MotionFsm>k__BackingField`.)
- **Root motion**: driven manually — per-frame position updates during a window of the animation
  (eased 0.18→0.65 progress), applied with `transform:set_Position(...)` **followed by
  `CharacterController:warp()`** so physics accepts the move; wall hits detected as drift between
  intended and actual position. The set_Position-then-warp pair is the documented gotcha: without
  `warp()` the controller snaps the character back.
- **Timing**: per-frame work runs in a `PrepareRendering` callback.

## The gap that matters to us

`CAF_NativeDodge.lua` performs **no weapon/aim-state gating at all** — no `SurvivorCondition`
checks, no `IsHold`/`IsReload`, nothing about the weapon layer. A dodge fired while aiming simply
hijacks layer 0. For Visceral's dodge (and for any custom animation played while armed), the
armed-state integration — cancel aim first? preserve the weapon-grip layer? block during reload?
— is exactly the part we would have to design ourselves, using the state surface in
`2026-08-29-weapon-equipped-state-surface.md`. Knowledge carries, code never: mechanism above is
understanding, our implementation is written fresh.

## The docs folder (reference material in its own right)

`docs/` includes `actor_motion_systems.md` (layer architecture: `via.motion.TreeLayer` per-layer
BankID/MotionID/Speed/Frame/EndFrame/Weight/WrapMode/BlendMode; motion bank loading),
`re_engine_animation_types.md`, `mot_format_specification.md`, `motlist_format_guide.md`,
`paired_anims_and_mod_api.md`, and `RE32 Jill Dodge Doc.md` (analysis of RE3's *original* dodge:
action-priority "Precede" values, invincibility as an animation control track —
`EsAnimationInvincibleControlTrack` — i.e. i-frames live in animation data, not code).

## Related find: changing movement speed while armed

The same search surfaced **"Better Movement Speed RE2"** (Nexus 2391, Junh2x), a port of their
RE9 mod (source: https://github.com/Junh2x/RE9-Movement-Speed-Mod). Technique (RE9 namespaces,
but the shape transfers): multiply the motion **layer's `set_Speed`** when the active motion name
matches walk/run (and skip attack/finisher names), and — the clever half — **hook the movement
driver's `getMoveSpeed` and scale its return value** so actual locomotion speed follows the
animation speed-up instead of foot-sliding. Relevant to the old AC-era open problem "aim-walk
speed ×1.3 desynced footstep audio": scaling the *animation* and the *speed* together is the
version of that change that stays in sync (audio events ride the animation). `[hypothesis]` for
RE2 until tried.

## Sources

- CAF repo + docs (godlock2000-eng / NonRTX) — link above; read via the repo's own viewers 2026-08-29.
- Nexus: RE2 mod 2056 (CAF), mod 2391 (Better Movement Speed) — pages 403 automated fetch; details
  from search snippets and the GitHub sources.
- Junh2x's RE9-Movement-Speed-Mod repo — link above.

## Concrete next step

When roadmap 15 (dodge) or 11 (manual reload animations) opens: read CAF's
`actor_motion_systems.md` and `motlist_format_guide.md` in full in a browser, then prototype the
armed-state gating CAF lacks (IsHold/IsReload checks before hijacking the layer). For roadmap 1's
aim/walk matching, the layer-speed + getMoveSpeed-hook pairing is the first thing to test against
the footstep-desync problem.
