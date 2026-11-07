// shellreport.cpp -- TEST TOOL, only in a VN_TOOLS build (never in a player build). Twice a second while a pump shotgun is in
// hand: the room-space move that brings a shell held in the left hand to the loading port. For simulator drives
// (2026-11-07, layer 3 checks).
#include "tools.h"
#include "ghosthand.h"
#include "gun_data.h"
#include "gun_frame.h"
#include "joints.h"
#include "settings.h"

namespace vn::tools {

using namespace joints;

void shellreport() {
    static float last = 0.0f;
    const auto* sg = guns::shell_gun(guns::gf().wp);
    if (sg == nullptr || guns::gf().gun == nullptr || now_s() - last < 0.5f) return;
    last = now_s();
    Vec3 hp, pw, sr, pr;
    Quat hq, pq;
    if (!get_v3(JOINT, guns::gf().lwrist, "get_Position", hp) || !get_q(JOINT, guns::gf().lwrist, "get_Rotation", hq)) return;
    const Vec3 shell = hp + rotate(qnorm(hq), Vec3{cfg::SHELL_HOLD.ox, cfg::SHELL_HOLD.oy, cfg::SHELL_HOLD.oz});
    if (!guns::from_gun_local(sg->port, Quat{0, 0, 0, 1}, pw, pq)) return;
    if (!ghosthand::to_room_point(shell, sr) || !ghosthand::to_room_point(pw, pr)) return;
    const Vec3 d = pr - sr;
    LOGI("%s shellreport: move the left hand by %.4f %.4f %.4f (room, %.3f m) to the port", TAG, d.x, d.y, d.z, dist(pr, sr));
}

} // namespace vn::tools
