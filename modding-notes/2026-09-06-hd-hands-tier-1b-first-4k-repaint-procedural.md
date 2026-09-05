# HD hands, tier 1b first pass: a procedural 4K repaint of the hand strip (2026-09-06 03:40–04:20, home PC, `/pd`, static)

**The game was not launched. Nothing here has been seen in the engine.** Follows
`2026-09-06-hd-hands-tier-1a-pores-through-the-detail-slot.md`; same brief, same reference shots.

## What is built `[compile-verified 2026-09-06; previews inspected by eye]`

`tools/blender/hd_hands_paint.py` (headless Blender + RE Mesh Editor, 17 s at 4K) re-authors Claire's two body
textures at **4096²** from the 1024² originals, touching only the skin material's hand + forearm UV region
(12.1 % of the image; everything else is a clean upscale):

1. **Joint creases where the skin actually folds.** Every finger/wrist joint is found on the mesh as the zone
   where two adjacent bones share the weight; smooth skinning blends almost everywhere (mean 0.44 over the
   hand), so only the near-50/50 core is kept (`clip((J-0.62)/0.38)²`), rasterised into UV space, blurred a
   few texels → a groove in the normal map and a faint redness in the albedo, exactly at the knuckles and
   flexion lines. No hand-placed coordinates anywhere: it follows the rig.
2. **Micro-relief baked in**: our own tiling detail normal (`make_skin_detail.py`), 6 tiles across, composited
   into the normal map's RG inside the mask, slightly stronger on the fingers. So pores exist in the base
   normal even if the material's detail slot turns out to be ignored.
3. **Colour**: low-frequency mottling ±2 %, a barely-there pore darkening (6 %), knuckle redness. The first
   attempt (25 % pore darkening, 3.5 % mottling) read as dirt speckles in the preview and was cut.
4. Alpha channels carried over (ALBM alpha = metallic, untouched; NRMR alpha nudged −0.06 in creases on the
   hypothesis that it is roughness).

PNGs are written with a pure-Python writer so Blender's colour management never touches the data (the numbers
read from the game's PNG are the numbers written). `tools/re-engine/body_tex_build.py` turns them into
`pl3000_Body_ALBM.tex.34` (BC7 sRGB, dxgi 99) and `pl3000_Body_NRMR.tex.34` (BC7, dxgi 98), 4096², with mips;
22.4 MB each.

**Deployed** (loose, `natives/stm/sectionroot/character/player/pl3000/pl3000/`): the two 4K textures and the
patched MDF from tier 1a; plus `natives/stm/visceral/visceral_skin_detail_NRM.tex.34`. **None of the three
body files are in the repo** — they are derivatives of the player's textures; the scripts rebuild them.

Previews (hand strip, original above / new below): `hd-hands-out/preview_albedo_strip.png`,
`preview_normal_strip.png` in the extracted folder. What they show: the original albedo already carries faint
dorsal veins; the new one adds mottling and knuckle redness without speckle; the new normal has fine grain on
all skin and grooves at the joints.

## What the first look decides (FLAT, same launch as the head hider and tier 1a)

| what you see on Claire's hands | it means |
| --- | --- |
| skin grain, knuckle creases catching the light, slightly uneven colour | 1b works. Judge scale: grain too coarse/fine → `--pores`, creases too deep/shallow → `--crease`, colour → `--tone`; 17 s per iteration + `body_tex_build.py` |
| grain but no creases | the joint field landed off the visible knuckles (UV mirroring or the threshold too strict — lower 0.62) |
| a visible **seam or tint change at the wrist / forearm edge** | the mask feather (1.5 px at 4K) is too tight, or the cloth sleeve boundary — raise the blur |
| **whole-body texture looks wrong** (jacket, ribbon) | the upscale itself; unlikely (bicubic of the same pixels), but then use `--size 1024` to isolate |
| shimmer at distance | mips exist (13 levels at 4K); would be the grain amplitude — lower `--pores` |
| hands unchanged | loose textures not taken (check `reframework_loose_files.txt` for `pl3000_Body_ALBM`) |

