# Visceral RE2: the archived source trees (2026-11-07, the clean rebuild)

Moved here, unchanged, when the clean rebuild started (`modding-notes/2026-11-07-clean-rebuild-design.md`). Git tag
`pre-rebuild-2026-11-07` marks the last commit before the move. Kept for reading and diffing, never built again.

| folder | what it is |
| --- | --- |
| `native-gen1/` | the native plugin of 2026-10-05 to 2026-11-05 (builds b060-b192): the body files are copied out of it into the new tree |
| `plugin-gen0/` | the first native plugin (2026-09-04, the recon probe and the first dock), read-only since 2026-10-05 |
| `lua-gen1/` | every Lua probe and fix script of September and October |

The released mod in `mod/` is left as it is until the rebuilt mod replaces it.
