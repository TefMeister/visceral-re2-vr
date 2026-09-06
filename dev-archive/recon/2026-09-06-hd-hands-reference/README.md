# HD hands (roadmap H2) — Tefa's brief, 2026-09-06 02:20

Four of Tefa's own screenshots (no game files), kept as the reference for what H2 has to reach:

| file | what it shows |
| --- | --- |
| `Screenshot 2026-09-06 015204.png` | **Leon**, RE2 first person, hands on the tarmac: gloved, fingertips bare — reads fine because leather hides low detail |
| `Screenshot 2026-09-06 015149.png` | **Claire**, same view: smooth, waxy skin, no pores/veins/knuckle creases, faceted silhouette on the fingers — built for third-person distance |
| `Screenshot 2026-09-06 015142.png` | **Ada**, same: the same skin quality problem |
| `Screenshot 2026-09-06 015534.png` | **Resident Evil 7** (Ethan's hands, the target): pores, veins, knuckle wrinkles, moist specular highlights, subsurface-lit skin, a scar — textures and shading made to be looked at from 20 cm |

**The brief (Tefa):** "I would like them to look, if at all possible, more realistic, like they do in Resident Evil 7."

## What the reference actually consists of `[inferred-static 2026-09-06]`

RE7's hands are the same engine's skin shader fed by **first-person-grade textures**: a high-resolution
albedo with pores and veins, a normal map carrying knuckle wrinkles and tendons, and a roughness/specular
map that makes the skin look moist. The mesh is denser too, but most of what the eye reads as "real" in
that screenshot is texture and shading, not polygons.

Claire's body mesh (`pl3000.mesh`, 27 k verts including the arms) uses `pl3000_body_albm` + `pl3000_body_nrmr`
(both pulled, `.tex.34`) — one texture set for the whole body, so the hands get a small share of it.

## Plan, cheapest first

1. **Textures only** — repaint the hand/forearm region of the albedo and normal map at higher effective
   resolution (pores, veins, creases, specular), ship as loose `.tex.34` files. Same shader, same mesh,
   biggest visual gain per hour. Needs a source of skin detail we may use: a CC0/CC-BY hand scan (Sketchfab
   via the Blender MCP, checked per-model licence) baked onto Claire's UVs in Blender, or hand-painted.
   **Never RE7's own textures** — even on a machine that owns both games, the mod cannot ship them and the
   UVs differ anyway.
2. **Mesh smoothing** — subdivide/smooth the hand region of `pl3000.mesh` in Blender (RE Mesh Editor export
   is proven end to end) to kill the faceted finger silhouette. Weights and UVs carry over on a subdivision.
3. **Full sculpt** — new hands, retopo, rebake. The roadmap's "largest single asset job"; only if 1 + 2 fall short.

Same three tiers apply to Ada (`pl5xxx`) and, for the bare fingertips, Leon.

## Result, 2026-09-06 ~03:00 (Tefa, in VR, after pass 2 + forearms) `[verified-live, n=1]`

`vr-result-*.jpg` — three of Tefa's ten headset captures. Verdict: **"this looks really good!!! unbelievable."**
Remaining notes from the same look, in Tefa's words:
- "still a bit grainy" — one number each way (`--pores`, `skin_detail_build.py --normal`);
- "nails need more definable lines" — nail bed / cuticle edges: a paint job on the nail UV islands (Blender);
- "the grime on hands is very low resolution up close" — that is the game's Record system dirt overlay
  (`Rec_Mud_Map`, `Record_Injury_Map`, `VFX/RecordSystem/RecordTexture/BaseTextures/*`, listed in the MDF),
  not our textures. Higher-res replacements for those masks are the same loose-file trick, one more `[PD]`.
