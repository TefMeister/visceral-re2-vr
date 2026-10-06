# No body in menus: ported to C++ (b088, 2026-10-06)

**Build:** `Menu body/v0.2.0-b088 - no body in menus, first C++ port` (feature folder). Installed in the Steam game;
unworn. Holds b087 (running stop), b086 (ladder hold) and b085 (holsters) too.

**Tefa's ask (2026-10-06):** "No 3rd person player visible in menus."

## What it is

`dev-archive/native/src/menu_body.cpp`, a C++ re-write of Arcade Controls' `re2_vr_menu_hide_player.lua` (on by
default from 2026-08-20 after Tefa saw it in the headset `[reported 2026-08-20]`):

- While GUIMaster says the **inventory, map or pause menu** is open, every `via.render.Mesh` under the player
  (walked down the transform tree) and under the weapon in hand gets DrawDefault, DrawShadowCast and DrawRaytracing
  switched off. Re-asserted every frame while the menu stays open; each mesh's own values put back on close.
- The camera still swings to the outside view (that is a separate, older job: keep the menu camera first person);
  there is just no body to see, like the item box.
- Only while the headset is live; flat play is left alone.
- Difference from Arcade Controls `[inferred-static]`: AC listed every component of each game object; this asks each
  game object for its Mesh (one per object). If a body part still shows in a menu, that is the first thing to check.

## Proving it (`re2_framework_log.txt`)

- `menu: opened -- player hidden (N meshes)` on open (N should be well above 1: body, head, hair, clothes, weapon).
- `menu: closed -- N meshes put back` on close.

## How to take it out

`builds.py restore visceral-re2-vr 87 --yes` (b087, running stop).
