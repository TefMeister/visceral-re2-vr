// gun_frame.h -- the player, the gun in hand and the wrists, looked up once and shared by every gun file (layer 3).
// refresh() runs at the start of the gun code's frame and again at each late point (the objects can change in between).
// Every pointer may be null; each is valid only until the next refresh.
#pragma once
#include "game.h"
#include "common.h"

namespace vn::guns {

struct GunFrame {
    MO* player = nullptr;      // the player's GameObject
    MO* equipment = nullptr;   // <ns>survivor.Equipment
    MO* inventory = nullptr;   // <ns>survivor.Inventory
    MO* gun = nullptr;         // the Gun in hand (an implement.Gun; null for the knife, grenades, bare hands)
    MO* gun_tf = nullptr;      // the gun's transform (its +Z is the barrel)
    MO* lwrist = nullptr;      // l_arm_wrist / r_arm_wrist joints of the drawn body
    MO* rwrist = nullptr;
    int wp = -1;               // the gun in hand, WP number (-1 = none)
    bool claire = false;       // the gun holds' second set (game::second_character: Claire in RE2)
};

void refresh();
const GunFrame& gf();

// world <-> the gun transform's frame (false when there is no gun this frame)
bool to_gun_local(Vec3 world, Quat wrot, Vec3& lpos, Quat* lrot);
bool from_gun_local(Vec3 lpos, Quat lrot, Vec3& world, Quat& wrot);
// yaw about Z, then pitch about Y, then roll about X, degrees (Andyalpa's convention for his hold numbers)
Quat ypr_deg(float yaw, float pitch, float roll);

} // namespace vn::guns
