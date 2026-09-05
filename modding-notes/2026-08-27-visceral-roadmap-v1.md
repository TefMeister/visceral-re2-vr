# Visceral — roadmap v1 (user, 2026-08-27, late night)

> **SUPERSEDED 2026-09-05 by [2026-09-05-visceral-roadmap-v2.md](2026-09-05-visceral-roadmap-v2.md)**,
> which merges this list with 22 further items and puts the whole thing in working order.
> Kept as the historical record of the first plan.


The user's first full feature plan, dictated at the end of the night the RE Village scope
showed its first real image. Their framing: *"I'm sure more will surface as we go along,
but this is our playground and a plan."* Sequencing: **Visceral starts once the Village
scopes are done** (§7 of STATUS). Every line of code written fresh — study anything
(including our own frozen Arcade Controls), copy nothing.

Items marked `??` are the user's own open questions, not commitments.

1. **Torso twist removal in VR** — plus: feet/lower body follow body rotation; **no pose
   change while aiming** — the body must look like it's walking normally whether aiming
   or not. (Mechanism studied: see `2026-08-27-spine-twist-mechanism-study.md`.)
2. **Ladder climbing fix** — camera gets taken over, teleported and panned while
   climbing. AC got close (the per-frame anchor-HOLD + measured-view servo design, case
   study `2026-08-22-the-jacked-camera-and-four-dead-levers.md`); study that material
   and get it to 100% or as close as possible.
3. **Mr. X persistence** — he disappears/teleports outside the RPD once he fully loses
   sight of the player and doesn't return until a scripted sequence. Idea: create MORE
   scripted-sequence-style appearances so he stays a threat throughout.
4. **Shadow restoration** — done in AC (`re2_vr_head_shadow`), study and re-apply fresh.
   **Needs a fix on top:** Claire (maybe Ada too) loses the neck as well as the head, so
   looking down shows straight through the body shell — restore the neck/close the view.
5. **Open doors with hands only.**
6. **Automatic item pick-up.**
7. **No dogs get killed** — replace dogs with zombies/lickers. (Carried over from the AC
   backlog's dog→zombie swap; recon probes exist as study material.)
8. **RG+LG→RG keeps holding the weapon** (grip chording — release LG without dropping
   two-handed hold).
9. **Smooth snap-on two-handing** of a weapon.
10. **3D ammo counter.**
11. **Manual reloads + reload animations.**
12. **Holsters HMD-relative.**
13. **Item placement changed.**
14. **More zombies** — police officers in RPD, workers in the sewers, scientists in the
    lab.
15. `??` **Dodge from RE3.**
16. **More guns to choose from** — Leon: a different handgun instead of stock Matilda;
    grenade launcher in the back of the police car; CAP locker holds the Quickshot
    handgun; S.T.A.R.S. HQ holds the MQ 11 + ammo instead of the magnum (the magnum
    arrives when Claire gets hers). Claire can get the shotgun and the M19.
17. **Scope see-through** — if the RE Village scope project (§7) succeeds, bring the
    technique here.
18. `??` **Damage rebalance** — more ammo hidden off the beaten path to reward
    exploration; headshots unchanged; limb shots remove limbs more easily; but zombies
    die harder overall.
19. **More gore** — the parked Visceral-named idea (see §4/§8 STATUS history; agreed
    easiest→hardest ladder: censor-state check → decals → hit VFX → own textures →
    park gibs).
20. **Player-visible first-person menus** — and the game image instead of black during
    item pick-up.
21. **No grip needed to shoot** (added the same evening, home PC session) — **feasibility
    PROVEN flat**: `InputSystem:setForce(HOLD, true/false)` is a clean latching switch;
    design = latch on trigger-touch, unlatch on release. See
    `2026-08-27-no-grip-to-shoot-proven-in-one-evening.md`.

Existing design decisions that slot in: the shotgun pump lock (LT = slide release, pump
worked physically; `2026-08-23-clean-slate-and-pump-lock-design.md`) belongs to 9/11's
territory; the ladder fix's "snap then rotation" residual (upstream FirstPerson
`bone_scale == 0.0f` interp bug) is item 2's known hard part; menu-camera HOLD v2 design
belongs to item 20.
