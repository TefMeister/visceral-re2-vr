# Always-ready design spec v2 — the left-hand DOCK system (user dictation, 2026-08-29 evening)

Supersedes the "always-ready while walking" framing of spec v1
([2026-08-29-always-ready-design-spec.md]). v1 asked for doors/pickups to work
*while latched*; v2 reframes readiness around a deliberate DOCK gesture — you
dock to be ready, you undock to interact — which fits the HOLD-state reality we
mapped ([2026-08-29-fire-path-architecture.md], [2026-08-29-doors-items-test-result.md])
far better. Firing itself is the proven latch (setForce HOLD, 0–4 ms).

## The four requirements (verbatim intent)

1. **Proximity dock.** Bringing the hands close enough activates an area; while
   in that area, pressing **LG (left grip)** docks the left hand to the gun.
   Must be a **smooth transition, not a snap-to-weapon**. **Press-to-grip,
   press-again-to-release** (toggle), "if this is doable."
2. **Dock → latch ON.** LG pressed + left hand docked triggers the (micro)latch
   ON. For two-handed weapons the player still has **two-handed aiming** while
   the left hand is docked. (LG may also be held after docking if the player
   wants; letting go and pressing LG again is what un-docks.)
3. **Smooth undock, cosmetic tail.** Pressing LG again to detach must **not**
   detach the left hand immediately — the hand leaves only when the hands are
   **moved apart enough** (true for long guns AND pistols/revolvers). After the
   release-press the attachment is **cosmetic only**: the still-attached left
   hand **must not move the front end of the weapon** anymore.
4. **Speed matching.** aim / walk / run must share the **same max speed**, so
   movement feels natural — because actual speed is still driven by **how far
   the LS is physically pushed** (analog). No separate slow "aim-walk" cap.

## Engineering read (Claude, same session — feasibility, not a build)

**Reqs 1–3 are a self-contained VR hand system; the risk is polish, not access.**
Everything they need — both controller world transforms, the weapon grip/foregrip
anchor node, hand IK targets, and the HOLD latch — is stuff we can already read
or drive. None of it needs the FSM-writing lane.

- **Req 1 — doable.** Proximity = distance(left controller, foregrip anchor) <
  threshold → in-zone bool. LG-in-zone → dock (toggle = trivial input state).
  The one genuine unknown is the **smooth (non-snap) transition**: lerp the
  left-hand IK target from its tracked pose to the grip anchor over ~100–200 ms.
  Doable; whether it *feels* right is an in-headset tuning problem, and the game's
  own two-hand IK may need to be blended so it doesn't fight the lerp. Medium
  confidence on feel, high on mechanism.

- **Req 2 — the strongest part.** Dock engages two things we've each proven:
  (a) the HOLD latch (setForce, 0–4 ms), (b) two-handed aim = weapon orientation
  from the right-hand pivot + left-hand foregrip direction. **Consequence to
  accept:** while docked you are continuously in HOLD, which re-imposes the HOLD
  bundle — interaction lock (no doors/pickups), and the aim body pose. In this
  design that's *correct*: docked = deliberately ready; you undock to interact.
  This is why v2 drops v1's "doors while latched" fight.

- **Req 3 — subtle but doable.** It's a **decouple of two things we control
  separately**: the IK target (visual) vs the aim-solve input (functional). On
  release-press: drop aim influence + unlatch immediately, but keep the hand
  parented to the grip node so it rides the weapon cosmetically. "Must not move
  the front end" = weapon reverts to **one-handed aim** (right hand only) the
  instant LG is pressed; the left hand contributes zero. Detach when
  distance(tracked controller, grip node) > threshold. Watch item: the
  two-hand→one-hand aim-authority handoff can pop where the muzzle points unless
  we blend it — a tuning point, not a blocker.

- **Req 4 — the one that reaches back into the engine.** Matching the aim-state
  speed cap to run is a value change (per-state locomotion speed param; NOT yet
  located — new recon). BUT the wall we already hit stands: *raising move speed
  alone did not change footstep sound, leg animation, or enemy awareness*. If
  docked movement is meant to feel like real movement at speed, those three are
  coupled to the state and may need real wiring — the known-dangerous lane.
  Mitigation the dock model gives us: you likely dock to shoot (stationary /
  slow reposition) and undock to sprint, so full-speed docked movement may be a
  rare case. Highest uncertainty of the four; separable; tackle last.

