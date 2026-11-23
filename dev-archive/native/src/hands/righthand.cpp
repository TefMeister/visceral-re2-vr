// righthand.cpp -- see righthand.h.
#include "righthand.h"
#include "bridge.h"
#include "common.h"
#include "gun_frame.h"
#include "joints.h"

#include <cmath>

namespace vn::righthand {

using namespace joints;

void begin_render() {
    static float k = 0.0f, last = -1.0f;
    static MO* tf = nullptr;
    static MO* jt[3] = {};
    static Quat base[3] = {};          // the finger as it stood at this frame's first call: BeginRendering comes twice a frame on
    static float base_frame = -1.0f;   // Two Frame Sequential, and bending what was already bent doubled the pull (Village, measured)
    static bool said = false;
    const auto& g = guns::gf();
    const float t = now_s(), dt = last < 0.0f ? 0.0f : std::fmin(0.1f, t - last);
    last = t;
    const bool want = TRIGGER_PULL_ON && bridge::live() && g.gun != nullptr && bridge::held(bridge::S_RTRIG);
    const float step = TRIGGER_PULL_S > 0.0f ? dt / TRIGGER_PULL_S : 1.0f;
    k = want ? std::fmin(1.0f, k + step) : std::fmax(0.0f, k - step);
    if (k <= 0.0f || g.player == nullptr) return;
    auto* body = call_ptr(g.player, "get_Transform");
    if (body == nullptr) return;
    if (body != tf) { tf = body; for (auto& j : jt) j = nullptr; }
    const float u = ease(k);
    const float frame = bridge::view(bridge::S_FRAME);
    const bool fresh = frame != base_frame;
    base_frame = frame;
    int found = 0;
    for (int i = 0; i < 3; ++i) {
        if (jt[i] == nullptr) jt[i] = joint_by_name(tf, INDEX_JOINTS[i]);
        Quat cur;
        if (jt[i] == nullptr || !get_q(JOINT, jt[i], "get_LocalRotation", cur)) continue;
        ++found;
        if (fresh) base[i] = qnorm(cur);
        const float h = TRIGGER_PULL_SIGN * TRIGGER_PULL_DEG[i] * u * 3.14159265f / 360.0f;
        Quat turn{0, 0, 0, std::cos(h)};
        (TRIGGER_PULL_AXIS == 0 ? turn.x : TRIGGER_PULL_AXIS == 1 ? turn.y : turn.z) = std::sin(h);
        Quat q = qnorm(qmul(base[i], turn));
        set_any(JOINT, jt[i], "set_LocalRotation", &q);
    }
    if (!said) { said = true; LOGI("%s right hand: the trigger finger bends on RT (%d of 3 index joints found)", TAG, found); }
}

}  // namespace vn::righthand
