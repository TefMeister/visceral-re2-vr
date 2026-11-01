// breech.cpp -- see breech.h. Joints found 2026-11-01 on the simulator: the game's own reload turns _01 40 deg (the barrel's
// hinge: turned 40 deg about its local x by hand, the barrel drops open), _06 50 deg (the latch), and moves the rounds _04,
// _100, _101; _100 sits in the chamber. vfx_muzzle1 is the muzzle.
#include "breech.h"
#include "bridge.h"
#include "common.h"
#include "joints.h"
#include "settings.h"
#include "sfx.h"
#include "weapons.h"

#include <cmath>

namespace vn::breech {
using namespace joints;
namespace {
constexpr const char* TAG = "[visceral-native]";


int g_wp = -2;
MO* g_gun = nullptr;
MO* g_wtf = nullptr;
MO* g_hinge = nullptr;
MO* g_muzzle = nullptr;
MO* g_chamber = nullptr;
Quat g_rest{0, 0, 0, 1};
bool g_have_rest = false;
bool g_open = false;
float g_swing = 0.0f, g_swing_t = -1.0f;   // 0 shut .. 1 open, eased
float g_last_pitch = 0.0f, g_last_t = -1.0f, g_peak = 0.0f, g_peak_t = 0.0f;
float g_cool = 0.0f;
// the pitch over the last BREAK_FLICK_WINDOW_SEC: the rate is taken across it, so one stepped frame is not a flick
constexpr int NHIST = 32;
float g_hist_t[NHIST]{}, g_hist_p[NHIST]{};
int g_hist_n = 0, g_hist_i = 0;
int g_logged_near = -1;

MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }

void set_open(bool open, const char* why) {
    if (open == g_open) return;
    g_open = open;
    g_cool = now_s() + cfg::BREAK_COOLDOWN_SEC;
    sfx::play(open ? "mag_grab" : "mag_insert", "grenade_launcher", 1.0f);   // the launcher folder's own sounds (no break-open sound of its own yet)
    bridge::rumble(bridge::LEFT, cfg::BREAK_BUZZ_AMP, cfg::BREAK_BUZZ_SEC);
    LOGI("%s breech: GM 79 %s (%s)", TAG, open ? "OPEN: rounds go in at the chamber" : "closed", why);
}

void resolve() {
    const int wp = weapons::current_id();
    if (wp != g_wp) {
        g_wp = wp;
        g_open = false; g_swing = 0.0f; g_have_rest = false; g_logged_near = -1;
    }
    g_gun = g_wtf = g_hinge = g_muzzle = g_chamber = nullptr;
    if (wp != cfg::BREAK_WP) return;
    g_gun = field_obj(component(player_go(), "app.ropeway.survivor.Equipment"), "<EquipWeapon>k__BackingField");
    g_wtf = call_ptr(call_ptr(g_gun, "get_GameObject"), "get_Transform");
    if (g_wtf == nullptr) return;
    g_hinge = joint_by_name(g_wtf, L"_01");
    g_muzzle = joint_by_name(g_wtf, L"vfx_muzzle1");
    g_chamber = joint_by_name(g_wtf, L"_100");
}

// the front grip: along the barrel from the hinge towards the muzzle
bool front_grip(Vec3& p) {
    Vec3 h, m;
    if (!get_v3(JOINT, g_hinge, "get_Position", h) || !get_v3(JOINT, g_muzzle, "get_Position", m)) return false;
    p = h + (m - h) * cfg::BREAK_FRONT_T;
    return true;
}
bool left_on_front(float& d) {
    Vec3 f, w;
    auto* wrist = joint_by_name(call_ptr(player_go(), "get_Transform"), L"l_arm_wrist");
    if (!front_grip(f) || !get_v3(JOINT, wrist, "get_Position", w)) return false;
    d = dist(f, w);
    return d <= cfg::BREAK_FRONT_M;
}
} // namespace

bool here() { return g_wp == cfg::BREAK_WP && g_hinge != nullptr; }
bool is_open() { return here() && g_open; }
bool blocks_fire() { return is_open(); }
bool loading_allowed() { return g_wp != cfg::BREAK_WP || g_open; }

bool port_local(float out[3]) {
    if (!here() || g_chamber == nullptr) return false;
    Vec3 c, wp;
    Quat wr;
    if (!get_v3(JOINT, g_chamber, "get_Position", c) || !get_v3(XFORM, g_wtf, "get_Position", wp) || !get_q(XFORM, g_wtf, "get_Rotation", wr)) return false;
    const Vec3 l = rotate(qinv(qnorm(wr)), c - wp);
    out[0] = l.x; out[1] = l.y; out[2] = l.z;
    return true;
}

