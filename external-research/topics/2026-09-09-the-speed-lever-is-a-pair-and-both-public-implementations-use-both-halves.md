Supersedes: `visceral-re2-vr/engine-research/ENGINE-DOSSIER.md` §8d — the clause "Because RE2 locomotion is root-motion driven (§8), a playback-rate clamp scales travel, leg cycle and footstep events together — req 4's 'drive legs and footsteps from speed' holds by construction"; and this lane's own `topics/2026-09-02-a-writable-speed-lever-exists-the-motion-layers-playback-speed.md`, the clause calling the `getMoveSpeed` hook "Requiem-specific"

# The movement-speed lever is a PAIR, not one clamp — and both public implementations write both halves

**Status:** 🆕 new · **Priority:** high — it corrects a load-bearing "by construction" claim in
`ENGINE-DOSSIER.md` §8d that the req 4 plan rests on, and it changes what the board's already-armed
NUM7 probe should look for.

## The correction first

§8d records that RE2 locomotion is root-motion driven, and concludes that a playback-rate clamp on
the motion layer therefore scales **travel, leg cycle and footstep events together**, so req 4
("drive legs and footsteps from speed") "holds by construction". This lane's 2026-09-02 topic
reinforced that by describing the reference mod's `app.MovementDriver.getMoveSpeed` hook as
"Requiem-specific" — i.e. as an artefact of that game rather than part of the technique.

**Both readings are contradicted by the source itself.** Junh2x's shipping mod does not treat the
`getMoveSpeed` hook as optional or game-specific: it installs the hook as a first-class part of the
feature, alongside the layer write, and applies **the same scale factor to both**
`[reported 2026-09-09, from source]`.

That matters because the two halves are not redundant. If the layer playback rate alone moved the
character — which is what "by construction" claims — the author would have no reason to scale the
movement driver's own returned speed as well. Two independent implementations doing it anyway is
evidence that the animation rate and the travel rate are **separately driven** in RE Engine, and
that keeping them in sync is the mod author's job, not the engine's.

## The second, independent implementation

`Namsku/re-engine-trainer` is a public RE Engine trainer repo (RE7 and RE9/Requiem, MIT-style
layout, study-only — nothing copied). Its Requiem feature module does exactly the same two things
for its "Player Speed" feature `[reported 2026-09-09, from source]`:

1. It resolves the type `app.MovementDriver`, takes its `getMoveSpeed` method, and installs a
   **return-value hook** that multiplies the returned float by the configured factor. It logs
   `"MovementDriver.getMoveSpeed hook installed"` on success and warns
   `"Could not find app.MovementDriver:getMoveSpeed"` on failure — so the name is load-bearing in a
   shipped tool, not incidental.
2. Per frame, it detects the current move type and picks **separate walk and run factors**, then
   writes that same factor as the motion **layer** speed.

It also keeps a small "reset frames" counter that restores the layer speed to 1.0 over several
frames after the feature is switched off, rather than in a single frame — a detail worth copying in
spirit (not in code) if our own implementation ever leaves the layer clamped.

This is the first source found for this project that is **fully fetchable in public** and shows the
whole pairing. Everything previously known about the technique came from Junh2x's Requiem repo; this
corroborates it from a second author who arrived at the same shape independently.

## What it does NOT answer

It does **not** give RE2's name for the movement driver. Both sources are Requiem (`app.*`); RE2's
gameplay namespace is `app.ropeway.*`, and no public source found so far names the RE2 equivalent.
So the board's `[USER]` row — hand-download "Better Movement Speed RE2" (Nexus 2391), because Nexus
403s automated fetch — **still stands, but its value is now narrower and should be re-stated**: the
*method* is doubly public and fully documented here, so the only thing that download can still buy
is the RE2 **class and method name**. That is one name, and our own reflection dump can produce it.

## The concrete next step it unlocks

The board's already-armed NUM7 probe currently asks one question — does `set_PlaySpeed` appear in
the `via.motion.Motion` surface list. **It should ask a second on the same keypress:** enumerate the
player's gameplay components for a movement-driver-shaped type and a `getMoveSpeed`-shaped method
(any `app.ropeway.*` type whose method list carries a float getter named for move speed). If such a
method exists, req 4 needs both halves; if it genuinely does not, then §8d's "by construction"
reading survives for RE2 specifically and that is worth recording as a difference between the two
games rather than an assumption.

Cheapest possible test if the name is found: scale the returned speed and the layer rate by the same
factor and check that footstep cadence still matches travel. A mismatch is exactly the failure the
pairing exists to prevent.

## Sources

- [Junh2x/RE9-Movement-Speed-Mod](https://github.com/Junh2x/RE9-Movement-Speed-Mod) —
  `reframework/autorun/re9_movement_speed.lua`, the shipping mod already credited by this project.
  Read for structure only; nothing copied.
- [Namsku/re-engine-trainer](https://github.com/Namsku/re-engine-trainer) — `re9/requiem_trainer.lua`
  and `re9/requiem_trainer/features.lua`. Read for structure only; nothing copied.
- [praydog/REFramework](https://github.com/praydog/REFramework) — the scripting platform both rely on.
