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
