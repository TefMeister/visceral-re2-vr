// gun_frame.cpp -- see gun_frame.h.
#include "gun_frame.h"
#include "joints.h"
#include "weapons.h"

#include <cmath>
#include <string>

namespace vn::guns {

using namespace joints;

namespace {
GunFrame g;
} // namespace

void refresh() {
    g = GunFrame{};
    g.player = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    if (g.player == nullptr) return;
    g.equipment = component(g.player, "app.ropeway.survivor.Equipment");
    g.inventory = component(g.player, "app.ropeway.survivor.Inventory");
    g.claire = read_string(call_ptr(g.player, "get_Name")).rfind("pl1", 0) == 0;
    auto* body = call_ptr(g.player, "get_Transform");
    g.lwrist = joint_by_name(body, L"l_arm_wrist");
    g.rwrist = joint_by_name(body, L"r_arm_wrist");
    g.wp = weapons::current_id();
    auto* w = field_obj(g.equipment, "<EquipWeapon>k__BackingField");
    if (w == nullptr || type_name(w).find("Gun") == std::string::npos) return;
    g.gun = w;
    g.gun_tf = call_ptr(call_ptr(w, "get_GameObject"), "get_Transform");
}

const GunFrame& gf() { return g; }

bool to_gun_local(Vec3 world, Quat wrot, Vec3& lpos, Quat* lrot) {
    Vec3 gp;
    Quat gq;
    if (!get_v3(XFORM, g.gun_tf, "get_Position", gp) || !get_q(XFORM, g.gun_tf, "get_Rotation", gq)) return false;
    const Quat inv = qinv(qnorm(gq));
    lpos = rotate(inv, world - gp);
    if (lrot != nullptr) *lrot = qnorm(qmul(inv, wrot));
    return true;
}

bool from_gun_local(Vec3 lpos, Quat lrot, Vec3& world, Quat& wrot) {
    Vec3 gp;
    Quat gq;
    if (!get_v3(XFORM, g.gun_tf, "get_Position", gp) || !get_q(XFORM, g.gun_tf, "get_Rotation", gq)) return false;
    world = gp + rotate(qnorm(gq), lpos);
    wrot = qnorm(qmul(qnorm(gq), lrot));
    return true;
}

Quat ypr_deg(float yaw, float pitch, float roll) {
    const float h = 3.14159265f / 360.0f;   // half-angle, radians
    const Quat qz{0, 0, std::sin(yaw * h), std::cos(yaw * h)};
    const Quat qy{0, std::sin(pitch * h), 0, std::cos(pitch * h)};
    const Quat qx{std::sin(roll * h), 0, 0, std::cos(roll * h)};
    return qnorm(qmul(qmul(qz, qy), qx));
}

} // namespace vn::guns
