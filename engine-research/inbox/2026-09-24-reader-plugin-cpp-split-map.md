# Split map for `dev-archive/plugin/src/Plugin.cpp` (reader, 2026-09-24)

Written by the read-only reader beside a live `/lm` on this game. Nothing was built, run, moved
or edited; this file is the only thing created. Every claim below is `[inferred-static]` (read
from the source at HEAD `08da697`, the v0.18 idle-phase commit) unless marked otherwise.

Template: the RE Village scope split of 2026-09-17
(`staging/re-village-scope-vr/SPLIT-MAP-2026-09-17.md` + its `plugin/src/rsv.h` and
`plugin/CMakeLists.txt`). Same shape here: one shared header, one `.cpp` per job, a named
namespace instead of the anonymous one, move only.

⚠️ The file is **2,637 lines** by `wc -l` at HEAD (the board says 2,632; v0.18 added the
`IdlePhase.h` include and three call lines). Hard limit is 1,500, so the split is the job.

---

## 1. What is in the file, by job

Line numbers are HEAD `08da697`. "Job" is the target file in §2.

| Lines | What it is | Job |
|---:|---|---|
| 1–57 | header comment: v0.1 probe story, v0.4 dock story | entry (`Plugin.cpp`, keep) |
| 59–75 | includes, `using reframework::API` | shared header |
| 77 | `namespace {` opens — **everything to 2610 is inside it** | see §3 |
| 79–84 | `g_param`, `g_api_ok`, the `LOGI/LOGW/LOGE` macros | shared header (macros) + entry (definitions) |
| 86–90 | `TAG`, `BRACELET_MDF_PATH`, `BRACELET_MESH_PATH[2]`, `BRACELET_K[4]` | shared header |
| 92–94 | `Vec3`, `Quat`, `Mat4` | shared header |
| 96–170 | pure maths: `dist`, `vsub/vadd/vscale/vlerp/vlen`, `Rows` + `rows_*`, `quat_*`, `vec_mul_rows` | reflect/maths |
| 172–204 | `find_method_deep`, `find_field_deep`, `is_managed`, `tname` | reflect/maths |
| 206–272 | `Inv` + `inv`, `inv_ptr`, `call_direct<T>`, `inv_bool`, `inv_bool_i`, `call_void_direct`, `float_arg`, `inv_u32`, `inv_f32`, `inv_vec3`, `inv_mat4`, `inv_nullable_mat4` | reflect/maths (templates → header) |
| 274–283 | `sysstr` | reflect/maths |
| 285–296 | managed-array layout globals `g_arr_count_off/elem_off/measured` + `arr_count`, `arr_ptr_at`, `arr_f32` | reflect/maths |
| 298–325 | `measure_array_layout` (finds the Lua sentinel 12345) | bridge |
| 327–346 | `field_at<T>`, `get_component`, `joint_name`, `lower` | reflect/maths |
| 348–515 | `struct State { … Dock, Plug, Bracelet(s), Head … } g;` — the one global state | shared header (struct) + entry (`State g` definition) |
| 517–586 | route-E catch list: `g_head_hide_hair`, `MESH_CATCH_MAX`, `MeshCatcher g_catch`, `catch_mesh_args` | head hider |
| 588–593 | dock hook counters `g_calls_aid/ikl`, `g_self_call`, `SelfCall` guard | dock (`SelfCall` struct → header) |
| 595–600 | `now_s()` | reflect/maths |
| 602–616 | `enum Slot` (bridge slot map), `bridge_live`, `bridge_vec3` | bridge (enum → header) |
| 618–657 | `g_grab_named/g_grab_name`, `dump_joints` (also binds the arm/neck/wrist joints into `g`) | dumps |
| 659–847 | `log_nullable_mat`, `dump_weapon`, `dump_type_surface`, `dump_ik`, `dump_layers`, `dump_motion` | dumps |
| 849–963 | `summary_line` (the 1 Hz / 10 Hz trace line; reads dock, bridge, plug, bracelets, catch list, head) | dumps |
| 965–993 | neck-plug story, `mesh_material_names` | plug + bracelets |
| 995–998 | `PLUG_MESH_PATH`, `PLUG_MDF_PATH`, `struct V4` | shared header |
| 1000–1073 | `create_resource_holder` (fwd-declared at 1022, defined 1053), `remat_if_empty` | plug + bracelets |
| 1075–1155 | `plug_create`, `plug_update` | plug + bracelets |
| 1157–1301 | bracelets story, `bracelet_create`, `bracelets_update` | plug + bracelets |
| 1303–1326 | head-hider story, `void update_camera();` forward decl, `HEAD_REVEAL_DIST_M`, `HEAD_REVEAL_TAIL_S`, `HEAD_RESCAN_MIN_S` | head hider (consts → header) |
| 1328–1388 | `head_pattern`, `head_restore_all`, `head_walk` (route A) | head hider |
| 1390–1521 | route B: `HeadProbeB`, `head_b_add_mesh`, `head_b_add_go`, `head_b_walk`, `head_probe_b` | head probes |
| 1523–1633 | route C helpers: `current_scene`, `scene_find_components`, `mesh_first_material`, `player_like`, `component_types`, `ancestor_chain`, `tf_go_name`, `HeadProbeC` | head probes |
| 1635–1723 | `head_probe_c`, `head_probe_owners` | head probes |
| 1725–1747 | `mesh_belongs_to_player` | head hider |
| 1749–1807 | `head_take_caught` (route E feeds the hider) | head hider |
| 1809–1938 | `head_scan` (runs A, then B/C/D/E/F and logs the comparison) | head hider |
| 1940–2044 | `head_update` (per frame: take catches, reveal gate, apply flags, stale rescan) | head hider |
| 2046–2055 | `game_is_foreground` | entry (hotkeys) |
| 2057–2067 | `KIND_HOLD`, `KIND_ATTACK`, `KIND_ARM`, `set_force` | dock (consts → header) |
| 2069–2105 | `poll_hotkeys` (all 15 numpad keys) | entry (hotkeys) |
| 2107–2117 | `DOCK_BLEND_S`, `DOCK_ORBIT_*`, `DOCK_REACH_FRAC` | shared header |
| 2119–2146 | `update_camera` (the read-once one the head hider now calls every frame) | dock (camera) |
| 2148–2236 | `update_camera2` (the v0.11 diagnostic reader) | dock (camera) |
| 2238–2358 | `update_dock` (blend weight, HOLD reconcile, reach clamp, target) | dock |
| 2360–2398 | `rebind_player` | entry (per-frame driver) |
| 2400–2464 | `on_frame` (the per-frame order of everything) | entry (per-frame driver) |
| 2466–2492 | `pre_mailbox` (the Lua hand-over hook) | bridge |
| 2494–2544 | `pre_passthrough`, `post_aid_target`, `post_ik_left_arm`, `install_shift_hooks` | dock |
| 2546–2594 | `MESH_HOOK` macro, ten `pre_*` hooks, `install_mesh_catch_hooks` | head hider |
| 2596–2602 | `install_bridge_hook` | bridge |
| 2604–2608 | `on_initialized` (hook install order: bridge, shift, mesh-catch) | entry |
| 2610 | `} // namespace` | — |
| 2612–2637 | the two exported `reframework_plugin_*` functions | entry |

