# The zombie "montage" system — how RE2 assembles a zombie's look, and where the face pool lives

**Author:** modding session, home PC (RTX), 2026-09-12. Static analysis only — no game launched.
**Answers:** mod-ideas → `games/re2-visceral.md` → "More zombie faces" enabling question.
**Sources:** the game's own `il2cpp_dump.json` (REFramework class dump beside `re2.exe`), the nine
montage `.user.2` files pulled from `re_chunk_000.pak` with `dev-archive/tools/re-engine/pak_pull.py`,
and `reframework_accessed_files.txt` (loader log) from the 2026-09-07 and 2026-09-10 runs.
⚠️ The pulled `.user.2` files are game content and were NOT committed; only what they say is.

## 1. The classes `[measured 2026-09-12]`

Namespace `app.ropeway.enemy.em0000` covers three zombie kinds: **em0000 male, em0100 female,
em0200 police** (em0200's outfits are SHIRT04/05 + HAT00 = uniform).

- `Em0000MontageCatalogRegister` holds, per kind, three `via.UserData` assets:
  `*PartsContainer` (part key → prefab list), `*MontageTableData` (named outfits), `*CombinationRule`.
- `MontageManagerBase<T>` is the picker: `makeMontageGroupID(string id, uint? fashionSeed)` resolves
  an outfit ID to key names, then `getFaceRandomVariation / getFaceMaterialRandomVariation(KeyName,
  FashionSeed, otherPartsKeys)` (and the same for body/shirt/pants/accessory) choose a variant of
  each part from the container list. **Runtime works on strings** (`Em0000MontageData.FaceKeyName`
  etc.); the enums (`EM0000_MONTAGE_PARTS_FACE`, …) exist for the editor-side `EM0000_MontageData`.
- `Em0000SimpleMontageBase` is the per-zombie component: fields `FaceMesh / BodyMesh / ShirtMesh /
  PantsMesh / HatMesh` (all `via.render.Mesh`), `CompletedMontage`, `RandomSeed`;
  `attachedMontageMesh(Face, Body, Shirt, Pants)` is the moment the meshes land.
- `via.render.Mesh` exposes `setMesh(MeshResourceHolder)`, `set_Material(MeshMaterialResourceHolder)`,
  `setMaterialTexture(mtr_idx, var_idx, TextureResourceHolder)` — the same calls `Plugin.cpp`
  already makes for the neck plug and bracelets (`create_resource_holder` → `setMesh` / `set_Material`).

## 2. The data files, and that the loader looks for loose copies `[measured 2026-09-12]`

`natives/STM/SectionRoot/UserData/Character/Enemy/em0000/Montage/` —
`em0000PartsContainer.user.2`, `em0000MontageTableData.user.2`, `em0000CombinationRule.user.2`,
the same three for em0100 and em0200, plus `em0000PartsContainer_Zombie_AfterChapter2.user.2` and
`em0000MontageTableData_Zombie_AfterChapter2.user.2`. **All of them appear in the LooseFileLoader
log** on 2026-09-07 08:42 and 2026-09-10 22:06 — the game asked for loose copies at startup. That a
loose copy is *accepted* and *used* is `[hypothesis]` (n=0 runs with one present).

Face assets: `natives/STM/SectionRoot/Character/Enemy/em0000/Face/FaceNN/em0050_FaceNN.{pfb.17,
mesh.2109108288, mdf2.21}` + `_ALBM/_ATOS/_NRMR.tex.34` (note the **em0050** prefix on face files
under the em0000 folder; hair textures / chain files alongside on some faces).

## 3. The numbers `[measured 2026-09-12]`

| kind | face prefabs on disk | in the container list | outfits | faces used by outfits (× count) |
| --- | --- | --- | --- | --- |
| em0000 male | 15: 00–07, 10, 11, 14, 70–73 | 14 (+FACE04, FACE10 in AfterChapter2) | 65 (+3) | 00×5 01×8 02×7 03×9 04×8 05×5 06×10 07×4 11×1 14×4, 70/71/72/73 ×1 each (ID900–903, unique zombies with their own SHIRT70–73) |
| em0100 female | 5: 00–04 | 5 | 27 | 00×6 01×5 02×6 03×5 04×5 |
| em0200 police | 5: 00, 01, 03, 04, 70 | 5 | 14 | 00×4 01×4 03×4 04×1 70×1 |

- Enum `EM0000_MONTAGE_PARTS_FACE` = FACE00–14, FACE70–76, NONE. **Empty slots with no prefab:
  FACE08, FACE09, FACE12, FACE13, FACE74, FACE75, FACE76** — seven names the code already accepts.
- **Variant naming already exists for other parts:** `BODY00_00_00 … BODY00_00_14` (15 prefabs of one
  body), `SHIRT03_00_00 … SHIRT03_05_01`, `PANTS08_00_00 … _03`. Two indices: `_mesh_material` by the
  look of `getBodyRandomVariation` vs `getBodyMaterialRandomVariation`. **No face has any variant.**
  That the picker selects variants by key-name prefix + seed for faces exactly as for bodies is
  `[hypothesis]` — the names are measured, the code path is not decompiled.
- `CombinationRule` for em0000 only mentions Face00/01/02/04/05/06/07/13/14 with Body00_00_00 and
  Face03 with Body00_00_01 — i.e. face↔body-skin pairing (skin tone match). New faces need a rule
  row, or the matching body. `[inferred-static]`

## 4. Routes, cheapest first

1. **Rebalance only (no art):** edit `em0000MontageTableData.user.2` so all 11 usable faces
   appear evenly (RszTool / RE_RSZ template, public). One flat run proves loose overrides of these
   tables. `GATE: FLAT` for the proof.
2. **Face skin variants (the idea as filed):** `FACE00_00_00`, `FACE00_00_01`, … prefabs = copies of
   the face pfb pointing at a copied mdf2 with our textures (`tex_build.py` / `skin_detail_build.py`
   pipeline); add rows to the container. If the prefix-picker hypothesis holds, no code.
3. **Whole new faces:** copy a head mesh + prefab into FACE08/09/12/13, add container + table rows.
4. **Runtime fallback (no `.user.2` editing):** hook `Em0000SimpleMontageBase.attachedMontageMesh`
   (or poll `CompletedMontage`) in the plugin and `set_Material` a per-seed variant onto `FaceMesh`.
   Same calls as the plug/bracelets; untested on a zombie. `[inferred-static]`

## 5. Risks carried in

- 2026-09-10: our hand textures reverted to stock on a later level and the loader never asked for
  them — loose-file loading is not proven reliable across level loads. Any face test must
  account for it.
- Nothing above has run. Every `[hypothesis]` here closes with one flat-screen launch.
