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
  1. **Undocked** (left hand NOT on the weapon), any weapon: fire = **micro-latch
     on RT** (pulse HOLD for the shot, release). Between shots, no aim state.
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

### What the three-state model settles, and what it leaves
The corrected model resolves the earlier "can the off-hand steer without the aim
state?" worry: **docked deliberately uses the sustained aim state**, so two-handed
aiming runs on the proven path (aim state on → off-hand steers the muzzle, as in
the base VR handling). No gamble there.

The consequence to design around: **while docked you are in the sustained aim
(HOLD) state**, which caps speed and locks interaction. Interaction is now handled
by **auto-undock (v2.2)** — the block is dropped exactly when you approach a
door/item. Speed is handled by req 4's unification + speed-driven presentation, so
docked movement doesn't feel like vanilla slow aim-walk. **Speed matching remains
the main technical risk** (needs the movement-speed blend param); auto-undock's
risk is smaller and is really a recon + tuning item.

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
2b. **Auto-undock trigger:** find the game's "interactable in range" / action
   check and whether it's readable while in the aim state (v2.2); fallback is our
   own proximity scan. Also check whether the interact prompt is suppressed during
   aim.
3. **The now-main risk:** locomotion **movement-speed blend param** (safe path
   for the speed-driven presentation system) + per-state speed caps to unify so
   the *docked/aim* cap == run cap (req 4); re-test whether footstep/leg/
   awareness can be driven by measured speed without FSM writes.
Then prototype the hand system (reqs 1→2→3) before touching 4. Input map to
build against: LG=dock, RT=fire (micro-latch), RG=ready sub-weapon, RG+RT=throw.
