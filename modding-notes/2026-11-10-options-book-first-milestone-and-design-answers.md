# The in-world options book: the first milestone reached with the games' own models, and four design answers (2026-11-10, Fable, simulator)

Follows `2026-11-07-in-world-options-book-design.md` (Tefa's requirements, the seven open questions). Tefa, 2026-11-10:
*"vr options overlays for re2 and re3 as well"*, one night, nobody at the PC. Found with the live question line
(`visceral-re8-vr dev-archive/native/tools/sim/zz_visceral_eval.lua` + `rv.py`, pointed at RE2 / RE3 with `RV_GAME_DIR`).

## What is in both games now: `lua/visceral_options_book.lua` (shared, installed, not worn)

**X + A held 0.5 s** (or `reframework/data/visceral/options_book_cmd.json` `{"cmd": "open"}` from the desk) hangs a book
in the world in front of the player, facing them, level; it stays where it was put (free look); the same again puts it
away. Until Tefa's Blender models exist it is the game's own:

- **RE2: the officer's notebook itself**, the 3D model from the scene where the officer hands it over
  (`sm60_035_pocketbook01a`), open, with its sketches of the statues on the pages (the page Tefa named as the
  reference), five times life size, one metre away.
- **RE3: the UBCS binder** (`sm36_465_es_ubcsbinder01a`, a set-dressing prop), closed, cover to the player, straight,
  twice life size.

`[verified-live 2026-11-10, simulator, n=1 each game from a fresh start: the log says "drawn (made 2 time(s))", a
screenshot shows it]`. No options on it, no freeze, no navigation yet: this is Tefa's first milestone and no more.

## The design answers (section 5 of the 11-07 note)

1. **How it is placed and drawn (5.1): our own object in the scene, not a GUI screen, not the VR overlay.**
   `via.GameObject.create(name, <the camera's folder>)` + a `via.render.Mesh` component + `setMesh` / `set_Material`
   with holders from `sdk.create_resource`. It is drawn in both eyes, in the world, lit by the room, and stays put when
   the head turns. `[verified-live]` Two traps, both met: created with no folder it draws nothing; and **the first ask
   for a model's files in a session gives an object that never draws** (MaterialReady false, 0 materials; handing the
   files to the same mesh part again does not help; a NEW object a moment later draws at once), so the script takes a
   book that is not ready after a few frames away and makes it again (once is enough, n=2).
2. **How the pages show live content (5.2): the page's picture is swapped while it hangs there.**
   `Mesh.updatableMaterial()` then `Mesh.setMaterialTexture(material, slot, holder)`; without the first call the second
   throws. Proved by swapping the notebook's page picture for another of the game's (`..._b_albm`): the sketches
   changed. `[verified-live, n=1]` So: Tefa's hand-written pages as texture files, one per page state; the on / off
   marks and slider knobs either as more page pictures or as small extra objects made the same way. **Not proved: a
   texture file of OUR OWN loading this way** (September's trouble was our own material files; a texture alone on the
   game's own material is the smaller ask and the next test: one `.tex.34` under `natives/stm/visceral/`).
4. **The leather (5.4): solved for now by not needing our own material.** The game's notebook and binder carry their
   own. Tefa's Blender model can reuse the game's material file (same texture slots) with its own pictures swapped in
   by 2. above, which sidesteps the September material problem entirely `[hypothesis]`.
6. **The input (5.6), half:** X + A held is read in Lua from REFramework's own actions (`get_action_a_button` on both
   hands), no plugin needed. `[verified-live: VR Bench held X + A, the book came out]`. Still open: keeping the game
   from acting on the left stick and A while the book is open (in the test, X also made Claire raise her left hand).

Still open: **5.3 the freeze** (the game's own pause without its menu), **5.5 REFramework's settings** (read / change
live), **5.7 the swoosh**, the rest of 5.6.

## Next, in order

1. One texture file of our own on a page (the last unknown of the route). Then Tefa's blue-pen pages can go on.
2. The freeze and the input swallow.
3. With Tefa in Blender: the notebook's and the folder's own models, one part at a time; they replace the two paths in
   the script's `MODELS` table.

## Later the same night: a picture of OUR OWN on a page does not load yet `[measured 2026-11-10, simulator, n=3 runs]`

Built a 2048 x 2048 test page (blue "VR OPTIONS" on paper) with `dev-archive/tools/re-engine/tex_build.py`
(BC7_UNORM_SRGB, mipmaps; 5,595,624 bytes) and asked for it three ways, each with the swap call returning success:

| our file placed as | result on the page |
| --- | --- |
| a new name, `natives/stm/visceral/options_book/...tex.34` | unchanged (the default sketches) |
| the notebook's own `..._c_albm` name, game restarted with it in place | unchanged |
| the notebook's own `..._b_albm` name, game restarted with it in place | unchanged; the swap to `b` **no longer shows the statue page** |

Control, same sessions: the game's ORIGINAL `..._b_albm` shows the three statue sketches at once. So the swap works,
the name works, and **the file our tool writes is what the game will not take** (with our file standing in for `b`, the
engine keeps the old picture rather than show ours). September's grey bracelets used textures from the same tool: this
is very likely the same fault, and it was blamed on the material then `[hypothesis]`. HD hands' textures, which do
load, come from `body_tex_build.py`: the next step is a byte-level comparison of the two tools' headers against a
shipped `.tex.34` of the same size (streaming flag, mip table, the header's own size field). `MODEL: OPUS`.
The test files are out of the game folder, kept in `D:/RE2 REFramework builds/_archive-2026-11-10-options-book-test-textures/`
(sha256 5790028705e65222...).

Also fixed in the script: the first ask for a page picture in a session throws; it is now asked again by itself.