Not in this file and untouched: `IdlePhase.cpp` / `IdlePhase.h` (own anonymous namespace, own
`[visceral_phase]` logging, does not use `g` or `LOGI`).

---

## 2. Proposed target files

Eight `.cpp` files plus one shared header, all under `dev-archive/plugin/src/`. Nothing is
archived: unlike the scope's Lua, almost every line here is live or reachable from a numpad key.

| New file | Moves from (lines) | ~Lines | Job |
|---|---|---:|---|
| `visceral.h` | includes 59–75; macros 82–84; `TAG` + bracelet consts 86–90; `Vec3/Quat/Mat4/Rows` 92–94, 111; `Inv` 206–209; templates 229–235, 240–247, 327; `State` 352–515 (whole, unchanged order); `MeshCatcher` 542–562; `SelfCall` 593; `enum Slot` 603–613; plug consts + `V4` 995–998; head consts 1324–1326; `KIND_*` 2058–2061; `DOCK_*` 2111–2117; `extern` lines for every global in §3; one prototype per function that another file calls | ~380 | shared declarations |
| `Plugin.cpp` | header comment 1–57; `g_param`/`g_api_ok`/`State g` definitions; `game_is_foreground` 2050–2055; `poll_hotkeys` 2069–2105; `rebind_player` 2360–2398; `on_frame` 2400–2464; `on_initialized` 2604–2608; exports 2612–2637 | ~300 | entry, per-frame driver, hotkeys |
| `reflect.cpp` | 96–170 (maths), 172–204, 211–227, 236–237, 250–283, 291–296, 329–346, `now_s` 595–600 | ~270 | engine reflection + pure maths + array layout |
| `bridge.cpp` | `measure_array_layout` 300–325; `bridge_live`/`bridge_vec3` 615–616; `pre_mailbox` 2470–2492; `install_bridge_hook` 2596–2602 | ~75 | the Lua bridge contract (slot map, sentinel, mailbox hook) |
| `dumps.cpp` | 618–963 | ~350 | NUM7/NUM9 dumps and the summary line |
| `plug_bracelets.cpp` | 965–993, 1000–1301 | ~340 | the neck plug and the forearm bracelets (they share every helper: holder, re-material, create/pin) |
| `head_hider.cpp` | 517–586 (catch list), 1328–1388, 1725–2044, 2546–2594 (mesh hooks) | ~520 | the head hider: route A walk, route E take, scan, per-frame apply, the assignment hooks |
| `head_probes.cpp` | 1390–1723 | ~345 | the diagnostic routes B, C, owners and the scene helpers they use (`current_scene`, `scene_find_components`, `component_types`, `ancestor_chain`, `tf_go_name`) — still called from `head_scan`, so not archived |
| `dock.cpp` | 588–593 (counters + `g_self_call` definition), `set_force` 2062–2067, `update_camera` 2119–2146, `update_camera2` 2148–2236, `update_dock` 2238–2358, hooks 2494–2544 | ~330 | the dock: camera pose, blend, HOLD latch, the two aid-target hooks |

