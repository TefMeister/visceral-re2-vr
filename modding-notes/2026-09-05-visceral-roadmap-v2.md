# Visceral — roadmap v2, the merged to-implement list (user, 2026-09-05 evening)

Supersedes: 2026-08-27-visceral-roadmap-v1.md

Tefa dictated 22 further items this evening. This file is **v1 plus those 22, de-duplicated
and put into an order to actually start working through**. v1 is left in place as the
historical record of the first plan; from here on this file is the list.

## Changes since 2026-09-05 (this file is edited in place; each change is dated)

- **2026-09-06 02:50 — NEW 23 added:** G5, the flashlight off the hand.
- **2026-09-06 12:10 — three additions/changes dictated by Tefa:**
  1. **G5 revised (NEW 23 → shoulder lamp, not headlamp).** The flashlight's holster zone is
     the **left shoulder, reached with the left hand** — not the head. And, if possible, the
     light itself should be offset so it shines from the player's **left shoulder**, not from
     the headset. The cutscene exception is unchanged.
  2. **NEW 24 — tutorial prompts for everything the mod changes** (movement, holsters, item
     pick-up, reloads, and so on). Deliberately one of the LAST things to build, once the final
     feature set is known. New phase I.
  3. **NEW 25 — manual item pick-up, fully specified.** Replaces the one-line F2. Left hand
     grabs, hold to carry, release to bank it into the inventory; a right-hand grab only when
     no gun is in that hand, and only if it does not mean re-wiring RG again. Detail in F2.
- **2026-09-06 14:12 — NEW 26 added by Tefa: a main-menu background for VR.** In the headset the
  menu is black with a blue smear; the Raccoon City image that fills the flat screen sits far
  back as a tiny picture nobody can make out. New G6.
- **2026-09-06 12:15 — flashlight-in-hand rules added to G5:** no two-handing a long weapon
  while the flashlight is held (holster it first); the handgun dock becomes a wrist-to-wrist
  dock (position locked to the right controller, each hand keeps its own rotation); no item
  pick-up while the flashlight is held. Detail under G5.

## The source rule, restated by the user tonight (unchanged)

> "We do not copy any code. Arcade Controls, now with Andyalpa's mod files, is ours to
> directly look at and rebuild, even line by line, but no copy-paste ever."

That is exactly the standing policy and nothing about it changes: AC 1.5.0 (minus the VR
Light mod) and Andyalpa's mod are **open to read, open to rebuild line by line, never to
paste**. The port map (`2026-09-05-arcade-controls-port-map.md`) exists precisely so the
rebuild is done from a written specification rather than from an open source file. C++-first
still stands — AC's Lua is the spec, the native plugin is the product.

## How the merge was done

Six of the 22 new items were already on v1. They are folded in rather than listed twice, and
where the new wording is more specific it wins:

| New item | Folded into | Note |
| --- | --- | --- |
| 5 more enemies | v1 #14 | identical, same three populations |
| 7 hands open doors | v1 #5 | v1 item was "do it"; new item is "redo it in C++" |
| 8 dodge from RE3 | v1 #15 | new item adds the binding: **RS down** |
| 12 Claire's neck | v1 #4 | v1 flagged the hollow neck; new item names the method (Blender) |
| 16 dogs → lickers | v1 #7 | new item is stricter: lickers specifically, **cutscene dog kept** |
| 22 3D ammo counter | v1 #10 | same item |

Two need a note about what they replace:

- **New 15 (damage + locational overhaul) supersedes the damage half of v1 #18.** They
  disagree: v1 said limb shots should remove limbs *more easily*, the new spec says limbs
  come off but **do no damage at all**. The new spec wins.
- **New 4 (more ammo) is the surviving half of v1 #18** — more ammo, off the beaten path,
  rewarding exploration.

One is a genuine fork, not a duplicate: **new 21 (physical item grabbing)** and **v1 #6
(automatic item pick-up)** are two different answers to the same problem. Both are kept, F2
and F3 below, and only one will ship.

