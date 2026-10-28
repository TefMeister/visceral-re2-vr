# 2026-10-28 — a shadow-only copy of the head, from the game's own face mesh (home PC, `/lm`, Fable, flat)

## The question
Tefa: *"can you see if RE2 has any way, any way at all to have the neck present as it is in VR by default and also
have the head joint removed, but a shadow visible. making a whole new neck part in Blender seems like crazy work that
is never gonna look right."*

## What was known
- REFramework's Hide Joint Mesh (what we run: `FirstPerson_HideJointMesh=true`) scales the `head` joint to zero: the
  neck stays, the head is gone from every pass, shadow included.
- Hiding the face mesh by its draw flags (the old head hider) keeps the shadow but takes the neck with it: the neck tube
  is part of the face mesh.

## What was measured today
- **The neck has no switch of its own.** Claire's face file `pl1050.mesh` has ONE part (viscon group) with five
  material submeshes (Face, Eyelash, Tearline, Eyes In, Eyes Out); the Face submesh runs from y 1.40 to 1.71, so the
  neck tube and the face are one piece `[measured 2026-10-28, RE Mesh Editor's reader outside Blender]`. The body
  file `pl1000.mesh` has five parts (body/clothes, holster, radio, sling, gun) and its skin ends at y 1.445. The live
  type list confirms `via.render.Mesh` has `setPartsEnable` and `setMaterialsEnable`, both all-pass; neither can
  split neck from head.
- **So: keep the joint route (neck present), and add the shadow back from a second copy of the face mesh, drawn in the
  shadow pass only.** `dev-archive/native/src/shadowhead.cpp` (new): a GameObject `visceral_shadowhead` with a
  `via.render.Mesh`, parented to the player, placed every frame so the copy's bind-pose head (0, 1.564, 0.018 in the
  file) sits on the live `head` joint (position and rotation; the joint's zero scale leaves both readable). Modes:
  0 off, 1 visible copy, 2 shadow only, 3 visible copy held 1 m in front / 0.3 m down (to see it from inside the head).
  `reframework/data/visceral_shadowhead.txt` holds the start mode; NUM8 cycles, NUM9 re-reads the file.
- ⭐⭐ **THE WEEK-OLD "SPAWNED MESHES NEVER GET A MATERIAL" FAULT IS SOLVED (dossier §7k).** Three runs, flat, RPD hall:
  1. The game's own `pl1050.mesh` + `pl1050.mdf2` through holders WE make (`create_resource` + a holder allocated
     without its constructor, the old plugin's route): materials 0, `MeshReady` 0, `MaterialReady` 0, `ReadyToDraw` 0,
     `HasJoint` 0: the copy draws NOTHING, not even grey, held 1 m in front `[verified-live 2026-10-28, n=2 runs]`.
     So the names were never the problem; a resource made our way never becomes ready.
  2. The same copy given the player's face mesh's OWN holders (`SurvivorCostumeChanger.get_Face()` →
     `getMesh()` / `get_Material()`, handed over once the face exists, which is after our creation moment):
     materials 6, `MeshReady` 1, `MaterialReady` 1, `ReadyToDraw` 1, `HasJoint` 1, `SkinningMesh` 1 within half a
     second, and **a full textured head (Leon's, the save's character) floats where the copy is placed**
     `[verified-live 2026-10-28, n=1 run, picture 30]`. A skinned mesh on a joint-less GameObject draws in its bind
     pose at the GameObject's transform.
  3. Handing an object through the live link (the RE Engine MCP's `invoke_method` with an address) passes null: the
     copy's holders were cleared `[measured 2026-10-28]`. Objects go through the plugin, not the link.
- Shadow casting of the copy: NOT judged by eye flat. The RPD hall spot shows no player shadow at all (soft overhead
  light), and the first-person camera sways enough that picture differences are drift, not shadow. The copy is a real
  `via.render.Mesh` with `DrawShadowCast` on and `ReadyToDraw` true, the same thing the draw-flag head hider used when
  Tefa saw the head's shadow on a clean load (§7h): the headset decides.

## What this means for the ask
- Neck present (the joint route, untouched) + head gone (the joint route) + a head shadow (the copy, shadow only) is
  BUILT, with no new geometry and no Blender: the game's own face mesh with the game's own material is the stand-in.
- The copy's head follows the head joint's position and rotation, in bind pose: no facial animation, the hair as the
  file has it (the separate hair mesh is not copied yet; `get_Hair()` would give it the same way).
- Installed (plugin a6848da7, start mode 2 = shadow only). Nothing changes on screen flat; the headset run shows
  whether the shadow has its head back.

## Not established
- The shadow itself, by eye. The headset run is the test (Tefa).
- Whether the copy's bind-pose neck tube (below the head, inside the collar) doubles the neck's shadow visibly.
- The copy is not switched off in cutscenes / third person; its shadow then overlaps the real head's (harmless, untested).
- Left over on the game side: the mode-3 debug offset and the readiness log stay in; a `visceral_tuning` switch
  would be nicer than a file.

## Files
- `dev-archive/native/src/shadowhead.cpp/.h`, `CMakeLists.txt`, `plugin.cpp` (one include, one frame call).
- Pictures: `C:/NonSteam/re2-shots-2026-10-28/` (game content, this PC only; 30 = the floating head).
- The mesh reader outside Blender: session scratch `re2neck/mesh_parts.py` (copies RE Mesh Editor's `modules/` into
  a package without the add-on's `bpy` import; worth keeping in `dev-archive/tools/`).
