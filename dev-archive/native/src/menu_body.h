// menu_body.h -- no third-person body in menus (Tefa 2026-10-06), ported to C++ from Arcade Controls'
// re2_vr_menu_hide_player.lua (worn and liked 2026-08-20; on by default from then).
//
// The inventory, map and pause menus swing the camera round to look at the player from outside. Until the camera
// itself can be kept first person, the player (body and weapon in hand) simply is not drawn while one is open, the
// way the item box already looks. Per mesh: DrawDefault, DrawShadowCast and DrawRaytracing off on open, re-asserted
// every frame while open (the game may rewrite them), each mesh's own values put back on close.
// Menu detection: GUIMaster get_IsOpenInventory / get_IsOpenMap / get_IsOpenPause / get_IsOpenPauseForEvent
// (all proven in Arcade Controls). Only while the headset is live; flat play is left alone.
#pragma once

namespace vn::menu_body {
void frame();   // once per frame
// the camera is held where it was before a menu opened, so the picture does not jump to the menus' outside spot
// (Tefa 2026-10-06). Called at LockScene PRE (false) and PrepareRendering POST (true, also records the no-menu spot).
void camera_point(bool last_point);
// LateUpdateBehavior POST: hide the body the moment the menu first reads open, before the frame is prepared
void early_hide();
bool is_menu_open();   // inventory, map or pause open now (fire.cpp leaves RT alone then)
// for menu_probe.cpp only
bool probe_menu_open();
bool probe_body_hidden();
void* probe_camera_tf();
} // namespace vn::menu_body
