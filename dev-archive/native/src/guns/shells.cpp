// shells.cpp -- see shells.h. The shell is the gun's own mesh part (hidden by the game unless it is loading) on its shell
// joint, posed at the left wrist in the gun's local space.
#include "shells.h"
#include "ammo.h"
#include "bridge.h"
#include "cylinder.h"
#include "game_reload.h"
#include "gun_data.h"
#include "gun_frame.h"
#include "joints.h"
#include "pump.h"
#include "rounds.h"
#include "settings.h"
#include "sfx.h"

#include <algorithm>
#include <cmath>

namespace vn::guns::shells {

using namespace joints;

namespace {
const ShellGun* g_w = nullptr;
MO* g_joint = nullptr;
bool g_have_rest = false;
Vec3 g_rest{};
Quat g_rest_rot{0, 0, 0, 1};
int g_n = 0;                 // shells in the hand
int g_item = 0;              // their kind, for a gun with more than one (rounds.h); 0 = the gun's only kind
bool g_docked = false;       // the hand is at the port
bool g_feeding = false;      // LT was pressed at the port: they go in one by one
float g_next = 0.0f;
float g_last_grab = -10.0f;
Vec3 g_dock_p{};
Quat g_dock_q{0, 0, 0, 1};

bool drawn() { return g_w != nullptr && g_w->joint[0] != '\0' && g_w->mesh_part >= 0; }

void find_joint() {
    g_joint = (drawn() && gf().gun_tf != nullptr) ? joint_by_name(gf().gun_tf, widen(g_w->joint).c_str()) : nullptr;
    if (g_joint != nullptr && !g_have_rest) g_have_rest = get_v3(JOINT, g_joint, "get_LocalPosition", g_rest) && get_q(JOINT, g_joint, "get_LocalRotation", g_rest_rot);
}

void write_local(Vec3 p, Quat q) {
    set_any(JOINT, g_joint, "set_LocalPosition", &p);
    set_any(JOINT, g_joint, "set_LocalRotation", &q);
}

void shell_part(bool show) {
    if (!drawn()) return;
    auto* mesh = call_ptr(gf().gun, "get_Mesh");
    auto* m = mesh ? find_method_deep(mesh->get_type_definition(), "setPartsEnable(System.UInt64, System.Boolean)") : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)mesh, (uint64_t)g_w->mesh_part, show);
}

bool hand_pose(Vec3& pos, Quat& rot) {
    Vec3 hp;
    Quat hr;
    if (!get_v3(JOINT, gf().lwrist, "get_Position", hp) || !get_q(JOINT, gf().lwrist, "get_Rotation", hr)) return false;
    const cfg::ShellHold h = cfg::SHELL_HOLD;
    hr = qnorm(hr);
    pos = hp + rotate(hr, Vec3{h.ox, h.oy, h.oz});
    rot = qnorm(qmul(hr, ypr_deg(h.yaw, h.pitch, h.roll)));
    return true;
}

void done(const char* why) {
    if (g_n > 0 || g_docked) LOGI("%s shells: %s (loaded %d, %d back to the pouch)", TAG, why, ammo::loaded(), g_n);
    g_n = 0;
    g_docked = g_feeding = false;
    if (g_w != nullptr && gf().gun != nullptr) shell_part(false);
    if (g_joint != nullptr && g_have_rest) write_local(g_rest, g_rest_rot);
}

void pose() {
    if (g_n > 0 && g_item != 0 && rounds::chambered()) {      // a drum gun: empty chambers lend the hand their rounds
        Vec3 hw;
        Quat hq;
        if (hand_pose(hw, hq)) rounds::show_hand(g_n, g_item, hw, hq);
        return;
    }
    if (g_n <= 0 || g_joint == nullptr) return;
    Vec3 wp, lp;
    Quat wr, lr;
    if (hand_pose(wp, wr) && to_gun_local(wp, wr, lp, &lr)) write_local(lp, lr);
}

// the hand at the port takes its place there (in the right wrist's space, so it moves with the gun)
bool dock_here() {
    Vec3 lw, rw;
    Quat lq, rq;
    if (!get_v3(JOINT, gf().lwrist, "get_Position", lw) || !get_q(JOINT, gf().lwrist, "get_Rotation", lq) ||
        !get_v3(JOINT, gf().rwrist, "get_Position", rw) || !get_q(JOINT, gf().rwrist, "get_Rotation", rq))
        return false;
    const Quat inv = qinv(qnorm(rq));
    g_dock_p = rotate(inv, lw - rw);
    g_dock_q = qnorm(qmul(inv, qnorm(lq)));
    return true;
}

