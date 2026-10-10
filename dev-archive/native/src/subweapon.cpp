// subweapon.cpp -- see subweapon.h.
#include "subweapon.h"
#include "bridge.h"
#include "common.h"
#include "holster.h"
#include "menu_body.h"
#include "settings.h"
#include "weapons.h"

#include <chrono>
#include <cmath>

namespace vn::subweapon {

namespace {
bool g_out = false;          // the right grip holds the sub weapon out
bool g_throw_armed = false;  // RT is down with the sub weapon out and the swing has not happened yet
bool g_thrown = false;       // this RT press has thrown
Vec3 g_last_rpos{};
float g_last_t = -1.0f, g_speed = 0.0f, g_peak = 0.0f;

float now_s() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
}

void swing_speed() {   // the right controller's speed in the room, smoothed a little
    const Vec3 p = bridge::hand_pos(bridge::RIGHT);
    const float t = now_s();
    if (g_last_t >= 0.0f && t > g_last_t) {
        const float v = dist(p, g_last_rpos) / (t - g_last_t);
        g_speed = g_speed * (1.0f - cfg::THROW_SWING_SMOOTH) + v * cfg::THROW_SWING_SMOOTH;
    }
    g_last_rpos = p;
    g_last_t = t;
}
} // namespace

// b136: the VR layer (our patched REFramework, VR.cpp openvr_input_to_re2_re3) asks for these every frame and writes
// HOLD / SUPPORT_HOLD / ATTACK from them. b134's writes into the button record at UpdateBehavior pre never reached
// the game (worn 2026-10-10: the sub weapon still came from the left grip, never from the right), and setForce at
// the back spot froze the picture for a second and left the menus scrolling.
struct VisceralVrButtons { int version; int sub_out; int rg_never_aims; int attack_ok; };
VisceralVrButtons g_buttons{1, 0, cfg::RG_NEVER_AIMS ? 1 : 0, 1};
extern "C" __declspec(dllexport) VisceralVrButtons* visceral_vr_buttons() { return &g_buttons; }

bool active() { return g_out; }

void frame() {
    if (!cfg::SUB_ON_RG || !bridge::live()) { g_out = false; g_buttons.sub_out = 0; g_buttons.attack_ok = 1; return; }
    const bool rg = bridge::held(bridge::S_RGRIP), rt = bridge::held(bridge::S_RTRIG);
    const bool menu = menu_body::is_menu_open();
    swing_speed();

    // the latch: pressed in the back spot, kept while held
    if (!g_out && bridge::pressed(bridge::S_RGRIP) && holster::right_hand_in_back_zone() && !menu) {
        g_out = true;
        g_throw_armed = g_thrown = false;
        g_peak = 0.0f;
        bridge::rumble(bridge::RIGHT, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
        LOGI("%s sub: right grip at the back -- sub weapon OUT (was holding %s)", TAG, weapons::current_id() < 0 ? "nothing" : weapons::name(weapons::current_id()));
    } else if (g_out && !rg) {
        g_out = false;
        LOGI("%s sub: right grip let go -- sub weapon away, back to the gun (peak swing %.2f m/s)", TAG, g_peak);
    }
    g_buttons.sub_out = (g_out && !menu) ? 1 : 0;

    // the throw: RT down with the sub weapon out waits for the swing
    if (g_out && !menu) {
        if (g_speed > g_peak) g_peak = g_speed;
        if (!rt) { g_throw_armed = g_thrown = false; }
        else if (!g_thrown) {
            if (!g_throw_armed) { g_throw_armed = true; LOGI("%s sub: RT down, waiting for the swing (now %.2f m/s, need %.2f)", TAG, g_speed, cfg::THROW_SWING_MPS); }
            if (g_speed >= cfg::THROW_SWING_MPS) {
                g_thrown = true;
                LOGI("%s sub: SWING %.2f m/s -- throw let through", TAG, g_speed);
            }
        }
        g_buttons.attack_ok = (g_thrown && g_speed >= cfg::THROW_SWING_MPS * 0.5f) ? 1 : 0;   // the swing is on: let the throw through
    } else {
        g_buttons.attack_ok = 1;
    }
}

} // namespace vn::subweapon
