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
// on the drum TAKE it; while both stay held the drum follows the hand (it no longer swings out by itself and leaves the hand
// behind); at the end of its swing it clicks open; pushed back all the way it clicks shut. Let go part way, it stays there.
// Tefa, the same night: "push down and to the right to close, left and up to open, yes the grip": the drum follows the hand ROUND
// ITS HINGE (on the MGL the hinge sits 9.6 cm above the drum, so opening carries it left AND up), and LG + LT at the front grip,
// which is where the drum sits shut, takes it too (with the drum open, the grip is 15 cm from it).
bool g_grab = false;
float g_grab_ang = 0.0f;     // the hand's angle round the hinge when taken (degrees, + = the opening way)
float g_grab_deg = 0.0f;     // the swing then
// the hinge, measured once per gun in the gun's own frame while the drum is shut: its place, its axis, the shut drum's place
bool g_geom = false;
Vec3 g_pivot_l{}, g_axis_l{}, g_drum_l{};

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

bool gun_pose(Vec3& gp, Quat& gq) {
    return gf().gun_tf != nullptr && get_v3(XFORM, gf().gun_tf, "get_Position", gp) && get_q(XFORM, gf().gun_tf, "get_Rotation", gq);
}
// once per gun, with the drum shut: the hinge and the drum in the gun's own frame
void learn_geom() {
    if (g_geom || g_deg > 0.0f || g_crane == nullptr || gf().gun_tf == nullptr) return;
    MO* drum = joint_by_name(gf().gun_tf, widen(g_w->drum).c_str());
    Vec3 gp, cw, dw; Quat gq, cq;
    if (drum == nullptr || !gun_pose(gp, gq) || !get_v3(JOINT, g_crane, "get_Position", cw) || !get_q(JOINT, g_crane, "get_Rotation", cq) ||
        !get_v3(JOINT, drum, "get_Position", dw)) return;
    const Quat gi = qinv(qnorm(gq));
    g_pivot_l = rotate(gi, cw - gp); g_drum_l = rotate(gi, dw - gp); g_axis_l = rotate(gi, rotate(qnorm(cq), Vec3{0, 0, 1}));
    g_geom = true;
    LOGI("%s cylinder: WP%04d hinge at %.1f %.1f %.1f cm, the drum shut at %.1f %.1f %.1f cm (the gun's frame)", TAG, gf().wp,
         g_pivot_l.x * 100.0f, g_pivot_l.y * 100.0f, g_pivot_l.z * 100.0f, g_drum_l.x * 100.0f, g_drum_l.y * 100.0f, g_drum_l.z * 100.0f);
}
// the hinge, its axis and the shut drum's place in ROOM space (where the controllers are)
bool geom_room(Vec3& p, Vec3& a, Vec3& d0) {
    Vec3 gp; Quat gq;
    if (!g_geom || !gun_pose(gp, gq)) return false;
    gq = qnorm(gq);
    return ghosthand::to_room_point(gp + rotate(gq, g_pivot_l), p) && ghosthand::to_room_dir(rotate(gq, g_axis_l), a) &&
           ghosthand::to_room_point(gp + rotate(gq, g_drum_l), d0);
}
float dot3(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross3(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
// the left controller's angle round the hinge from the shut drum, degrees, + = the way the drum opens
bool hand_angle(float& deg) {
    Vec3 p, a, d0;
    if (!geom_room(p, a, d0)) return false;
    const float al = std::sqrt(dot3(a, a));
    if (al < 1e-6f) return false;
    a = a * (1.0f / al);
    Vec3 r = d0 - p, v = bridge::hand_pos(bridge::LEFT) - p;
    r = r - a * dot3(r, a); v = v - a * dot3(v, a);
    const float th = std::atan2(dot3(a, cross3(r, v)), dot3(r, v)) * 180.0f / PI;
    deg = g_w->open_deg >= 0.0f ? th : -th;   // the crane turns by +open_deg about its own Z to open
    return true;
}
// LG + LT also take the drum at the place it sits SHUT (the front grip just ahead of it on the MGL), so it can be pushed shut from there
bool hand_at_shut_place() {
    Vec3 p, a, d0;
    return geom_room(p, a, d0) && dist(bridge::hand_pos(bridge::LEFT), d0) < cfg::CYLINDER_GRAB_M;
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
    learn_geom();
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
    g_geom = false;
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
    // LG + LT on the drum or at its shut place (the front grip), the hand empty: the drum is taken (either button may come last)
    float ang = 0.0f;
    if (!g_grab && lg && lt && (bridge::pressed(bridge::S_LTRIG) || bridge::pressed(bridge::S_LGRIP)) && !shells::in_hand() &&
        (hand_at_drum() || hand_at_shut_place()) && hand_angle(ang)) {
        g_grab = true; g_grab_ang = ang; g_grab_deg = g_deg;
        bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP * 0.5f, cfg::RELOAD_GRAB_BUZZ_SEC);
        LOGI("%s cylinder: WP%04d drum taken (LG + LT) at %.0f%% open: it follows the hand round its hinge", TAG, gf().wp,
             full > 0.0f ? g_deg / full * 100.0f : 0.0f);
    }
    if (g_grab && (!lg || !lt)) {
        g_grab = false;
        LOGI("%s cylinder: WP%04d drum let go at %.0f%% open%s", TAG, gf().wp, full > 0.0f ? g_deg / full * 100.0f : 0.0f,
             g_deg >= full ? " (clicked open)" : g_deg > 0.0f ? ": it stays there, the gun will not fire until it is shut" : " (shut)");
    }
    if (g_grab && full > 0.0f && hand_angle(ang)) {
        float d = ang - g_grab_ang;
        while (d > 180.0f) d -= 360.0f;
        while (d < -180.0f) d += 360.0f;
        float deg = g_grab_deg + d;
        if (deg > full || deg < 0.0f) {   // past either end: measure on from here, so the way back moves it at once
            deg = std::clamp(deg, 0.0f, full); g_grab_ang = ang; g_grab_deg = deg;
        }
        float u = deg / full;
        if (!g_want_open && u >= cfg::CYLINDER_CLICK) {
            g_want_open = true; u = 1.0f; g_grab_ang = ang; g_grab_deg = full;
            sfx::play("mag_grab", g_w->sfx, g_w->vol);
            bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
            LOGI("%s cylinder: WP%04d clicked OPEN (pulled up and out by the hand)", TAG, gf().wp);
        } else if (g_want_open && u <= 1.0f - cfg::CYLINDER_CLICK) {
            g_want_open = false; u = 0.0f; g_grab_ang = ang; g_grab_deg = 0.0f;
            sfx::play("mag_insert", g_w->sfx, g_w->vol);
            bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
            LOGI("%s cylinder: WP%04d clicked SHUT (pushed down and in by the hand)", TAG, gf().wp);
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