**G6. A main-menu background in VR.** `[FLAT]` then `[VR]` — NEW 26, Tefa 2026-09-06 14:12.
Flat, the title menu sits on a full-screen image of Raccoon City; in the headset that image is
a small quad far behind the menu, unreadable, and the menu floats in black with the blue smear
of the shipped fog. Wanted: the city image readable behind the menu at a comfortable distance
and size — either the shipped background quad brought forward and scaled to the view, or a
replacement panel/skybox placed by the mod when the title scene is up. Recon first
(`[FLAT]`): which GameObject carries the image (a `via.gui` element, or a scene quad under the
title level) and whether its transform is writable from Lua/the plugin; then the placement is a
headset judgement. Low risk, high first-impression value; belongs with G2's menu work.

## Why this order

1. **Finish what is on the bench first.** The dock is mid-flight and half the hand items
   below sit on top of it. Reordering now would throw away understanding that is currently
   loaded.
2. **The data pass second, because it is one investigation that unlocks nine items.** Where
   RE2 keeps its placed lights, its enemy spawns, its item drops and its weapon tables is a
   single question. Answer it once and B1–B6 and C1–C3 stop being nine problems and become
   nine edits. It is also all flat-verifiable — the headset stays off — and it is the phase
   with the highest ratio of "the game feels different" to effort spent.
3. **Combat rules third, after the population is final.** The damage model has to be tuned
   against the enemies that will actually be in the game. Doing D before B means tuning it
   twice.
4. **Body/motion, then hands, then HUD.** The hand items need the dock finished; the HUD
   items are the least load-bearing and several are cosmetic.
5. **The Blender track is last in priority but first in availability.** H1 and H2 need no
   game running at all — they are the thing to work on when the game cannot be launched.

Gate tags per item: `[PD]` needs nothing running · `[FLAT]` needs a flat-screen run ·
`[VR]` needs the headset to judge.

---

# A. In flight — finish these before opening anything new

**A1. Left-hand dock / two-handed hold.** `[VR]` — v1 #8 (RG+LG→RG keeps holding) and v1 #9
(smooth snap-on two-handing). Plugin v0.6 is deployed and proven flat; the first headset run
is the current OPEN row on the status board. Everything in section F waits on this. **Design
note for later (2026-09-06):** G5 adds a second dock shape — with the flashlight in the left
hand the dock is wrist-to-wrist (position locked, rotations free) and long-weapon two-handing is
refused. Keep the engage test and the "what is locked" choice as data, not hard-coded.

**A2. Manual reloads + reload animations.** `[FLAT]` then `[VR]` — v1 #11. Fully specified in
port map §B (magazine state machine, HMD-yaw ammo pouch, the four suppression layers, the
exact engine calls that change the ammo). This is a build, not a hunt.

**A3. Slide racking / pump lock.** `[FLAT]` then `[VR]` — port map §C plus the existing
design in `2026-08-23-clean-slate-and-pump-lock-design.md`. Same session as A2; they share
the anchor resolution and the motion scrubbing.

**A4. Aim-walk speed (req 4).** `[FLAT]` — the NUM7 read on the status board settles whether
this is a component-wide lever or a per-layer hunt. Belongs here because the aim posture work
in E1 will be judged partly on how the character *moves*, not just how it stands.

---

# B. The world-data pass — placement, population, atmosphere

One investigation, six payoffs. Start with B1 as the probe.

**B1. Most lights out in the RPD main hall.** `[FLAT]` — NEW. Put first deliberately: it is
one visible, single-map, trivially reversible change, so it is the cheapest possible test of
the question the whole phase depends on — *can we edit RE2's placed scene data at all, and
does it survive a level reload?* If the lights go out, B2–B6 are mostly the same skill. If
they cannot, we learn that before spending a session on enemy spawns.

**B2. More ammo throughout the game.** `[FLAT]` — NEW 4, and the surviving half of v1 #18.
Hidden off the main path, so exploration pays. Tune against D1 — a zombie that takes three
headshots eats ammo far faster than vanilla, so B2 and D1 are one balance conversation.

