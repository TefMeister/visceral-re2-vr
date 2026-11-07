// rackreport.cpp -- TEST TOOL, only in a VN_TOOLS build (never in a player build). Twice a second while a gun with a slide or
// a pump is in hand: the room-space move that brings the drawn left wrist onto the slide / fore-end joint, and the barrel's
// direction in the room (a rack pulls the left hand back along it). For simulator drives (2026-11-07, layer 3 checks).
#include "tools.h"
#include "ghosthand.h"
#include "gun_frame.h"
#include "joints.h"
#include "rack.h"

namespace vn::tools {

using namespace joints;

void rackreport() {
    static float last = 0.0f;
    auto* sj = static_cast<MO*>(guns::rack::slide_joint());
    if (sj == nullptr || now_s() - last < 0.5f) return;
    last = now_s();
    Vec3 jp, hp, jr, hr, fwd;
    Quat gq;
    if (!get_v3(JOINT, sj, "get_Position", jp) || !get_v3(JOINT, guns::gf().lwrist, "get_Position", hp)) return;
    if (!ghosthand::to_room_point(jp, jr) || !ghosthand::to_room_point(hp, hr)) return;
    if (!get_q(XFORM, guns::gf().gun_tf, "get_Rotation", gq) || !ghosthand::to_room_dir(rotate(qnorm(gq), Vec3{0, 0, 1}), fwd)) return;
    const Vec3 d = jr - hr;
    LOGI("%s rackreport: move the left hand by %.4f %.4f %.4f (room, %.3f m) to the slide; barrel %.3f %.3f %.3f", TAG, d.x, d.y, d.z,
         dist(jr, hr), fwd.x, fwd.y, fwd.z);
}

} // namespace vn::tools
