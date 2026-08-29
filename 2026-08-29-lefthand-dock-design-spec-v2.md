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

**Open questions for the user (load-bearing, not bikeshed):**
- **One-handed pistol fire without docking?** v2 routes all firing through the
  dock (left hand on gun → latch → fire). If a pistol should also fire
  one-handed with no off-hand dock, that path needs its own momentary
  (true "micro") latch on the trigger. Not covered by 1–4 as written.
- **Confirm the fire input while docked:** right trigger = ATTACK, firing via
  the sustained latch (the proven path). Assumed, worth a yes.

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
1. Locate the off-hand grip/foregrip anchor node on weapon models + confirm both
   controller transforms are readable in our own code (proximity math for 1 & 3).
2. Confirm hand IK target access + how the base two-hand aim solve reads the
   off-hand (for 2 & 3's decouple).
3. Find the per-state locomotion speed params (for 4) and re-test whether the
   footstep/leg/awareness coupling can be reached without FSM writes.
Then prototype 1→2→3 (the hand system) before touching 4.
