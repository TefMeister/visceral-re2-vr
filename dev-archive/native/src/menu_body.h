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
// (Tefa 2026-10-06). Called at LockScene PRE (false) and PrepareRendering POST (true). Since b100 (2026-10-08) the
// held camera is written STRIPPED of the headset turn whenever FirstPerson is not driving, because the VR layer then
// multiplies the headset turn back on itself (see menu_body.cpp, "the camera in menus").
void camera_point(bool last_point);
// BeginRendering PRE, after the VR layer's own camera pass (b103, 2026-10-09): while the camera is held, the camera's
// root joint is written with the final view, pin x inv(H0) x H, whatever FirstPerson or the VR layer did this frame.
// The VR layer itself writes joint 0 at this point, so it is the one write the renderer is sure to use. Why: the
// bridge's FirstPerson reading is a frame late at the close (FirstPerson flips back late in the frame), so b100-b102
// wrote a stripped view into a frame the VR layer did not add the headset turn to: one un-pitched frame = the flicker.
void render_point();
// LockScene POST (b106, 2026-10-09): the held view written once more, after the game's own camera step inside LockScene.
// The probe (b105) showed the inventory's closing flicker: in the close frame the camera's yaw moved ~2 deg between
// LockScene PRE (after our write) and BeginRendering (the game handing the camera back from the inventory camera);
// the pause menu does not do this. No counting here; just the write.
void late_write();   // b104: empty (b103's write is out), kept so the hook line stays simple
// LateUpdateBehavior POST: hide the body the moment the menu first reads open, before the frame is prepared
void early_hide();
bool is_menu_open();   // inventory, map or pause open now (fire.cpp leaves RT alone then)
// for menu_probe.cpp only
bool probe_menu_open();
bool probe_body_hidden();
int probe_gui_state();   // GUIMaster State_ (FirstPerson steps aside on PAUSE and INVENTORY)
void* probe_camera_tf();
void* probe_camera();     // the primary via.Camera (b105: FOV and tone mapping in the probe line)
} // namespace vn::menu_body
