# 2026-09-06 — HD hands: the nails, drawn from the rig

**Status:** deployed 17:48 local after Tefa's first VR look (pass 12). The section at the foot of this note supersedes the grey free edge, the shadow and the plate sizing described above. Previous texture set kept as `.tex.34.prev` beside each file.
**Character:** Claire, `pl1000`, skin material `pl1000_Body_Mat` on the `pl1000_Jacket_*` atlas.
**Tools:** `dev-archive/tools/blender/hd_hands_paint.py` (nail pass added), `hd_hands_render.py` (`--focus fingers|thumb`),
`dev-archive/tools/re-engine/body_tex_build.py` (unchanged).

## The problem

The artist's nail is a 14-pixel pale blob per finger in the 1024² atlas. Upscaled to 4K it is a soft smudge with a
white sliver at the tip. The 2026-09-06 03:00 look in VR asked for "lines" on the nails; every later pass left them alone.

## What the pass does

The paint script already rasterises the mesh into UV space to find joints. The nail pass extends that:

1. **Per-pixel 3D position and surface normal** over every finger island (`raster_assign`, overwrite rather than max,
   because coordinates are signed).
2. **One frame per nail** from the distal phalanx: axis = previous joint → distal joint; back direction = the hand-wide
   dorsal direction bent perpendicular to the bone, then **replaced by the cross-section's shape** — 2D PCA of the
   distal verts' radial offsets, the major axis is lateral, the nail lies on the face perpendicular to it. This moved the
   little finger's back by 19° and the thumb's by 47° `[measured 2026-09-06]`; the hand-wide guess was wrong on both.
3. **Plate = rounded-rectangle signed distance** in texels: X = lateral offset from the bone's sagittal plane (not an
   angle round the bone — the little finger's bone runs off-centre and an angle put its plate on the side), Y = distance
   along the bone. Cuticle at 42 % of the phalanx, free edge at 92 % (97 % reached the tip cap, where the frame is
   meaningless), width 62 % of the finger's width from the verts' 90th-percentile lateral offset (85 % read as a cap over
   the whole tip). Proximal corners rounder than distal, radius continuous in Y. Coordinates gaussian-smoothed over the
   pixel set (σ 2.5 texels) so the coarse mesh's facets do not wobble the outline.
4. **Back test by surface normal** (`normal · back > 0.1`), not bone height — the little finger's bone hugs its back, so
   height faded its plate. Tip cap excluded by radial distance.
5. **Wipe the artist's blob**: within 14 texels of the plate, on the back or the tip cap, only where the pixel is brighter
   than the median of the local skin ring 6–16 texels out. The first pass wiped the whole distal segment to the proximal
   skin colour and darkened every fingertip, because the tip skin is painted lighter than the finger.
6. **Colour** (multiplicative on that local ring colour, so both hands and all fingers stay consistent): plate = skin
   lifted a fifth toward white and a touch pinker; lunula = skin lifted halfway to white, crescent at the base; free edge
   = greyish white over the last third; cuticle band −6 %, side-fold groove −5 %, hyponychium shadow −18 %. The first
   pass's skin × pink read as a dark brown sticker.
7. **Relief** into the normal map through the same height → normal path as the creases: plate +0.05, transverse dome
   +0.05, free edge +0.08, outline groove −0.12 (σ 1.1 texels, cuticle + sides only), skin fold +0.08 outside the groove,
   longitudinal ridges 0.004 (0.01 read as stripes). No wrinkles, baked pores, pore darkening or tiled detail (MSK1) on
   the plate; plate roughness −0.18 in the NRMR alpha `[hypothesis]` that alpha is roughness, as before.

## Static proof

Every plate's centroid lands 7–23 texels from the centroid of the artist's blob (bright, low-saturation pixels in the
same zone of the original albedo); the blobs are about 50 texels across `[verified-numerically 2026-09-06, n=10 nails]`.
Plate sizes: index 58×59 texels, middle 60×61, ring 53×61, little 41×52, thumb 63×61 (≈ 10.6 × 10.8 mm on the index at
0.183 mm/texel). Blender close-ups (`claire-hands-work/renders/fingers/`, `renders/thumb/`): four fingers show a
distinct plate, side folds, cuticle and a pale free edge; the thumb a plate on its flat face.

## Eight passes, what each one taught

| pass | seen | fix |
| --- | --- | --- |
| 1 | dark brown sticker, whole fingertip darkened, wedge at the tip | lighter plate, local ring colour, free edge at 90 %, radius continuous |
| 2 | white sliver of the artist's free edge beside the plate; little finger a wedge | wipe reaches the tip cap; rn guard |
| 3 | mean-normal refinement moved the back by 1–4°, little finger unchanged | lateral distance instead of angle |
| 4 | wipe covered the pad too (lateral X is small on the pad) | wipe limited to the back + cap |
| 5 | little finger's plate faded (bone height small on its back) | back test by surface normal |
| 6 | outline wobbled 2–6 texels with the facets; plates too wide | coordinates smoothed |
| 7 | plates read as caps | width 85 % → 62 % of the finger |
| 8 | little finger and thumb still off their backs | PCA of the cross-section |