Largest file ~520 lines; all under the 800 soft limit. If a separate camera file is wanted
(`camera.cpp` = 2119–2236, ~120 lines) it lifts straight out of `dock.cpp`; not needed for size.

**Header split option:** the scope used `rsv.h` plus small per-file headers. Here one
`visceral.h` is enough at ~380 lines; `HeadProbeB`/`HeadProbeC` (1419–1425, 1626–1633) can sit in
a tiny `head_probes.h` if the main header should not carry them. Either way is move only.

**What goes into `visceral.h` and why (the "shared declarations" list):**
- `extern const REFrameworkPluginInitializeParam* g_param;` — the three log macros read it from every file.
- `extern std::atomic<bool> g_api_ok;` — `is_managed` (reflect), `catch_mesh_args` (head), `on_frame`, the exports.
- `extern State g;` — every file.
- `extern MeshCatcher g_catch;` — head hider, `summary_line` (959), `rebind_player` (2383).
- `extern uint32_t g_arr_count_off, g_arr_elem_off; extern bool g_arr_measured;` — defined in reflect, **written** by `measure_array_layout` (bridge), read by `dump_joints` (627) and `pre_mailbox` (2478).
- `extern std::atomic<uint32_t> g_calls_aid, g_calls_ikl; extern bool g_self_call;` — dock hooks write, `summary_line` (886) reads/resets; `SelfCall` used by `dump_weapon` (712–713) and `summary_line` (866).
- `extern std::atomic<bool> g_head_hide_hair;` — only `head_take_caught` reads it (see §3, it is never written).
- The templates `call_direct`, `call_void_direct`, `field_at` — bodies must be in the header.
- Prototypes: everything in §1 that is called from a different job file. The cross-file calls are
  `update_camera` (head → dock), `set_force` (entry + dock), `head_restore_all` (entry → head),
  `dump_*`/`summary_line` (entry → dumps), `plug_update`/`bracelets_update`/`head_update`/`update_dock`/`update_camera2` (entry), `mesh_material_names` (dumps, head, entry → plug_bracelets), `install_*` (entry), `bridge_live`/`bridge_vec3` (dumps, head, dock → bridge), `catch_mesh_args` (hooks → head), `get_component`/`dump_joints` (entry), the route B/C probe functions (head_hider → head_probes), `current_scene`/`scene_find_components` (head_probes only, may stay file-local).

---

## 3. What makes a move-only split risky here