**Nice emergent property:** v2 may dissolve v1's "banned aim animation" problem
for free. The ugly aim shuffle was a walking-around concern; with the dock model
the aim pose only shows *while docked* (i.e. while genuinely aiming, where a pose
is expected), and normal locomotion has no aim pose. If the docked pose looks OK
and speed is matched, req 4 of v1 is largely satisfied by construction.

## v2.1 refinements (user, same evening) — firing is a UNIVERSAL micro-latch

Answers + additions that change the firing model and the speed model:

- **The latch has THREE behaviours, keyed to hand/dock state (user correction,
  2026-08-29 evening — this replaces the "universal micro-latch" wording above):**
  1. **Undocked** (left hand NOT on the weapon), any weapon: fire = **trigger-held
     latch** — HOLD on for as long as RT is held, off when RT releases. Semi-auto =
     a blip; **auto-fire weapons stay latched for the whole burst** (user add,
     2026-08-29). Between shots/bursts, no aim state. ("Micro-latch" = the
     semi-auto case of this one rule.)
  2. **Docked** (LG pressed, left hand functionally on the weapon), pistols AND
     long guns: **latch SUSTAINED ON the whole time the hand is docked**, until
     LG is pressed again. Two-handed aiming lives here.
  3. **Released-but-cosmetic** (LG pressed again to release; the hand is still
     visually on the gun during the pull-apart tail): **latch OFF**, and fire is
     back to **micro-latch on RT** — i.e. it behaves exactly like undocked.
  So: docking = sustained aim state; undocked and the cosmetic tail = momentary
  per-shot pulses.
- **RT = fire weapon** (confirmed).
- **New input map:** **RG (right grip) = ready sub-weapon; RG + RT = throw
  grenade/flashbang.** (LG = dock left hand, per req 1.)
- **Speed model, final form (req 4):** the run BUTTON does not exist. walk,
  aim-ready, and run all share ONE max speed = vanilla run speed; actual speed is
  the analog LS deflection. Aim-ready feels identical to walking.
- **User's proposed fix for the coupling wall — drive presentation from measured
  speed.** Instead of relying on the movement state to set footstep sound, leg
  animation, and body sway/pose, *measure* the player's actual speed each frame
  and drive sway + pose + foot-anim cadence + footstep audio proportionally to
  it. This is the right architecture: it decouples "how the body looks/sounds"
  from "which state the engine thinks it's in," which is exactly the coupling
  that beat us before.

## v2.2 refinement (user, same evening) — AUTO-UNDOCK near interactables; DROP the cosmetic tail
> **SUPERSEDED by v2.3 below:** hold-to-grip removes the need for auto-undock (the
> two conflict). The cosmetic-tail removal and the no-muzzle-pop handoff survive.

The docked-state door/item block is resolved by design, not by fighting it:

- **Auto-undock near an interactable.** When the player is close enough to an
  item or door that the interaction (A) prompt would apply, the mod **auto-
  releases the dock** (drops the sustained latch → aim state off). This both
  restores interaction AND lets the gun micro-latch fire again. Immersion logic
  (user): you can't grab things or open doors with both hands on the weapon, so
  freeing the off-hand to interact is natural.
- **Why this is especially clean:** the door/item block we measured
  ([2026-08-29-doors-items-test-result.md]) IS the aim/HOLD state itself. Docked =
  sustained aim state = blocked. Auto-undock drops that exact state, so it is the
  *precise* cure — not a workaround.
