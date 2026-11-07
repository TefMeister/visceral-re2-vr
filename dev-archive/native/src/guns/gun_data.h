// gun_data.h -- the per-gun numbers of the reload by hand, as DATA from Andyalpa's RE2VRMODRELOADED 1.0.1
// (re2_vr_reload.json, used with his permission 2026-09-04; shipping his data confirmed by Tefa 2026-09-27). Credit: Andyalpa.
// The code that uses them is ours. Guns not listed keep the game's own reload (the revolvers and the GM 79 until layer 4,
// the flamethrower, the Spark Shot's shots, the launchers).
#pragma once
#include "common.h"

namespace vn::guns {

// where the magazine sits in the left hand: offset from the left wrist joint in the wrist's axes (metres), and a turn
// (degrees: yaw about Z, then pitch about Y, then roll about X)
struct Hold { float ox, oy, oz, yaw, pitch, roll; };

// a magazine gun: the joint that IS the magazine, where it sits just out of the magwell (the joint's local space), how close
// a magazine must come to go in, the sound folder under reframework/data/custom_sfx/ and its volume, the hold per character
struct MagGun { int wp; const char* joint; Vec3 exit; float dock; const char* sfx; float vol; Hold leon, claire; };

constexpr Hold HOLD_GENERIC = {0.115f, -0.135f, 0.010f, -180.0f, -92.0f, 2.5f};
constexpr Hold HOLD_GENERIC_2 = {0.092f, -0.074f, 0.010f, -180.0f, -92.0f, 2.5f};

constexpr MagGun MAG_GUNS[] = {
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

// a slide or a pump: the joint that moves and its local-Z travel: rest = closed, parked = locked open, back = fully pulled
struct SlideGun { int wp; const char* joint; float rest_z, parked_z, back_z; };
constexpr SlideGun SLIDE_GUNS[] = {
    {0,    "_01", 0.060f, 0.038f,  0.013f},
    {100,  "_01", 0.101f, 0.057f,  0.007f},
    {200,  "_01", 0.101f, 0.072f,  0.044f},
    {400,  "_01", 0.101f, 0.057f,  0.007f},
    {600,  "_01", 0.098f, 0.060f,  0.025f},
    {700,  "_01", 0.050f, 0.021f, -0.005f},
    {2000, "_01", 0.004f, 0.003f, -0.072f},
    {2200, "_02", 0.300f, 0.300f,  0.250f},
    {3000, "_01", 0.098f, 0.058f,  0.030f},
    {7000, "_01", 0.108f, 0.074f,  0.040f},
    {1000, "_01", 0.000f, 0.000f, -0.080f},   // the pumps: the fore-end joint
    {1100, "_01", 0.000f, 0.000f, -0.080f},
    {1200, "_01", 0.000f, 0.000f, -0.080f},
    {1500, "_01", 0.000f, 0.000f, -0.080f},
    {4300, "_01", 0.320f, 0.320f,  0.230f},   // the Spark Shot: a pump-kind rack on its slide
};

// a pump shotgun's shells: the shell is the gun's own hidden mesh part on a joint; the port is where it goes in (the gun's
// local space); his insert distance (ours is capped by SHELL_PORT_DOCK_M)
struct ShellGun { int wp; const char* joint; int mesh_part; Vec3 port; float dock; const char* sfx; float vol; };
constexpr ShellGun SHELL_GUNS[] = {
    {1000, "_04", 31, {-0.001f, -0.030f, 0.148f}, 0.22f, "shotgun_w870", 2.0f},
    {1100, "_04", 31, {-0.001f, -0.030f, 0.148f}, 0.22f, "shotgun_w870", 2.0f},
    {1200, "_04", 31, {-0.001f, -0.030f, 0.148f}, 0.22f, "shotgun_w870", 2.0f},
    {1500, "_04", 31, {-0.001f, -0.030f, 0.148f}, 0.22f, "shotgun_w870", 2.0f},
};

inline const MagGun* mag_gun(int wp) { for (const auto& m : MAG_GUNS) if (m.wp == wp) return &m; return nullptr; }
inline const SlideGun* slide_gun(int wp) { for (const auto& s : SLIDE_GUNS) if (s.wp == wp) return &s; return nullptr; }
inline const ShellGun* shell_gun(int wp) { for (const auto& s : SHELL_GUNS) if (s.wp == wp) return &s; return nullptr; }
inline bool is_pump(int wp) { return wp == 1000 || wp == 1100 || wp == 1200 || wp == 1500 || wp == 4300; }
inline const char* pump_sfx(int wp) { return wp == 4300 ? "spark_shot" : "shotgun_w870"; }

} // namespace vn::guns
