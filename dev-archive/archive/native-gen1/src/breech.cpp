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
MO* g_case = nullptr;            // _101, the spent case / second round
// 2026-11-01 (Tefa, worn: "the inserted round doesn't show up in the barrel ... it has to travel with the barrel inside it"):
// the rounds' place in the barrel's frame, read while shut, written back each frame while it is open
struct Rel { Vec3 p; Quat q; bool ok; };
Rel g_rel_chamber{}, g_rel_case{};
bool g_hand_on = false;          // the left hand took the barrel at the front and LG is still held
Quat g_hand_rel{0, 0, 0, 1};     // the left wrist's turn in the barrel's frame when it took it
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
        g_open = false; g_swing = 0.0f; g_have_rest = false; g_logged_near = -1; g_hand_on = false;
        g_rel_chamber.ok = g_rel_case.ok = false;
    }
    g_gun = g_wtf = g_hinge = g_muzzle = g_chamber = nullptr;
    if (wp != cfg::BREAK_WP) return;
    g_gun = field_obj(component(player_go(), "app.ropeway.survivor.Equipment"), "<EquipWeapon>k__BackingField");
    g_wtf = call_ptr(call_ptr(g_gun, "get_GameObject"), "get_Transform");
    if (g_wtf == nullptr) return;
    g_hinge = joint_by_name(g_wtf, L"_01");
    g_muzzle = joint_by_name(g_wtf, L"vfx_muzzle1");
    g_chamber = joint_by_name(g_wtf, L"_100");
    g_case = joint_by_name(g_wtf, L"_101");
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

bool hand_socket(float p[3], float q[4]) {
    if (!cfg::BREAK_ON || !cfg::BREAK_HAND_RIDES || !here() || !g_hand_on || g_swing <= 0.0f || !bridge::held(bridge::S_LGRIP)) return false;
    Vec3 f, rp; Quat rq, hq;
    auto* rwrist = joint_by_name(call_ptr(player_go(), "get_Transform"), L"r_arm_wrist");
    if (!front_grip(f) || !get_v3(JOINT, rwrist, "get_Position", rp) || !get_q(JOINT, rwrist, "get_Rotation", rq) || !get_q(JOINT, g_hinge, "get_Rotation", hq)) return false;
    const Quat inv = qinv(qnorm(rq));
    const Vec3 l = rotate(inv, f - rp);
    const Quat lq = qnorm(qmul(inv, qmul(qnorm(hq), g_hand_rel)));
    p[0] = l.x; p[1] = l.y; p[2] = l.z; q[0] = lq.x; q[1] = lq.y; q[2] = lq.z; q[3] = lq.w;
    return true;
}

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
    if (!bridge::held(bridge::S_LGRIP)) g_hand_on = false;
    if (on && bridge::held(bridge::S_LGRIP) && bridge::pressed(bridge::S_LTRIG) && now >= g_cool) {
        set_open(!g_open, "LG + LT on the front grip");
        // the hand rides on the barrel from here: its turn in the barrel's frame now
        Quat hq, wq;
        auto* wrist = joint_by_name(call_ptr(player_go(), "get_Transform"), L"l_arm_wrist");
        if (get_q(JOINT, g_hinge, "get_Rotation", hq) && get_q(JOINT, wrist, "get_Rotation", wq)) {
            g_hand_rel = qnorm(qmul(qinv(qnorm(hq)), qnorm(wq)));
            g_hand_on = true;
        }
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
            // 2026-11-01 (Tefa, worn: "downward flick also closes it ... the flick is a little too easy"): the wrist's bounce back
            // up after a downward snap read as an upward flick; now the gun must also END higher than it was
            // BREAK_FLICK_RISE_SEC ago, without first dipping below where it started
            int back = -1;
            for (int k = 1; k < g_hist_n; ++k) {
                const int j = (g_hist_i - 1 - k + NHIST) % NHIST;
                if (now - g_hist_t[j] >= cfg::BREAK_FLICK_RISE_SEC) { back = j; break; }
            }
            float lowest = pitch;
            for (int k = 0; k < g_hist_n; ++k) {
                const int j = (g_hist_i - 1 - k + NHIST) % NHIST;
                if (now - g_hist_t[j] > cfg::BREAK_FLICK_RISE_SEC) break;
                lowest = std::fmin(lowest, g_hist_p[j]);
            }
            const bool rose = back >= 0 && pitch - g_hist_p[back] >= cfg::BREAK_FLICK_RISE_DEG && g_hist_p[back] - lowest <= cfg::BREAK_FLICK_DIP_DEG;
            if (g_open && rate >= cfg::BREAK_FLICK_DEG_S && rose && now >= g_cool) set_open(false, "flicked up");
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
    if (g_swing <= 0.0f) {   // shut: the game's own animation; read where the rounds sit in the barrel's frame
        Vec3 hp; Quat hq;
        if (get_v3(JOINT, g_hinge, "get_Position", hp) && get_q(JOINT, g_hinge, "get_Rotation", hq)) {
            const Quat inv = qinv(qnorm(hq));
            MO* js[2] = {g_chamber, g_case};
            Rel* rs[2] = {&g_rel_chamber, &g_rel_case};
            for (int i = 0; i < 2; ++i) {
                Vec3 jp; Quat jq;
                if (js[i] != nullptr && get_v3(JOINT, js[i], "get_Position", jp) && get_q(JOINT, js[i], "get_Rotation", jq))
                    *rs[i] = Rel{rotate(inv, jp - hp), qnorm(qmul(inv, qnorm(jq))), true};
            }
        }
        return;
    }
    const float a = cfg::BREAK_OPEN_DEG * g_swing * 3.14159265f / 360.0f;
    const Quat turn{std::sin(a), 0.0f, 0.0f, std::cos(a)};
    const Quat q = qnorm(qmul(g_rest, turn));
    set_any(JOINT, g_hinge, "set_LocalRotation", &q);
    if (cfg::BREAK_ROUNDS_FOLLOW) {   // the rounds ride in the barrel
        Vec3 hp; Quat hq;
        if (get_v3(JOINT, g_hinge, "get_Position", hp) && get_q(JOINT, g_hinge, "get_Rotation", hq)) {
            hq = qnorm(hq);
            MO* js[2] = {g_chamber, g_case};
            Rel* rs[2] = {&g_rel_chamber, &g_rel_case};
            for (int i = 0; i < 2; ++i) {
                if (js[i] == nullptr || !rs[i]->ok) continue;
                const Vec3 wp = hp + rotate(hq, rs[i]->p);
                const Quat wq = qnorm(qmul(hq, rs[i]->q));
                set_any(JOINT, js[i], "set_Position", &wp);
                set_any(JOINT, js[i], "set_Rotation", &wq);
            }
        }
    }
}

} // namespace vn::breech