**B3. More enemies throughout.** `[FLAT]` — NEW 5 = v1 #14. More police officers in the RPD,
workers in the sewers, scientists in the lab. Same placement machinery as B1.

**B4. Item placement changed.** `[FLAT]` — v1 #13. Follows B2/B3 because it is the same
tables and should be decided with the ammo and enemy layout in view.

**B5. Dogs replaced by lickers.** `[FLAT]` — NEW 16 = v1 #7. **The cutscene dog stays** (it is
scripted; swapping it would break the scene). The standing reason is unchanged: no dogs get
killed in this mod. Recon probes from the AC-era dog→zombie swap exist as study material.

**B6. Mr. X persistence.** `[FLAT]` — v1 #3. The hardest item in this phase and correctly
last: he currently despawns once he fully loses the player outside the RPD and only returns
on a scripted beat. The idea on the table is to author *more* scripted-appearance-style
triggers so he stays a live threat. Depends on B3 in the sense that a busier RPD changes how
often he is naturally encountered.

---

# C. The weapon-data pass

Same skill as B, different tables. Split out because it can be done in a separate sitting.

**C1. Matilda upgrade rework.** `[FLAT]` — NEW 2. Remove the **shoulder stock** upgrade from
the game entirely; move its effect (**burst fire**) onto the **muzzle** upgrade. The muzzle's
vanilla recoil reduction is not wanted and can go. Net: one upgrade part fewer in the world,
and the part that remains grants the thing worth having.

**C2. LE5 as a real, findable weapon.** `[FLAT]` — NEW 3. Today it exists only as an
infinite-ammo unlock. Wanted: placed in the world to be found, consuming ammo, reloadable,
with its ammo type findable. Do it after C1 because C1 teaches the weapon tables on a
smaller change. Note it must also work with A2's manual reload — a new weapon means a new
magazine entry in the reload state machine.

**C3. More guns to choose from.** `[FLAT]` — v1 #16. The redistribution plan: Leon gets a
different handgun in place of the stock Matilda; grenade launcher in the back of the police
car; the CAP locker holds the Quickshot handgun; S.T.A.R.S. HQ holds the MQ 11 and its ammo
instead of the magnum (the magnum arrives when Claire gets hers); Claire can get the shotgun
and the M19. Last in the phase because it is the largest single edit and it interacts with
both C1 and C2.

---

# D. Combat rules — how things die

All of these are hooks on the damage path rather than table edits, so they are one body of
code. D1 first: it defines the baseline every other item in the phase is tuned against.

**D1. Damage multipliers and locational damage overhaul.** `[FLAT]` — NEW 15, superseding
the damage half of v1 #18. The spec:
- **3 headshots** to kill a zombie.
- **10 body shots** to down one — but it **gets back up** after a while.
- Limbs can still be shot off as in vanilla, but **removing a limb does no damage**. A
  legless zombie keeps coming for you; only three shots to the head end it.

This is the single biggest change to how the game plays and it is the reason B2 (more ammo)
exists. Expect the tuning of the two to be iterative.

**D2. Shotgun blasts a zombie off its feet.** `[FLAT]` — NEW 17. The movie-shotgun feel.
Tefa's idea: reuse the **grenade blast impulse** and trigger it where the pellets land on the
body. Worth trying exactly that first — if the grenade already applies a ragdoll impulse we
can call, this is cheap. Sits right after D1 because D1 decides whether the zombie that gets
blasted off is dead or getting back up.

**D3. Thrown grenades do escape-level damage.** `[FLAT]` — NEW 10. A grenade used as a
sub-weapon escape does far more damage than the same grenade thrown. Make the thrown damage
match. Small, self-contained, and a good first look at the grenade code that D2 and D4 both
need.

**D4. The player can be caught in a grenade blast.** `[FLAT]` — NEW 11. Treat it as a Mr. X
hit: knock the player down. Direct follow-on from D3 — same code, opposite direction.

**D5. Plant zombies do not catch fire.** `[FLAT]` — NEW 19. In the lab: short flamethrower
bursts should not ignite them and should not apply burn-over-time. Only a **continuous flame
stream** drains their health and stops them moving. Cut the fire off and they recover.