void checks() {
    if (g_n <= 0) return;
    if (!bridge::held(bridge::S_LGRIP)) { done("left grip let go"); return; }
    // rounds in the hand, away from the gun, LT: the pouch's next kind, and the hand now holds that kind
    if (!g_docked && g_item != 0 && bridge::pressed(bridge::S_LTRIG)) {
        const int it = rounds::cycle();
        if (it != 0 && it != g_item) {
            g_item = it;
            g_n = std::max(1, std::min(cfg::SHELLS_PER_GRAB, std::min(rounds::carried(it), ammo::space())));
            sfx::play("mag_grab", g_w->sfx, g_w->vol);
        }
        bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
        return;
    }
    if (!g_docked) {
        Vec3 wp, lp;
        Quat wr;
        if (!hand_pose(wp, wr) || !to_gun_local(wp, wr, lp, nullptr)) return;
        if (cylinder::ours() && !cylinder::is_open()) return;      // a drum that swings out takes rounds only while open
        float d = dist(lp, g_w->port);
        if (g_item != 0 && rounds::chambered() && !rounds::nearest_empty(lp, d)) return;   // a drum: the nearest empty chamber
        if (d > std::fmin(g_w->dock, cfg::SHELL_PORT_DOCK_M) || !dock_here()) return;
        g_docked = true;
        bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
        LOGI("%s shells: %d at the port (%.3f m): LT puts them in", TAG, g_n, d);
        return;
    }
    const float now = now_s();
    if (!g_feeding && bridge::pressed(bridge::S_LTRIG)) { g_feeding = true; g_next = now; }
    if (!g_feeding || now < g_next) return;
    if (g_n > 0 && ammo::space() > 0) {
        sfx::play("mag_insert", g_w->sfx, g_w->vol);
        bridge::rumble(bridge::LEFT, cfg::RELOAD_INSERT_BUZZ_AMP, cfg::RELOAD_INSERT_BUZZ_SEC);
        const bool was_empty = ammo::loaded() == 0;
        Vec3 hw, hl;
        Quat hq;
        const bool have_hand = hand_pose(hw, hq) && to_gun_local(hw, hq, hl, nullptr);
        const bool went_in = g_item != 0 ? rounds::insert(g_item, have_hand ? &hl : nullptr) : ammo::insert_one();
        if (went_in && is_pump(gf().wp)) pump::on_shell_in(was_empty);
        game_reload::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
        --g_n;
        g_next = now + cfg::SHELLS_STEP_S;
        if (g_n > 0 && ammo::space() > 0) return;
    }
    done("shells in");
}
} // namespace

void on_gun_changed(int new_wp) {
    if (g_n > 0 || g_docked) { g_n = 0; g_docked = g_feeding = false; }   // the old gun's part and joint are gone with it
    g_w = shell_gun(new_wp);
    g_have_rest = false;
}

bool ours() { return cfg::RELOAD_ON && g_w != nullptr && gf().gun != nullptr && bridge::live(); }
bool in_hand() { return ours() && g_n > 0; }

int hand_socket(Vec3& p, Quat& q) {
    if (!ours() || !g_docked) return -1;
    p = g_dock_p;
    q = g_dock_q;
    return 1;
}

bool pouch_grab() {
    if (!ours()) return false;
    const int item = rounds::ours() ? rounds::selected() : 0;
    const float now = now_s();
    if (now - g_last_grab < cfg::RELOAD_GRAB_COOLDOWN_SEC || g_n > 0) return true;
    g_last_grab = now;
    const int carried = item != 0 ? rounds::carried(item) : ammo::reserve(), space = ammo::space();
    if (carried <= 0 || space <= 0) {
        LOGI("%s shells: none to take (carried %d, space %d)", TAG, carried, space);
        bridge::rumble(bridge::LEFT, cfg::RELOAD_DENY_BUZZ_AMP, cfg::RELOAD_DENY_BUZZ_SEC);
        return true;
    }
    find_joint();
    if (drawn() && g_joint == nullptr) { LOGW("%s shells: joint %s not found on WP%04d", TAG, g_w->joint, gf().wp); return true; }
    g_n = std::min(cfg::SHELLS_PER_GRAB, std::min(carried, space));
    g_item = item;
    shell_part(true);
    sfx::play("mag_grab", g_w->sfx, g_w->vol);
    bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
    LOGI("%s shells: %d in the left hand, item %d (carried %d, space %d)", TAG, g_n, g_item, carried, space);
    return true;
}

void late_point() {
    find_joint();
    if (!ours()) return;
    checks();
    pose();
}

void render_point() {
    find_joint();
    if (ours()) pose();
}

} // namespace vn::guns::shells