void frame() {
    if (!cfg::BREAK_ON) return;
    resolve();
    if (!here() || !bridge::live()) return;
    const float now = now_s();
    // LG held + LT pressed with the left hand on the front grip: open, or shut
    float d = 99.0f;
    const bool on = left_on_front(d);
    if (on != (g_logged_near == 1)) {
        g_logged_near = on ? 1 : 0;
        if (on && bridge::held(bridge::S_LGRIP)) LOGI("%s breech: the left hand on the GM 79's front grip (%.3f m): LT %s it", TAG, d, g_open ? "shuts" : "opens");
    }
    if (on && bridge::held(bridge::S_LGRIP) && bridge::pressed(bridge::S_LTRIG) && now >= g_cool) {
        set_open(!g_open, "LG + LT on the front grip");
        return;
    }
    if (bridge::pressed(bridge::S_LTRIG) && bridge::held(bridge::S_LGRIP) && !on)
        LOGI("%s breech: LT with LG, but the left hand is %.3f m from the front grip (needs %.2f)", TAG, d, cfg::BREAK_FRONT_M);
    // the flick: the gun hand thrown up fast closes it (never opens it). Read from the right CONTROLLER, as Village does: the
    // drawn gun jumps thousands of deg/s on equipping and firing (simulator, 2026-11-01)
    {
        const Quat rq = bridge::right_rot();
        if (rq.x >= bridge::NO_VALUE * 0.9f || std::fabs(rq.w) + std::fabs(rq.x) + std::fabs(rq.y) + std::fabs(rq.z) < 0.5f) return;
        const Vec3 f = rotate(qnorm(rq), Vec3{0.0f, 0.0f, -1.0f});
        const float len = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
        const float pitch = len > 1e-4f ? std::asin(std::fmax(-1.0f, std::fmin(1.0f, f.y / len))) * 57.2958f : 0.0f;
        g_hist_t[g_hist_i] = now; g_hist_p[g_hist_i] = pitch; g_hist_i = (g_hist_i + 1) % NHIST; if (g_hist_n < NHIST) ++g_hist_n;
        int old = -1;   // the newest sample at least the window old
        for (int k = 1; k < g_hist_n; ++k) {
            const int j = (g_hist_i - 1 - k + NHIST) % NHIST;
            if (now - g_hist_t[j] >= cfg::BREAK_FLICK_WINDOW_SEC) { old = j; break; }
        }
        if (old >= 0 && now > g_hist_t[old]) {
            const float rate = (pitch - g_hist_p[old]) / (now - g_hist_t[old]);
            if (rate > g_peak) { g_peak = rate; }
            if (now - g_peak_t > 1.0f) {
                if (g_peak >= cfg::BREAK_FLICK_LOG_DEG_S) LOGI("%s breech: fastest upward swing this second %.0f deg/s (a flick needs %.0f)", TAG, g_peak, cfg::BREAK_FLICK_DEG_S);
                g_peak = 0.0f; g_peak_t = now;
            }
            if (g_open && rate >= cfg::BREAK_FLICK_DEG_S && now >= g_cool) set_open(false, "flicked up");
        }
        g_last_pitch = pitch;
        g_last_t = now;
    }
}

void pose_point() {
    if (!cfg::BREAK_ON || !here()) return;
    if (!g_have_rest) {
        if (!get_q(JOINT, g_hinge, "get_LocalRotation", g_rest)) return;
        g_have_rest = true;
    }
    const float now = now_s();
    const float dt = g_swing_t < 0.0f ? 0.0f : std::fmin(0.1f, std::fmax(0.0f, now - g_swing_t));
    g_swing_t = now;
    const float step = cfg::BREAK_SWING_SEC > 0.0f ? dt / cfg::BREAK_SWING_SEC : 1.0f;
    const float want = g_open ? 1.0f : 0.0f;
    g_swing = want > g_swing ? std::fmin(want, g_swing + step) : std::fmax(want, g_swing - step);
    if (g_swing <= 0.0f) return;   // shut: the game's own animation (the reload is ours, so it stays put)
    const float a = cfg::BREAK_OPEN_DEG * g_swing * 3.14159265f / 360.0f;
    const Quat turn{std::sin(a), 0.0f, 0.0f, std::cos(a)};
    const Quat q = qnorm(qmul(g_rest, turn));
    set_any(JOINT, g_hinge, "set_LocalRotation", &q);
}

} // namespace vn::breech
