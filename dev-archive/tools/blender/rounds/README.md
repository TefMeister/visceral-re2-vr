# The rounds in RE2's handgun magazines (2026-11-12)

Background Blender (RE Mesh Editor) scripts that made the models in `dev-archive/native/assets/natives/`. Note:
`modding-notes/2026-11-12-our-rounds-in-six-re2-handguns.md`. `<extracted>` = the local folder of game files pulled with
`dev-archive/tools/re-engine/pak_pull.py` (never committed).

| Script | What it does |
| --- | --- |
| `mag_islands.py <mesh>` | (plain Python) the magazine joint's (`_04`) pieces per part: size and place |
| `chamber_rounds.py <mesh>...` | (plain Python) pieces on any other joint shaped like a round (a round in the chamber) |
| `mesh_lods.py <mesh>...` | (plain Python) the parts on each detail level, to check an export |
| `look_mag.py -- <mesh> <out> <up_min> [max_width] [min_verts]` | renders the magazine alone, pieces matching the rule in red |
| `look_mag_islands.py` | the same, each matching piece in its own colour, listed |
| `look_mag_textured.py -- <mesh> <albm.tex> <out>` | the magazine with the gun's own colour texture |
| `split_rounds.py -- <in> <out> <up_min> <max_width> <min_verts> <g:new> [joint fwd_min fwd_max]` | moves matching pieces into a new part, every detail level, and writes the mesh |
| `add_rounds.py -- <spec.json>` | sets the WP0400's two rounds into another gun's magazine as a new part, lowers the follower, adds the WP0400's material to the gun's material file (`specs/`) |

Run with `"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --python <script> -- <args>`; the plain-Python ones
read with RE Mesh Editor's parser (copied to a temp folder, as `dev-archive/tools/mesh_parts.py` does).
