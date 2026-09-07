# The wrist grain step now has a knob — and it is deliberately switched off (2026-09-07 23:05 local, home PC, static, NO LAUNCH)

**Tool touched:** `dev-archive/tools/blender/hd_hands_paint.py` — one new argument, one line of arithmetic,
two new print lines. Nothing was rebuilt and no texture was regenerated.

## What this is finishing

This morning's session **cancelled the colour seam** and measured it out of existence: our build's step
across a UV seam went from 3.47 to 1.11 in 0–255 units, against the artist's own 2.51. So the band Tefa can
still see is not colour.

The `[PD]` row that came out of that named the next candidate, and it is a **grain** step rather than a
colour one. The hands are painted at ~1.6 atlas texels per millimetre of skin; the forearms at ~1.0. That is
a **×1.62 jump across the left wrist seam and ×1.58 across the right** `[measured 2026-09-07]`. The shipped
material still runs the engine's own `Detail_Skin` tile (128², `Detail_UVScale` 0.5), and that tile repeats
in **UV space** — so its grain lands 1.6× coarser on the forearm than on the hand and steps exactly where
Tefa sees the band. Our own pores are sized in 3D and cannot step this way. `[hypothesis]` that this is what
is left.

## The knob

`--detail-tile` (float, default **1.0**) scales how much of that engine tile survives over the skin we
paint. It multiplies only the in-paint term of the mask we write:

```python
m = msk[..., 0] * (1.0 - ed) + msk[..., 0] * pp * a.detail_tile * ed
```

`ed` is the soft paint mask, `pp` the existing palm rule. **Texels outside our paint are untouched**, which
matters because this atlas is shared with the jacket.

## Why it ships OFF, and that is the whole point of the row

`[verified-numerically 2026-09-07]` At the default the arithmetic is **bit-identical** to every build before
today — `np.array_equal(old, new)` is `True`, max absolute difference `0.0`, over a 256² random case. So no
texture Tefa has judged changes by a single texel unless the knob is moved.

Three reasons not to move it unasked:

1. **It changes skin that is already good.** Tefa signed off the nails and the hand surface on 2026-09-06
   (*"fingernails are perfect now!!!"*). This alters the micro-texture across all of it.
2. **It is a two-knob change, not one.** With the tile off there is strictly *less* total relief, so
   `--pores` (currently 0.3) wants raising to keep the same amount of texture. The build now prints that
   reminder with the current value whenever the knob is below 1.0, so the second half cannot be forgotten.
3. **The evidence does not yet justify it.** The density step is measured, but "the tile is what Tefa is
   seeing" is still `[hypothesis]`. The launch that would confirm it is the one already on the board: if the
   band comes back **fainter but still there** after the colour fix, this is the next lever. If the band is
   gone, this knob is never needed and the row closes without being used.

## One behaviour worth knowing before it is used

At `--detail-tile 0` the mask inside the paint collapses to `msk * (1 - ed)`, not to a flat zero
`[verified-numerically 2026-09-07]`. Because `ed` is a *soft* mask, the tile fades out across the paint
boundary rather than being cut at it. That is the wanted behaviour — a hard cut would draw a new line of its
own, exactly the failure this whole thread has been chasing — but it means the tile is not perfectly absent
right at the edge of the painted region, and a very close look there would still find a little of it.

## Also printed now, on every run

The mask block reports how many texels lie **outside** our paint and their mean, so the "the jacket's share
of this shared atlas is untouched" claim is visible in the build log instead of being asserted in a comment.

## State

`[compile-verified 2026-09-07]` — `py_compile` clean; the default-is-a-no-op and the outside-the-paint
claims are both checked numerically, above. **Not run through Blender, no texture rebuilt, nothing
deployed.** The next build of the hands picks the knob up at its default and produces what is already
installed.
