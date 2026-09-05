# The hollow neck is an open tube, the plug is our own mesh, and the pak is open (2026-09-05 night, home PC)

Roadmap v2 **H1** ("give Claire a neck so the body is not hollow when looking down") went from
"Blender track, some day" to **built, compiled and deployed for a flat run** in one sitting, and
the same sitting unblocked every pak-extraction item on the board. Nothing was launched.

## 1. Why Claire is hollow — the actual mechanism `[inferred-static 2026-09-05]`

Claire's default model is **four mesh files**, not one (paths from the RE2 RT file list):

| file | what it is | verts |
| --- | --- | --- |
| `character/player/pl3000/pl3000/pl3000.mesh` | body from the collar down: skin (arms), cloth, ribbon, necklace chain + pendant | 27 k |
| `pl3000/pl3050/pl3050.mesh` (+ `_blend`) | **the face, with the neck attached**: Face_Mat, Hair_Mat (hairline), Eyelashes, Tearline | 31 k |
| `pl3000/pl3070/pl3070.mesh` | hair | — |
| `pl3001` / `pl3002` / `pl3003` | costume variants (Sherry's jacket, the watch outfit, the jacket outfit) — **not** the head | — |

The body's skin submesh stops at z = 1.00 m (the shoulders) and its cloth collar is a ring at
z ≈ 1.125 m, radius 6.1 cm. **The neck belongs to the face file**, as a skin tube from z = 1.08 up
to the chin at ~1.16, weighted to `spine_2` → `neck_0` → `neck_1`. The face proper is weighted to
`head` (z = 1.22) and its children.

REFramework's FirstPerson hides the head by **scaling the `head` joint to zero** (`HideJointMesh`).
Every vertex weighted to `head` collapses to that one point — the whole face, jaw, mouth — while
the neck tube, weighted to the neck joints, stays exactly where it was: **a 4.5 cm wide open tube,
inner faces back-face culled, directly under a camera that sits at eye height.** Looking down, you
look through it into the torso and out through the cloth. That is the hollow.

Measured on the real files (RE-Mesh-Editor import, Blender space, metres, feet at 0):

| height | neck ring centre (x, y) | mean radius | note |
| --- | --- | --- | --- |
| 1.10 | (0.00, +0.023) | 0.063 | collar level, widening into the shoulders |
| 1.12 | (0.00, +0.027) | 0.057 | `neck_0` takes over from `spine_2` |
| 1.14 | (0.00, +0.024) | 0.045 | `neck_0` / `neck_1` |
| 1.16 | (0.00, −0.020) | 0.043 | chin begins (`chin_M` weights appear); back of the neck at y ≤ 0.061 |
| 1.18–1.20 | — | — | teeth, tongue, `head` |

Joint bind poses in the engine's own space (row-major, Y up, rows = joint axes, last row =
translation), read from the mesh's stored joint matrices:
`neck_0` = rot about X (cos 0.952, sin 0.306), t = (0, 1.1004, −0.0408);
`neck_1` t = (0, 1.1585, −0.0221); `head` t = (0, 1.2195, −0.0132).

## 2. The fix — a plug that is ours, on its own object `[compile-verified 2026-09-05]`

Not a mesh edit. Three routes were weighed:

1. **Edit `pl3000.mesh`** (add a stump to the body file) — works, but the shipped file would
   contain the whole original body, so it could only ever ship as a rebuild script.
2. **Hide the face by draw flags instead of the bone** (dossier §7) — removes the tube too, and
   leaves the collar hole open instead. Not a fix on its own.
3. **Our own tiny mesh, spawned at runtime and pinned to `neck_0`** — nothing of the game's in
   the download, no game file replaced, and it follows the deep-end rule (C++, engine systems).

Route 3 is built:

- **`tools/blender/build_neckplug.py`** (Blender 5.2 headless + RE Mesh Editor) generates a capped
  cylinder, r = 4.0 cm, from z = 1.095 (inside the collar) to a dome top at 1.165 (under the chin),
  centred on the measured neck axis (y = +0.025), 194 verts / 384 tris, one material named
  **`pl3000_Skin_Mat`** with every UV inside the shoulder-skin patch of Claire's body texture
  (u 0.85, v 0.35). It clears the back of the neck at every height by ≥ 4 mm, so in third person it
  is inside the skin and invisible. Exported as `.mesh.2109108288` (RE2 RT); **re-imports cleanly**.
  Two builds: one in character space, and **`visceral_neckplug_neck0local`** with the vertices
  pre-transformed into `neck_0`'s bind frame so the runtime only has to copy the joint's pose.
