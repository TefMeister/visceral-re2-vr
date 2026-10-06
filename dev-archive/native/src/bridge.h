// bridge.h -- controller buttons in, rumble out, through lua/visceral_bridge.lua's shared float array.
#pragma once
#include "common.h"

namespace vn::bridge {

enum Slot : int {
    S_FRAME = 0, S_HMD = 1,
    S_LGRIP = 2, S_LTRIG = 3, S_RGRIP = 4, S_RTRIG = 5,
    S_LA = 6, S_LB = 7, S_RA = 8, S_RB = 9,
    S_RUMBLE_L_AMP = 10, S_RUMBLE_L_SEC = 11, S_RUMBLE_R_AMP = 12, S_RUMBLE_R_SEC = 13,
    S_HMD_POS = 14, S_HMD_ROT = 17, S_LPOS = 21, S_RPOS = 24,        // VR tracking space (the real room)
    S_ACK = 30, S_SENTINEL = 31,
    // view readings for the ladder/cupboard hold (2026-10-06), written by the Lua at LateUpdateBehavior PRE, so the
    // plugin reads them fresh at LateUpdateBehavior POST through view(). Yaws in radians, game convention
    // atan2(forward.x, forward.z); NO_VALUE when vrmod could not give one.
    S_FP_USED = 32,       // 1 = REFramework's FirstPerson is driving the view
    S_HMD_YAW = 33,       // raw headset yaw (vrmod:get_transform(0))
    S_OFFEXT_YAW = 34,    // yaw of rotation_offset * raw headset
    S_CAM_YAW = 35,       // the game camera's yaw (primary camera world matrix)
    S_RENDER_YAW = 36,    // the yaw actually rendered last frame (vrmod:get_last_render_matrix)
    S_COUNT = 40,
};
constexpr float SENTINEL = 54321.0f;
constexpr float NO_VALUE = 999.0f;

enum Hand : int { LEFT = 0, RIGHT = 1 };

void install();                    // the mailbox hook (call once, from the game thread)
bool live();                       // array attached and the headset active
bool held(Slot s);                 // button held this frame
bool pressed(Slot s);              // went down this frame (edge)
void frame_begin();                // snapshot buttons for edge detection; once per frame, before any feature
void rumble(Hand h, float amplitude, float seconds);
Vec3 hmd_pos();
Quat hmd_rot();
Vec3 hand_pos(Hand h);
float view(Slot s);                // read a view slot NOW (not the frame-start snapshot); NO_VALUE if not live
inline bool has(float v) { return v < NO_VALUE * 0.5f; }

} // namespace vn::bridge
