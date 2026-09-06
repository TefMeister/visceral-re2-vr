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

---

# Pass 13 — the swollen rim (2026-09-06 18:00–18:12, deployed)

Tefa's second VR look: *"they look much better now but still have faulty areas"*. Four screenshots, and every mark
on both hands circles the same thing — **a raised lip of skin standing proud around the outside of each nail**,
strongest on whichever side the room light rakes across. Nothing else was marked.

**Cause, ours again.** The nail pass draws a skin fold just outside the plate, at `sdf ≈ 2` texels, and it was
weighted **+0.08** in the height field — taller than the nail plate itself at +0.05, and the largest positive
relief anywhere in the nail. It was meant to be the soft ridge of skin that a nail sits in; at that height it is a
swelling.

**Fix.** The fold drops to **0.022**, its spread narrows from 1.6 to 1.1 texels, and it moves out from 2.0 to 2.6
so it stops merging with the plate's own groove. Its contribution to the albedo darkening drops from 0.5 to 0.2.
The transverse dome eases 0.05 → 0.035, the free edge 0.05 → 0.04 and the plate gloss 0.10 → 0.06, because next to
that rim the plates were reading a little wet. New knob: `--nail-fold`.

**Static check.** Normal-map tilt in the ring of texels just outside each plate, which is where the fold lives:

| region | mean tilt | 95th percentile |
| --- | --- | --- |
| the rim ring, after | 0.111 | 0.274 |
| the nail plate | 0.100 | 0.266 |
| ordinary hand skin, for scale | 0.076 | 0.255 |

So the rim now sits within a hair of ordinary skin instead of dominating it `[verified-numerically 2026-09-06]`.
The plate's own outline groove still peaks near 0.9, which is intended — that is what makes the nail read.

Deployed 18:12. `.tex.34.prev` still holds the pre-nails set.

---

# Pass 14 — the lighter base comes back (2026-09-06 18:25–18:37, deployed)

Tefa's reference for the nails was a 17:17 screenshot — the **16:53 build** — and what they liked in it is the faint
lighter crescent at the base of each nail, with the instruction to keep the tips as they are now (no grey). That
crescent is the lunula the nail pass has always drawn; it was at 0.6 in the 16:53 build and was cut to 0.35 in pass
12 when everything else was being tamed. Back to **0.6**, now the knob `--nail-lunula`. Nothing else changed from
pass 13 (the rim fix is in this build too). Deployed 18:37; `.tex.34.prev` still holds the pre-nails set.

Tefa also reported the nails "still like they were before" on the second look. The 18:12 build needed a game
restart to show (textures load at boot); whether they had restarted is not known, so the pass-13 rim fix is
**unseen** until this build is looked at.

---

# Pass 15 — back to the 16:53 shape, centred on the artist's nail (2026-09-06 20:04, deployed, UNSEEN)

Tefa's 21 screenshots of pass 14: the plates fitted to the artist's blob read as **teardrops / gel blobs** in VR; the
16:53 build (pass 8) was "the most accurate, only the fingertips are wrong, nearly everything else really good".
So pass 15 = pass 8's frame, shape and size (3D bone frame, rounded rectangle 62 % of the finger's width, 58 % of
the phalanx, corners 0.75/0.5), with only the CENTRE taken from the artist's nail; the pass-12 wipe, no grey tips,
the lunula and the cut-down fold all stay. A UV-Jacobian rescale of the bone sizes was tried first and produced
28×14 and 34×149 — discarded. Session ended at Tefa's 3 % usage; renders not made.

**Bracelets, from the 19:51 log:** every loose file loaded (both meshes, the MDF, all six textures), `BRACELET l/r
CREATED`, `DrawDefault=1`, `brac=on` — and nothing visible. `materials: n=0` on both, **but the neck plug reports
n=0 too and has never been visibly confirmed either** (2026-09-06 01:10: "no visible change"). Two hypotheses: the
material read-back at creation is too early and something else hides both; or a mesh with an unresolved material
does not draw at all, plug included. `twist l=` swung to ±90° while Tefa moved: **the wrist twist relative to the
radius joint is the full pronation, so the radius joint carries none of it** — k must be ~1 once the bands show.
