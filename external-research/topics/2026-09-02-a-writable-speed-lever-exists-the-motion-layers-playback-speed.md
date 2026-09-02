# A writable speed lever exists: the motion layer's playback speed — and with root motion it moves the feet and the body together

**Status:** 🆕 new · **Priority:** high — it answers the board's "MAIN technical risk" (does a
writable locomotion movement-speed parameter exist?) with a public, shipping precedent, and the
answer changes which lane req 4 lands in.

## What was found

Two public REFramework scripts change the player's movement speed in RE Engine titles without
touching the FSM `[reported 2026-09-02, from source]`:

- **`Junh2x/RE9-Movement-Speed-Mod`** (Requiem; the RE2 port "Better Movement Speed RE2" on Nexus is
  the same author's remake — its page 403s automated fetch, so the RE2-specific hook name is unread).
  It does two things each frame from a `LateUpdateBehavior` callback: it walks
  `CharacterManager → getPlayerContextRef() → get_GameObject() → getComponent(via.motion.Motion)
  → getLayer(0) → get_HighestWeightMotionNode() → get_MotionName()`, decides walk vs run by matching
  `"walk"`/`"run"` in the motion name (and the character by the `ch0100`/`ch0200`/`ch0300` prefix),
  and **writes `set_Speed(multiplier)` on the motion layer**. Separately it `sdk.hook`s
  **`app.MovementDriver.getMoveSpeed`** and scales the return value — that half is Requiem-specific
  (RE2 has no `app.MovementDriver`; its equivalent lives under `app.ropeway`).
- praydog's own `re2_smooth_movement.lua` in REFramework's `scripts/` takes the other route entirely:
  it replaces root motion by **writing the body `via.Transform` position each `UpdateMotion`** toward
  the left-stick direction, smoothed. That is a position write, not a speed parameter — the "manual
  root motion needs `warp()` too" caveat from the CAF topic applies.

## Why the motion-layer speed is the lever req 4 wants

`via.motion` is engine-level, present in every RE Engine title, and **`MotionLayer.set_Speed` is
writable from Lua** — it is what the Requiem script ships on. In RE2, locomotion is root-motion driven
(dossier §8: the same bank ids play whether armed or unarmed; movement comes out of the animation),
so scaling the layer's playback speed **scales the body's travel, the leg cycle and the footstep
events together**, because they are one animation. That is req 4's "drive footsteps, leg animation and
awareness from measured speed" satisfied by construction rather than by a blend parameter — the
speed *is* the playback rate.

What it is not: a blend between walk and run poses. A run animation at 0.6× is a slow run, not a
walk. So the unified aim/docked/run cap in req 4 becomes: pick the bank/state the game would pick,
then clamp the layer speed to `cap / nominal_speed_of_that_motion`, which the script's
motion-name match already shows how to gate. Aim and idle motions on the same layer must be
excluded by name, as the script does, or they slow down too.

## What is NOT established

- **Which layer.** The script uses `getLayer(0)`; RE2's locomotion may be on another layer index
  (dossier §8: the grip lives on a separate layer). One probe line — dump `get_MotionName()` per
  layer while walking — settles it.
- Whether RE2's `via.motion.MotionLayer` exposes `set_Speed` identically — near-certain
  (engine-level type), but `[reported]` until the reflection dump says so.
- Whether awareness (enemy hearing) reads the motion or a separate speed value — the Requiem script
  does not touch awareness, so req 4's awareness half is still ours to verify.

## Concrete next steps

1. Add to the queued reqs-1-and-3 probe: enumerate the player `via.motion.Motion` layers with
   `get_MotionName()` and `get_Speed()` while walking and running — reads only.
2. If locomotion is on a findable layer, prototype req 4's cap as a `set_Speed` clamp gated on
   motion name; measure travel speed against the existing telemetry.
3. Keep praydog's transform-write route as the fallback for locomotion that root motion cannot
   express (analog strafing), with the `warp()` caveat.

## Addendum (2026-09-02, same day, follow-up pass): layer index confirmed, awareness stays open

Read `re9_movement_speed.lua` directly (public GitHub blob, online only, nothing downloaded) to
settle "which layer" rather than guess `[reported 2026-09-02, from source]`:

- **The script hard-codes layer index 0**: `getComponent(via.motion.Motion):getLayer(0)`, then
  reads `get_HighestWeightMotionNode():get_MotionName()` off that one layer and string-matches
  `"walk"`/`"run"`. It never enumerates other layers or checks a layer count — layer 0 is an
  assumption baked into a shipping mod, not a documented constant. REFramework's own API book
  (`cursey.github.io/reframework-book`) does not document `via.motion.Motion`/`MotionLayer` at all —
  it is a per-game managed type, reflection-only, exactly as dossier §4 already says, so there is no
  public authority to check the assumption against beyond "another mod's author picked 0 and it
  worked for them, on Requiem, not RE2."
- **The same script confirms enemies use the identical API**, which is useful even though it doesn't
  answer the awareness question: for non-player characters it walks
  `get_EnemyContextList()` and applies `getLayer`/`set_Speed` to each enemy's own **animation**
  playback (their walk/run cycle), explicitly excluding boss-tier IDs (`B030`, `V000`, `V100` —
  scripted/boss logic, not generic walk-cycle blending) and motions containing `"attack"`/`"dead"`.
  This is enemies' own locomotion presentation, symmetric to the player's — **not** an AI
  perception/awareness system reading player speed.
- **No public source was found on RE2/RE Engine enemy awareness reading player movement speed at
  all** `[checked 2026-09-02]` — targeted web searches for zombie/enemy detection-radius or
  noise-vs-speed mods and writeups returned only cosmetic/audio-replacement mods (classic zombie
  sounds, model swaps), nothing gameplay-AI-adjacent. RE2 is not a stealth-mechanics game in the
  first place, so this may simply not exist as a modder-documented system; treat req 4's "drive
  awareness from measured speed" as **new ground for this project**, not something to go looking
  for again without a different angle (e.g. a live reflection dump of the enemy AI/perception
  component itself, once one is identified).
- **No turnkey layer-dumper tool exists publicly either.** Checked alphaZomega's EMV Engine
  toolkit (already known to this project, dossier §12) — its Console/Poser/Action Monitor/Hooked
  Method Inspector do not include a motion-layer enumerator; the closest is its generic managed-
  object property panel (the same reflection technique dossier §4 already documents). So the
  concrete next probe below is genuinely the cheapest way to get a real answer, not a second-best
  substitute for a public tool that exists somewhere.

**Suggested probe (3 reflection calls, read-only, no game state changed):** on the player's
`via.motion.Motion` component, loop `i = 0..N` calling `getLayer(i)` until it returns `nil` (or use
whatever count method the live reflection dump exposes — dossier §4's field-enumeration technique
answers that in one call if `get_LayerCount`/similar isn't a real property), and at each layer log
`get_HighestWeightMotionNode():get_MotionName()` while walking, running, aiming and idle. Whichever
layer's name changes with the player's real movement is the locomotion layer; if more than one
layer's name tracks movement, that says something about how aim-pose and locomotion are blended
that the dossier doesn't know yet either.

## Sources

- https://github.com/Junh2x/RE9-Movement-Speed-Mod — `reframework/autorun/re9_movement_speed.lua` (no licence file visible; study-only; read online, not downloaded)
- https://www.nexusmods.com/residentevil22019/mods/2391 — "Better Movement Speed RE2" (403 on fetch; existence and feature list from search)
- https://github.com/praydog/REFramework/blob/master/scripts/re2_smooth_movement.lua — MIT; the transform-write route
- https://cursey.github.io/reframework-book/ — checked for `via.motion.Motion`/`MotionLayer` documentation; confirmed absent (per-game reflected type, out of scope for the engine-level API book)
- https://github.com/alphazolam/EMV-Engine — checked for an existing layer-dump tool; none found
