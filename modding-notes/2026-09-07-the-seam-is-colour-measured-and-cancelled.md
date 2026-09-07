# The remaining hand seam: measured instead of guessed, and cancelled (2026-09-07 08:00–09:00 local, home PC, static, NO LAUNCH)

**Tools:** new `dev-archive/tools/blender/seam_measure.py` (read-only measurement), new
`dev-archive/tools/blender/uv_seams.py` (island + seam finding, shared), `hd_hands_paint.py` (`--seam-blend`).

Tefa, this morning: *"can you try blend those hand texture seams… you said you can look at the exact colour
number and make the transition seamless."* The `[PD]` row on the board said the same thing and said it needed
no game running, so this is that row, done.

## The rule this session followed

Inference had been wrong three times on this seam and Tefa held the ground truth each time. So nothing was
reasoned about: a measuring tool was written first, and every claim below is a number it printed.

## Two things the measurement had to get right before it could be believed

1. **The mesh has no shared edges at a UV seam.** A game mesh duplicates its vertices there, so the two sides
   are separate geometry sitting in the same place. The first run reported **14 islands and 0 seams**. Boundary
   edges have to be paired by POSITION. `[measured 2026-09-07]`
2. **Blender does not linearise these PNGs.** `img.pixels` hands back the stored bytes ÷ 255, with no colour
   management: mean |blender − raw PNG bytes| = **0.00000** over 200 000 texels, against **0.222** for a
   linearised copy `[verified-numerically 2026-09-07]`. An early version of the tool applied an sRGB encode on
   top and inflated every number it printed by ~65 %. The comment in `hd_hands_paint.py` claiming linear floats
   was wrong the same way and is now corrected; its `save_np` docstring was right all along.

Two more traps were caught and fixed inside the tool, both of which had produced confident nonsense: comparing
world normals without removing the mesh's own turn reported every knuckle crease as a seam fault, and averaging
world normals along a seam that **circles the wrist** cancels to noise (it reported a 110° error at the left
wrist, unchanged at every sampling distance and present in the artist's own map — the signature of a broken
measurement, not a broken texture). Both are now measured per sample in the sample's own frame.

## What the seam actually is `[verified-numerically 2026-09-07]`

3 570 samples over 2 291 mm of island boundary, sampled 0.1 mm inside each island, in the 0–255 numbers a paint
program shows:

| albedo | mean \|step\| | median | p90 | p99 |
| --- | --- | --- | --- | --- |
| the artist's own 1024 | 2.51 | 1.70 | 6.21 | 16.0 |
| **our 23:31 build** | **3.47** | **2.65** | **8.58** | **21.3** |
| our build with `--seam-blend` | **1.11** | **0.77** | **2.74** | **6.68** |

Three things follow, and the first two contradict what the board expected:

- **It is not whole islands carrying different average colour.** Their means sit within 4 units of each other
  (221.5–225.4 on R). The step is **local, along the seam line itself**.
- **It is not the normal map, and not tangent handedness.** Systematic tilt across a seam is 1.5–4.3° in our
  map and 1.4–3.4° in the artist's — no worse. Handedness flips on **0.0 %** of the seam length.
- **About 40 % of the step was ours** (3.47 against the artist's 2.51). The 4× upscale, the rim repair pulling
  each island's own interior colour out to its edge, and every field that fades near a border each add a little.

## The fix: `--seam-blend` (5 mm, on by default)

No cleverness about causes is needed. The two sides of a seam are the same skin, so: read both **0.30 mm** in,
move each **half way to the other**, and let that correction fade smoothly into the island. Corrections spread
only within their own island — the neighbour needs the opposite sign, so a blur crossing the boundary would
cancel exactly what it is meant to fix. The spread is the standard harmonic one (coarse-to-fine masked blur
with the seam values re-imposed), and it is carried a few texels into the gutter so the shader's filtering at
the border picks up the corrected colour.

7 087 seed texels, correction |mean| 0.03 and max 17.3 units — i.e. it does nothing except at the seams.
At the two places Tefa named:

| | before | after |
| --- | --- | --- |
| right wrist (islands 7/12, 98.6 mm) | +1.11, sd 2.14 | **+0.22, sd 0.61** |
| left wrist (1/9, 140.8 mm) | +0.80 | **+0.09** |
| base of the left thumb (0/1, 82.7 mm) | +0.87, sd 5.68 | **−0.39, sd 2.68** |

**Deployed 08:38** as `pl1000_Jacket_ALBM.tex.34` only — NRMR and MSK1 came out byte-identical, so they were
not touched. Restore point: `pl1000_Jacket_ALBM.tex.34.preseam` (the 23:31 build). Unseen in game.

**The pipeline was verified reproducible first:** rebuilding the 23:31 build with `--seam-blend 0` gave
**byte-identical ALBM, NRMR and MSK1**, and rebuilding the unchanged NRMR `.tex.34` matched the deployed file
byte for byte `[verified-numerically 2026-09-07]`. So the only difference Tefa will see is the seam correction.

## What is still there, and is the next candidate: a 1.6× step in texel density at both wrists

`[measured 2026-09-07]` — atlas texels per millimetre of skin, at 1024:

| seam | length | density ratio |
| --- | --- | --- |
| left wrist (hand island 1 / forearm island 9) | 140.8 mm | **×1.62** |
| right wrist (12 / 7) | 140.4 mm | **×1.58** |
| base of each thumb (0/1, 11/12) | 82.7 / 29.6 mm | ×1.13–1.14 |
| little-finger root (1/10, 12/13) | 55.6 mm | ×1.18 |

Hands run at ~1.6 texels/mm, forearms at ~1.0. This matters because the shipped material still runs the
engine's own `Detail_Skin` tile (128², `Detail_UVScale` 0.5) **in UV space**, so its grain is as fine as the
atlas is dense — 1.6× coarser on the forearm than on the hand, stepping exactly at the wrist. That is the same
"grainy one side, smooth the other" recorded in the forearm-seam note, and it lands precisely where Tefa sees a
band. Our own pores are 3D-sized and do not step; the engine's tile does. `[hypothesis]` that this is what is
left of the band — the density step is measured, the tile's contribution to it is not.

**We can switch it off:** the MDF already points `DetailMaskMap` at our own `pl1000_Jacket_MSK1`
`[verified-live, deployed since 2026-09-06 15:06]`, so setting that mask to 0 over hands and forearms stops the
engine's tile there and leaves only our own relief, which is continuous across every seam. It is one knob and a
~1 min rebuild, but it changes skin Tefa has already judged good, so it is not being switched on unasked —
`--pores` would want raising to carry the micro-detail alone.

## In the headset, one launch reads

| what is seen | what it means |
| --- | --- |
| the faint band across the right wrist and the diagonal at the thumb base are **gone** | the colour step was the whole of it; done |
| they are **fainter but still there** | the residue is the ×1.6 density step; next is killing the engine's detail tile over the skin |
| **unchanged** | colour was never it — go straight to the detail tile, and treat this session's measurement as necessary but not sufficient |
| anything looks blotchy or patchy near a seam | the spread distance is wrong; `--seam-blend 2` or `10`, ~1 min |

Restore: rename `pl1000_Jacket_ALBM.tex.34.preseam` back over `pl1000_Jacket_ALBM.tex.34`.