**D6. Sewer grabbers do not catch fire.** `[FLAT]` — NEW 20. Identical rule to D5:
incapacitated while under the stream, no ignition, no burn damage afterwards. Grouped with D5
because it is the same status-effect change applied to a second enemy — do them together.

**D7. Always escape the rear grapple.** `[FLAT]` — NEW 9. The automatic escape from a zombie
grabbing the player from behind currently fires only sometimes `[reported]`; make it every
time. Worth an early look at whether it is a random roll or a state condition — if it is a
roll, this is a one-line change and could be promoted above D5/D6.

**D8. More gore.** `[FLAT]` — v1 #19. The agreed easiest-to-hardest ladder is unchanged:
censor-state check → decals → hit VFX → our own textures → gibs (parked). Last in the phase
because it is presentation on top of a damage model that D1 has to settle first.

---

# E. Body, posture and motion

**E1. Aim posture, round two.** `[VR]` — v1 #1 plus NEW 6: make the aiming body look like it
is **simply walking**. Read `2026-08-30-aim-pose-and-foot-grounding-solved.md` before
starting: the float is solved (pelvis-drop 0.175 m) and the torso twist is largely solved
(spine straighten), and the confirmed dead ends are animation swap, motlist swap, weapon
category spoof, bank poison, TargetBankType forcing, and escaping the aim state. What is left
is the leg brace itself, which the note records as an accepted residual. NEW 6 asks us to
stop accepting it. The honest position: this is the item most likely to end in "as close as
we can get", and the port map's clavicle-stretch finding is the newest lead worth spending on.

**E2. Poison posture and the RE3 vomit.** `[FLAT]` — NEW 18. Two parts: make the poisoned
walk match the normal walk (same machinery as E1, hence the adjacency), and replace death-by-
prolonged-poison with the RE3 behaviour — the player **vomits the poison out** and is left at
the lowest possible health instead of dying.

**E3. Dodge from RE3.** `[FLAT]` then `[VR]` — v1 #15 plus NEW 8's binding: **right stick
down** triggers it. v1 carried this as an open question; the binding makes it a commitment.

**E4. Ladder climbing fix.** `[VR]` — v1 #2. The camera is taken over, teleported and panned
during a climb. AC got close with the per-frame anchor-HOLD plus measured-view servo; the
case study is `2026-08-22-the-jacked-camera-and-four-dead-levers.md`. Known hard part: the
upstream FirstPerson `bone_scale == 0.0f` interpolation bug leaves a rotation residual after
the snap. Last in the phase because it is the one with a known unfixed upstream cause.

---

# F. Hands and interaction — all of this waits on A1

**F1. Hands open doors, in C++.** `[VR]` — v1 #5 plus NEW 7. This was done in Lua; the item
is to redo it natively and see whether C++ changes the outcome. ⚠️ Read
`2026-08-29-doors-items-test-result.md` first: doors **cannot be opened while the game is in
its aim state at all**, vanilla RMB included, so the blocker was never our latch. Whether C++
changes that is exactly the open question, and it is a shared blocker with F3.

**F2. Manual item pick-up, by hand.** `[VR]` — NEW 21, fully specified as NEW 25 by Tefa
2026-09-06 12:10. Reach out and take the item. The more interesting of the two pick-up answers
and the one that fits the mod. The spec:
- **Which hand.** The **left hand (LG) is the baseline** and is enough on its own. The right hand
  too, **when no gun is in it**, would be excellent — but it may be too much hassle, because RG
  is already re-wired (RG = equip sub-weapon) and a right-hand grab means re-wiring a system
  that has already been re-wired once. Build left first; add right only if it comes cheap.
- **Hold to carry.** While LG is held the item stays in the hand and the player can **walk
  around carrying it**. Nothing is banked until the grip opens.
