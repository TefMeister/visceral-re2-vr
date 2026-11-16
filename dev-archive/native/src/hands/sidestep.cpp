// sidestep.cpp -- see sidestep.h.
#include "sidestep.h"
#include "bridge.h"
#include "common.h"
#include "game.h"
#include "joints.h"

#include <chrono>
#include <cmath>
#include <string>

namespace vn::sidestep {

namespace {
using clock_t_ = std::chrono::steady_clock;

bool g_moving = false;
float g_t = 0.0f;                 // seconds into the step
float g_done = 0.0f;              // the eased share of the distance already moved (0..1)
Vec3 g_dir{0, 0, 0};              // world, level, unit
Vec3 g_from{0, 0, 0};
bool g_inv_ours = false;          // we switched Invincible on (and so switch it off again)
clock_t_::time_point g_last_start{};
clock_t_::time_point g_last_frame{};
int g_logs = 0;

float smoothstep(float u) { u = u < 0 ? 0 : u > 1 ? 1 : u; return u * u * (3.0f - 2.0f * u); }

MO* player() {
    static const std::string pm_name = std::string(game::ns()) + "PlayerManager";
    return call_ptr(API::get()->get_managed_singleton(pm_name), "get_CurrentPlayer");
}
MO* hit_points(MO* pl) {
    static const std::string hp_name = std::string(game::ns()) + "HitPointController";
    return component(pl, hp_name.c_str());
}

void set_invincible(MO* pl, bool on) {
    auto* hp = hit_points(pl);
    if (hp == nullptr) return;
    if (on) {
        if (call_direct<bool>(hp, "get_Invincible", false)) { g_inv_ours = false; return; }   // the game's own: leave it
        call_direct<void*>(hp, "set_Invincible", nullptr, true);
        g_inv_ours = true;
    } else if (g_inv_ours) {
        call_direct<void*>(hp, "set_Invincible", nullptr, false);
        g_inv_ours = false;
    }
}

// the world direction of the step: the left stick turned by the game camera (the bridge Lua works it out), or straight back
bool step_dir(Vec3& out) {
    const float mag = bridge::view(bridge::S_LSTICK_MAG);
    if (bridge::has(mag) && mag >= STICK_DEADZONE) {
        const float x = bridge::view(bridge::S_LSTICK_DIR), z = bridge::view((bridge::Slot)(bridge::S_LSTICK_DIR + 1));
        if (bridge::has(x) && std::fabs(x) + std::fabs(z) > 1e-3f) { out = {x, 0.0f, z}; return true; }
    }
    const float fx = bridge::view(bridge::S_CAM_FWD), fz = bridge::view((bridge::Slot)(bridge::S_CAM_FWD + 1));
    if (!bridge::has(fx) || std::fabs(fx) + std::fabs(fz) < 1e-3f) return false;
    out = {-fx, 0.0f, -fz};
    return true;
}

bool allowed() {
    if (!bridge::live() || bridge::view(bridge::S_FP_USED) < 0.5f) return false;   // gameplay only
    if (bridge::view(bridge::S_LEFT_ON_GUN) > 0.5f) return false;                  // not with the left hand on the gun
    return true;
}

void finish(MO* pl, const char* why) {
    set_invincible(pl, false);
    g_moving = false;
    if (g_logs < 40) {
        ++g_logs;
        Vec3 p{};
        auto* tf = call_ptr(pl, "get_Transform");
        if (tf != nullptr && call_vec3(tf, "get_Position", p)) {
            const Vec3 d = p - g_from;
            LOGI("%s sidestep: %s: moved %.2f m (asked %.2f) in %.2f s", TAG, why, std::sqrt(d.x * d.x + d.z * d.z), DISTANCE_M, g_t);
        }
    }
}
} // namespace

bool owns_click() { return ON; }

void frame() {
    if (!ON) return;
    const auto now = clock_t_::now();
    float dt = std::chrono::duration<float>(now - g_last_frame).count();
    g_last_frame = now;
    if (dt <= 0.0f || dt > MAX_FRAME_S) dt = MAX_FRAME_S;

    MO* pl = player();
    if (pl == nullptr) { g_moving = false; g_inv_ours = false; return; }

    if (!g_moving) {
        if (!bridge::pressed(bridge::S_RCLICK)) return;
        if (!allowed()) { if (g_logs < 40) { ++g_logs; LOGI("%s sidestep: click ignored (not in play, or the left hand is on the gun)", TAG); } return; }
        if (std::chrono::duration<float>(now - g_last_start).count() < COOLDOWN_S) return;
        Vec3 dir;
        if (!step_dir(dir)) { if (g_logs < 40) { ++g_logs; LOGW("%s sidestep: no direction (the bridge sent none)", TAG); } return; }
        const float l = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        g_dir = {dir.x / l, 0.0f, dir.z / l};
        auto* tf = call_ptr(pl, "get_Transform");
        if (tf == nullptr || !call_vec3(tf, "get_Position", g_from)) return;
        g_moving = true;
        g_t = 0.0f;
        g_done = 0.0f;
        g_last_start = now;
        set_invincible(pl, true);
        bridge::rumble(bridge::LEFT, BUZZ_AMP, BUZZ_S);
        bridge::rumble(bridge::RIGHT, BUZZ_AMP, BUZZ_S);
        if (g_logs < 40) { ++g_logs; LOGI("%s sidestep: start, direction %.2f %.2f (%s), invincible %s", TAG, g_dir.x, g_dir.z,
                                         bridge::view(bridge::S_LSTICK_MAG) >= STICK_DEADZONE ? "the stick" : "back", g_inv_ours ? "on" : "already on (the game's)"); }
        return;   // the first nudge next frame: this frame's buttons are read before the game moves her
    }

    if (!allowed()) { finish(pl, "stopped early (play ended: menu, cutscene, grab or the left hand on the gun)"); return; }
    g_t += dt;
    if (g_t >= INVINCIBLE_S) set_invincible(pl, false);
    const float share = smoothstep(g_t / DURATION_S);
    const float step = (share - g_done) * DISTANCE_M;
    g_done = share;
    auto* tf = call_ptr(pl, "get_Transform");
    Vec3 p{};
    if (tf != nullptr && call_vec3(tf, "get_Position", p)) {
        p = {p.x + g_dir.x * step, p.y, p.z + g_dir.z * step};
        joints::set_any(joints::XFORM, tf, "set_Position", &p);
    }
    if (g_t >= DURATION_S) finish(pl, "done");
}

} // namespace vn::sidestep
