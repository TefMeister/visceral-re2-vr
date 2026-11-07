// magreport.cpp -- TEST TOOL, only in a VN_TOOLS build (never in a player build). Twice a second while a magazine is in the
// left hand: where it is against the magwell, as a room-space move of the left controller (metres). A test drive on the
// OpenXR Simulator moves the hand by exactly that and the magazine goes in (2026-11-07, layer 3 checks).
#include "tools.h"
#include "ghosthand.h"
#include "gun_frame.h"
#include "joints.h"
#include "magazine_internal.h"

namespace vn::tools {

using namespace joints;
using namespace guns::magazine::detail;

void magreport() {
    static float last = 0.0f;
    if (g_st != St::IN_HAND || g_w == nullptr || now_s() - last < 0.5f) return;
    last = now_s();
    Vec3 mp, ew, mr, er;
    Quat mq, eq;
    if (!hand_pose(mp, mq) || !guns::from_gun_local(g_w->exit, mq, ew, eq)) return;
    if (!ghosthand::to_room_point(mp, mr) || !ghosthand::to_room_point(ew, er)) return;
    const Vec3 d = er - mr;
    LOGI("%s magreport: move the left hand by %.4f %.4f %.4f (room, %.3f m) to the magwell", TAG, d.x, d.y, d.z, dist(er, mr));
}

} // namespace vn::tools
