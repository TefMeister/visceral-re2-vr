# The RE2 game folder before the clean rebuild (2026-11-07, Task 1)

`2026-11-07-inventory.tsv`: every file in the RE2 folder that Steam does not own (the root's game files, the paks and
D3D12/ left out), with its size, sha256 and verdict `[measured 2026-11-07]`. 257 files.

| verdict | what it means | count |
| --- | --- | --- |
| KEEP | goes back in, in the layer named | 186 |
| TOOL | dev tools (the REFramework MCP plugin, Hand Poser's live files, Enhanced Model Viewer, Flickers BeG0nE): installed by their own tools, never by build.sh | 37 |
| DROP | gen-1 files, old copies, DLSS: moved to the backup, not reinstalled | 22 |
| NATIVE | a Lua script whose job moves into the plugin (Task 7) | 4 |
| LOG | logs and a crash dump, kept with the backup | 4 |
| REWRITE | the plugin and the bridge Lua, made new | 2 |
| GAME DATA / LEAVE | the old clips folder (regenerated in Task 11); one empty file from 2026-10-04 | 2 |

Two things the design had missed, now in it:
- **Item 0b, the hold poses:** 71 `.motlist.524` and 9 look-at `.user.2` files under `natives/` are our generated relaxed
  hold animations (Leon's one-handed hold, the custom-part lists of 2026-10-14). Modified game files, kept on this PC only;
  they go back with the body in layer 2, from the backup.
- **The controller bindings** (`_interaction_profiles_oculus_touch_controller.json`, the D-pad on the right stick click, LT
  free for the flashlight): layer 1.

Tefa's tuned numbers: `tuned-numbers-2026-11-07.json` (only three holds were ever moved from zero: the Matilda's right hand
and its slide hand, the W-870's right hand; plus the global slide grip, slide fingers and shell hold).
