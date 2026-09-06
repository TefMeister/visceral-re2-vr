# 2026-09-06 — HD hands: the nails, drawn from the rig

**Status:** built, deployed 16:53 local, unseen in game. Previous texture set kept as `.tex.34.prev` beside each file.
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
