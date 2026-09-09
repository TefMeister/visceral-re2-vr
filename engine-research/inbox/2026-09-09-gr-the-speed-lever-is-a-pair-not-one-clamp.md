# §8d: the movement-speed lever is a PAIR, and "by construction" does not survive the source

Supersedes: ENGINE-DOSSIER.md §8d — the clause "Because RE2 locomotion is root-motion driven (§8), a playback-rate clamp scales travel, leg cycle and footstep events together — req 4's 'drive legs and footsteps from speed' holds by construction"

**From:** `/gr` (estate sweep, 2026-09-09) · **For:** the modding lane, to fold into
`ENGINE-DOSSIER.md` **§8d** (and to add one question to the board's already-armed NUM7 probe row)

**Full write-up:** [`external-research/topics/2026-09-09-the-speed-lever-is-a-pair-and-both-public-implementations-use-both-halves.md`](../../external-research/topics/2026-09-09-the-speed-lever-is-a-pair-and-both-public-implementations-use-both-halves.md)

## The claim being corrected

§8d, verbatim:

> "Because RE2 locomotion is root-motion driven (§8), a playback-rate clamp scales travel, leg cycle
> and footstep events together — req 4's 'drive legs and footsteps from speed' holds by
> construction."

And this lane's 2026-09-02 topic called the `app.MovementDriver.getMoveSpeed` hook
"Requiem-specific".

## What the source actually shows

**Junh2x's shipping mod hooks `app.MovementDriver:getMoveSpeed` as a first-class part of the
feature**, alongside the layer-0 `set_Speed` write, applying the same factor to both
`[reported 2026-09-09, from source]`. It is not a Requiem garnish — it is half the mechanism.

**`Namsku/re-engine-trainer` does the identical pairing independently** for its Requiem "Player
Speed" feature: resolve `app.MovementDriver`, return-hook `getMoveSpeed` and scale the returned
float, and write the same factor as the motion layer speed — with separate walk/run factors and a
multi-frame restore of the layer rate on disable `[reported 2026-09-09, from source]`.

Two independent authors would not both scale the driver's own returned speed if clamping the layer
rate already moved the character. So the animation rate and the travel rate look **separately
driven**, and keeping them in sync is the mod's job.

⚠️ This is evidence about the technique's authors, not a measurement of RE2. It downgrades "holds by
construction" to `[hypothesis]`; it does not disprove it for RE2 specifically.

## Suggested dossier change

In §8d, replace the "by construction" sentence with: the layer/component rate is **one of two
halves**; both public implementations pair it with a movement-driver `getMoveSpeed` return-scale at
the same factor, so req 4 should be planned as a pair until RE2 is measured otherwise. Fix the
2026-09-02 "Requiem-specific" characterisation at the same time.

## One line to add to the NUM7 probe row

The probe already asks whether `set_PlaySpeed` is in the `via.motion.Motion` surface list. On the
same keypress, also enumerate the player's gameplay components for a movement-driver-shaped
`app.ropeway.*` type carrying a `getMoveSpeed`-shaped float getter. Found ⇒ req 4 needs both halves.
Genuinely absent ⇒ §8d's reading survives for RE2, and that difference between the two games is
itself worth recording.

## Also worth knowing (board `[USER]` row)

The `[USER]` row asking the user to hand-download "Better Movement Speed RE2" (Nexus 2391) **still
stands, but its stated value has shrunk.** The *method* is now documented twice from fully fetchable
public repos; the only thing that download can still buy is the RE2 **class and method name**, which
our own reflection dump can also produce. Worth re-wording rather than deleting.

Credit: **Junh2x**, **Namsku**. Both read for structure only; nothing copied.
