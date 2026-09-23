# The red dot sits in the sight's glass, not on the world

Order: 5001
From: mod-ideas `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-09-23

`[raw]` · `[not judged]` — *nothing checked against the game*

> "red dot scope has a red dot in the middle, red dot from the world disappears, only stays on Jmb
> handgun with a laser attatchement"

Verbatim record: [`inbox/2026-09-15l-re2-red-dot-sight-dot-in-glass.md`](../inbox/2026-09-15l-re2-red-dot-sight-dot-in-glass.md)

Read as: a gun with a red dot sight shows its dot **in the middle of the sight itself**, the way a
real one does, and the aiming dot painted onto walls and enemies goes away. The one exception is the
**JMB handgun with its laser attachment** — a laser really does put a dot on what it hits, so that one
keeps it.

Filed under visuals because it changes where the dot is *shown*; it touches weapons and aiming too.

#### ⭐ EXTENDED 2026-09-20 — not just the dot: the whole picture through the glass

`[raw]` · `[looks doable]` — *the parts are confirmed present in RE2; the path is not tested here*

> "RE2 doesn't have a sniper, but it does have the Magnum red dot scope and LE5 has one too, i would
> love to have the same sort of picture drawn on them scopes as well, just instead of a cross it would
> have a red dot in the middle of the glass."

Verbatim record: [`inbox/2026-09-20-re2-visceral-drawn-picture-on-magnum-and-le5-red-dot-glass.md`](../inbox/2026-09-20-re2-visceral-drawn-picture-on-magnum-and-le5-red-dot-glass.md)

The September entry above asked for the dot to be **in** the glass instead of painted on the world.
This asks for the **view through the glass** as well — a real rendered picture, the way the Village
sniper scope draws one — with a red dot in the middle where that project has a crosshair. Named
weapons: the **Magnum's red dot sight** and the **LE5**.

**Why this moved from `[not judged]` to `[looks doable]`, and exactly how far that goes.** Checked
2026-09-20 against RE2's own type database (`il2cpp_dump.json`, 494 MB, already on the home PC):

- ⚠️ **RE2 has no scope or sight system whatsoever** — every `scope` name in the whole dump is sound,
  pointgraph or unrelated `[measured 2026-09-20]`. So there is nothing of the game's own to switch on,
  and RE4 Remake's route (it renders its scope picture natively and a VR mod need only re-pose that
  camera) is **closed here**. The picture has to be built.
- ⭐ **But every part the Village picture is built from is present in RE2**: `via.render.Mirror`,
  `updatableMaterial`, `CapturePlane` and `RenderOutput` `[measured 2026-09-20]`. So this is a **port of
  work that already exists**, not a new investigation. `[inferred-static 2026-09-20]`

**What it'd take.** The Village stack, moved over, then simplified — and the simplifications are real,
because a red dot is a far easier target than a sniper scope:

1. **No magnification.** A red dot is 1×. That removes the crop maths, the zoom-per-scope work and the
   edge-streak problem that have taken up most of the Village project — the picture through a 1× sight
   is just the view from where the sight is.
2. **Two weapons, two sets of offsets**, in a per-weapon table — the same shape RE4's working mod uses
   (it needs one even with the engine handing it a correct camera), so plan for it from the start.
3. **The dot itself** is the easy half and is already described in the entry above.
4. ⚠️ **The unsolved Village faults would come with it** — the jitter when the head turns, and the
   boot-time buffer latch. Worth waiting until those are settled there rather than porting them here.

⚠️ **Not checked in game.** Nobody has yet put a mirror on a weapon in RE2; the verdict above rests on
the parts existing and on the Village project's experience with the same parts on RE8.

**What it'd take — a guess, not a finding:** two pieces. Hiding the world dot for every gun except the
laser one, and drawing a dot inside the sight's glass. ⚠️ In VR a dot in the glass only lines up when
your eye is behind the sight, which is how real red dots behave — good — but it has to be placed so it
matches where the bullet goes. Unchecked; the Village scope project has already solved drawing
something onto a sight's glass on this engine, which may be reusable.

---
