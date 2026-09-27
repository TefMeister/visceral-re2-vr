# Split done (2026-09-27, home PC, static) — branch `split-plugin-2026-09-27`, tag `pre-split-2026-09-27`

The plan in `SPLIT-MAP-2026-09-24.md` was carried out as written, by a script (every original line placed
exactly once; line 78, a blank line after `namespace {`, is the only line not carried), with one change of
detail: the plain globals' `extern` lines sit **before** the types in `visceral.h`, because `SelfCall` writes
`g_self_call` inside its body.

| File | Lines |
| --- | ---: |
| `visceral.h` | ~470 |
| `Plugin.cpp` (entry, per-frame driver, hotkeys, exports) | 264 |
| `reflect.cpp` | 239 |
| `bridge.cpp` | 78 |
| `dumps.cpp` | 359 |
| `plug_bracelets.cpp` | 319 |
| `head_hider.cpp` | 517 |
| `head_probes.cpp` | 302 |
| `dock.cpp` | 318 |

Proof so far `[compile-verified 2026-09-27]`:
- builds with **0 warnings** (`tools/build.sh`, no `--deploy`);
- same two exports, same 84 imports (`pefile`);
- every null-terminated text in the old DLL is in the new one, apart from five that the compiler now builds
  from immediates (`l_arm_radius`, `l_arm_wrist`, `r_arm_radius`, `r_arm_wrist`) or stores with a different
  neighbouring byte (`?visceral/visceral_neckplug_neck0local.mesh`, `?[visceral]`, `?get_Joints`); no log
  text lost;
- `code-shape-scan.py`: 0 over 800, 0 over 1500.

**Still owed before merging to `main`:** step 5 of the plan, one flat run of each build from the same save and
a compare of the `[visceral]` milestone lines. ⚠️ Since 2026-09-27 nothing of Visceral goes into the real game
folder: run it in the test copy (`D:/RE2 test copy`), and do NOT use `tools/build.sh --deploy`, which still
targets the real game.
