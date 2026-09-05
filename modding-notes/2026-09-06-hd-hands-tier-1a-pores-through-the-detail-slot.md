# HD hands, tier 1a: pores through the skin material's empty detail slot (2026-09-06 02:20–03:30, home PC, `/pd`, static)

**The game was not launched. Nothing here has been seen in the engine.** Brief and reference shots:
`dev-archive/recon/2026-09-06-hd-hands-reference/`. This note records what was measured about Claire's skin
material and textures, and what is now built and deployed for the first look.

## 1. What Claire's hands are made of `[measured 2026-09-06]`

| thing | fact |
| --- | --- |
| mesh | `pl3000.mesh`, skin submesh `pl3000_Skin_Mat` = arms + hands only, 7,493 verts / 13,444 faces, of which **7,854 faces are hand** (top weight on `l/r_hand_*` or the wrists) and 1,798 forearm |
| textures | ONE 1024×1024 set for skin AND cloth: `pl3000_Body_ALBM` (BC7 sRGB, dxgi 99), `pl3000_Body_NRMR` (BC7, dxgi 98), `pl3000_Body_ATOS` (BC1), `pl3000_Body_MSK1` (BC4, 2048) |
| where the hands are | a strip **u ∈ [0.005, 0.994], v ∈ [0.005, 0.144]** — the bottom ~145 rows of the 1024 image; hands + forearm skin together cover **12.2 %** of the texture. Masks: `hand_mask_1024.png` / `hand_mask_4096.png` in the work folder |
| normal map there | almost flat (RG std 0.028/0.021 under the mask). **There is no knuckle or tendon detail to lose** — the smoothness in the screenshots is the texture, not the shading |
| skin material | `pl3000_Skin_Mat` on `MasterMaterial/Master/Record_Player.mmtr`, 15 texture slots, 37 float properties. **`DetailMap` = `MasterMaterial/Textures/NullDetail.tex`, `Detail_UVScale` = 1, `Detail_Normal_Intensity` = 0, `Detail_AO_Intensity` = 0.** The pore layer exists in the shader and is switched off |
| detail mask | `DetailMaskMap` = `pl3000_Body_MSK1`, which is **white (1.0) over the whole hand strip** — detail would apply to the hands as-is |
| the game's own detail normals | `Detail_Niron_NRM`, `jacket_detail_NRM`: 512², BC7, RGB tangent normal (R/G mean 0.5, B ≈ 0.88–0.98), alpha ≈ 0; `NullDetail`: 32², flat, alpha 0.99 |
| ATOS | R = 1, G = 1, B = varying (0.615 mean), A = 1 — B is the per-pixel SSS/occlusion term; untouched |

Tools that made this possible, all already on the machine: RE Mesh Editor's tex converter (`.tex` ⇄ DDS ⇄ PNG,
DirectXTex BC7 inside — **needs `CoInitializeEx` when called headless**, or WIC fails with
`80004002 No such interface supported`) and its MDF reader/writer, which rebuilds string tables from the
object fields so a path or a float can simply be assigned and written back.

## 2. What is built `[compile-verified 2026-09-06]`

- **`tools/blender/make_skin_detail.py`** — Visceral's own tiling skin-detail normal, procedural (jittered pore
  dimples + ridged crease noise + broad undulation → height → tangent normal, alpha = AO), 1024², seamless,
  deterministic (seed 7). Mean RGBA 0.50/0.50/0.92/0.88, in the range of the game's own detail maps. No game
  data and no third-party texture in it.
- **`tools/re-engine/skin_detail_build.py`** — PNG → BC7 DDS with 11 mips → `visceral_skin_detail_NRM.tex.34`;
  then the player's own `pl3000.mdf2.21` with `pl3000_Skin_Mat.DetailMap` → `visceral/visceral_skin_detail_NRM.tex`,
  `Detail_UVScale` 6, `Detail_Normal_Intensity` 1.0, `Detail_AO_Intensity` 0. Re-reads its output and checks
  every other material is byte-for-byte the same list of paths and values. 30,016 → 30,014 bytes (the one path
  is a character shorter).
- **Deployed**: both files under the game's `natives/stm/…` (loose loader on). The texture also lives in
  `dev-archive/plugin/assets/natives/stm/visceral/`, so `build.sh --deploy` ships it from now on. **The patched
  MDF is not in the repo** — it is the player's file; the script rebuilds it.
- Also: `tools/blender/hd_hands_recon.py` (the measurements above, plus the masks) and `tools/blender/tex2png.py`.

**Why UV scale 6:** the hand strip is ~0.14 of V for a ~18 cm hand, so one UV unit ≈ 1.3 m of skin. At scale 6 a
1024 tile spans 0.22 m, one texel ≈ 0.21 mm, pores (220 per tile) ≈ 1 mm apart. Real pores are 0.3–0.5 mm; the
knuckle crease network is coarser. If it reads as sandpaper, lower `--pores` or raise `--uv-scale`; if it reads
as nothing, raise `--normal` (the game's Niron cloth runs the same slot at default intensity 1 in other
materials) `[hypothesis]`.

## 3. What the first look decides (FLAT, one launch, share it with the head-hider run)

Look at Claire's bare hands close up under a light.

| what you see | it means |
| --- | --- |
| skin grain / pores that stay crisp as the hand nears the eye | the slot works; tune `--uv-scale` (size of the grain) and `--normal` (depth) by taste — one command, no rebuild |
| hands unchanged | the loose MDF was not taken (check `reframework_loose_files.txt` for `pl3000.mdf2.21`), or the shader ignores DetailMap unless a flag is set (then compare against `Detail_Niron_NRM`'s material, which has intensity 1) |
| grain also on the **jacket / cloth** | expected NOT to happen (only `Skin_Mat` was patched); if it does, the MDF write touched more than it should — `verify` says it did not |
| hands turn grey / black / pink | the material failed to load the texture (path or format); the `.tex.34` header is BC7 like the originals, so suspect the path case |
| shimmering / moiré | mips are there (11), so this would be the scale being too fine; lower `--pores` |

To remove: delete the two loose files (`natives/stm/sectionroot/character/player/pl3000/pl3000/pl3000.mdf2.21`,
`natives/stm/visceral/visceral_skin_detail_NRM.tex.34`).

## 4. Still to do on H2 (tiers 1b, 2, 3)

- **1b — repaint the hand strip at 4K**: `pl3000_Body_ALBM/NRMR` re-authored at 4096² with veins, knuckle creases,
  tendons and colour variation painted into the 12 % that is hands and forearms (the masks say exactly where);
  the rest upscaled. Needs a skin source we may use (CC hand scan baked in Blender, or painted). Roughness
  channel of NRMR still to be identified (A mean 0.675 under the hands, B ≈ 1.0 — A is the candidate).
- **2 — subdivide the hand region** of the mesh to remove the faceted finger silhouette.
- **3 — full sculpt** only if 1 + 2 fall short.
- Ada (`pl5xxx`) and Leon's fingertips: same script, other character prefix, once Claire's is judged.