## What the headset decides

| seen | means |
| --- | --- |
| each fingertip shows a distinct nail with a paler tip | works; tune `--nail-width` / `--nail-length` / `--nails` by number |
| little finger's nail off to one side, thumb's on the wrong face | the cross-section rule fails on a rolled finger; add a per-finger override |
| a nail with a hard straight edge | a UV seam runs through the plate; smooth the coordinates across islands |
| nails invisible | the plate/skin contrast is below the game's lighting; raise the lift toward white |

Restore: rename the three `.prev` files back. Rebuild ≈ 4 min (paint 2 min, convert 1 min, copy).

---

# Pass 9–12 — Tefa's first VR look, and what it changed (2026-09-06 17:00–17:48)

Deployed 17:48. The `.tex.34.prev` files still hold the pre-nails set, so they remain the restore point.

## What Tefa saw, in their own marks

Five annotated 4K screenshots (`Desktop\Claire hands\`). Every nail on both hands was circled at the same place:
a **dark grey-brown band across the distal end**, and on one nail a green curve was drawn over the shape the
boundary should have, against a red curve over the shape it had. Tefa's own reading of the fix:

> "i think the fix to fingernails is not painting the tips grey at all"

That is the whole of it, and it was right.

## Three causes, all ours

1. **We painted the tips grey.** The greyish free edge (`fe_col`) plus a hyponychium shadow beyond the plate. Both
   are gone from pass 12; the free edge now reads by relief alone. A static count over the nail zone finds **0 px**
   that are both desaturated and dark, before OR after `[verified-numerically 2026-09-06]` — so the "original nail
   underneath" in the screenshots was in fact our own grey, not the artist's.
2. **The wipe could only see brighter pixels.** It fired on `lum > skin + 0.03`, so anything of the artist's nail
   that was darker than skin survived and sat just past our edge. Pass 12 wipes on how far a pixel is from *its own
   finger's* skin, in both directions. Over the nail zone: **1,252 pale pixels before, 0 after** `[verified-numerically
   2026-09-06]`, and peak luminance drops 0.872 → 0.767.
3. **The plate was sized from fractions of the bone**, so it stopped short of what the artist had painted. Pass 12
   **fits the plate to the artist's own nail**, which is the thing that has been right all along.

## How the fit works, and why in UV

- **Detect** the artist's nail by **saturation**, not brightness: it is pale grey-pink on pink skin, so it desaturates
  by 0.03–0.06 while its luminance overlaps the skin's own shading `[measured 2026-09-06, n=4 nails]`. The first try at
  pass 9 used overall colour distance, caught the whole shaded fingertip, and the plate swallowed the tip.
- **Fit in texture space.** The artist drew in UV, so measuring and drawing in UV cancels the island's stretch exactly
  where a 3D-projected box does not. The only thing still taken from the rig is which way the finger runs: a
  least-squares fit of pixel coordinates against along-bone distance gives that as a UV direction.
- **Result:** plate centroids land **0–4 texels** from the artist's nail centroid, against 7–23 before
  `[verified-numerically 2026-09-06, n=10 nails]`. About 12–22 % of the artist's nail still falls outside the rounded
  rectangle at the corners; that part is wiped to skin rather than covered, which is the intended outcome.
- The distal end now carries the **large** corner radius, so the free edge is a strong arc bulging toward the
  fingertip — Tefa's green curve. Pass 8's flat distal edge with small corners was the red one.
- The plate is no longer clamped a second time by the back-of-finger mask; that was biting notches out of an outline
  that had already been fitted inside it.

## Still open

- **Contrast.** The plate is skin lifted a third toward white with a pink tint. It reads in the Blender renders; if it
  disappears under RPD lighting the number to raise is that 0.30.
- **The straight lines on the forearm** (Tefa's other two screenshots) are a different fault and are NOT fixed here.
  Cause found while reading the recon dump: the skin submesh `Group_0_Sub_0__pl1000_Body_Mat` carries only **798
  forearm faces**, while **`pl1000_Jacket_Mat` carries 8,125** `[measured 2026-09-06, recon dump]`. Most of the bare
  forearm is drawn by the *jacket* submesh, which our paint has never touched — so our treatment stops dead at the
  submesh boundary and that boundary is the straight band across the arm. The fix is to rasterise the jacket
  submesh's forearm-bone faces into the same paint mask. Queued, not attempted.
