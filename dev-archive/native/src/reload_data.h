// reload_data.h -- the per-weapon numbers for the manual magazine reload, taken as DATA from Andyalpa's
// RE2VRMODRELOADED 1.0.1 (`reframework/data/re2_vr/re2_vr_reload.json`, the shipped values; used with his permission,
// 2026-09-04, and shipping his data confirmed by Tefa 2026-09-27). Credit: Andyalpa. The code that uses them is ours.
//
//   joint     the weapon joint that IS the magazine (`mag_node_by_wp`); hidden by scaling it to zero
//   exit      where the magazine sits when it has just left the magwell, in the joint's local space (`mag_exit_by_wp`):
//             the drop slides the magazine here, and a new one must be brought within `dock` metres of it to go in
//   dock      insert distance (`mag_dock_by_wp`)
//   sfx       the folder under reframework/data/custom_sfx/ (`weapon_sfx.by_wp[..].sfx_folder`); files are <kind>.ogg/.wav
//   vol       volume of the reload sounds in that folder (`volume_by_kind`; 2.0 for the handgun pack, else 1.0)
//   leon/claire  where the magazine sits in the left hand (`mag_hand_hold.by_wp`): offset from the left wrist joint in
//             the wrist's own axes (metres) and a turn (degrees, applied yaw about Z, then pitch about Y, then roll about X)
#pragma once
#include "common.h"

namespace vn::reload {

struct Hold { float ox, oy, oz, yaw, pitch, roll; };

struct MagWeapon {
    int wp;
    const char* joint;
    Vec3 exit;
    float dock;
    const char* sfx;
    float vol;
    Hold leon, claire;
};

// Bundle 1 (2026-10-10): every magazine-fed gun RELOADED enables. Not here yet: the Spark Shot (pump, bundle 2), the
// flamethrower canister and the revolvers / shotguns / grenade launcher (bundle 3). Those keep the game's own reload.
constexpr Hold HOLD_GENERIC = {0.115f, -0.135f, 0.010f, -180.0f, -92.0f, 2.5f};
constexpr Hold HOLD_GENERIC_2 = {0.092f, -0.074f, 0.010f, -180.0f, -92.0f, 2.5f};

constexpr MagWeapon MAG_WEAPONS[] = {
    {0,    "_04", {-0.004f, -0.110f, -0.026f}, 0.05f, "handgun",  2.0f, {0.099f, -0.024f, 0.056f, 72.5f, 15.0f, 106.5f}, HOLD_GENERIC},
    {100,  "_04", { 0.001f, -0.093f, -0.016f}, 0.03f, "handgun",  2.0f, {0.127f, -0.030f, 0.052f, -106.5f, 156.0f, -61.0f}, HOLD_GENERIC},
    {200,  "_04", { 0.000f, -0.089f, -0.023f}, 0.03f, "handgun",  2.0f, {0.100f, -0.032f, 0.028f, -82.5f, -180.0f, -53.5f}, HOLD_GENERIC},
    {400,  "_01", { 0.000f, -0.103f, -0.035f}, 0.05f, "handgun",  2.0f, HOLD_GENERIC, HOLD_GENERIC},
    {600,  "_04", { 0.000f, -0.103f, -0.035f}, 0.05f, "handgun",  2.0f, HOLD_GENERIC, HOLD_GENERIC},
    {700,  "_04", {-0.002f, -0.072f, -0.019f}, 0.03f, "handgun",  2.0f, {0.127f, -0.030f, 0.052f, -106.5f, 156.0f, -61.0f}, {0.127f, -0.030f, 0.052f, -106.5f, 156.0f, -61.0f}},
    {2000, "_04", {-0.002f, -0.108f,  0.006f}, 0.02f, "smg_mq11", 1.0f, {0.093f, -0.021f, 0.045f, -90.0f, 172.5f, -72.5f}, HOLD_GENERIC_2},
    {2200, "_04", { 0.000f, -0.006f,  0.164f}, 0.05f, "smg_mp5",  1.0f, {0.110f, -0.028f, 0.053f, 49.5f, 33.0f, 136.5f}, HOLD_GENERIC_2},
    {3000, "_04", { 0.000f, -0.103f, -0.022f}, 0.05f, "magnum",   1.0f, HOLD_GENERIC_2, HOLD_GENERIC_2},
    {7000, "_04", { 0.002f, -0.109f, -0.021f}, 0.04f, "handgun",  2.0f, {0.097f, -0.028f, 0.032f, -115.0f, 167.5f, -64.5f}, HOLD_GENERIC_2},
};

inline const MagWeapon* mag_weapon(int wp) {
    for (const auto& m : MAG_WEAPONS)
        if (m.wp == wp) return &m;
    return nullptr;
}

} // namespace vn::reload
