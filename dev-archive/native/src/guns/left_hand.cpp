// left_hand.cpp -- see left_hand.h. The socket's words: active = dock the left hand at (p, q) in the right wrist's space;
// owned = this gun's grip is ours (the loader must not dock on the gun's own aid joint when we have no socket); steer = the
// docked hand may turn the gun.
#include "left_hand.h"
#include "bridge.h"
#include "exports.h"
#include "gun_frame.h"
#include "magazine.h"
#include "rack.h"
#include "settings.h"
#include "shells.h"

#include <cmath>

namespace vn::guns::left_hand {

namespace {
const char* g_who = "nobody";

void put(exports::VisceralDockSocket& s, Vec3 p, Quat q) {
    s.px = p.x; s.py = p.y; s.pz = p.z;
    s.qx = q.x; s.qy = q.y; s.qz = q.z; s.qw = q.w;
}
} // namespace

void frame() {
    auto& s = exports::dock_socket();
    Vec3 p{};
    Quat q{0, 0, 0, 1};
    bool steer = false;
    const char* who = "nobody";

    const bool on_slide = rack::socket(p, q, steer);
    s.owned = rack::owns_grip() ? 1 : 0;
    s.steer = steer ? 1 : 0;
    s.active = on_slide ? 1 : 0;
    if (on_slide) { put(s, p, q); who = "the slide / fore-end"; }

    int r = shells::hand_socket(p, q);
    const char* reload_who = "the shells at the port";
    if (r < 0) { r = magazine::hand_socket(p, q); reload_who = r == 1 ? "the magazine (docked on it)" : "a magazine in it (free)"; }
    if (r == 2) {
        s.owned = 1;   // handguns: the grip handle is no spot for the left hand; the slide socket (if on) stands
    } else if (r >= 0) {
        s.owned = 1;
        s.steer = 0;   // the right hand alone aims while the reload has the left
        s.active = r == 1 ? 1 : 0;
        if (r == 1) put(s, p, q);
        who = reload_who;
    }
    if (who != g_who) { LOGI("%s left hand: %s has it", TAG, who); g_who = who; }

    // Hand Poser holds the left fingers only while the hand is on the gun: our socket, or LG held near the gun hand
    bool on = s.active != 0;
    if (!on && gf().wp >= 0 && bridge::live() && bridge::held(bridge::S_LGRIP)) {
        const Vec3 d = bridge::hand_pos(bridge::LEFT) - bridge::hand_pos(bridge::RIGHT);
        on = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) < cfg::LEFT_ON_GUN_NEAR_M;
    }
    bridge::put(bridge::S_LEFT_ON_GUN, on ? 1.0f : 0.0f);
}

} // namespace vn::guns::left_hand
