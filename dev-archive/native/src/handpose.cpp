// handpose.cpp -- see handpose.h.
#include "handpose.h"

#include <algorithm>

#include "handpose_data.h"
#include "bridge.h"
#include "joints.h"
#include "rack.h"
#include "reload.h"
#include "settings.h"

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
    if (go == nullptr) { g_factor = 0; g_pose = nullptr; return; }
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
    if (g_pose == nullptr) return;

    const float step = cfg::HANDPOSE_BLEND_SEC <= 0.0001f ? 1.0f : std::min(1.0f, dt / cfg::HANDPOSE_BLEND_SEC);
    g_factor += ((want ? 1.0f : 0.0f) - g_factor) * step;
    if (!want && g_factor < 0.001f) {
        LOGI("%s handpose: fingers back to the game's animation", TAG);
        g_factor = 0; g_pose = nullptr; return;
    }

    auto* tf = call_ptr(go, "get_Transform");
    for (int i = 0; i < kBoneCount; ++i) {
        auto* j = joint_by_name(tf, kBones[i]);
        if (j == nullptr) continue;
        Vec3 t{g_pose->a[i][0], g_pose->a[i][1], g_pose->a[i][2]};
        // b159: the finger sliders (Lua menu "Visceral: fingers on the slide") bend each finger on top of the pose
        const float fon = bridge::view(bridge::S_FING_ON);
        if (on_slide && fon > 0.5f && fon < 2.0f) {
            const int f = i < 3 ? 0 : i < 6 ? 1 : i < 10 ? 2 : i < 14 ? 3 : 4;   // index middle ring little thumb (kBones order)
            const bool first = i == 0 || i == 3 || i == 6 || i == 10 || i == 14;
            t.z += bridge::view(static_cast<bridge::Slot>(bridge::S_FING + f));
            if (first) t.y += bridge::view(static_cast<bridge::Slot>(bridge::S_FING + 5 + f));
        }
        if (g_factor < 0.9999f) {
            Vec3 cur{};
            if (!get_v3(JOINT, j, "get_LocalEulerAngle", cur)) continue;
            t = Vec3{wrap(cur.x + wrap(t.x - cur.x) * g_factor), wrap(cur.y + wrap(t.y - cur.y) * g_factor),
                     wrap(cur.z + wrap(t.z - cur.z) * g_factor)};
        }
        Vec3 before{};
        const bool proof = !g_logged_proof && g_factor >= 0.9999f && i == 1;   // index finger, once per pose
        if (proof) get_v3(JOINT, j, "get_LocalEulerAngle", before);
        set_any(JOINT, j, "set_LocalEulerAngle", &t);
        if (proof) {
            Vec3 after{};
            get_v3(JOINT, j, "get_LocalEulerAngle", after);
            LOGI("%s handpose: proof l_hand_index_1 curl %.3f -> wrote %.3f -> reads %.3f", TAG, before.z, t.z, after.z);
            g_logged_proof = true;
        }
    }
}

} // namespace vn::handpose