1. **One anonymous namespace (77–2610).** Every function and global has internal linkage today.
   Split as-is, each file would need its own copy of every helper it uses (header with `static`
   bodies), which under `/W4` fires C4505 "unreferenced function" in every file that includes a
   helper it does not call — `build.sh` greps for `warning`, so the build would look dirty. The
   scope solved this by moving to a named namespace (`rsv`). Do the same: `namespace visceral`
   in every file, `extern` for globals, prototypes in the header. That is the one non-mechanical
   change and it applies to every moved line. ⚠️ It also means the DLL cannot be byte-identical;
   prove equivalence by exports / imports / strings and a flat log, as the scope did.
2. **`State g` (352–515) is the whole program's memory.** It must move to the header **whole and in
   member order**: `on_frame` (2413–2414) and `rebind_player` (2385) use C++20 designated
   initialisers (`State{.bridge = …, .frame = …}`, `State::Head{.mode = …}`), which fail to
   compile if members are reordered, and it must stay an aggregate (no constructors added).
3. **Default arguments.** `inv`/`inv_ptr` (211, 221: `args = {}`), `dump_type_surface` (723–724:
   the hunt-word list), `component_types` (1597: `cap = 32`), `remat_if_empty` (1025:
   `manual = true`). A default may appear on the header prototype **or** the definition, never
   both (MSVC C2572). Put them on the prototypes and strip them from the moved definitions.
4. **Templates with bodies:** `call_direct` (229–235), `call_void_direct` (240–247),
   `field_at` (327) — into the header, or every caller fails to link.
5. **Forward declarations already in the file** — `create_resource_holder` at 1022 and
   `update_camera` at 1323 — become header prototypes; delete the in-file copies or MSVC warns
   about a redeclaration inside the namespace (harmless but noisy).
6. **The `LOGI/LOGW/LOGE` macros (82–84)** read `g_param` directly. They go into `visceral.h`
   after the `extern g_param` line; `IdlePhase.cpp` has its own logging and must **not** include
   `visceral.h` (it would drag `State` into a file that needs none of it, and it already defines
   its own anonymous-namespace helpers).
7. **Function-local `static` state.** All of it stays inside its function and moves with it;
   nothing reaches across: `now_s` (596), `summary_line::last_rows_t` (911), `head_take_caught::last_reported` (1800),
   `head_scan::quiet_retries/probe_runs` (1811, 1838), `head_update::last_taken/take_gen/last_catch/announce_gen`
   (1949–1950, 1965–1966), `poll_hotkeys::prev` (2070), `update_camera::m_view/m_cam/m_wm/looked` (2129),
   `update_camera2::m_view/m_cam/looked2` (2181), `on_frame::last_t/last_hold` (2438, 2448),
   `reframework_plugin_initialize::hooked` (2631). ⚠️ `update_camera` and `update_camera2` each
   have their own `m_view`/`m_cam` statics with the same names; fine today because they are
   function-local, and still fine after the move. Do not lift them to file scope.
8. **The `MESH_HOOK` macro (2548–2561)** defines ten functions and is `#undef`'d at 2561. Keep the
   macro, the ten expansions and `install_mesh_catch_hooks` in the same file (`head_hider.cpp`)
   so nothing needs those hook names outside it.
9. **`constexpr` at namespace scope** (`TAG`, `BRACELET_*`, `PLUG_*`, `HEAD_*`, `KIND_*`, `DOCK_*`,
   `MESH_CATCH_MAX`) has internal linkage, so putting them in the header gives each file its own
   copy with no link clash. `BRACELET_K` is read from three jobs (dumps 955, bracelets 1263,
   hotkeys 2098), so it must be in the header, not `plug_bracelets.cpp`.
10. **Static-initialisation order across files.** `g` (a `std::vector`/`std::string`-bearing
    struct) and `g_catch` (holds a `std::string`) are dynamically initialised in whichever file
    defines them. Nothing touches them before `reframework_plugin_initialize` runs, so the order
    between translation units does not matter. Just do not add a global whose initialiser reads `g`.
11. **Hook install order and per-frame order are behaviour.** `on_initialized` (2605–2607:
    bridge, shift, mesh-catch) and `on_frame` (2404–2463: hotkeys → rebind → weapon → attack pulse
    → camera2 → dock → plug → bracelets → head → IsHold edge → dumps → layers → idle-phase → summary)
    stay exactly as written, in `Plugin.cpp`.