Remove: delete the two `pl3000_Body_*.tex.34` (and, for 1a, the MDF + detail tex).

## Still open on H2

- **1b, second pass**: a real skin source for the back of the hand (veins with depth, tendons when the hand
  opens, knuckle wrinkle *networks* rather than single grooves) — a CC-licensed hand scan baked in Blender, or
  hand-painted over these masks. Also: the original's faint dorsal veins could be strengthened in the normal
  map by extracting them from the albedo (they are there, low contrast).
- **Roughness channel** of NRMR still unidentified (A is the candidate). One in-game A/B decides it.
- **Tier 2** (subdivide the hand mesh) and **tier 3** (sculpt) wait on 1a + 1b being judged.
- Ada and Leon's fingertips: same scripts with the other character's prefix, once Claire's are judged.

## Second pass (04:35–05:05) — after the first in-game look `[verified-live 2026-09-06, n=1, Tefa]`

Tefa's screenshot (`Screenshot 2026-09-06 023323.png`, handgun raised, hands close): **"better for sure, but
grainy and need more polish."** The pores were plainly there, so **the detail slot works and the loose textures
are taken** — both FLAT questions answered. The grain was two layers stacked at a coarse scale (detail slot at
UV scale 6 / intensity 1.0, plus the same relief baked into the normal map at 0.35).

Changes, all deployed (game must be restarted to see them):

| what | pass 1 | pass 2 |
| --- | --- | --- |
| tile (`make_skin_detail.py`) | 220 pores, strength 1.0 (the flag was cancelled by normalisation — fixed) | 260 pores, strength 0.45 → RG std 0.075 (was 0.145) |
| detail slot (MDF) | UV scale 6, intensity 1.0 | UV scale 10, intensity 0.35 |
| baked relief in NRMR | 0.35, 6 tiles | 0.30 × 0.3 default, 10 tiles |
| joint wrinkles | — | fine ridged noise near joints, 0.18 |
| veins | — | lifted from the original albedo, gated on cooler-than-skin colour at vein width; **finds blobs, not lines** (2.7 % of the hand above 0.3), so shipped as a hint (0.35) — real veins still need a real source |

What the next look decides: grain should now read as skin, not sandpaper. If still too much, `--pores 0.15`
and `skin_detail_build.py --normal 0.2`; if too smooth, the other way. Creases and wrinkles at the knuckles are
the thing to look for next.

## Third pass (05:10–06:00) — VR verdict, forearms, and the anatomy attempt that did NOT ship

- **VR verdict on pass 2 (Tefa, ~05:15): "much better going by memory of what they looked like before."**
  `[verified-live 2026-09-06, n=1, VR]`
- **Forearms and wrists get the full treatment** (Tefa's request): the paint mask weight on forearm skin went
  from 0.6 to 1.0, so grain, mottling and colour variation now run all the way up the sleeve. Deployed.
- **Rig-derived veins and tendons — built, tried twice, switched OFF by default (`--anatomy 0`).** The idea:
  the back of each hand is the side facing away from the thumb's offset from the palm plane; four extensor
  tendons run wrist → knuckle and three dorsal veins wrist → the gaps between knuckles; draw them in UV space.
  Attempt 1 anchored on joint positions and straddled the seam between the hand island and the finger
  islands, collapsing into a squiggle. Attempt 2 walked the mesh surface, snapping 3D samples to the nearest
  dorsal vertex — only 165 candidates qualified per hand and they sat on the island edge, so the lines came out
  as hard angular ridges (scars, not veins). Diagnosis `[hypothesis]`: the dorsal/palmar pick is inverted or
  the dorsal skin is a different UV island than assumed; the fix is to dump `Dv` per vertex as a vertex-colour
  preview and look at it in Blender, not to guess again at 6 a.m. Code stays in `hd_hands_paint.py` behind the
  flag. Real veins with depth still want a scanned or painted source.

Deployed set at the end of the night (`natives/stm/sectionroot/character/player/pl3000/pl3000/`): pass-2 grain
levels + full forearms, no anatomy lines; MDF detail slot UV 10 / 0.35; tile `visceral_skin_detail_NRM.tex.34`.
