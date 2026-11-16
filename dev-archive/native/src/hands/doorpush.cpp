// doorpush.cpp -- see doorpush.h.
#include "doorpush.h"
#include "bridge.h"
#include "common.h"
#include "game.h"
#include "joints.h"

#include <chrono>
#include <cmath>
#include <string>

namespace vn::doorpush {

namespace {
using namespace joints;
using clock_t_ = std::chrono::steady_clock;
constexpr float DEG = 3.14159265f / 180.0f;

struct Door {
    MO* go = nullptr;
    MO* tf = nullptr;
    MO* hinge = nullptr;
    bool active = false;      // the game has it open
    float drawn = 0.0f;       // our angle, degrees, 0..MAX_DEG
    float game_prev = 0.0f;
    float sign = 0.0f;        // which way the game turns the hinge to open it (+1 / -1; 0 = not seen yet)
    float hand_prev[2] = {0, 0};
    bool hand_prev_ok[2] = {false, false};
};
Door g_door;
clock_t_::time_point g_last_scan{};
clock_t_::time_point g_last_touch{};
int g_pulse = 0;
int g_logs = 0;
float g_log_t = 0.0f;

float now_s() { static const auto t0 = clock_t_::now(); return std::chrono::duration<float>(clock_t_::now() - t0).count(); }
void say(const char* fmt, float a = 0, float b = 0, float c = 0, float d = 0) {
    if (g_logs >= 80) return;
    ++g_logs;
    char line[240];
    std::snprintf(line, sizeof line, fmt, a, b, c, d);
    LOGI("%s doorpush: %s", TAG, line);
}

MO* player() { return call_ptr(API::get()->get_managed_singleton(game::type("PlayerManager").c_str()), "get_CurrentPlayer"); }

MO* scene() {
    static API::Method* cur = API::get()->tdb()->find_method("via.SceneManager", "get_CurrentScene");
    void* sm = API::get()->get_native_singleton("via.SceneManager");
    if (cur == nullptr || sm == nullptr) return nullptr;
    return (MO*)cur->call<void*>(API::get()->get_vm_context(), sm);
}

float yaw_of_local(MO* joint) {   // the joint's local turn about Y, degrees (the hinge turns only about Y)
    Quat q;
    if (!get_q(JOINT, joint, "get_LocalRotation", q)) return 0.0f;
    return 2.0f * std::atan2(q.y, q.w) / DEG;
}
void set_local_yaw(MO* joint, float deg) {
    const float a = deg * DEG * 0.5f;
    const Quat q{0.0f, std::sin(a), 0.0f, std::cos(a)};
    set_any(JOINT, joint, "set_LocalRotation", &q);
}

// a world point's angle around the hinge, degrees, measured from the shut leaf in the hinge's turning direction (the
// game's sign; +1 before it is known), and its distance from the hinge on the level
bool angle_of(const Door& d, Vec3 p, float& deg, float& r) {
    Vec3 h;
    Quat q;
    if (!get_v3(JOINT, d.hinge, "get_Position", h) || !get_q(XFORM, d.tf, "get_Rotation", q)) return false;
    const Vec3 l = rotate(qinv(qnorm(q)), p - h);
    r = std::sqrt(l.x * l.x + l.z * l.z);
    const float s = LEAF_SIDE;
    // the leaf at local yaw t lies along (s cos t, 0, -s sin t): the point's own t
    const float t = std::atan2(-l.z * s, l.x * s) / DEG;
    deg = t * (d.sign != 0.0f ? d.sign : 1.0f);
    return true;
}

bool hand_world(int i, Vec3& out) {
    auto* body = call_ptr(player(), "get_Transform");
    auto* w = joint_by_name(body, i == 0 ? L"l_arm_wrist" : L"r_arm_wrist");
    return get_v3(JOINT, w, "get_Position", out);
}

void scan() {
    MO* pl = player();
    Vec3 pp;
    if (pl == nullptr || !get_v3(XFORM, call_ptr(pl, "get_Transform"), "get_Position", pp)) return;
    auto* sc = scene();
    if (sc == nullptr) { say("no scene: nothing to push"); return; }
    // the doors' own parts: RE3's carry GimmickOptionDoorSettings, RE2's gimmick.action.GimmickDoor (read live 2026-11-16)
    MO* best = nullptr;
    float best_d = SCAN_M;
    int n = 0;
    for (const char* kind : {"GimmickOptionDoorSettings", "gimmick.action.GimmickDoor"}) {
        auto* rt = API::get()->typeof(game::type(kind).c_str());
        if (rt == nullptr) continue;
        MO* list = call_ptr(sc, "findComponents(System.Type)", {rt});
        const int k = call_direct<int>(list, "get_Count", 0);
        n += k;
        for (int i = 0; i < k; ++i) {
            MO* c = call_ptr(list, "get_Item", {(void*)(intptr_t)i});
            MO* go = call_ptr(c, "get_GameObject");
            Vec3 p;
            if (go == nullptr || !get_v3(XFORM, call_ptr(go, "get_Transform"), "get_Position", p)) continue;
            if (std::fabs(p.y - pp.y) > SCAN_UP_M) continue;   // a door on another floor (RE2's nearest at the start was 4.4 m up)
            const float dd = std::sqrt((p.x - pp.x) * (p.x - pp.x) + (p.z - pp.z) * (p.z - pp.z));
            if (dd < best_d) { best_d = dd; best = go; }
        }
    }
    if (best == g_door.go) return;
    if (g_door.active && g_door.hinge != nullptr) return;   // keep the one we are drawing until the game closes it
    g_door = Door{};
    if (best == nullptr) return;
    g_door.go = best;
    g_door.tf = call_ptr(best, "get_Transform");
    g_door.hinge = joint_by_name(g_door.tf, HINGE);
    if (g_door.hinge == nullptr) { say("the nearest door has no hinge joint _00: left alone"); g_door = Door{}; return; }
    say("nearest door %.1f m away (of %.0f doors in the scene)", best_d, (float)n);
}
} // namespace

bool action_pulse() { return g_pulse > 0; }

void frame() {
    if (g_pulse > 0) --g_pulse;
    if (!ON || !bridge::live()) return;
    const auto now = clock_t_::now();
    if (std::chrono::duration<float>(now - g_last_scan).count() >= SCAN_S) { g_last_scan = now; scan(); }
    if (!TOUCH_OPENS || g_door.hinge == nullptr || g_door.active) return;
    if (std::chrono::duration<float>(now - g_last_touch).count() < TOUCH_COOLDOWN_S) return;
    // the hand pressing on the shut leaf, moving into it: A for the game
    for (int i = 0; i < 2; ++i) {
        Vec3 hp;
        float a, r;
        if (!hand_world(i, hp) || !angle_of(g_door, hp, a, r)) { g_door.hand_prev_ok[i] = false; continue; }
        const float m = std::asin(std::fmin(1.0f, HAND_R / std::fmax(r, HAND_R))) / DEG;
        const bool touching = r >= 0.12f && r <= LEAF_M && std::fabs(a) <= m;
        const bool closing_in = g_door.hand_prev_ok[i] && std::fabs(a) < std::fabs(g_door.hand_prev[i]);
        g_door.hand_prev[i] = a;
        g_door.hand_prev_ok[i] = true;
        if (touching && closing_in) {
            g_pulse = TOUCH_PULSE_FRAMES;
            g_last_touch = now;
            say("a hand on the shut door (%.2f m from the hinge): A for the game", r);
            break;
        }
    }
}

void after_motion() {
    if (!ON || g_door.hinge == nullptr) return;
    const float raw = yaw_of_local(g_door.hinge);   // the game's own, just written by the motion
    const float game = std::fabs(raw);
    if (!g_door.active) {
        if (game > OPEN_SEEN_DEG) {
            g_door.active = true;
            g_door.sign = raw < 0.0f ? -1.0f : 1.0f;
            g_door.drawn = INITIAL_DEG;
            g_door.hand_prev_ok[0] = g_door.hand_prev_ok[1] = false;
            say("the game opened the door (it turns %.0f): drawn at %.0f deg, the hands and the body push it from here", g_door.sign, INITIAL_DEG);
        } else {
            g_door.game_prev = game;
            return;
        }
    }
    // the game closing it (its angle falling, under ours): ours goes down with it; shut = done
    if (game < g_door.game_prev - 0.05f && game < g_door.drawn) g_door.drawn = game;
    if (game < 0.2f && g_door.game_prev < 0.2f) {
        say("the game shut the door: ours too");
        g_door.active = false;
        g_door.drawn = 0.0f;
        g_door.game_prev = game;
        set_local_yaw(g_door.hinge, raw);
        return;
    }
    // the hands: a hand that crossed the leaf since last frame, or is on it, pushes it to just past itself
    for (int i = 0; i < 2; ++i) {
        Vec3 hp;
        float a, r;
        if (!hand_world(i, hp) || !angle_of(g_door, hp, a, r) || r < 0.12f || r > LEAF_M + HAND_R) { g_door.hand_prev_ok[i] = false; continue; }
        const float m = std::asin(std::fmin(1.0f, HAND_R / r)) / DEG;
        const bool crossed = g_door.hand_prev_ok[i] && g_door.hand_prev[i] <= g_door.drawn + m && a + m > g_door.drawn;
        const bool on_it = std::fabs(a - g_door.drawn) <= m;
        if ((crossed || on_it) && a + m > g_door.drawn && a + m < MAX_DEG + 20.0f) g_door.drawn = std::fmin(MAX_DEG, a + m);
        g_door.hand_prev[i] = a;
        g_door.hand_prev_ok[i] = true;
    }
    // the body: never drawn through the player standing in the doorway
    if (auto* pl = player()) {
        Vec3 pp;
        float a, r;
        if (get_v3(XFORM, call_ptr(pl, "get_Transform"), "get_Position", pp) && angle_of(g_door, pp, a, r) && r > BODY_R && r < LEAF_M + BODY_R) {
            const float m = std::asin(std::fmin(1.0f, BODY_R / r)) / DEG;
            if (a + m > g_door.drawn && a - m < MAX_DEG) g_door.drawn = std::fmin(MAX_DEG, a + m);
        }
    }
    g_door.game_prev = game;
    set_local_yaw(g_door.hinge, g_door.sign * g_door.drawn);
    const float t = now_s();
    if (t - g_log_t > 1.0f) { g_log_t = t; say("drawn %.1f deg, the game's %.1f", g_door.drawn, game); }
}

void render_point() {
    if (!ON || !g_door.active || g_door.hinge == nullptr) return;
    set_local_yaw(g_door.hinge, g_door.sign * g_door.drawn);
}

} // namespace vn::doorpush
