// cylinder.cpp -- see cylinder.h.
#include "cylinder.h"
#include "bridge.h"
#include "gun_data.h"
#include "ghosthand.h"
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
bool g_want_open = false;    // clicked open (latched) - it clicks shut again only when pushed all the way back
float g_deg = 0.0f;          // how far the drum is swung out now
float g_last = 0.0f;
// 2026-11-23 (Tefa: "have to open with motion, LG+LT held until it clicks, can either push closed from the handle or place hand on
// the drum and push close, always LG+LT for these actions. and hand should always ride along in any games on any weapoins"): LG + LT
// on the drum TAKE it; while both stay held the drum follows the hand's sideways way (it no longer swings out by itself and leaves
// the hand behind); at the end of its swing it clicks open; pushed back all the way it clicks shut. Let go part way, it stays there.
bool g_grab = false;
Vec3 g_grab_p{};             // the left CONTROLLER (room) the swing is measured from: the drawn wrist is pinned to the drum
float g_grab_u = 0.0f;       // the swing (0 shut .. 1 open) at that x

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

// the gun's +x (its left) in room space, the way the drum swings for a positive open_deg
bool side_dir(Vec3& d) {
    Quat gq;
    return gf().gun_tf != nullptr && get_q(XFORM, gf().gun_tf, "get_Rotation", gq) && ghosthand::to_room_dir(rotate(qnorm(gq), Vec3{1, 0, 0}), d);
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
    g_grab = false;
}

bool ours() { return cfg::RELOAD_ON && has_crane() && gf().gun != nullptr && bridge::live(); }
bool hand_on_drum() { return ours() && bridge::held(bridge::S_LGRIP) && (g_grab || hand_at_drum()); }
bool is_open() { return ours() && g_deg >= std::fabs(g_w->open_deg) * cfg::CYLINDER_OPEN_ENOUGH; }
bool blocks_fire() { return ours() && g_deg > 0.0f; }

void frame() {
    g_last = now_s();
    if (!ours()) return;
    const float full = std::fabs(g_w->open_deg);
    const bool lg = bridge::held(bridge::S_LGRIP), lt = bridge::held(bridge::S_LTRIG);
    // LG + LT on the drum, the hand empty: the drum is taken (either button may come last)
    if (!g_grab && lg && lt && (bridge::pressed(bridge::S_LTRIG) || bridge::pressed(bridge::S_LGRIP)) && !shells::in_hand() && hand_at_drum()) {
        {
            g_grab = true; g_grab_p = bridge::hand_pos(bridge::LEFT); g_grab_u = full > 0.0f ? g_deg / full : 0.0f;
            bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP * 0.5f, cfg::RELOAD_GRAB_BUZZ_SEC);
            LOGI("%s cylinder: WP%04d drum taken (LG + LT) at %.0f%% open: it follows the hand", TAG, gf().wp, g_grab_u * 100.0f);
        }
    }
    if (g_grab && (!lg || !lt)) {
        g_grab = false;
        LOGI("%s cylinder: WP%04d drum let go at %.0f%% open%s", TAG, gf().wp, full > 0.0f ? g_deg / full * 100.0f : 0.0f,
             g_want_open ? " (clicked open)" : g_deg > 0.0f ? ": it stays there, the gun will not fire until it is shut" : " (shut)");
    }
    Vec3 dir;
    if (g_grab && full > 0.0f && side_dir(dir)) {
        // the drum's side (the sign of open_deg: + = the gun's +x, measured on RE3's MGL), CYLINDER_TRAVEL_M of hand for the whole swing
        const float side = g_w->open_deg >= 0.0f ? 1.0f : -1.0f;
        const Vec3 hp = bridge::hand_pos(bridge::LEFT), m = hp - g_grab_p;
        float u = g_grab_u + (m.x * dir.x + m.y * dir.y + m.z * dir.z) * side / cfg::CYLINDER_TRAVEL_M;
        if (u > 1.0f || u < 0.0f) {   // past either end: measure on from here, so the way back moves it at once
            u = std::clamp(u, 0.0f, 1.0f); g_grab_p = hp; g_grab_u = u;
        }
        if (!g_want_open && u >= cfg::CYLINDER_CLICK) {
            g_want_open = true; u = 1.0f; g_grab_p = hp; g_grab_u = u;
            sfx::play("mag_grab", g_w->sfx, g_w->vol);
            bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
            LOGI("%s cylinder: WP%04d clicked OPEN (pulled out by the hand)", TAG, gf().wp);
        } else if (g_want_open && u <= 1.0f - cfg::CYLINDER_CLICK) {
            g_want_open = false; u = 0.0f; g_grab_p = hp; g_grab_u = u;
            sfx::play("mag_insert", g_w->sfx, g_w->vol);
            bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
            LOGI("%s cylinder: WP%04d clicked SHUT (pushed back by the hand)", TAG, gf().wp);
        }
        // the latches at both ends: within the last few percent it sits fully shut (or fully open), so a hand's small wobble
        // does not leave a shut drum a hair open (robot run 2026-11-23: "let go at 0% open: it stays there")
        if (!g_want_open && u <= 1.0f - cfg::CYLINDER_CLICK) u = 0.0f;
        if (g_want_open && u >= cfg::CYLINDER_CLICK) u = 1.0f;
        g_deg = u * full;
    }
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