- **DROP the cosmetic tail (was req 3's "hand lingers on the gun until pulled
  apart").** User calls this out for removal; it conflicts with a clean auto-
  undock and was the fiddliest piece anyway. Undock (manual OR auto) becomes just
  the smooth reverse of docking: the hand animates back to the real tracked pose
  over ~100–200 ms. **Keep only the useful half of old req 3:** the two-hand→one-
  hand aim-authority handoff must be **blended so the muzzle does not pop** when
  the off-hand leaves.

### Feasibility of auto-undock (Claude read)
- **Best trigger = the game's own "interactable in range" signal**, not the
  visible prompt. Reason (chicken-and-egg): we don't yet know if the prompt is
  *suppressed* while in the aim state — if it is, "prompt visible" can never fire
  while docked. Reading the underlying action/interaction check (RE2 has a clear
  action system; `action`-named components showed up in the player dump) sidesteps
  that. **Fallback:** our own proximity scan for nearby interactable objects if
  the check can't be read during aim. Recon item.
- **Auto-undock only; manual re-dock.** Freeing the hand on approach is
  predictable; auto-*re*-docking when you walk away would yank the hand back
  unexpectedly. Re-dock stays an LG press. (Confirm with user.)
- **Debounce** so the hand doesn't flicker if the interactable signal flickers.
- **Watch in playtest — combat near items.** Fighting beside a pickable item
  could repeatedly pop the off-hand off the gun. Tuning options if it's annoying:
  only auto-undock on doors (deliberate) vs items, or only when not actively
  aim-steering, or a brief suppression while the trigger's been used recently.
  Not a blocker; a feel-tuning knob.

## v2.3 refinement (user, same evening) — LG is HOLD-to-grip, not a toggle; auto-undock removed

Two decisions that simplify the whole dock down to two clean states:

- **Re-dock is manual — confirmed.** (Now inherent: you just hold LG again.)
- **LG is HOLD-to-grip.** Hold LG = left hand docked on the weapon (sustained
  latch, two-handed aim). Release LG = undock (latch off, back to one-handed
  micro-latch fire). No toggle.

**The latch is now TWO states, not three:**
1. **LG not held (undocked):** fire = micro-latch on RT.
2. **LG held (docked):** sustained latch ON, two-handed aiming; the instant you
   release LG you're back to state 1.
(The cosmetic tail is already gone per v2.2; hold-to-grip makes that doubly true —
release = the hand smoothly comes off, with the no-muzzle-pop aim handoff kept.)

**Why this kills the auto-undock (and its recon dependency):** with hold-to-grip,
undocking to interact is just *letting go of the button* — the player does it
naturally as they reach for a door/item. Auto-undock would actively FIGHT this: if
the mod force-released while you're physically holding LG, the hand would drop off
a button you're still pressing (feels broken), or it'd snap back the instant you
step clear (the auto-re-dock we already rejected). So hold-to-grip **replaces**
auto-undock; we drop that mechanism and no longer need to read the game's
interactable check during aim. The combat-near-items worry also dissolves: you're
only docked while actively holding LG, so grabbing an item mid-fight is just
releasing the grip (which you'd likely do to reload or fire one-handed anyway).

**Residual to watch in playtest (minor):** while holding LG (aim state on), the
interaction prompt may be suppressed, so you might not *see* that a door/item is
usable until you release. Almost certainly fine (lower the gun to interact), but
if it reads as confusing we can look at surfacing the prompt during dock later.

**Accepted trade-off:** hold-to-grip means continuous LG hold during long
two-handed stretches (finger fatigue) — the user weighed this against the toggle
and chose hold-to-grip for the simplicity it buys everywhere else.

## v2.4 refinement (user, same evening) — the two-stage interaction prompt

Resolves the v2.3 residual (prompt maybe hidden while LG held) using UI that
already exists:

- RE2 already shows a **directional arrow/chevron indicator** when an interactable
  is near-but-slightly-too-far ("get closer") — the ⌄ over the typewriter in the
  user's screenshot; the actual **button prompt** appears when you're in range.
- **Design:** keep the **arrow indicator visible even while LG is held** (docked/
  aim), so the player always knows something is there; make the **actual
  interaction button prompt appear only once LG is released.** Free hand → prompt →
  interact. Immersive AND informative.
- **Feasibility:** GUI-layer work, separable from core mechanics, low risk (worst
  case cosmetic). We have prior art manipulating RE2 `via.gui` controls (the
  route-fix draw-time text-hold). Recon: is the prompt GUI one combined element or
  separate arrow-vs-button controls, and does the arrow currently survive the aim
  state? If the arrow is suppressed during aim we force it visible; the button we
  gate on LG-released. Build this AFTER the core hand system.

### What the two-state model settles, and what it leaves
The corrected model resolves the earlier "can the off-hand steer without the aim
state?" worry: **docked deliberately uses the sustained aim state**, so two-handed
aiming runs on the proven path (aim state on → off-hand steers the muzzle, as in
the base VR handling). No gamble there.

The consequence to design around: **while LG is held you are in the sustained aim
(HOLD) state**, which caps speed and locks interaction. Interaction is handled by
simply **releasing LG** (v2.3) — that drops the aim state, restoring doors/pickups
and one-handed micro-latch fire. Speed is handled by req 4's unification +
speed-driven presentation, so held-LG movement doesn't feel like vanilla slow
aim-walk. **Speed matching remains the main technical risk** (needs the
movement-speed blend param).

### Feasibility of the speed-driven presentation system
Right instinct; difficulty hinges on one thing:
- **Reading speed:** easy (position delta or a velocity/speed field).
- **Body sway / pose:** doable procedurally (additive bone/camera offsets).
- **Foot/leg animation:** the risk. If RE2's locomotion has a **movement-speed
  blend parameter** we can write (very common — a float that drives the
  walk/run blend), driving IT by our measured speed is elegant and SAFE (no FSM
  writing). If there is no such param, matching leg anim to speed is the
  dangerous FSM lane. **Recon must find this param before we commit.**
- **Footstep audio:** the game uses Wwise (WwiseTagTrigger etc. seen on the
  weapon/player components). Firing footstep events manually at a speed-matched
  cadence is possible but fiddly; fallback is to let the anim system emit them if
  the speed-blend param also drives the existing footstep events.

## Recon leads found in the SDK dump (2026-08-29, first pass — flat, no MCP)
- **`app.ropeway.survivor.SurvivorMotionSpeedController`** exists (VA 141a91xxx
  family) — modulates motion speed (tension/water-resistance speed fields +
  `getDefaultSpeed`, `doMotionSpeedControllerUpdate`). Speed modifiers live here;
  the base walk/run locomotion speed + the anim blend still to be pinned.
- Speed-related method names present across types: `MoveSpeed`, `SpeedRate`,
  `SpeedScale`, `MaxSpeed`/`SpeedMax`, `SpeedBlendRate`, `SpeedAnimation`,
  `MoveSpeedChecker` — strong signal that a **speed→locomotion-animation blend**
  exists (the "safe path" the speed-driven presentation system needs). Exact
  owner + writability must be confirmed with a LIVE probe (read values while
  moving at different LS deflections).
- This is encouraging for req 4: RE2 clearly has speed/anim machinery to drive
  rather than us hand-authoring leg animation.

### Measured speed results (2026-08-29 evening, flat, KB/M, speed recon v2)
Ground speed measured from position delta (units/sec), labelled by IsHold:
- **Aiming (IsHold=true) ≈ 1.5** peak (the vanilla aim-walk penalty).
- **Walk (IsHold=false) ≈ 1.7–2.1.**
- **Run / full movement (IsHold=false) ≈ 3.1–3.68, max seen 3.77.**
So the **aim state genuinely MOVES you at ~40% of full speed** (position-delta, not
just animation) — confirms req 4's premise. **Target cap to unify everything ≈
3.77.** In the dock design this matters ONLY for the docked state (sustained aim);
undocked already runs at full speed since the micro-latch only blips the aim state.

**The internal speed value is NOT a simple property.** All 59 collected float
getters were `get_DeltaTime` (frametime), `get_ElapsedSecond`, or constants —
`getDefaultSpeed=1.0`, `get_PlaySpeed=1.0`, `get_TensionSpeed=1.0`,
`get_WaterResistanceSpeed=1.0` (all motion-PLAYBACK multipliers, all 1.0 during
aim-walk → the aim slowdown is NOT a playback multiplier; it's the aim
locomotion's inherent per-cycle ground distance). The real move-speed / walk→run
blend value lives as a **`via.motion.MotionFsm2` FSM variable** (accessed by
name/hash, not a get_ method) — that's the next probe.

**Good omens intact:** footsteps are velocity-driven (`WwiseVelocityTriggerList`)
so they'll follow measured speed; arm IK components confirmed present for the later
hand work (`app.ropeway.IkArmFit`, `IkController`, `via.motion.IkLeg`).

### Motion-variable recon results (2026-08-29 evening) — THE LOCOMOTION IS VARIABLE-DRIVEN AND WRITABLE
`player -> via.motion.Motion -> get_VariablesHub()` exposes **47 named user
variables**, and **almost all are writable** (RO=false, WP=false; only `LowAttack`
RO). This is the safe path we hoped for — the whole locomotion/animation system is
driven by named inputs we can read AND write, so NO FSM surgery is needed.

Key variables (name, kind, writable):
- **`Jog`** (bool, writable) — **the walk↔run selector.** Stream proof: flips to 1
  when running, back to 0 when slowing.
- **`MoveStickPower`** (float, writable) — analog move input power. **0 on KB/M**
  (digital movement bypasses it); expected to carry the LS deflection under VR/pad
  = the natural input for req 4's analog speed.
- **`HoldUp`** (bool, writable) — aim/weapon-up. `MoveDir`/`TargetMoveDir`/
  `WatchDir`/`CameraDir` (dir), `MoveStickPower`, `StepLeft` (footstep phase),
  `Fire`, `CanRapidFire`, `Relax`, plus reload/door/damage/event triggers.

**Speed tiers, now explained by the variables (measured, IsHold-labelled):**
- Aim-walk (aiming, Jog=0): **~1.0**
- Walk (no aim, Jog=0): **~1.8–2.1**
- Jog/Run (no aim, **Jog=1**): **~2.7–3.35, max 3.79**
So the walk→run jump IS the `Jog` variable, and aiming imposes a further slowdown
on top of Jog=0. NB: RE2 has no run button on pad normally — default is jog; the
"walk" tier here is the analog/slow band. Footsteps are velocity-driven, and the
animation blend is fed by these very variables, so **driving the variables gives
correct legs + footsteps for free** — exactly the "speed-driven presentation" the
user wanted, built into the engine.

**The likely lever for req 4 (docked = full speed):** force **`Jog=1`** (and manage
`MoveStickPower`) while docked/aiming. Open question needing a WRITE test: (a) does
the game overwrite Jog each frame (→ we write per-frame like the fire latch), and
(b) does an aim-jog animation exist, or does forcing Jog=1 while HoldUp look wrong /
do nothing? That is the next probe — the **first write** (safe: a documented
writable motion variable, fully reversible, with a panic key).

## Static-analysis assets ready for this (2026-08-29)
- SDK dumped: `<game>/il2cpp_dump.json` (471 MB) maps every managed method to
  its static VA (`"function": "140xxxxxx"`, image base 0x140000000). Query by
  `grep '"function": "<hexVA>"'` then read back for the method key + type key.
  Confirmed: `Equipment.requestFire` = 140aecbd0; its lone caller 14037bd5b is
  NOT in the dump = native FSM glue (no managed hook point → gate is structural).
- Address bridge (REFramework log VA → PowerShell module base → static VA →
  ghidrust/dump) documented in [2026-08-29-fire-path-architecture.md].

## Next
Recon for the build, in this order (all read-only first):
1. Off-hand grip/foregrip anchor node on weapon models + confirm both controller
   transforms readable in our own code (proximity math for reqs 1 & 3).
2. Hand IK target access + how two-hand aim reads the off-hand (for the smooth
   dock/undock animation + the no-muzzle-pop aim-authority handoff). Two-hand
   steering itself is settled — docked uses the sustained aim state (see above).
   Cosmetic tail is DROPPED (v2.2).
2b. ~~Auto-undock trigger recon~~ — DROPPED with auto-undock (v2.3). Optional
   playtest-only: is the interact prompt suppressed while LG is held?
3. **The now-main risk:** locomotion **movement-speed blend param** (safe path
   for the speed-driven presentation system) + per-state speed caps to unify so
   the *docked/aim* cap == run cap (req 4); re-test whether footstep/leg/
   awareness can be driven by measured speed without FSM writes.
Then prototype the hand system (reqs 1→2→3) before touching 4. Input map to
build against: LG=dock, RT=fire (micro-latch), RG=ready sub-weapon, RG+RT=throw.