- **Release to bank.** Releasing LG puts the item **into the inventory**.
- **Two possible shapes of that moment, decided by what the engine allows:**
  - **If an RE3-style straight-to-inventory route exists** (no pick-up screen — RE3 remake takes
    items directly), then on release the item should **visibly and quickly shrink in the
    player's hand** rather than just disappearing.
  - **If the inventory screen has to play out**, there is no difference from vanilla at that
    point: releasing LG still triggers the placement, and the pick-up screen follows. The hold-
    and-walk part stays exactly the same either way — the item can be held and carried for as
    long as LG is down.
- **A hand holding the flashlight cannot pick up items** (G5 rule 3, 2026-09-06) — holster the
  light on the left shoulder first.
- Shares F1's open question: whether interaction is reachable at all from the aim state, and
  whether C++ changes that.

**F3. Automatic item pick-up.** `[VR]` — v1 #6. The fallback if F2 proves impractical, and it
runs into the same aim-state interaction block as F1. Only one of F2/F3 ships.

**F4. Trigger fingers.** `[VR]` — NEW 14. In first person the trigger finger currently rests
along the side of the gun even while firing. Wanted: the finger on the trigger, moving with
it. Placed after the grab items because it is the same hand-bone write path they establish.

**F5. No grip needed to shoot.** `[FLAT]` then `[VR]` — v1 #21. **Feasibility already
proven flat**: `InputSystem:setForce(HOLD, true/false)` is a clean latching switch
(`2026-08-27-no-grip-to-shoot-proven-in-one-evening.md`). Design: latch on trigger-touch,
unlatch on release. Cheap and could be pulled forward if a quick win is wanted — the only
reason it sits here is that the latch drags the aim-state bundle along, which is E1's and
F1's problem.

**F6. Holsters, HMD-relative.** `[VR]` — v1 #12. Last: it is the item most dependent on
everything else in the phase feeling right first. **Flashlight slot (Tefa 2026-09-06 12:10):
the flashlight's holster zone is the LEFT SHOULDER, reached with the LEFT hand** — not the
head. Pairs with G5, where the light itself is offset to that same shoulder.

---

# G. HUD and presentation

**G1. 3D ammo counter.** `[VR]` — v1 #10 = NEW 22. First in the phase because A2's manual
reload gives it its reason to exist: once ammo is something you physically manage, a counter
you can look at is the natural partner.

**G2. Player-visible first-person menus.** `[VR]` — v1 #20. Plus: show the game image instead
of black during item pick-up. The menu-camera HOLD v2 design belongs to this item.

**G3. Shadow restoration.** `[FLAT]` then `[VR]` — v1 #4. Done in AC as
`re2_vr_head_shadow`; study and rebuild fresh. ⚠️ **G3 is the prerequisite for H1 mattering** —
the hollow-neck problem only appears once head shadows are on.

**G4. Scope see-through.** `[VR]` — v1 #17. Conditional on the RE Village scope project
succeeding; if it does, the technique comes here. Last because it is gated on another project.

**G5. Flashlight on the LEFT SHOULDER, not in the hand and not a headlamp.** `[FLAT]` then
`[VR]` — NEW 23, added by Tefa 2026-09-06 02:50 as a head-mounted torch, **revised by Tefa
2026-09-06 12:10: the light should read as coming from the player's LEFT SHOULDER** (a
shoulder-mounted lamp, the way a police radio or body torch sits), not from the headset. Two
parts: (a) the **holster zone** for the flashlight is the left shoulder, reached with the left
hand (recorded on F6 too); (b) **if possible, the light's actual origin is offset to the left
shoulder** — the emitter sits at the shoulder and points where the body/head faces, so the
beam visibly comes from beside the head rather than out of the eyes. Which direction it follows
(HMD yaw, body yaw, or a blend) is a headset judgement, not a design decision made here.
Everything else is unchanged: the light is taken off the hand everywhere EXCEPT in cutscenes,
where the game's own hand-held flashlight must play as authored. Needs a cutscene detector to
switch between the two — `visceral_cinematic_gate.lua` already publishes
`__visceral_cinematic_blocking()` and bridge slot 30 carries it into the plugin, so the switch has
its signal; what is missing is finding the flashlight's light object and re-parenting / re-posing
it each frame to a shoulder anchor (REFramework's ManualFlashlight is the reference for how it is
toggled). The head-mounted version is the natural first build step — same code, different
anchor — but it is a step, not the target.

