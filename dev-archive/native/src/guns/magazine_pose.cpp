// magazine_pose.cpp -- where the magazine is drawn in each state, and where the left hand goes to hold it.
// The magazine joint's LOCAL pose is written after the gun's own animation; a joint's world pose is never read back the
// same frame we wrote it (the gun transform is used instead).
#include "magazine_internal.h"
#include "gun_frame.h"
#include "joints.h"
#include "settings.h"

#include <cmath>

namespace vn::guns::magazine::detail {

using namespace joints;

namespace {
void write_local(Vec3 p, Quat q) {
    set_any(JOINT, g_joint, "set_LocalPosition", &p);
    set_any(JOINT, g_joint, "set_LocalRotation", &q);
}
void write_world(Vec3 p, Quat q) {
    set_any(JOINT, g_joint, "set_Position", &p);
    set_any(JOINT, g_joint, "set_Rotation", &q);
}
const Hold& hold() { return gf().claire ? g_w->claire : g_w->leon; }
Vec3 inserting(float u) {   // on the way in: from just out of the magwell up to its seat
    return Vec3{g_rest_pos.x, g_w->exit.y + (g_rest_pos.y - g_w->exit.y) * u, g_w->exit.z + (g_rest_pos.z - g_w->exit.z) * u};
}
} // namespace

void write_scale(Vec3 s) { set_any(JOINT, g_joint, "set_LocalScale", &s); }

bool hand_pose(Vec3& pos, Quat& rot) {
    Vec3 hp;
    Quat hr;
    if (!get_v3(JOINT, gf().lwrist, "get_Position", hp) || !get_q(JOINT, gf().lwrist, "get_Rotation", hr)) return false;
    const Hold& h = hold();
    hr = qnorm(hr);
    pos = hp + rotate(hr, Vec3{h.ox, h.oy, h.oz});
    rot = qnorm(qmul(hr, ypr_deg(h.yaw, h.pitch, h.roll)));
    return true;
}

void pose_joint() {
    if (g_joint == nullptr || g_w == nullptr) return;
    const float t = now_s() - g_t0;
    switch (g_st) {
    case St::SEATED:
        if (g_scale_restore) { write_scale(g_rest_scale); g_scale_restore = false; }
        return;
    case St::SLIDE_OUT:
        write_local(lerp(g_slide_from, g_w->exit, ease(t / cfg::RELOAD_SLIDE_SEC)), g_rest_rot);
        return;
    case St::FALL:
    case St::HAND_FALL: {
        const float u = std::fmin(1.0f, t / cfg::RELOAD_FALL_SEC);
        write_world(g_fall_from - Vec3{0, cfg::RELOAD_FALL_M * u * u, 0}, g_fall_rot);
        return;
    }
    case St::OUT:
        write_scale(Vec3{0, 0, 0});
        write_local(g_rest_pos, g_rest_rot);
        return;
    case St::IN_HAND: {
        Vec3 wp, lp;
        Quat wr, lr;
        if (!hand_pose(wp, wr) || !to_gun_local(wp, wr, lp, &lr)) return;
        write_scale(g_rest_scale);
        write_local(lp, lr);
        return;
    }
    case St::INSERT:
        write_scale(g_rest_scale);
        write_local(inserting(ease(t / cfg::RELOAD_INSERT_SEC)), g_rest_rot);
        return;
    }
}

bool mag_local_now(Vec3& p, Quat& q) {
    const float t = now_s() - g_t0;
    switch (g_st) {
    case St::SEATED:
        if (g_joint == nullptr || !get_v3(JOINT, g_joint, "get_LocalPosition", p) || !get_q(JOINT, g_joint, "get_LocalRotation", q)) return false;
        q = qnorm(q);
        return true;
    case St::SLIDE_OUT:
        p = lerp(g_slide_from, g_w->exit, ease(t / cfg::RELOAD_SLIDE_SEC));
        q = g_rest_rot;
        return true;
    case St::INSERT:
        p = inserting(ease(t / cfg::RELOAD_INSERT_SEC));
        q = g_rest_rot;
        return true;
    default:
        return false;
    }
}

// the wrist that holds the magazine standing at (lp, lq): the in-hand hold undone (magazine = wrist * hold), then moved
// `down` x MAG_DOCK_DOWN_M toward where the magazine comes out, so the hand holds its bottom, clear of the right hand
bool dock_wrist(Vec3 lp, Quat lq, float down, Vec3& wp, Quat& wq) {
    Vec3 mp, a, b;
    Quat mq, qa, qb;
    const Vec3 rest = g_st == St::SEATED ? lp : g_rest_pos;
    if (!from_gun_local(lp, lq, mp, mq) || !from_gun_local(rest, lq, a, qa) || !from_gun_local(g_w->exit, lq, b, qb)) return false;
    const float len = dist(a, b);
    const Vec3 out = len > 1e-4f ? (b - a) * (1.0f / len) : Vec3{0, 0, 0};
    const Hold& h = hold();
    wq = qnorm(qmul(mq, qinv(ypr_deg(h.yaw, h.pitch, h.roll))));
    wp = mp - rotate(wq, Vec3{h.ox, h.oy, h.oz}) + out * (cfg::MAG_DOCK_DOWN_M * down);
    return true;
}

float hand_to_dock() {
    Vec3 lp, wp, hp;
    Quat lq, wq;
    if (gf().lwrist == nullptr || !mag_local_now(lp, lq) || !dock_wrist(lp, lq, 1.0f, wp, wq) || !get_v3(JOINT, gf().lwrist, "get_Position", hp)) return 99.0f;
    return dist(wp, hp);
}

} // namespace vn::guns::magazine::detail
