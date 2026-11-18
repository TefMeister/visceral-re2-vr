// walls.cpp -- see walls.h. Ported from visceral-re8-vr src/walls.cpp (b141), 2026-11-18.
#include "walls.h"
#include "bridge.h"

#include <chrono>
#include <cmath>

namespace vn::walls {
namespace {
using namespace vn::bridge;
float g_log_t[2] = {-10.0f, -10.0f};
float now_s() { static const auto t0 = std::chrono::steady_clock::now(); return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count(); }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// not bridge::has(): it calls anything from 499 up "no value", and the frame count passes that in seconds (Village, 2026-11-18)
bool fresh(Slot stamp_slot) {
    const float stamp = view(stamp_slot), frame = view(S_FRAME);
    return std::isfinite(stamp) && std::isfinite(frame) && stamp >= 0.5f && stamp >= frame + 1.0f - (float)STALE_FRAMES;
}
void put3(Slot s, const float* v) { for (int i = 0; i < 3; ++i) put((Slot)(s + i), v[i]); }
void put4(Slot s, const float* v) { for (int i = 0; i < 4; ++i) put((Slot)(s + i), v[i]); }

bool plane_push(int side, float* pos, float& pushed) {
    if (!fresh(side == 0 ? S_WALL_L_ON : S_WALL_R_ON)) return false;
    const Vec3 p = view_vec3(side == 0 ? S_WALL_L_P : S_WALL_R_P);
    Vec3 n = view_vec3(side == 0 ? S_WALL_L_N : S_WALL_R_N);
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(n.x)) return false;
    const float nl = std::sqrt(dot(n, n));
    if (nl < 0.5f) return false;
    n = n * (1.0f / nl);
    const Vec3 tp{pos[0], pos[1], pos[2]};
    const float d = dot(tp - p, n);
    if (d >= 0.0f || d < -MAX_PUSH_M) return false;
    pos[0] -= n.x * d; pos[1] -= n.y * d; pos[2] -= n.z * d;
    pushed = -d;
    return true;
}
bool tilt(float* quat) {
    if (!fresh(S_WALL_TILT_ON)) return false;
    Quat dq{view(S_WALL_TILT_Q), view((Slot)(S_WALL_TILT_Q + 1)), view((Slot)(S_WALL_TILT_Q + 2)), view((Slot)(S_WALL_TILT_Q + 3))};
    if (!std::isfinite(dq.x) || !std::isfinite(dq.w) || std::fabs(dq.w) > 0.99999f) return false;
    const Quat r = qnorm(qmul(qnorm(dq), qnorm(Quat{quat[0], quat[1], quat[2], quat[3]})));
    quat[0] = r.x; quat[1] = r.y; quat[2] = r.z; quat[3] = r.w;
    return true;
}
} // namespace
} // namespace vn::walls

// Our REFramework places each hand from its controller each frame and asks this first (hand 0 left, 1 right; pos world
// x y z w, quat x y z w). Here the controller's hand is the input every time, so a turn put on is never put on twice.
extern "C" __declspec(dllexport) int visceral_hand_wall(int hand, float* pos, float* quat) {
    using namespace vn::walls;
    using namespace vn::bridge;
    if (!ON || pos == nullptr || quat == nullptr || !live() || (hand != 0 && hand != 1)) return 0;
    if (hand == 1) { put3(S_HW_RAW_R, pos); put4((Slot)(S_HW_RAW_R + 3), quat); }
    else put3(S_HW_RAW_L, pos);
    put(S_HW_STAMP, view(S_FRAME));
    float pushed = 0.0f;
    const bool moved = plane_push(hand, pos, pushed);
    const bool turned = hand == 1 && tilt(quat);
    if (hand == 1) { put3(S_HW_OUT_R, pos); put4((Slot)(S_HW_OUT_R + 3), quat); }
    if (!moved && !turned) return 0;
    const float t = now_s();
    if (t - g_log_t[hand] > 1.0f) {
        g_log_t[hand] = t;
        if (moved) LOGI("%s walls: the %s hand stopped at a wall (it would have gone %.0f cm in)", vn::TAG, hand == 0 ? "left" : "right", pushed * 100.0f);
        if (turned) LOGI("%s walls: the gun turned at the wrist, away from a wall", vn::TAG);
    }
    return 1;
}
