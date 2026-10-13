// handpose.cpp -- see handpose.h.
#include "handpose.h"

#include <algorithm>

#include "handpose_data.h"
#include "bridge.h"
#include "joints.h"
#include "rack.h"
#include "reload.h"
#include "settings.h"
#include "tune.h"
#include "weapons.h"

namespace vn::handpose {
using namespace vn::joints;

namespace {
const FingerPose* find_pose(Ctx ctx, int wp, bool claire) {
    const FingerPose* other = nullptr;
    for (const auto& p : kPoses) {
        if (p.ctx != ctx || p.wp != wp) continue;
        if (p.claire == claire) return &p;
        other = &p;   // the other character's pose for the same gun, if this one has none
    }
    return other;
}

float g_factor = 0.0f;                  // 0 = the game's fingers, 1 = the saved pose
float g_last_t = -1.0f;
const FingerPose* g_pose = nullptr;     // the pose blending in or out
bool g_logged_proof = false;

float wrap(float a) {                   // radians, to -pi..pi
    const float pi = 3.14159265f;
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    return a;
}
} // namespace

// b172: the layers on top of the game's own fingers, each faded in and out on its own:
//   free  -- "left hand on its own" sliders (no left button held, not near a gun with a slide / pump)
//   open  -- the hand opening as it nears the slide / fore-end (built-in OPEN_FINGER_CURL + the "opening" sliders)
//   grip  -- the saved grip / magazine pose (absolute), which curls the open hand closed over HANDPOSE_BLEND_SEC
// The game's animation rewrites the fingers every frame; when it has not (our write still stands), the game's last
// value is kept, so an offset is never added twice.
namespace {
float g_open = 0.0f, g_open_h = 0.0f, g_free = 0.0f;   // b173: g_open_h = opening for the handle
Vec3 g_base[kBoneCount]{}, g_wrote[kBoneCount]{};
bool g_have[kBoneCount]{};
int g_open_logged = 0;   // b173: 0 none, 1 slide, 2 handle
bool g_free_logged = false;

float approach(float cur, float target, float dt, float sec) {
    const float step = sec <= 0.0001f ? 1.0f : std::min(1.0f, dt / sec);
    return cur + (target - cur) * step;
}
Vec3 mix(Vec3 a, Vec3 b, float f) {
    return Vec3{wrap(a.x + wrap(b.x - a.x) * f), wrap(a.y + wrap(b.y - a.y) * f), wrap(a.z + wrap(b.z - a.z) * f)};
}
int finger_of(int i) { return i < 3 ? 0 : i < 6 ? 1 : i < 10 ? 2 : i < 14 ? 3 : 4; }   // index middle ring little thumb (kBones order)
} // namespace

void point() {
    if (!cfg::HANDPOSE_ON) return;
    const bool holding = reload::mag_in_hand() || reload::shell_in_hand();
    const bool on_slide = !holding && (rack::active() || rack::grip_pose());   // b154/b155: the hand on the slide / pump
    const bool want = holding || on_slide;
    const Ctx ctx = holding ? Ctx::MAG : Ctx::SLIDE;
    const float now = now_s();
    const float dt = (g_last_t < 0 || now - g_last_t > 0.25f) ? 1.0f / 60 : now - g_last_t;
    g_last_t = now;

    auto* go = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    if (go == nullptr) { g_factor = 0; g_pose = nullptr; g_open = g_open_h = g_free = 0; return; }
    if (want) {
        const bool claire = read_string(call_ptr(go, "get_Name")).rfind("pl1", 0) == 0;
        const int wp = holding ? reload::wp_now() : rack::wp_now();
        const FingerPose* p = find_pose(cfg::HANDPOSE_SLIDE_USES_MAG ? Ctx::MAG : ctx, wp, claire);
        if (p != g_pose && p != nullptr) {
            LOGI("%s handpose: %s fingers on for WP%04d (%s pose)", TAG, ctx == Ctx::MAG ? "magazine-hold" : "slide-grip",
                 p->wp, p->claire ? "Claire's" : "Leon's");
            g_logged_proof = false;
        } else if (p == nullptr && g_pose == nullptr && g_factor == 0) {
            static int last_missing = -2;
            if (last_missing != wp) LOGI("%s handpose: no saved %s pose for WP%04d", TAG, ctx == Ctx::MAG ? "magazine" : "slide", wp);
            last_missing = wp;
        }
        if (p != nullptr) g_pose = p;
    }

    // b172: open near the slide / fore-end (also while gripping, underneath, so a let-go opens first); free elsewhere
    // b173: only inside the LG grab zone (the dock's own test), and a second shape near the gun hand, for the handle
    float ds = 99.0f, dh = 99.0f;
    const bool zs = !holding && rack::in_grab_zone(ds);
    if (!holding && weapons::current_id() >= 0) {
        auto* tfp = call_ptr(go, "get_Transform");
        auto* lw = joint_by_name(tfp, L"l_arm_wrist");
        auto* rw = joint_by_name(tfp, L"r_arm_wrist");
        Vec3 lp, rp;
        if (lw != nullptr && rw != nullptr && get_v3(JOINT, lw, "get_Position", lp) && get_v3(JOINT, rw, "get_Position", rp)) dh = dist(lp, rp);
    }
    const bool zh = !holding && dh <= cfg::OPEN_HANDLE_NEAR_M;
    const int spot = zs && (!zh || ds <= dh) ? 1 : zh ? 2 : 0;   // the nearer of the two when both
    const bool near = spot != 0 || (!holding && rack::hand_dist_m() <= cfg::OPEN_HAND_NEAR_M);
    const bool buttons = bridge::held(bridge::S_LGRIP) || bridge::held(bridge::S_LTRIG) || bridge::held(bridge::S_LA) || bridge::held(bridge::S_LB);
    const bool free = !want && !near && !buttons;
    g_open = approach(g_open, spot == 1 ? 1.0f : 0.0f, dt, cfg::OPEN_HAND_BLEND_SEC);
    g_open_h = approach(g_open_h, spot == 2 ? 1.0f : 0.0f, dt, cfg::OPEN_HAND_BLEND_SEC);
    g_free = approach(g_free, free ? 1.0f : 0.0f, dt, cfg::FREE_HAND_BLEND_SEC);
    if (g_open < 0.001f) g_open = 0.0f;
    if (g_open_h < 0.001f) g_open_h = 0.0f;
    if (g_free < 0.001f) g_free = 0.0f;
    if (spot != g_open_logged) {
        if (spot == 1) LOGI("%s handpose: left hand opening for the slide / pump (%.3f m, inside the LG zone)", TAG, ds);
        else if (spot == 2) LOGI("%s handpose: left hand opening for the handle (%.3f m from the gun hand)", TAG, dh);
        else LOGI("%s handpose: left hand no longer opening", TAG);
        g_open_logged = spot;
    }
    if (free != g_free_logged) { LOGI("%s handpose: left hand %s", TAG, free ? "on its own (its own sliders)" : "busy (grab / button / near the gun)"); g_free_logged = free; }
    tune::free_wrist(g_free);

    if (g_pose != nullptr) {
        const float step = cfg::HANDPOSE_BLEND_SEC <= 0.0001f ? 1.0f : std::min(1.0f, dt / cfg::HANDPOSE_BLEND_SEC);
        g_factor += ((want ? 1.0f : 0.0f) - g_factor) * step;
        if (!want && g_factor < 0.001f) {
            LOGI("%s handpose: fingers back to the game's animation", TAG);
            g_factor = 0; g_pose = nullptr;
        }
    }
    if (g_pose == nullptr && g_open == 0.0f && g_open_h == 0.0f && g_free == 0.0f) { for (auto& h : g_have) h = false; return; }

    auto* tf = call_ptr(go, "get_Transform");
    for (int i = 0; i < kBoneCount; ++i) {
        auto* j = joint_by_name(tf, kBones[i]);
        if (j == nullptr) continue;
        Vec3 cur{};
        if (!get_v3(JOINT, j, "get_LocalEulerAngle", cur)) continue;
        // the game's own value this frame: if our last write still stands, the game did not rewrite it
        const bool ours = g_have[i] && std::fabs(cur.x - g_wrote[i].x) + std::fabs(cur.y - g_wrote[i].y) + std::fabs(cur.z - g_wrote[i].z) < 1e-5f;
        if (!ours) g_base[i] = cur;
        const Vec3 game = g_base[i];
        Vec3 t = game;
        if (g_free > 0.0f) t = mix(t, game + tune::finger(tune::FREE, i), g_free);
        if (g_open > 0.0f) {
            const bool first = i == 0 || i == 3 || i == 6 || i == 10 || i == 14;
            Vec3 o = game + tune::finger(tune::OPEN, i);
            o.z += cfg::OPEN_FINGER_CURL[finger_of(i)] * (first ? 0.5f : 1.0f);
            t = mix(t, o, g_open);
        }
        if (g_open_h > 0.0f) {   // b173: its own shape; the same built-in opening to start from
            const bool first = i == 0 || i == 3 || i == 6 || i == 10 || i == 14;
            Vec3 o = game + tune::finger(tune::OPEN_HANDLE, i);
            o.z += cfg::OPEN_FINGER_CURL[finger_of(i)] * (first ? 0.5f : 1.0f);
            t = mix(t, o, g_open_h);
        }
        if (g_pose != nullptr && g_factor > 0.0f) {
            Vec3 g{g_pose->a[i][0], g_pose->a[i][1], g_pose->a[i][2]};
            // b159/b164: Tefa's finger numbers on the slide grip, built in (each whole finger curled, its base joint spread)
            if (on_slide) {
                const bool first = i == 0 || i == 3 || i == 6 || i == 10 || i == 14;
                g.z += cfg::SLIDE_FINGER_CURL[finger_of(i)];
                if (first) g.y += cfg::SLIDE_FINGER_SPREAD[finger_of(i)];
            }
            // b164: every joint's own sliders for this hold, this gun, this character (tune.h)
            g = g + tune::finger(holding ? (reload::shell_in_hand() ? tune::SHELL : tune::MAG) : tune::SLIDE, i);
            t = mix(t, g, g_factor);
        }
        Vec3 before{};
        const bool proof = !g_logged_proof && g_factor >= 0.9999f && i == 1;   // index finger, once per pose
        if (proof) before = cur;
        set_any(JOINT, j, "set_LocalEulerAngle", &t);
        Vec3 back{};
        g_have[i] = get_v3(JOINT, j, "get_LocalEulerAngle", back);
        if (g_have[i]) g_wrote[i] = back;
        if (proof) {
            LOGI("%s handpose: proof l_hand_index_1 curl %.3f -> wrote %.3f -> reads %.3f", TAG, before.z, t.z, back.z);
            g_logged_proof = true;
        }
    }
}

} // namespace vn::handpose
