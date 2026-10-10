# Every menu over the live game world: no tint, no blur (2026-10-10, home PC, /lm, Fable; Tefa at the monitor)

**Builds:** b128 → b131, all in `D:\Visceral build versions\menus over the live world\`. **b131 is installed** in the Steam game.
Tefa's ask (with a screenshot of the use-item screen at a locked door): *"this is our goal, to make all menus and inventorys,
item pick up screen, map screen, item box, to look like this seamlessly. no black or blue tint in the background, just game
world"* — plus the camera jump to a preset door angle must go.

## What was found (static, type database + REFramework source, reader + own reads) `[inferred-static 2026-10-10]`

- The inventory (`app.ropeway.gui.NewInventoryBehavior`) has one open path per mode: `CallOpenMode` Normal 0, Map 1,
  GetMap 2, Map4th 3, GetItem 4, GetItemShortcut 5, **UseItem 6**, ItemBox 7. `GUIMaster` wraps them
  (`openInventory`, `openInventoryGetItemMode`, `openInventoryItemBoxMode`, `openInventoryMapMode`, `openInventoryUseMode`).
- **The tint is the inventory's post effect**: `activatePostEffect` picks `activatePostEffectNormal` @0x140f68570 /
  `activatePostEffectCapture` @0x140f684b0 / `activatePostEffectUseItem` @0x14004fd20 by mode — and **the UseItem one is the
  shared empty stub**. So the use-item screen Tefa likes is just the inventory with its post effect never switched on. The
  effect itself is `app.ropeway.posteffect.cascade.InventoryLayer` (colour filter, slots `InventoryFilterLabel.Slot`) + a blur.
- **The backdrop** (`GuiBack`, `NewInventoryBackBehavior`: `CapturePanel`, `RenderTex`, `BgBlur`) is a CAPTURED SCREEN
  shown as a GUI panel. The VR layer refuses to draw `GuiBack` at all in RE2/RE3 (REFramework `VR.cpp` 3112, "the weird
  buggy overlay in the inventory"), so in the headset only the post effect was tinting the live world.
- **The pause menu has no post effect**: its blur and darkening are INSIDE the `GUI_Pause` element: `main > mask_all`
  (full-screen Texture), `c_blur` (Rect + Texture + a `via.gui.BlurFilter` named `blur`), `c_bg` (two Rects)
  `[verified-live 2026-10-10, element tree logged]`.
- Music volume is option 19 (`AudioBGMVolume`, 0..10) in the system save, no file; `via.wwise.WwiseManager` has static
  `set_MuteMusic(bool)` / `set_VolumeMusic(float)`.

## What was built

- `src/menu_tint.cpp` (+ `.h`, settings `MENU_TINT_*`): pre-hooks on `activatePostEffect` / `...Normal` / `...Capture`
  return SKIP (every mode takes the use-item path); `InventoryLayer.activate/deactivate` and `deactivatePostEffect` logged;
  the layer's `isActive` + `CurrentSlot` logged on open/close as proof. GUI draw callback: names every element drawn in
  a menu once (`tint: drawn <name>`); **flat only** skips `GuiBack` (so a flat screenshot shows what the headset shows);
  walks `GUI_Pause`'s tree and sets `blur`, `mask_all`, `c_blur`, `c_bg` invisible (re-applied if the game turns them back).
- `reframework/autorun/visceral_mute_music.lua`: music muted every launch via `WwiseManager.set_MuteMusic(true)` +
  `set_VolumeMusic(0)`, re-applied once a second, read back in the log (`before ? / 10.0, after true / 0.0`)
  `[verified-live 2026-10-10, n=4 launches]`.
- `dev-archive/tools/re2drive.py`: `GAME` pointed back at the Steam game (the test copy was deleted 2026-10-04).

## What was seen — flat, no headset (Virtual Desktop was not linked: `XR_ERROR_FORM_FACTOR_UNAVAILABLE`)

| build | screen | result |
| --- | --- | --- |
| b128 | inventory, map | post effect never on (`InventoryLayer isActive=0` all frames), activate call `SKIPPED`; flat still dark+grainy = the captured backdrop `GuiBack` (flat only) `[verified-live 2026-10-10, n=2]` |
| b129 | inventory, map | `GuiBack` skipped flat: **live world, sharp, no tint** (screenshot `b129-inv.png`) `[verified-live, n=1]` |
| b129 | pause | still blurred + dark; probe: only `GUI_Pause`, `BlackFade`, `WhiteFade`, guides drawn — the blur is inside `GUI_Pause` |
| b130 | pause, inventory, map, item box | Tefa at the monitor: inventory, map, item box **"all good"**; pause **not blurry but still darker** `[verified-live 2026-10-10, Tefa, n=1]` |
| b131 | pause | `mask_all`, `c_blur`, `c_bg` hidden — Tefa: **"looks great!"** `[verified-live 2026-10-10, Tefa, n=1]` |

Pick-up (board 6b) was not exercised (no loose item near the save). Item box opened fine over the world.

## Camera

Flat, the inventory and pause still move the camera to the game's third-person spot (expected: `menu_body.cpp`'s hold
runs only with the headset live, b100-b102). Whether the use-item screen at a door still jumps in the headset is
**unknown until worn**; that screen IS `openInventoryUseMode`, which `GUIMaster.get_IsOpenInventory` covers, so the hold
should already apply `[hypothesis]`.

## Not established / open

- The headset picture: not worn yet. The VR layer draws `GUI_Pause` on a quad; the hidden nodes should carry over.
- Whether skipping the capture post effect breaks the item DETAIL view (the 3D item close-up) — not opened this session.
- The pick-up black (6b): the tint is now off; if black remains it is not the post effect. Still `[hypothesis]`: a fade.
- ghidrust: every `decompile` on re2.exe (92 MB .text) hung for the 30-minute idle limit; static reads came from the type
  database and REFramework's source instead. Try `function_create` first next time, or keep ghidrust for xrefs only.
- The reader's second question (inventory camera preset source; pick-up black) was stopped unanswered at 36 min.
