// cylinder.cpp -- see cylinder.h.
#include "cylinder.h"
#include "bridge.h"
#include "gun_data.h"
#include "gun_frame.h"
#include "joints.h"
#include "rounds.h"
#include "settings.h"
#include "sfx.h"
#include "shells.h"

#include <algorithm>
#include <cmath>

namespace vn::guns::cylinder {

using namespace joints;

namespace {
constexpr float PI = 3.14159265f;

const RoundGun* g_w = nullptr;
MO* g_crane = nullptr;
bool g_have_rest = false;
Quat g_rest{0, 0, 0, 1};
bool g_want_open = false;
float g_deg = 0.0f;          // how far the drum is swung out now
float g_last = 0.0f;

bool has_crane() { return g_w != nullptr && g_w->crane != nullptr && g_w->crane[0] != '\0'; }

// the left wrist's distance from the drum, in the gun's own space
bool hand_at_drum() {
    MO* drum = gf().gun_tf != nullptr ? joint_by_name(gf().gun_tf, widen(g_w->drum).c_str()) : nullptr;
    Vec3 dw, hw, dl, hl;
    if (drum == nullptr || !get_v3(JOINT, drum, "get_Position", dw) || !get_v3(JOINT, gf().lwrist, "get_Position", hw)) return false;
    const Quat id{0, 0, 0, 1};
    if (!to_gun_local(dw, id, dl, nullptr) || !to_gun_local(hw, id, hl, nullptr)) return false;
    return dist(dl, hl) < cfg::CYLINDER_GRAB_M;
}

// the muzzle's rise: the gun's +Z against straight up
float muzzle_up() {
    Quat gq;
    if (gf().gun_tf == nullptr || !get_q(XFORM, gf().gun_tf, "get_Rotation", gq)) return 0.0f;
    return rotate(qnorm(gq), Vec3{0, 0, 1}).y;
}

void write() {
    if (!has_crane() || gf().gun_tf == nullptr) return;
    g_crane = joint_by_name(gf().gun_tf, widen(g_w->crane).c_str());
    if (g_crane == nullptr) return;
    if (!g_have_rest) g_have_rest = get_q(JOINT, g_crane, "get_LocalRotation", g_rest);
    if (!g_have_rest || g_deg <= 0.0f) return;
    const float a = std::copysign(g_deg, g_w->open_deg) * PI / 180.0f * 0.5f;   // the sign of open_deg is the side it swings to
    Quat q = qnorm(qmul(g_rest, Quat{0, 0, std::sin(a), std::cos(a)}));
    set_any(JOINT, g_crane, "set_LocalRotation", &q);
}
} // namespace

void on_gun_changed(int new_wp) {
    g_w = round_gun(new_wp);
    g_crane = nullptr;
    g_have_rest = false;
    g_want_open = false;     // a gun put away is closed when it comes back
    g_deg = 0.0f;
}

bool ours() { return cfg::RELOAD_ON && has_crane() && gf().gun != nullptr && bridge::live(); }
bool is_open() { return ours() && g_deg >= std::fabs(g_w->open_deg) * cfg::CYLINDER_OPEN_ENOUGH; }
bool blocks_fire() { return ours() && g_deg > 0.0f; }

void frame() {
    const float now = now_s();
    const float dt = std::clamp(now - g_last, 0.0f, 0.1f);
    g_last = now;
    if (!ours()) return;
    // LG on the drum + LT, the hand empty: open, or closed again
    if (bridge::held(bridge::S_LGRIP) && bridge::pressed(bridge::S_LTRIG) && !shells::in_hand() && hand_at_drum()) {
        g_want_open = !g_want_open;
        sfx::play(g_want_open ? "mag_grab" : "mag_insert", g_w->sfx, g_w->vol);
        bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
        LOGI("%s cylinder: WP%04d %s (LG on the drum + LT)", TAG, gf().wp, g_want_open ? "opening" : "closing");
    }
    const float target = g_want_open ? std::fabs(g_w->open_deg) : 0.0f;
    const float step = cfg::CYLINDER_SWING_DEG_S * dt;
    g_deg = g_deg < target ? std::min(target, g_deg + step) : std::max(target, g_deg - step);
    // open and tipped muzzle-up: the spent cases fall out
    if (is_open() && muzzle_up() >= cfg::CYLINDER_TIP_UP) {
        const int n = rounds::dump_spent();
        if (n > 0) {
            sfx::play("mag_grab", g_w->sfx, g_w->vol);
            bridge::rumble(bridge::RIGHT, cfg::RELOAD_INSERT_BUZZ_AMP, cfg::RELOAD_INSERT_BUZZ_SEC);
            LOGI("%s cylinder: WP%04d %d spent case(s) tipped out", TAG, gf().wp, n);
        }
    }
}

void late_point() { if (ours()) write(); }
void render_point() { if (ours()) write(); }

} // namespace vn::guns::cylinder
