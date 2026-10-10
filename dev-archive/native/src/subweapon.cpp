// subweapon.cpp -- see subweapon.h.
#include "subweapon.h"
#include "bridge.h"
#include "common.h"
#include "holster.h"
#include "inputbits.h"
#include "menu_body.h"
#include "settings.h"
#include "weapons.h"

#include <chrono>
#include <cmath>

namespace vn::subweapon {

namespace {
bool g_out = false;          // the right grip holds the sub weapon out
bool g_force_on = false;     // InputSystem.setForce(SUPPORT_HOLD) is on
bool g_throw_armed = false;  // RT is down with the sub weapon out and the swing has not happened yet
bool g_thrown = false;       // this RT press has thrown
int g_hold_cleared = 0, g_sh_cleared = 0, g_attack_held = 0;
Vec3 g_last_rpos{};
float g_last_t = -1.0f, g_speed = 0.0f, g_peak = 0.0f;

float now_s() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
}

// the game's own "hold this button" switch, as suppress.cpp uses it for the left grip
void set_force(bool on) {
    if (on == g_force_on) return;
    auto* is = API::get()->get_managed_singleton("app.ropeway.InputSystem");
    auto* m = is ? find_method_deep(is->get_type_definition(), "setForce") : nullptr;
    if (m == nullptr) { LOGW("%s sub: InputSystem.setForce not found", TAG); return; }
    m->call<void>(API::get()->get_vm_context(), (void*)is, inputbits::SUPPORT_HOLD, on);
    g_force_on = on;
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

bool active() { return g_out; }

void frame() {
    if (!cfg::SUB_ON_RG || !bridge::live()) { if (g_force_on) set_force(false); g_out = false; return; }
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
    set_force(g_out && !menu);

    // the buttons the game reads this frame
    if (cfg::RG_NEVER_AIMS && inputbits::clear(inputbits::HOLD) && ++g_hold_cleared <= 5)
        LOGI("%s sub: HOLD (right grip aim) cleared (#%d)", TAG, g_hold_cleared);
    if (g_out && !menu) {
        inputbits::hold_on(inputbits::SUPPORT_HOLD);
    } else if (inputbits::clear(inputbits::SUPPORT_HOLD) && ++g_sh_cleared <= 5) {
        LOGI("%s sub: SUPPORT_HOLD (left grip sub weapon) cleared (#%d)", TAG, g_sh_cleared);
    }

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
        if (!g_thrown || g_speed < cfg::THROW_SWING_MPS * 0.5f) {   // not yet, or the swing is over: no attack
            if (inputbits::clear(inputbits::ATTACK) && ++g_attack_held <= 5) LOGI("%s sub: ATTACK held back until the swing (#%d)", TAG, g_attack_held);
        }
    }
}

} // namespace vn::subweapon
