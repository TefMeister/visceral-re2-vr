// guns_table.cpp -- RE2's per-gun numbers for the reload by hand (declared in guns/gun_data.h), as DATA from Andyalpa's
// RE2VRMODRELOADED 1.0.1 (re2_vr_reload.json, used with his permission 2026-09-04; shipping his data confirmed by Tefa
// 2026-09-27). Credit: Andyalpa.
#include "gun_data.h"

namespace vn::guns {

namespace {
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

// the magazine's top rounds as a mesh part of their own (read from the model files 2026-11-11, modding-notes
// 2026-11-11-the-rounds-seen-in-the-magazine.md): only the Broom Hc (WP0700, part 1, one round) and the Lightning Hawk (part 5, which the
// game leaves off). The other RE2 handguns had no such part (their magazine was one piece of the gun's mesh); since 2026-11-12 our own
// models give them one (dev-archive/native/assets/, modding-notes/2026-11-12-our-rounds-in-six-re2-handguns.md).
constexpr MagRounds MAG_ROUNDS[] = {
    {700,  -1, 1, -1, -1},
    {3000, -1, 5, -1, -1},
    {400,  -1, 41, -1, -1},   // the WP0400: its two top rounds cut out of the magazine as part 41 (2026-11-12, our model)
    {600,  -1, 61, -1, -1},   // the MUP: its top round cut out as part 61 (2026-11-12, our model)
    // 2026-11-12: these four had no round modelled; ours added (two of the WP0400's rounds, our models and material files)
    {0,    5, 7, 6, 8},       // the Matilda: standard magazine 5 -> rounds 7, long magazine 6 -> rounds 8
    {100,  -1, 11, -1, -1, 12},   // the M19 (.45, the rounds 12 % larger); its chamber round cut out of the frame as part 12
    {200,  5, 7, 6, 8},       // the JMB Hp3: standard 5 -> 7, long 6 -> 8
    {7000, -1, 1, -1, -1},    // the Samurai Edge
};

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

constexpr ShellGun SHELL_GUNS[] = {
    {1000, "_04", 31, {-0.001f, -0.030f, 0.148f}, 0.22f, "shotgun_w870", 2.0f},
    {1100, "_04", 31, {-0.001f, -0.030f, 0.148f}, 0.22f, "shotgun_w870", 2.0f},
    {1200, "_04", 31, {-0.001f, -0.030f, 0.148f}, 0.22f, "shotgun_w870", 2.0f},
    {1500, "_04", 31, {-0.001f, -0.030f, 0.148f}, 0.22f, "shotgun_w870", 2.0f},
};
} // namespace

const MagGun* mag_gun(int wp) { for (const auto& m : MAG_GUNS) if (m.wp == wp) return &m; return nullptr; }
const MagRounds* mag_rounds(int wp) { for (const auto& m : MAG_ROUNDS) if (m.wp == wp) return &m; return nullptr; }
const RoundGun* round_gun(int) { return nullptr; }   // RE2: the GM 79's flame / acid item numbers are not read yet (board row)
const SlideGun* slide_gun(int wp) { for (const auto& s : SLIDE_GUNS) if (s.wp == wp) return &s; return nullptr; }
const ShellGun* shell_gun(int wp) { for (const auto& s : SHELL_GUNS) if (s.wp == wp) return &s; return nullptr; }
bool is_pump(int wp) { return wp == 1000 || wp == 1100 || wp == 1200 || wp == 1500 || wp == 4300; }
const char* pump_sfx(int wp) { return wp == 4300 ? "spark_shot" : "shotgun_w870"; }
bool long_mag(int wp) { return wp == 2200; }   // the LE 5: the magazine sits ahead of the grip (LG on it + LT pulls it out)

} // namespace vn::guns
