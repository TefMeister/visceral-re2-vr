# 2026-09-06 — Forearm bracelets: plugin v0.10, our own meshes, materials and textures, deployed 18:57

**Status:** built, deployed, **unseen in game**. Previous DLL kept as `reframework/plugins/visceral_core.dll.prev`.
**Origin:** Tefa, 2026-09-06 17:50, in VR: cover the straight seam lines on the forearms with bracelets, "something
that looks like metal and leather combined, different on both hands". Shapes and positions agreed over three renders
(`2026-09-06-forearm-seam-the-submesh-theory-is-wrong.md` has the arm measurements).

## What ships

| piece | file | made by |
| --- | --- | --- |
| left arm mesh | `natives/stm/visceral/visceral_bracelet_l_radiuslocal.mesh.2109108288` (43,648 B) | `tools/blender/build_bracelets.py --export` |
| right arm mesh | `…/visceral_bracelet_r_radiuslocal.mesh.2109108288` (48,000 B) | same |
| materials | `…/visceral_bracelets.mdf2.21` — 4 of ours, on `Record_Player.mmtr` | `tools/re-engine/mdf_build_bracelets.py` |
| textures | `…/visceral_bracelet_{leather,metal}_{ALBM,NRMR,ATOS}.tex.34`, 512², BC7/BC7/BC1 with mips | `make_bracelet_textures.py` → `tex_build.py` |
| runtime | `visceral_core.dll` v0.10, sha256 `f62248c411082beb` | `plugin/tools/build.sh --deploy` |

**Left arm** (two thin bands, tight above her watch): lower band **dark red** at 3.6 cm from the wrist joint with
three steel rings threaded on it; upper band **red-purple** at 5.0 cm, nearest the jacket, with a steel clasp on the
back of the arm. **Right arm:** one wide **brown** leather cuff from 1.5 to 5.5 cm with a steel plate across the
back, four rivets and raised welts. All wrapped onto the arm's real cross-section sampled from the skin mesh.

## How it is put together

**Mesh.** Built in world space on the imported skeleton, then every vertex moved into the `<side>_arm_radius`
joint's local bind frame with the conversion that reproduces `build_neckplug.py`'s hard-coded `neck_0` numbers
exactly `[verified 2026-09-06]`: RE bind rows = the Blender bone matrix's columns each mapped (x, y, z) → (x, z, −y),
local = rows · (world − T), handed back to Blender as (x, −z, y) so RE Mesh Editor's exporter lands on RE-local. One
object per material (`LOD_0_Group_0_Sub_N__<material>`), collection tagged `RE_MESH_COLLECTION`. Leather and metal
are tagged per face at build time: the first preview lost that to `materials.clear()`, which resets every polygon's
material index — replace slots in place instead.

**Materials.** Our own MDF, from Claire's `pl1000.mdf2` as a template: the holster material (the plainest opaque
`Record_Player.mmtr` one) copied four times, renamed, its three texture slots pointed at our loose files
(`visceral/…_ALBM.tex` etc. — the same natives/stm-relative path form the skin-detail tile proved), `BaseColor`
tinted per material and `Rec_Cloth` off. The leather tile is **neutral grey** on purpose: brown, dark red and
red-purple are all one tint each on the same texture. Two facts about RE Mesh Editor's MDF writer, found the hard
way: it recomputes every name hash from the new names (so renaming is free), but it **mis-places the string table
whenever the material COUNT changes** (with one material kept, the strings landed 14 KB past where the entries said
and read back empty). With the template's twelve kept and four overwritten, everything round-trips. So the file
carries eight unused vanilla materials — harmless, the mesh binds by name — and for that reason the `.mdf2` is not
committed (it is game-derived); everything else in `plugin/assets` is ours.

**Textures.** Procedural, seamless (FFT-wrapped noise): pebbled leather with pores and a soft mottle, roughness
~0.62 in the NRMR alpha; brushed steel with directional streaks and sparse scratches, metallic 1 in the ALBM alpha,
roughness ~0.32. ATOS in the jacket's own convention `[measured 2026-09-06: jacket ATOS mean 1/1/0.78/1]`.
Formats as Claire's: ALBM BC7 sRGB (99), NRMR BC7 (98), ATOS **BC1 (71)**; the converter took BC1 directly.

**Runtime (plugin v0.10).** Same recipe as the neck plug: a GameObject per arm, `via.render.Mesh`,
`MeshResourceHolder` + `MeshMaterialResourceHolder`, Transform pinned every frame. **Pinned to the radius joint**,
because the live skeleton has **no twist helper bones** (`l_arm_radius → l_arm_wrist → l_weapon` in the 2026-09-06
joint dump; the `_twist_N_H` bones exist only in the mesh file) and the wrist joint carries the hand's flexion, which
a bracelet must not follow. The open question is pronation: does the radius joint turn with the hand, or only the
wrist? So the band can additionally be rotated about the forearm axis by a fraction `k` of the wrist's twist relative
to the radius — `theta`, computed from the two joints' world matrices by pure vector algebra (no quaternion
convention involved) and logged in the 1 Hz line as `twist l= r=` in degrees. Applying it does need the engine's
quaternion convention, which is not known: **NUM/** cycles the four order/sign combinations, **NUM-** cycles
k = 0 / 0.5 / 0.8 / 1.0, **NUM\*** toggles both bracelets. **k = 0 is the default and the safe baseline** — rigid to
the radius joint, nothing to get wrong.

## The first run decides (FLAT is enough)

| read | means |
| --- | --- |
| log `BRACELET l CREATED … materials: n=3: visceral_bracelet_leather_red, visceral_bracelet_metal, visceral_bracelet_leather_purple` and `BRACELET r … n=2` | mesh and MDF both loaded and bound by name |
| bands on the forearms, right colours | the whole pipeline works |
| bands present but grey/flat/black | MDF not taken (check `reframework_loose_files.txt` for `visceral_bracelets.mdf2`) or a texture path wrong |
| bands floating off the arm or inside it | the bind-frame conversion is wrong for the radius joint (it was right for neck_0); compare the logged position with the radius joint's |
| bands stay put when she turns her palm over, skin turns under them | radius carries no pronation: press NUM- to k = 0.8, then NUM/ until the band turns WITH the skin |
| bands turn with the hand already at k = 0 | the radius joint carries the pronation itself; leave k = 0 |
| `twist l=` reads ±90° when the palm is turned | that is the number to blend; near 0 whatever she does = the radius already rotated |
| nothing, log says `mesh resource missing` | loose file not found: names, or `LooseFileLoader_Enabled` |

Restore: delete `natives/stm/visceral/visceral_bracelet*` and swap the `.prev` DLL back — or just NUM\*.
