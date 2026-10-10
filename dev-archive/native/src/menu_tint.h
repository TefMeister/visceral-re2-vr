// menu_tint.h -- every menu over the live game world: no colour tint, no blur (Tefa 2026-10-10, with a screenshot of the
// use-item screen at a locked door: "this is our goal, to make all menus and inventorys, item pick up screen, map
// screen, item box, to look like this seamlessly. no black or blue tint in the background, just game world").
//
// What the game does [inferred-static 2026-10-10, type database + reader]: the inventory (NewInventoryBehavior) has one
// open path per mode (Normal, Map, GetItem, UseItem, ItemBox...). On open it calls activatePostEffect, which picks
// activatePostEffectNormal / ...Capture / ...UseItem by mode and switches on the inventory post-effect layer
// (app.ropeway.posteffect.cascade.InventoryLayer: the colour filter) plus a blur. activatePostEffectUseItem is an EMPTY
// stub -- so the use-item screen Tefa likes is simply the inventory with its post effect never switched on. The lever
// is therefore: make every mode take the use-item path, by skipping activatePostEffect (and the per-mode entries) in
// a pre-hook. The VR layer already refuses to draw the GuiBack backdrop (REFramework VR.cpp 3112), and menu_body.cpp
// already holds the camera at the pre-menu spot, so with the post effect gone the live world should be all that is
// behind the menu.
//
// Lever check (PROTOCOL §11): the tint is decided inside activatePostEffect, at open; a pre-hook on it runs BEFORE
// that decision, so this fixes rather than annotates. Proof of effect: the InventoryLayer's own isActive flag and
// current slot are logged on every open and close ("tint: ..."), so a run can tell "no effect" from "wrong switch".
#pragma once

namespace vn::menu_tint {
void install();   // the hooks (once, from the game thread)
void frame();     // once per frame: logs the layer state while a menu is open (first frames only)
bool gui_draw(void* gui_element, void* context);   // REFramework's pre GUI draw: names every element drawn in a menu once; flat, skips cfg::MENU_TINT_HIDE
} // namespace vn::menu_tint