- **Plugin v0.7** (`plugin/src/Plugin.cpp`, builds clean at /W4): on binding the player it captures
  `neck_0` from the skeleton walk, creates a GameObject (`via.GameObject.create`), adds a
  `via.render.Mesh`, gives it the plug mesh through REFramework's resource manager
  (`create_resource` + `create_holder`) and the game's own `pl3000.mdf2` as material — so the
  engine loads Claire's skin shader and textures from its pak and matches them to the plug by
  material name — then every frame writes `neck_0`'s world position and rotation into the plug's
  transform. NUM0 toggles `set_DrawDefault`/`set_DrawShadowCast`. If the GameObject stops being a
  managed object (scene wipe) it is recreated once the player rebinds. The summary line gained
  `plug=on @(x y z)` (expect ≈ 1.10 m above the feet) or the reason it is not there.
  The pattern is alphaZomega's EMV Engine `create_gameobj`, written fresh.
- **Deployed:** DLL hash-verified into `reframework/plugins/`, the mesh into
  `<game>/natives/stm/visceral/`, which REFramework's loose-file loader serves because it is now
  **on** (`LooseFileLoader_Enabled=true`, switched on earlier tonight). `tools/build.sh --deploy`
  now copies `plugin/assets/natives/` too.

**What one flat launch answers** (the `[FLAT]` row): does `PLUG CREATED` appear, or which of the
four failure lines (`GameObject.create`, `createComponent`, mesh resource missing, `setMesh`)? Does
the summary put the plug at neck height and does it move with the body? In first person looking
down: skin-coloured floor instead of the tube? Wrong material shows as a grey/black plug; wrong
frame maths shows as a plug floating off the neck by a fixed offset or rotation — both are one
constant away, not a redesign. NUM0 is the A/B.

**Leon** has the same joint names; the plug is Claire-sized and named for Claire's material. Once
the mechanism is proven, a `pl1000_*` variant is one script parameter.

## 3. The pak is open, and it took seconds, not a 24 GB extraction `[verified-numerically 2026-09-05, n=10]`

`tools/re-engine/pak_pull.py` reads the PAK v4 entry table (KPKA, 48-byte entries, MurmurHash3 of
the lower- and upper-cased UTF-16 path, seed 0xFFFFFFFF, zstd or deflate per entry; format from
Ekey's REE.PAK.Tool, reimplemented) and pulls named files straight out of `re_chunk_000.pak` and its
patches (patch_002 → patch_001 → chunk_000; patches 003–007 here are empty). With Ekey's
`RE2_RT_STM_Release.list` (42,236 paths) it pulled all ten requested files, sizes matching the
table, in under two seconds: Claire's and Leon's body meshes and MDFs, the face mesh, and **the four
animation banks the aim-walk splice needs** (`pl10`/`pl00` × `base_hdg_hold` / `base_cmn_move`).
Extracted game data lives in `D:\RE2 REFramework builds\extracted (game data - never commit)\` and
stays there.

The tooling now on this machine, all public, all read-only for us:

| tool | where | note |
| --- | --- | --- |
| RE Mesh Editor 0.66 + RE Asset Library (NSACloud) | Blender 5.2 add-ons, installed and enabled | `bpy.ops.wm.console_toggle()` crashes Blender in background mode — guarded locally with `bpy.app.background`; call the importer with `directory` + `files`, a bare `filepath` imports nothing |
| REE.PAK.Tool (Ekey) source + prebuilt + Projects lists | `D:\RE2 REFramework builds\tools\` | the file lists are the treasure |
| CAF (godlock2000-eng) | `…\study - CAF NonRTX (MIT, read-only)\caf` | motlist/mot format docs + a v85 writer |
| EMV Engine (alphaZomega) | `…\tools\EMV-Engine` | runtime spawning reference |

The two motion-name sweeps of the originals confirm the splice inputs: Claire's `base_cmn_move`
holds the `KFF_` locomotion set (GazingWalk F/L/R/Back loops, Jog cycles, Stairs, PivotTurns — 58
motions), and her original `base_hdg_hold` has exactly the 28 motions the Jill specimen showed.

## Next

- `[FLAT]` launch once; read the plugin log and look down. That single run also fills
  `reframework_accessed_files.txt`, which was armed earlier tonight.
- `[PD]` the aim-walk splice script — its four inputs are now on disk.
- `[PD]` Leon variant of the plug once Claire's is seen working.
