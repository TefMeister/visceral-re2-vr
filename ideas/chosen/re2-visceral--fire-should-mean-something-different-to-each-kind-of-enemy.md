# Fire should mean something different to each kind of enemy

Order: 5009
From: mod-ideas `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-09-23

`[raw]` · `[not judged]` — *nothing checked against the game*

> "grenade launcher flame rounds and flamethrower do not set Ivy zombies on fire for more than a
> second, they take one set amount of famage from the grenade impact itself and flame burst, but
> only take fire damage after for 1 second. Ivy zombies only set on fire once they are defeated and
> then burn to crisp. zombies with clothing get set on fire after continuous flamethrower flame on
> them after 3 seconds and the flames extinguish after 3 seconds of being on flames. lickers get the
> 1 second on flames as Ivy zombies, wet sewer monsters do not catch fire at all, only take damage
> from direct attack"

Verbatim record: [`inbox/2026-09-14f-re2-fire-damage-rules-per-enemy.md`](../inbox/2026-09-14f-re2-fire-damage-rules-per-enemy.md)

Right now fire is roughly one thing that happens to everything. This makes it **four different
rules**, one per kind of enemy, written out in full:

| Enemy | What fire does |
| --- | --- |
| **Ivy** | Impact + flame-burst damage, then burns for **1 second only**. Does not stay alight — **until it dies**, at which point it burns to a crisp |
| **Clothed zombie** | Needs **3 seconds of continuous flame** to catch. Then burns for **3 seconds** and goes out |
| **Licker** | Same as Ivy — **1 second**, no more |
| **Wet sewer monster** | **Never catches fire at all.** Only takes the direct hit |

⭐ **What makes this a good idea rather than a numbers tweak.** It turns the flamethrower from a
damage type into a **question about what you are looking at**. A clothed zombie is worth holding the
flame on; an Ivy is not, and you learn that by watching the flames die on it. The wet sewer creature
is the sharpest one — **being soaked is a visible reason**, so the rule explains itself the moment you
see it, without a single word of text. And the Ivy burning only *after* it dies is a proper piece of
horror writing: the fire is not how you kill it, it is what happens next.

**What it'd take** — honestly, nobody has looked yet, so this is reasoning, not a finding:

- Fire in most games is a **status effect with a duration** applied on hit. If this game works that
  way, then "Ivy burns for 1 second, clothed zombie for 3" is choosing a different duration depending
  on who was hit — the cheap half.
- **"Wet things do not catch"** is the same thing with a duration of zero, so it comes free if the
  above holds.
- **"3 seconds of continuous flame before it catches"** is the awkward one. That is not a duration,
  it is a *build-up* — the game has to remember how long this particular zombie has been standing in
  the flame, and forget it when they step out. If nothing like that exists, it has to be added.
- **"Burns to a crisp once defeated"** is a death effect, not a fire effect — likely a separate piece
  of work from all of the above, and possibly the most visible one.

⚠️ **Everything above is unchecked.** It needs a session with the game running to find out whether
fire is a tunable status at all, or something baked into each weapon. Until then the whole entry is
a description of the wish, not a plan.

- **Zombie face variety** → see [More zombie faces, so fewer of them share one](#more-zombie-faces-so-fewer-of-them-share-one)
  under Characters & appearance. Filed there because it changes appearance, not behaviour — but the
  part that makes it *work* is how the game picks a head, which is arguably this category's business.

---

_Other categories appear as ideas arrive. Nothing is missing; they just haven't been needed yet._

**To add one:** `[re2] your idea` — anywhere, any time.

🔗 Ideas for **all four** Resident Evil games at once — they share an engine and a modding framework — go on [`resident-evil-all.md`](resident-evil-all.md) (`[re games] your idea`).

Chosen by Tefa: 2026-09-24
