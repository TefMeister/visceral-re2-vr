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
// b168 (Tefa 2026-10-13: "make the bone sliders for the right hand as well ... curl the tip of the finger around the
// trigger and change how some weapons clip into the hand"): a fourth hold, GUN = the RIGHT hand on the gun, live
// whenever a gun is in hand. Item = the r_weapon joint (the gun in the hand), hand = r_arm_wrist, fingers = the
// r_hand_* joints, all ADDED to whatever the game / the VR layer put there this frame (see right_point).
// b169: visceral_hands_frozen() (exported, read by the VR layer) holds both hands still while the menu's tick box is on
// and its 10 s countdown has run out, so the controllers can be put down while the sliders are moved.
#pragma once
#include "common.h"

namespace vn::tune {
enum Hold : int { MAG = 0, SHELL = 1, SLIDE = 2, GUN = 3, HOLD_COUNT = 4 };
constexpr int kFingerJoints = 17;   // the order of handpose_data.h kBones

void frame();                     // UpdateBehavior pre: tell the Lua which gun and which character
void wrist_point();               // LateUpdateBehavior post / PrepareRendering post, BEFORE the item and the fingers
void right_point();               // after handpose::point: the right hand's sliders (GUN hold)
bool live();                      // the Lua is sending numbers
Vec3 item_move(Hold h);           // metres, wrist axes
Quat item_turn(Hold h);
Vec3 hand_move(Hold h);           // metres
Vec3 hand_turn_deg(Hold h);       // yaw pitch roll, degrees
Vec3 finger(Hold h, int joint);   // x twist, y spread, z curl -- radians, added to the joint's local Euler angles
Quat ypr(Vec3 deg);               // yaw about Z, then pitch about Y, then roll about X (his convention)
// b171 (Tefa 2026-10-14, after straightening the shotgun with the gun sliders: "i can NOW grip it in 2 spots"): a world
// pose read from anything carried by the gun (the fore-end, the slide, the gun's own transform) is the UNTUNED one on
// frames where the game's animation has rewritten r_weapon since our last write. This moves it to where the tuned gun
// puts it, so the grab zone and the dock spot follow the gun the player sees. Returns how far it moved the point
// (0 = the tuned gun was already standing, or no gun sliders are set).
float to_tuned(Vec3& p, Quat* q);
} // namespace vn::tune
