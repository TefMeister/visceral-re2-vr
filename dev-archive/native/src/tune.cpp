// tune.cpp -- see tune.h.
#include "tune.h"
#include "bridge.h"
#include "joints.h"
#include "reload.h"
#include "weapons.h"

namespace vn::tune {
using namespace joints;

namespace {
float at(Hold h, int i) {
    const float v = bridge::view(static_cast<bridge::Slot>(bridge::S_HT + h * bridge::HT_STRIDE + i));
    return bridge::has(v) ? v : 0.0f;
}
Vec3 at3(Hold h, int i) { return {at(h, i), at(h, i + 1), at(h, i + 2)}; }

// the wrist write: only once per engine pass (the game may or may not rewrite it in between, see wrist_point)
MO* g_wrist = nullptr;
bool g_wrote = false;
Vec3 g_wrote_pos{};
Quat g_wrote_rot{};
int g_char = -2;
} // namespace

bool live() { const float v = bridge::view(bridge::S_HT_ON); return v > 0.5f && v < 2.0f; }
Quat ypr(Vec3 d) {
    const float k = 3.14159265f / 360.0f;   // half-angle, radians
    const Quat qz{0, 0, std::sin(d.x * k), std::cos(d.x * k)};
    const Quat qy{0, std::sin(d.y * k), 0, std::cos(d.y * k)};
    const Quat qx{std::sin(d.z * k), 0, 0, std::cos(d.z * k)};
    return qnorm(qmul(qmul(qz, qy), qx));
}
Vec3 item_move(Hold h) { return live() ? at3(h, 0) : Vec3{}; }
Quat item_turn(Hold h) { return live() ? ypr(at3(h, 3)) : Quat{0, 0, 0, 1}; }
Vec3 hand_move(Hold h) { return live() ? at3(h, 6) : Vec3{}; }
Vec3 hand_turn_deg(Hold h) { return live() ? at3(h, 9) : Vec3{}; }
Vec3 finger(Hold h, int j) {
    if (!live() || j < 0 || j >= kFingerJoints) return {};
    const float r = 3.14159265f / 180.0f;   // the sliders are in degrees
    const Vec3 d = at3(h, 12 + j * 3);
    return {d.x * r, d.y * r, d.z * r};
}

void frame() {
    auto* go = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    // the character: the digit after "pl" in the player's name (pl0 Leon, pl1 Claire, ...); -1 = no player
    int ch = -1;
    std::string n;
    if (go != nullptr) {
        n = read_string(call_ptr(go, "get_Name"));
        if (n.size() >= 3 && n[0] == 'p' && n[1] == 'l' && n[2] >= '0' && n[2] <= '9') ch = n[2] - '0';
    }
    if (ch != g_char && go != nullptr) LOGI("%s tune: player %s -> character %d (its own save file)", TAG, n.c_str(), ch);
    g_char = ch;
    bridge::put(bridge::S_WHO, static_cast<float>(weapons::current_id()));
    bridge::put(static_cast<bridge::Slot>(bridge::S_WHO + 1), static_cast<float>(ch));
}

// The hand while it holds a magazine or a shell: the wrist joint moved and turned in its own axes. The VR layer places
// the wrist; if it rewrites it after us, these two sliders do nothing.
void wrist_point() {
    const bool mag = reload::mag_in_hand(), shell = !mag && reload::shell_in_hand();
    if (!live() || (!mag && !shell)) { g_wrote = false; return; }
    const Hold h = mag ? MAG : SHELL;
    const Vec3 mv = hand_move(h), tn = hand_turn_deg(h);
    if (mv.x == 0 && mv.y == 0 && mv.z == 0 && tn.x == 0 && tn.y == 0 && tn.z == 0) { g_wrote = false; return; }
    auto* go = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    auto* w = joint_by_name(call_ptr(go, "get_Transform"), L"l_arm_wrist");
    if (w == nullptr) return;
    Vec3 p;
    Quat q;
    if (!get_v3(JOINT, w, "get_LocalPosition", p) || !get_q(JOINT, w, "get_LocalRotation", q)) return;
    // our own write still standing (nobody rewrote it since): adding again would double it
    if (g_wrote && w == g_wrist && dist(p, g_wrote_pos) < 1e-5f && std::fabs(q.x - g_wrote_rot.x) + std::fabs(q.y - g_wrote_rot.y) +
        std::fabs(q.z - g_wrote_rot.z) + std::fabs(q.w - g_wrote_rot.w) < 1e-5f) return;
    q = qnorm(q);
    const Vec3 np = p + rotate(q, mv);
    const Quat nq = qnorm(qmul(q, ypr(tn)));
    set_any(JOINT, w, "set_LocalPosition", &np);
    set_any(JOINT, w, "set_LocalRotation", &nq);
    g_wrist = w; g_wrote = true; g_wrote_pos = np; g_wrote_rot = nq;
}

} // namespace vn::tune
