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
    // running stop (2026-10-06), written with the buttons at UpdateHID
    S_LCLICK = 37,        // left stick click (vrmod joystick-click action on the left hand)
    S_LSTICK_MAG = 38,    // how far the left stick is pushed, 0..1
    S_RCLICK = 39,        // 2026-11-16: right stick click (the side step, src/hands/sidestep.cpp)
    // menu camera (2026-10-08, src/menu_body.cpp): what the VR layer will multiply onto the camera while FirstPerson
    // is not driving. Written at LateUpdateBehavior PRE with the view readings; first component NO_VALUE on failure.
    S_HMD_Q = 40,         // raw headset turn as a quaternion x y z w (vrmod:get_transform(0):to_quat(), proven path)
    S_ROT_OFF = 44,       // the VR layer's rotation offset, quaternion x y z w (vrmod:get_rotation_offset())
    S_ORIGIN = 48,        // the VR layer's standing origin x y z (vrmod:get_standing_origin())
    S_RROT = 52,          // the right controller's turn in room space, quaternion x y z w (b143, motion racking)
    S_TUNE_ON = 56,       // b156: 1 = the shell-in-hand sliders (Lua menu) are live
    S_TUNE = 57,          // b156: shell hold ox oy oz (m) yaw pitch roll (deg), from the Lua menu sliders
    S_DOCK_ON = 64,       // b158: 1 = the slide-grip sliders (Lua menu) are live
    S_DOCK = 65,          // b158: hand-on-slide ox oy oz (m, slide joint axes) yaw pitch roll (deg)
    S_FING_ON = 72,       // b159: 1 = the finger sliders (Lua menu) are live (slide grip)
    S_FING = 73,          // b159: curl index middle ring little thumb, then spread the same five (radians)
    S_COUNT = 84,         // the frame snapshot (buttons) covers these; the slots below are read live with view()
    // 2026-10-13 hand tuning (src/tune.h, lua/visceral_tuning.lua): per character, per gun, per hold
    S_WHO = 84,           // WRITTEN BY THE PLUGIN: the gun in hand (WP number), then the character (pl digit) at +1
    S_HT_ON = 86,         // 1 = the Lua is sending tuning numbers
    S_HT = 88,            // seven holds (magazine, shell, slide, right hand on the gun, b172: left hand opening, left hand on its own, b173: left hand opening for the handle) of HT_STRIDE numbers each, see tune.h
    S_FREEZE = 536,       // b169: 1 = hold both hands still (the Lua menu's tick box, after its 10 s countdown), see tune.h; b172/b173: moved from 344 for the new holds
    S_LADDER_HOLD_OFF = 537, // b188: 1 = the ladder/cupboard view hold (ladder.cpp) is switched off by the Lua menu's tick box; 0 or unset = on
    S_AUTO_EXPOSURE_ON = 538,
    S_LEFT_ON_GUN = 539,  // 2026-11-01, WRITTEN BY THE PLUGIN: 1 = the left hand is on the gun (Hand Poser holds its fingers only then) // 2026-10-28: 1 = the game keeps its own automatic brightness (Lua tick box); 0 or unset = ours keeps it off (exposure.cpp)
    // 2026-11-16, the side step (src/hands/sidestep.cpp), written with the buttons at UpdateHID, read live with view():
    S_LSTICK_DIR = 604,   // the left stick turned by the game camera into a level world direction x z (unit; 0 0 when centred)
    S_CAM_FWD = 606,      // the game camera's level forward x z (unit), for a step back with the stick centred
    // 2026-11-18 (Tefa: "give re2 and re3 hands, walls and guns the same collision treatment" as Village b141,
    // src/hands/walls.cpp + lua/visceral_walls.lua). WRITTEN BY THE PLUGIN when our REFramework asks it to place the hands
    // (visceral_hand_wall): 608-614 the right hand as the controller puts it (world pos, turn x y z w), 615-621 the right
    // hand as handed back (after the wall), 622-624 the left hand as the controller puts it, 625 S_FRAME at that moment.
    S_HW_RAW_R = 608, S_HW_OUT_R = 615, S_HW_RAW_L = 622, S_HW_STAMP = 625,
    // WRITTEN BY THE LUA: per hand, S_FRAME + 1 of the frame a wall was last seen in front of it, a point on the hand's stop
    // plane (world) and the plane's normal (toward the player); the gun's turn at the wrist (x y z w) and its stamp; the
    // drawn muzzle (world) and its stamp (the doors push with it).
    S_WALL_L_ON = 626, S_WALL_L_P = 627, S_WALL_L_N = 630,
    S_WALL_R_ON = 633, S_WALL_R_P = 634, S_WALL_R_N = 637,
    S_WALL_TILT_Q = 640, S_WALL_TILT_ON = 644, S_MUZZLE_W = 645, S_MUZZLE_ON = 648,
    S_HT2 = 540,          // 2026-10-31: the eighth hold onward (tune.h MAG_DOCK: the left hand on the magazine in the gun), HT_STRIDE each; the array is 604 long (visceral_bridge.lua N)
};
constexpr float SENTINEL = 54321.0f;
constexpr float NO_VALUE = 999.0f;
constexpr int HT_STRIDE = 64;   // per hold: item move 3, item turn 3, hand move 3, hand turn 3, 17 finger joints x 3

enum Hand : int { LEFT = 0, RIGHT = 1 };

void install();                    // the mailbox hook (call once, from the game thread)
bool live();                       // array attached and the headset active
bool held(Slot s);                 // button held this frame
bool pressed(Slot s);              // went down this frame (edge)
void frame_begin();                // snapshot buttons for edge detection; once per frame, before any feature
void rumble(Hand h, float amplitude, float seconds);
Vec3 hmd_pos();
Quat hmd_rot();
Quat right_rot();                   // 2026-11-01: the right controller's turn in room space (S_RROT)
Vec3 hand_pos(Hand h);
float view(Slot s);                // read a view slot NOW (not the frame-start snapshot); NO_VALUE if not live
Quat view_quat(Slot s);            // four slots read NOW; check has(.x) before use
Vec3 view_vec3(Slot s);
void put(Slot s, float v);         // write a slot the Lua reads (S_WHO)            // three slots read NOW; check has(.x) before use
inline bool has(float v) { return v < NO_VALUE * 0.5f; }

} // namespace vn::bridge
