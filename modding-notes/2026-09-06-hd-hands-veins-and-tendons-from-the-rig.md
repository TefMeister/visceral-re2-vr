# HD hands: veins and tendons from the rig, made to land on the back of the hand (2026-09-06 11:30–11:55 local, home PC, static)

**The game was not launched.** Everything below was judged in headless Blender renders of Claire's own hand mesh
with the new textures on it; the in-game look is still Tefa's to give. Follows
`2026-09-06-hd-hands-tier-1b-first-4k-repaint-procedural.md` §third pass, which left the rig-derived lines switched
off after two attempts drew "scars, not veins".

## Why the first two attempts failed `[verified 2026-09-06 on renders, n=2 hands]`

The dorsal pick was `Dv = clip(-(normal · palm))` with `palm` = the thumb's offset from the wrist→middle axis. That
vector lies **in** the palm plane (it points at the thumb), so the pick selected the **little-finger edge** of the hand,
not its back — exactly the "island edge" the note described. Rendered as vertex colour, the old weight lit the ulnar
edge and the sides of the fingers; nothing on either face.

The plane's normal is `lat = axis × palm`, but its sign flips between the two hands. Two candidate rules disagreed:

| rule | left hand | verdict |
| --- | --- | --- |
| thumb's second joint vs the metacarpal centroid | 1.7 mm offset → says `+lat` | too small to trust, and **wrong** |
| finger curl: `Σ (unit(j2−j1) − unit(j1−j0)) · lat` over four fingers | +1.69 → palm is `+lat`, dorsal `−lat` | **right**: the `−lat` render shows the nails |

`hd_hands_dorsal_preview.py` renders the hand from both faces with the game albedo (nails = back) and with each
weight as vertex colour; that is what settled it. Note the albedo's alpha is metallic (~0.04), so Workbench draws the
hand invisible unless the image's alpha mode is set to none — a trap worth one line.

## Three more things the renders caught, each fixed in `hd_hands_paint.py`

1. **Knuckles are not all the same bone index.** `index_0`/`middle_0` start at the knuckle (6.8 cm along the hand) but
   `ring_0`/`little_0` are metacarpals starting 2 cm from the wrist `[measured 2026-09-06]`, so two tendons were
   stubs at the wrist. Now: per finger, the first bone more than 4 cm along the wrist→middle axis.
2. **Vertex snapping drew staircases.** Only ~250 vertices exist on the back of the hand, so snapping samples to the
   nearest vertex produced zigzags. Now each 3D sample is projected onto a BVH of the dorsal triangles and its UV is
   interpolated inside the triangle it lands on — smooth curves. Samples are lifted 1 cm toward the skin first
   (the bone segments run mid-hand) and any sample more than 1 cm off the one-sided surface breaks the line instead of
   snapping to the island edge.
3. **Widths in millimetres, no plateaus.** The back of the hand runs at **0.183 mm per texel at 4K** (3D area
   223 cm² over 668 k texels, identical for both hands) `[measured 2026-09-06]`. Tendons are drawn as ~4 mm
   ridges (σ 1.7 mm), veins ~2.8 mm (σ 1.2 mm); each line family is normalised by the analytic peak of one isolated
   blurred line and soft-clipped with `1 − exp(−1.2x)`, so crossings brighten smoothly instead of clipping into
   hard-edged pills (the "cross at the wrist" of the first fixed render). Line ends taper. Height amplitudes 0.55 / 0.40
   before the ×0.70 peak, chosen for a maximum normal tilt of roughly 0.25 (veins) and 0.12 (tendons).

Layout now: four extensor tendons from under the wrist to each knuckle, three dorsal veins from the knuckle gaps down
between them, a faint dorsal arch joining them a third of the way up, and a cephalic vein from the thumb/index web
toward the wrist. Coverage: veins 2.5 %, tendons 1.6 % of the hand mask. Both hands mirror-symmetric on their own
UV islands (the mesh has two UV layers; the renderer and the scripts both use `UVMap0`).

## How it was judged without the game

`hd_hands_render.py` — EEVEE close-up of one hand, dorsal or palm, any number of texture sets, raking key light on the
little-finger side of whichever hand. `hd_hands_debug_render.py` paints the line masks in loud colours over the albedo
so a render shows **where** each line lands, which is what caught items 1 and 2 above. Renders live in
`hd-hands-work/renders/` in the extracted folder (derivatives of game textures — not in the repo).

Final renders: the left and right backs of the hand show tendons as soft raised lines to each knuckle and veins as
subtler ridges between them; the deployed pass-3 set beside them has neither. Not judged: whether the strength suits
VR at 20 cm — that is the FLAT read.

## Deployed `[compile-verified 2026-09-06]`

`hd_hands_paint.py … --anatomy 1.0` → `body_tex_build.py` → the two 4K `.tex.34` copied into
`<RE2>\natives\STM\sectionroot\character\player\pl3000\pl3000\` at 11:51 local. The pass-3 pair is kept as
`extracted…\body-tex-out\pass3-backup-pl3000_Body_*.tex.34`; copy those back to revert. `--anatomy` still defaults
to 0 in the script until the VR verdict. New helper scripts: `hd_hands_dorsal_preview.py`, `hd_hands_render.py`,
`hd_hands_debug_render.py` (all `tools/blender/`).

## What the next look decides (FLAT, any launch as Claire)

| what you see on the back of the hands | it means |
| --- | --- |
| tendons and veins present, believable at arm's length and at 20 cm | done; tune with `--anatomy 0.7`/`1.3` only if asked |
| lines present but too strong / cord-like | `--anatomy 0.6`; if still hard-edged, the game's normal decode is stronger than EEVEE's — halve the amplitudes in the script |
| lines present but invisible | `--anatomy 1.5`, or the game's roughness is hiding the relief (NRMR alpha hypothesis) |
| hands unchanged from last night | loose files not re-read — restart the game, then check `reframework_accessed_files.txt` for `pl3000_Body_NRMR` |
| ridges look **inverted** (grooves) on one hand only | tangent handedness on the mirrored island — flip the line sign for that side (one line) |

Still open on the hands after this: nails (nail-bed and cuticle lines on the nail islands), the game's own grime overlay
(Record system masks), and the "touch grainy" one-number tune — the remaining `[PD]` row.
