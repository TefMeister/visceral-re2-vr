# v0.8: the head hider that keeps the shadow, plus the plug's read-back (2026-09-06 01:25–01:45, home PC, `/pd`, static)

**The game was not launched. Nothing here has been run.** Both items are the `[PD]` rows that fell out of
the first neck-plug run an hour earlier (`2026-09-06-neck-plug-first-run-collar-already-closed.md`).
Plugin `visceral_core.dll` v0.8 is built and deployed `[compile-verified 2026-09-06]`, hash
`dfad5ce46ec35d1e`; the previous DLL is kept beside it as `.prev`.

## 1. What the head hider does

Roadmap v1 #4 / v2 phase H. REFramework's own **Hide Joint Mesh** removes the head by scaling the `head`
joint to zero, which takes the head out of *every* render pass, shadow map included: a headless shadow.
`via.render.Mesh` has independent per-pass draw flags (dossier §7, praydog's `RE8VR.cpp`
`fix_player_shadow`), so v0.8 does it the other way:

- **walks the player's transform hierarchy** (`get_Child` / `get_Next` on `via.Transform`) and takes
  every `via.render.Mesh` component off every GameObject through `get_Components` (not `getComponent`,
  which returns only the first — eyelashes are often a second mesh on the same object);
- **decides by name AND material** — GameObject name or any material name containing `face, hair, head,
  eye, lash, brow, matsuge, beard, mustache, hige, tooth, teeth, tongue, tear, pl3050, pl3070, pl1050,
  pl1070`. Material names come from `get_MaterialNum` / `getMaterialName(i)` and are the reliable identity
  (Claire's face file carries `Face_Mat`, `Hair_Mat`, `Eyelashes`, `Tearline`; the body carries
  `pl3000_Skin_Mat`, and "skin" is deliberately not a pattern). The player root's first mesh is never
  hidden whatever matched;
- **writes** `set_DrawDefault(false)` + `set_DrawShadowCast(true)` (+ `set_DrawRaytracing(false)` where
  the method exists) and remembers the originals;
- **reveals the head again** (mode 1) while any of these holds, each with a 0.3 s tail: the cinematic gate
  says the player is not in first-person control (bridge slot 30), the player is grabbed
  (`app.ropeway.JackDominator.get_Jacked` — grabs only; an ordinary hit never sets it), REFramework's
  FirstPerson mod is not driving the camera (bridge slot 31, `firstpersonmod:will_be_used()`), or the
  camera is more than 0.35 m from the `head` joint;
- **detects a rebuilt player** the way Arcade Controls learned to: read `get_DrawDefault` back right after
  clearing it; a flag that will not stay cleared means those components are not the ones on screen, so the
  list is dropped and the walk repeated (rate-limited to once per 2 s). A player rebind drops the list too.
- **retries quietly** every 2 s while the walk finds nothing head-ish (the face/hair objects may attach
  after the player binds); the full mesh table is logged on the first scan and on NUM+.

Two new bridge slots carry the two facts only Lua can see: **slot 30** = cinematic gate verdict
(`__visceral_cinematic_blocking()`), **slot 31** = FirstPerson active. Both are written before the VR
early-out so they are live on a flat launch too. The bridge logs once whether either source exists.

**Keys** (numpad, as always): **NUM.** cycles the hider `0 off → 1 on with reveals → 2 on, forced`
(mode 2 ignores every reveal trigger, to isolate the reveal logic from the hiding itself);
**NUM+** rescans and prints the mesh table. Default mode is 1.

**The 1 Hz summary** gains ` | head=<mode> hid=<n>/<meshes> d=<camera-to-head m>[ REVEAL:<why>]`.

### Why this and the plug are one feature `[hypothesis]`

Hiding the face file by its draw flags hides the **neck** with it (the neck belongs to the face mesh, 05e
note §1), so the collar opens — and that open collar is exactly what the v0.7 plug was built to fill.
With Hide Joint Mesh on, the neck tube stayed and closed the collar, which is why last night's run saw no
difference from NUM0. **The head hider needs Hide Joint Mesh OFF**; the hider does not touch that
setting, the player does (Insert → FirstPerson → untick Hide Joint Mesh). For a release, the shipped
`re2_fw_config.txt` will need `FirstPerson_HideJointMesh=false` — not changed yet, nothing is proven.

## 2. The plug's read-back (item 1 of the two)

- `PLUG STATE: DrawDefault=… DrawShadowCast=… materials: n=K: …` right after creation. `n=1: pl3000_Skin_Mat`
  (or similar) means the material bound by name; `n=0` or `n/a` means it did not.
- `plug read-back: DrawDefault=X (wanted Y)` one frame after every NUM0, with `— THE WRITE DID NOT STICK` when
  they differ.
- `PLAYER BOUND` is followed by `body mesh materials: …`, because the player GameObject is named `pl1000`
  for Claire too (confirmed 2026-09-06: Tefa played Claire, the log said `pl1000`), and the body's material
  names are what actually say who is being played.

## 3. What is NOT established, and the one flat test

Nothing in §1 has run. The specific things the first launch decides, in the order they would fail:

| log line to read | good | bad → what it means |
| --- | --- | --- |
| `[visceral-bridge] slot sources: firstpersonmod=true cinematic_gate=true` | both true | `firstpersonmod=false` → slot 31 is dead, mode 1 will reveal forever with `REVEAL:not first person`; use **mode 2** for the run and the FP fact needs another source |
| `head: N transform(s) walked, M mesh(es) found, K to hide` | K ≥ 2 (face + hair at least) | K = 0 with meshes found → the patterns miss this build's names; the table above it says what they are, one line to fix. Meshes = 0 or 1 → the walk did not descend (get_Child/get_Next not the right pair here) |
| `head: K mesh(es) hidden, shadow kept` and it stays | steady | `stale mesh refs … rescanning` repeating → the read-back heuristic is wrong on this build (a flag that reads true after being cleared even on live meshes); switch to mode 2 and judge by eye |
| summary `d=` while standing in first person | ≈ 0.05–0.15 m | > 0.35 m → the camera is not where the head joint is (VR offset), the distance reveal fires constantly; the constant is `HEAD_REVEAL_DIST_M` |
| eyes: head gone, shadow of a head on the wall, collar shows a skin disc that NUM0 toggles | the whole feature | head gone but **no shadow** → Hide Joint Mesh was still on, or the shadow pass ignores the flag on RE2 (then the technique is dead here, not mis-tuned) |

**Flat is enough.** Launch, Insert → FirstPerson: Enabled ON, **Hide Joint Mesh OFF**. Stand near a
wall with a light behind, look at the wall for the shadow, look down for the collar, NUM0 twice, NUM.
to compare modes.

Write-up of the run goes in a new note; this one stays as the design.
