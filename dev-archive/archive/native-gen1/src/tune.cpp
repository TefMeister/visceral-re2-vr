// tune.cpp -- see tune.h.
#include "tune.h"
#include "bridge.h"
#include "handpose_data.h"
#include "joints.h"
#include "reload.h"
#include "weapons.h"

namespace vn::tune {
using namespace joints;

namespace {
float at(Hold h, int i) {
    const int base = h < MAG_DOCK ? bridge::S_HT + h * bridge::HT_STRIDE : bridge::S_HT2 + (h - MAG_DOCK) * bridge::HT_STRIDE;   // 2026-10-31: S_FREEZE sits after the seventh
    const float v = bridge::view(static_cast<bridge::Slot>(base + i));
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
    // 2026-10-31: not while the hand is docked on the magazine (the dock spot already carries its own sliders, MAG_DOCK)
    const bool mag = reload::mag_in_hand() && !reload::hand_docked(), shell = !reload::mag_in_hand() && reload::shell_in_hand();
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

// ---- b168: the RIGHT hand on the gun (GUN hold) -------------------------------------------------------------------
// The game's animation (and the VR layer, for the wrist) put these joints somewhere new every frame; we ADD the
// sliders to that. If nobody rewrote a joint since our last write (it still reads what we wrote), we start again from
// the value we added to last time, so an offset is never added twice and a slider moved while the hand is still is
// still applied. Same idea as wrist_point, per joint.
namespace {
struct Kept { MO* j = nullptr; bool on = false; Vec3 base_p{}; Quat base_q{0, 0, 0, 1}; Vec3 base_e{}; Vec3 wrote_p{}; Quat wrote_q{0, 0, 0, 1}; };
Kept g_rgun, g_rwrist, g_rfing[kFingerJoints], g_lfree;
bool g_rlogged = false;

bool same_q(Quat a, Quat b) {
    const float d = std::fabs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);   // q and -q are the same turn
    return d > 1.0f - 1e-6f;
}
bool zero(Vec3 v) { return v.x == 0 && v.y == 0 && v.z == 0; }

// move (metres, the joint's own axes) and turn (yaw pitch roll, degrees), added to the joint's local place
void add_move_turn(Kept& k, MO* j, Vec3 mv, Vec3 tn) {
    if (j == nullptr) return;
    Vec3 p;
    Quat q;
    if (!get_v3(JOINT, j, "get_LocalPosition", p) || !get_q(JOINT, j, "get_LocalRotation", q)) return;
    const bool ours = k.on && k.j == j && dist(p, k.wrote_p) < 1e-5f && same_q(q, k.wrote_q);
    if (!ours) { k.base_p = p; k.base_q = qnorm(q); }
    const bool want = !zero(mv) || !zero(tn);
    if (!want && !(ours && k.on)) { k.on = false; return; }
    const Vec3 np = k.base_p + rotate(k.base_q, mv);
    const Quat nq = qnorm(qmul(k.base_q, ypr(tn)));
    set_any(JOINT, j, "set_LocalPosition", &np);
    set_any(JOINT, j, "set_LocalRotation", &nq);
    k.j = j; k.on = want; k.wrote_p = np; k.wrote_q = nq;
}

// twist spread curl (radians), added to the joint's local Euler angles, like the left hand's fingers
void add_euler(Kept& k, MO* j, Vec3 d) {
    if (j == nullptr) return;
    Quat q;
    if (!get_q(JOINT, j, "get_LocalRotation", q)) return;
    const bool ours = k.on && k.j == j && same_q(q, k.wrote_q);
    if (!ours && !get_v3(JOINT, j, "get_LocalEulerAngle", k.base_e)) return;
    const bool want = !zero(d);
    if (!want && !ours) { k.on = false; return; }
    const Vec3 t = k.base_e + d;
    set_any(JOINT, j, "set_LocalEulerAngle", &t);
    Quat back{0, 0, 0, 1};
    k.j = j; k.on = want && get_q(JOINT, j, "get_LocalRotation", back);   // read back what really stands
    if (k.on) k.wrote_q = back;
}
} // namespace

void right_point() {
    if (!live() || weapons::current_id() < 0) {
        g_rgun.on = g_rwrist.on = false;
        for (auto& k : g_rfing) k.on = false;
        return;
    }
    auto* go = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    auto* tf = call_ptr(go, "get_Transform");
    if (tf == nullptr) return;
    static std::wstring names[kFingerJoints];
    if (names[0].empty()) for (int i = 0; i < kFingerJoints; ++i) { names[i] = handpose::kBones[i]; names[i][0] = L'r'; }   // l_hand_* -> r_hand_*
    auto* wrist = joint_by_name(tf, L"r_arm_wrist");
    auto* gun = joint_by_name(tf, L"r_weapon");
    add_move_turn(g_rwrist, wrist, hand_move(GUN), hand_turn_deg(GUN));
    add_move_turn(g_rgun, gun, item_move(GUN), at3(GUN, 3));
    int found = 0;
    for (int i = 0; i < kFingerJoints; ++i) {
        auto* j = joint_by_name(tf, names[i].c_str());
        if (j != nullptr) ++found;
        add_euler(g_rfing[i], j, finger(GUN, i));
    }
    if (!g_rlogged) {
        LOGI("%s tune: right hand joints found: wrist %s, gun (r_weapon) %s, fingers %d of %d", TAG, wrist ? "yes" : "NO",
             gun ? "yes" : "NO", found, kFingerJoints);
        g_rlogged = true;
    }
}

// ---- b172: the left hand on its own (no button, nowhere near the gun): its wrist sliders, faded in and out by f
void free_wrist(float f) {
    if (!live() && !g_lfree.on) return;
    auto* go = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    auto* w = joint_by_name(call_ptr(go, "get_Transform"), L"l_arm_wrist");
    const Vec3 mv = live() ? hand_move(FREE) : Vec3{}, tn = live() ? hand_turn_deg(FREE) : Vec3{};
    add_move_turn(g_lfree, w, Vec3{mv.x * f, mv.y * f, mv.z * f}, Vec3{tn.x * f, tn.y * f, tn.z * f});
}

// ---- b171: the gun-carried point as the TUNED gun puts it (see tune.h). The sliders move r_weapon by mv (its own axes)
// and turn it by R, so a point x carried by it goes from Pw + Qw x_l to Pw + Qw (mv + R x_l), x_l = Qw^-1 (x - Pw).
float to_tuned(Vec3& p, Quat* q) {
    if (!live()) return 0.0f;
    const Vec3 mv = item_move(GUN);
    const Vec3 tn = at3(GUN, 3);
    if (zero(mv) && zero(tn)) return 0.0f;
    MO* j = g_rgun.j;
    if (j == nullptr) return 0.0f;
    Vec3 lp, pw;
    Quat lq, qw;
    if (!get_v3(JOINT, j, "get_LocalPosition", lp) || !get_q(JOINT, j, "get_LocalRotation", lq)) return 0.0f;
    if (g_rgun.on && dist(lp, g_rgun.wrote_p) < 1e-5f && same_q(lq, g_rgun.wrote_q)) return 0.0f;   // ours stands: already tuned
    if (!get_v3(JOINT, j, "get_Position", pw) || !get_q(JOINT, j, "get_Rotation", qw)) return 0.0f;
    qw = qnorm(qw);
    const Quat r = ypr(tn);
    const Vec3 xl = rotate(qinv(qw), p - pw);
    const Vec3 np = pw + rotate(qw, mv + rotate(r, xl));
    const float moved = dist(np, p);
    p = np;
    if (q != nullptr) *q = qnorm(qmul(qmul(qw, qmul(r, qinv(qw))), qnorm(*q)));
    return moved;
}

// ---- b169: hold both hands still (Tefa 2026-10-13: "freeze hand positions in VR, without the game freezing ... it has
// to work with a countdown, so i tick the box, comfortably hold the controllers and in 10 seconds they stop moving").
// The tick box and the countdown are in lua/visceral_tuning.lua, which writes S_FREEZE = 1 once the 10 s are up. The VR
// layer asks this every frame (VR.cpp update_hmd_state, patch 2026-10-13-re2-hold-hands-still) and, while it says 1,
// keeps both controllers' poses as they were at the switch. Called from the VR layer's thread: reads one float only.
extern "C" __declspec(dllexport) int visceral_hands_frozen() {
    const float v = bridge::view(bridge::S_FREEZE);
    return v > 0.5f && v < 2.0f ? 1 : 0;
}

} // namespace vn::tune