12. **Warnings that exist today and would change shape:** `find_field_deep` (186) is defined and
    never called; in the anonymous namespace it is a C4505 today, in a named namespace it goes
    quiet. `float_arg` (250) is `[[maybe_unused]]` for the same reason. `KIND_ARM` (2061) is unused.
    None is a split blocker; just do not "fix" them during the move.
13. **Two dead side channels worth knowing, not part of the split:** `g_grab_named`/`g_grab_name`
    (622–623) are read by `dump_joints` (655) and set by nothing; `g_head_hide_hair` (540) is
    read at 1794 and written by nothing — its comment says "NUM- toggles it live" but NUM- is the
    bracelet twist step (2097). Leave both as they are in the move; they are candidates for a
    later behaviour commit.
14. **Nothing outside the plugin names `Plugin.cpp`** except `CMakeLists.txt`, two recon notes and
    a Blender script comment. `tools/build.sh` builds through CMake, so only the CMake list
    changes (§4). No deploy script, test or shader-check reads the file by path.

---

## 4. CMake after the split

`dev-archive/plugin/CMakeLists.txt`, the `add_library` block only; everything else in the file
(C++20, include dirs, `NOMINMAX`/`WIN32_LEAN_AND_MEAN`, `/W4 /utf-8 /wd4267 /wd4189`, `/Brepro`,
`user32`) stays as it is:

```cmake
# 2026-09-24: Plugin.cpp was split into translation units by job (move only; see src/visceral.h).
add_library(visceral_core SHARED
    src/Plugin.cpp
    src/reflect.cpp
    src/bridge.cpp
    src/dumps.cpp
    src/plug_bracelets.cpp
    src/head_hider.cpp
    src/head_probes.cpp
    src/dock.cpp
    src/IdlePhase.cpp
    # MinHook (Tsuda Kageyu, BSD-2, third_party/minhook/LICENSE.txt) -- the native detour for IdlePhase
    third_party/minhook/src/buffer.c
    third_party/minhook/src/hook.c
    third_party/minhook/src/trampoline.c
    third_party/minhook/src/hde/hde64.c)
```

`visceral.h` and `IdlePhase.h` are picked up by include, not listed (same as the scope's `rsv.h`).

---

## 5. What must not change, and how to prove the move changed nothing (no headset needed)

**Must not change:** the two export names; the DLL name `visceral_core.dll`; every log string
(the notes and board quote `PLAYER BOUND`, `WEAPON CHANGED`, `VR BRIDGE ATTACHED`, `ARRAY LAYOUT
MEASURED`, `meshcatch: 10/10`, `BRACELET l CREATED`, `PLUG CREATED`, `head: routeE took`, `DOCK
ENGAGED`, `REACH CLAMP`, the summary-line field names); the bridge slot numbers (603–613, a
contract with `visceral_native_bridge.lua`); the numpad map (2071–2101); the hook install order;
the per-frame order; the loose-file paths (88–89, 995–996).

**Checks, in the order the scope used them** `[the scope's own method, measured there 2026-09-17]`:
1. Tag `pre-split-2026-09-24` on `visceral-re2-vr` before touching anything; do the move on a branch.
2. Build the pre-split HEAD once with `/Brepro` and keep that DLL and its `dumpbin /exports`,
   `/imports` and the `strings` set.
3. Build the split: **0 warnings** (`build.sh` greps for them), same 2 exports, same import list,
   same string set. Byte-identical is not expected (named namespace, new TUs).
4. Run the lanes plugin's `tools/code-shape-scan.py` on `src/` — every file under 800.
5. One flat run of each build from the same save, compare the `[visceral]` milestone lines with
   addresses stripped: the hook ids, `meshcatch 10/10`, `PLAYER BOUND`, `BRACELET l/r CREATED`,
   `PLUG CREATED`, `head: routeE took N`, and a NUM7 dump. Identical apart from interleaving means
   the move is proven. That run is the modding session's, not the reader's.
6. Only after that, merge to `main`, then archive the installed pre-split DLL per the
   keep-every-build rule.

Nothing else in this plan needs the game running.