**Flashlight-in-hand rules (Tefa 2026-09-06 12:15).** When the flashlight is OUT of its
shoulder holster and in the left hand, three things change:
1. **No two-handing a long weapon.** The two-hand grip on a shotgun pump handle, a fore-end, or
   any long-weapon dock is suppressed while the flashlight is held. The player either shoots
   one-handed or puts the flashlight back in the left-shoulder holster first, and only then can
   the left hand grab the pump handle. Ties into A1 (the dock) and A3 (pump/slide racking): the
   dock's engage test gains a "left hand is empty" condition.
2. **The handgun dock becomes a WRIST dock, not a grip dock.** Today the left hand AND the
   flashlight would dock onto the gun's handle, which points the flashlight backwards with the
   light still showing forward — wrong. Instead, with the flashlight held, the dock is the
   classic wrist-over-wrist flashlight hold (reference: the RE2 Leon promotional pose — right
   hand on the handgun, left hand holding the flashlight forward and pressed under the right
   wrist, both pointing the same way; local copy of the picture at
   `Pictures\Screenshots\Screenshot 2026-09-06 121435.png`, not committed — it is Capcom's
   image). Mechanically: **the left wrist locks to the right wrist in POSITION, so moving the
   right controller moves the whole assembly exactly as it does when the left hand is docked on
   the grip — but both hands keep their own ROTATION**, so the player can freely angle the
   flashlight and the gun independently while docked. Position follows the right controller;
   orientation of each item follows its own controller.
3. **No item pick-up while the flashlight is held.** If manual pick-up (F2) is implemented at
   all, a hand holding the flashlight cannot grab items — holster the light first. (If F2 never
   ships, this rule is moot.)

Tracking is not a worry here, per Tefa (2026-09-06 12:25): with the wrists docked this way
both controllers stay in the headset's view — they have played like this before `[reported]` —
and in any case, once the dock is engaged and LG is held, **the dock stays docked regardless of
where the left controller actually is**, so a momentary tracking loss on the left cannot break
the hold. The standing stack-above rule is about the two-hand grip on a long weapon, not this
pose. All of this is theoretical until the dock runs in the headset; judge it then.

---

# H. The Blender track — no game required, available at any time

Both are Tefa's own "maybe!!! if possible". Lowest priority, highest availability: when the
game cannot be launched, this is the work.

**H1. Give Claire a neck.** `[PD]` — NEW 12. With head shadows on (G3) her body reads hollow,
because the head removal takes the neck with it and you can see straight down through the
shell. Model a neck in Blender so looking down shows a body. v1 flagged Ada as possibly
having the same problem — check her too.

**H2. HD hands for all playable characters.** `[PD]` — NEW 13. The hands are what you look at
for the entire game in VR, so this has an outsized effect for asset work. Largest single
asset job on the list.

---

# I. Release polish — deliberately last

**I1. Tutorial prompts for everything the mod changes.** `[FLAT]` to build and see them fire,
`[VR]` to judge readability and timing — NEW 24, added by Tefa 2026-09-06 12:10. The game
teaches its own controls; it does not teach ours. Every behaviour the mod changes needs a
prompt of its own, shown where the player first needs it: movement, holsters (including the
left-shoulder flashlight slot from F6/G5), item pick-up (F2), manual reload and slide racking
(A2/A3), the dodge binding (E3), no-grip-to-shoot (F5), the two-handed dock (A1), and whatever
else lands by then. **This is one of the LAST things to build, on purpose:** it can only be
written once Visceral's final feature set is known, or the prompts will teach a version of the
mod that no longer exists. Keep a running list of "changed behaviours" as items close, so the
prompt set can be written in one pass.

---

## Two things to decide before starting a phase

- **B2 and D1 are one conversation.** Three headshots per zombie changes ammo economy so
  much that tuning either alone will be wrong. Plan to do a pass of both together.
- **F2 or F3, not both.** Decide by trying F2 first; F3 is only worth building if physical
  grabbing proves impractical.
