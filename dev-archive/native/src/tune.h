// tune.h -- per character, per gun, per hold tuning (2026-10-13, Tefa: "build the tuning sliders for all guns in the
// game, also Leon, Claire, Ada and Hunk must all have different save files for them, because all their hands are
// different"). The sliders live in lua/visceral_tuning.lua (REFramework menu "Visceral: hand tuning"), one save file
// per character; the Lua sends the numbers for the gun in hand and the character playing through the bridge.
// Every number is an OFFSET added on top of what is built in (0 everywhere = as built), so a gun nobody has tuned
// looks exactly as before.
//
// Per hold: the ITEM in the hand (magazine / shell: move in the wrist's axes, turn yaw-pitch-roll), the HAND (on the
// slide: added to the dock spot; with a magazine or shell: the wrist joint itself is moved and turned), and every
// FINGER JOINT (17 joints: twist, spread, curl).
#pragma once
#include "common.h"

namespace vn::tune {
enum Hold : int { MAG = 0, SHELL = 1, SLIDE = 2, HOLD_COUNT = 3 };
constexpr int kFingerJoints = 17;   // the order of handpose_data.h kBones

void frame();                     // UpdateBehavior pre: tell the Lua which gun and which character
void wrist_point();               // LateUpdateBehavior post / PrepareRendering post, BEFORE the item and the fingers
bool live();                      // the Lua is sending numbers
Vec3 item_move(Hold h);           // metres, wrist axes
Quat item_turn(Hold h);
Vec3 hand_move(Hold h);           // metres
Vec3 hand_turn_deg(Hold h);       // yaw pitch roll, degrees
Vec3 finger(Hold h, int joint);   // x twist, y spread, z curl -- radians, added to the joint's local Euler angles
Quat ypr(Vec3 deg);               // yaw about Z, then pitch about Y, then roll about X (his convention)
} // namespace vn::tune
