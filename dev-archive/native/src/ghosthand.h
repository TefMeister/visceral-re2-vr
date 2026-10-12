// ghosthand.h -- Ghost Hand BeG0nE recorder (Lanes 0.62, begone/ghost-hand), ported from ghosthand.lua to C++ so the
// rack code can feed it directly (2026-10-13, Tefa: "i need to know that we can't get it working, not just assume").
// Writes %LOCALAPPDATA%\BeG0nE\ghost-hand\<game>_<date>.ghl in the Lua recorder's line format while the headset is
// live; Lanes' rider files it and ghost_hand_check.py reads it. A flat run writes nothing.
//
// Space: everything in the REAL ROOM (OpenXR tracking space, metres). The controllers come from the bridge as they
// are; the game's hand joints and the slide are carried from game world into the room through the camera:
// room = hmd_pos + hmd_rot * inv(cam_rot) * (world - cam_pos)   (the VR layer sets the camera from the headset).
#pragma once

namespace vn::ghosthand {
void render_point();                          // BeginRendering pre, after rack::render_point: one frame of samples
void event(const char* gesture, const char* what);   // "start" | "done" | "fail" | "cancel"
void value(const char* name, float v);
void shutdown();
} // namespace vn::ghosthand
