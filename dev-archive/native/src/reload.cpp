// reload.cpp -- see reload.h. Spec: RELOADED 1.0.1 ext_1 (Andyalpa), read as the specification and rewritten.
//
// Space convention (his, kept on purpose): the magazine joint's LOCAL pose is read and written, and world points are
// brought into the WEAPON TRANSFORM's frame to compare with his local numbers (`get_mag_parent_transform` falls back
// to the weapon transform for a joint). World positions are never read back from a joint we wrote the same frame
// (the stale-world-matrix trap, port map §D): the slide's end point is computed from the weapon transform instead.
#include "reload.h"
#include "bridge.h"
#include "menu_body.h"
#include "reload_block.h"
#include "reload_data.h"
#include "settings.h"
#include "sfx.h"
#include "weapons.h"
#include "joints.h"
#include "rack.h"
#include "pump_native.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace vn::reload {

using namespace joints;

namespace {
void write_shell_local(Vec3 p, Quat q);
bool to_weapon_local(Vec3 world, Quat wrot_in, Vec3& lpos, Quat* lrot);
int loaded();
int capacity();
int reserve();
MO* inventory();
MO* equipment();
enum class St { SEATED, SLIDE_OUT, FALL, OUT, IN_HAND, HAND_FALL, INSERT };
const char* st_name(St s) {
    switch (s) {
    case St::SEATED: return "seated";       case St::SLIDE_OUT: return "sliding out"; case St::FALL: return "falling";
    case St::OUT: return "out";             case St::IN_HAND: return "in the left hand";
    case St::HAND_FALL: return "dropped from the hand"; case St::INSERT: return "going in";
    }
    return "?";
}

St g_st = St::SEATED;
const MagWeapon* g_w = nullptr;   // the gun in hand, if it is one of ours
int g_wp = -1;
MO* g_gun = nullptr;              // this frame's Gun, joint and transforms (valid for the frame only)
MO* g_joint = nullptr;
MO* g_wtf = nullptr;
MO* g_wrist = nullptr;
bool g_claire = false;
float g_t0 = 0.0f;                // when the current state began
Vec3 g_rest_pos{}, g_rest_scale{1, 1, 1};
Quat g_rest_rot{0, 0, 0, 1};
Vec3 g_slide_from{};
Vec3 g_fall_from{};               // world
Quat g_fall_rot{0, 0, 0, 1};      // world
bool g_scale_restore = false;     // the magazine is back: write its rest scale once
float g_last_grab = -10.0f, g_last_dock = -10.0f;

// ---- shotgun shells (bundle 2): the gun's own hidden shell part rides the shell joint to the left wrist ------------
const ShellWeapon* g_sw = nullptr;
MO* g_shell_joint = nullptr;
bool g_shell_in_hand = false;
bool g_shell_have_rest = false;
Vec3 g_shell_rest{};
Quat g_shell_rest_rot{0, 0, 0, 1};
float g_shell_last_dock = -10.0f;


void set_state(St s, const char* why) {
    LOGI("%s reload: WP%04d magazine %s -> %s (%s)", TAG, g_wp, st_name(g_st), st_name(s), why);
    g_st = s;
    g_t0 = now_s();
}


Quat quat_from_ypr(const cfg::ShellHold& h) {
    const float d = 3.14159265f / 360.0f;
    const Quat qz{0, 0, std::sin(h.yaw * d), std::cos(h.yaw * d)};
    const Quat qy{0, std::sin(h.pitch * d), 0, std::cos(h.pitch * d)};
    const Quat qx{std::sin(h.roll * d), 0, 0, std::cos(h.roll * d)};
    return qnorm(qmul(qmul(qz, qy), qx));
}
Quat quat_from_ypr(const Hold& h) {   // his convention: yaw about Z, then pitch about Y, then roll about X (degrees)
    const float d = 3.14159265f / 360.0f;   // half-angle, radians
    const Quat qz{0, 0, std::sin(h.yaw * d), std::cos(h.yaw * d)};
    const Quat qy{0, std::sin(h.pitch * d), 0, std::cos(h.pitch * d)};
    const Quat qx{std::sin(h.roll * d), 0, 0, std::cos(h.roll * d)};
    return qnorm(qmul(qmul(qz, qy), qx));
}

// ---- the frame's objects ---------------------------------------------------------------------------------------------
MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }
MO* equipment() { return component(player_go(), "app.ropeway.survivor.Equipment"); }
MO* inventory() { return component(player_go(), "app.ropeway.survivor.Inventory"); }

void resolve_frame() {
    g_gun = g_joint = g_wtf = g_wrist = nullptr;
    auto* go = player_go();
    if (go == nullptr) return;
    g_claire = read_string(call_ptr(go, "get_Name")).rfind("pl1", 0) == 0;
    g_wrist = joint_by_name(call_ptr(go, "get_Transform"), L"l_arm_wrist");
    auto* gun = field_obj(component(go, "app.ropeway.survivor.Equipment"), "<EquipWeapon>k__BackingField");
    if (gun == nullptr || type_name(gun).find("Gun") == std::string::npos) return;
    g_gun = gun;
    g_wtf = call_ptr(call_ptr(gun, "get_GameObject"), "get_Transform");
    if (g_w != nullptr) g_joint = joint_by_name(g_wtf, widen(g_w->joint).c_str());
    g_shell_joint = g_sw != nullptr ? joint_by_name(g_wtf, widen(g_sw->joint).c_str()) : nullptr;
    if (g_shell_joint != nullptr && !g_shell_have_rest) {
        if (get_v3(JOINT, g_shell_joint, "get_LocalPosition", g_shell_rest) && get_q(JOINT, g_shell_joint, "get_LocalRotation", g_shell_rest_rot)) g_shell_have_rest = true;
    }
}

void shell_part(bool show) {   // the shell mesh part of the gun (hidden by the game unless it is being loaded)
    auto* mesh = call_ptr(g_gun, "get_Mesh");
    auto* m = mesh ? find_method_deep(mesh->get_type_definition(), "setPartsEnable(System.UInt64, System.Boolean)") : nullptr;
    if (m != nullptr && g_sw != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)mesh, (uint64_t)g_sw->mesh_part, show);
}

// the shell's world pose in the left hand: the wrist plus cfg::SHELL_HOLD (b142: at the wrist alone it poked through
// Leon's hand, Tefa 2026-10-10; his shell_hand is all zero, so this is ours to tune)
bool shell_hand_pose(Vec3& pos, Quat& rot) {
    Vec3 hp;
    Quat hr;
    if (!get_v3(JOINT, g_wrist, "get_Position", hp) || !get_q(JOINT, g_wrist, "get_Rotation", hr)) return false;
    hr = qnorm(hr);
    pos = hp + rotate(hr, Vec3{cfg::SHELL_HOLD.ox, cfg::SHELL_HOLD.oy, cfg::SHELL_HOLD.oz});
    rot = qnorm(qmul(hr, quat_from_ypr(cfg::SHELL_HOLD)));
    return true;
}

void shell_pose_joint() {
    if (g_shell_joint == nullptr || g_sw == nullptr || !g_shell_in_hand) return;
    Vec3 wp, lp;
    Quat wr, lr;
    if (!shell_hand_pose(wp, wr) || !to_weapon_local(wp, wr, lp, &lr)) return;
    write_shell_local(lp, lr);
}

void shell_put_back(const char* why) {
    if (!g_shell_in_hand) return;
    g_shell_in_hand = false;
    shell_part(false);
    if (g_shell_joint != nullptr && g_shell_have_rest) write_shell_local(g_shell_rest, g_shell_rest_rot);
    LOGI("%s reload: shell put back (%s)", TAG, why);
}

int tube_space() {   // shells the gun can still take
    const int cap = capacity(), have = loaded();
    return (cap > 0 && have >= 0) ? cap - have : 0;
}

// one shell in: the game's own +1 (RELOADED's first rung), then fire-ready
void shell_insert() {
    const int before = loaded(), res_before = reserve();
    bool ok = false;
    {
        reload_block::Commit c;
        if (res_before > 0) call_direct<bool>(inventory(), "reloadMainSlot", false, (int32_t)1);
        ok = loaded() > before;
        if (!ok && res_before > 0) {
            auto* m = API::get()->tdb()->find_method("app.ropeway.survivor.Equipment", "executeReload(app.ropeway.EquipmentDefine.WeaponType, System.Int32)");
            const int wt = call_direct<int>(g_gun, "get_WeaponType", -1);
            if (m != nullptr && wt >= 0) m->call<bool>(API::get()->get_vm_context(), (void*)equipment(), wt, (int32_t)1);
            ok = loaded() > before;
        }
        if (g_gun != nullptr) call_direct<void*>(g_gun, "executeEndReload", nullptr);
    }
    LOGI("%s reload: WP%04d shell in: loaded %d -> %d, carried %d -> %d%s", TAG, g_wp, before, loaded(), res_before, reserve(), ok ? "" : " -- NOTHING WENT IN");
    if (ok) pump_native::on_shells_inserted(before == 0);
    reload_block::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
}

void shell_checks() {
    if (!g_shell_in_hand || g_sw == nullptr) return;
    Vec3 wp, lp;
    Quat wr;
    if (!shell_hand_pose(wp, wr)) return;
    if (!bridge::held(bridge::S_LGRIP)) { shell_put_back("left grip let go"); return; }
    if (!to_weapon_local(wp, wr, lp, nullptr)) return;
    const float d = dist(lp, g_sw->port);
    const float now = now_s();
    if (d > g_sw->dock || now - g_shell_last_dock < cfg::RELOAD_DOCK_COOLDOWN_SEC) return;
    g_shell_last_dock = now;
    LOGI("%s reload: shell at the port (%.3f m, need %.3f)", TAG, d, g_sw->dock);
    sfx::play("mag_insert", g_sw->sfx, g_sw->vol);
    bridge::rumble(bridge::LEFT, cfg::RELOAD_INSERT_BUZZ_AMP, cfg::RELOAD_INSERT_BUZZ_SEC);
    g_shell_in_hand = false;
    shell_part(false);
    if (g_shell_joint != nullptr && g_shell_have_rest) write_shell_local(g_shell_rest, g_shell_rest_rot);
    shell_insert();
}

// world point / rotation -> the weapon transform's frame
bool to_weapon_local(Vec3 world, Quat wrot_in, Vec3& lpos, Quat* lrot) {
    Vec3 wp;
    Quat wr;
    if (!get_v3(XFORM, g_wtf, "get_Position", wp) || !get_q(XFORM, g_wtf, "get_Rotation", wr)) return false;
    const Quat inv = qinv(qnorm(wr));
    lpos = rotate(inv, world - wp);
    if (lrot != nullptr) *lrot = qnorm(qmul(inv, wrot_in));
    return true;
}
bool from_weapon_local(Vec3 lpos, Quat lrot, Vec3& world, Quat& wrot) {
    Vec3 wp;
    Quat wr;
    if (!get_v3(XFORM, g_wtf, "get_Position", wp) || !get_q(XFORM, g_wtf, "get_Rotation", wr)) return false;
    world = wp + rotate(qnorm(wr), lpos);
    wrot = qnorm(qmul(qnorm(wr), lrot));
    return true;
}

// the magazine's world pose when it sits in the left hand
bool hand_pose(Vec3& pos, Quat& rot) {
    Vec3 hp;
    Quat hr;
    if (!get_v3(JOINT, g_wrist, "get_Position", hp) || !get_q(JOINT, g_wrist, "get_Rotation", hr)) return false;
    const Hold& h = g_claire ? g_w->claire : g_w->leon;
    hr = qnorm(hr);
    pos = hp + rotate(hr, Vec3{h.ox, h.oy, h.oz});
    rot = qnorm(qmul(hr, quat_from_ypr(h)));
    return true;
}

// ---- ammo -------------------------------------------------------------------------------------------------------------
int loaded() { return g_gun ? call_direct<int>(g_gun, "getBulletNumber", -1) : -1; }
int capacity() { return call_direct<int>(call_ptr(inventory(), "get_MainSlot"), "get_MaxNumber", -1); }
int reserve() {   // the rounds that could go in: the larger of the two readings RELOADED trusts (after a save load one can lag)
    const int a = call_direct<int>(API::get()->get_managed_singleton("app.ropeway.gamemastering.InventoryManager"), "getMainWeaponReloadableBullet", -1);
    auto* m = API::get()->tdb()->find_method("app.ropeway.survivor.Inventory", "getReloadableBulletMainSlot");
    auto* inv = inventory();
    const int b = (m != nullptr && inv != nullptr) ? m->call<int>(API::get()->get_vm_context(), (void*)inv, false) : -1;
    return std::max(a, b);
}

// the game's own reload result, done by our hand: top the gun up from the ammo carried
void top_up() {
    const int before = loaded(), cap = capacity(), res_before = reserve();
    const int add = std::min(std::max(0, cap - before), std::max(0, res_before));
    const char* how = "nothing to add";
    if (add > 0) {
        reload_block::Commit c;
        call_direct<bool>(inventory(), "reloadMainSlot", false, (int32_t)add);
        how = "Inventory.reloadMainSlot";
        if (loaded() <= before) {   // fallback: the Equipment route (RELOADED's second rung)
            auto* m = API::get()->tdb()->find_method("app.ropeway.survivor.Equipment", "executeReload(app.ropeway.EquipmentDefine.WeaponType, System.Int32)");
            const int wt = call_direct<int>(g_gun, "get_WeaponType", -1);
            if (m != nullptr && wt >= 0) {
                m->call<bool>(API::get()->get_vm_context(), (void*)equipment(), wt, (int32_t)add);
                how = "Equipment.executeReload (fallback)";
            } else {
                how = "Inventory.reloadMainSlot did nothing, no fallback found";
            }
        }
    }
    if (g_gun != nullptr) call_direct<void*>(g_gun, "executeEndReload", nullptr);   // fire-ready again
    if (before == 0 && loaded() > 0) rack::need("a magazine into an empty gun");
    reload_block::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
    LOGI("%s reload: WP%04d topped up by %s: loaded %d -> %d (capacity %d), carried %d -> %d", TAG, g_wp, how, before, loaded(), cap,
         res_before, reserve());
}

bool supply() { return reserve() > 0 || loaded() > 0; }

// ---- the joint writes, every state -------------------------------------------------------------------------------------
void write_local(Vec3 p, Quat q) {
    set_any(JOINT, g_joint, "set_LocalPosition", &p);
    set_any(JOINT, g_joint, "set_LocalRotation", &q);
}
void write_world(Vec3 p, Quat q) {
    set_any(JOINT, g_joint, "set_Position", &p);
    set_any(JOINT, g_joint, "set_Rotation", &q);
}
void write_scale(Vec3 s) { set_any(JOINT, g_joint, "set_LocalScale", &s); }
void write_shell_local(Vec3 p, Quat q) {
    set_any(JOINT, g_shell_joint, "set_LocalPosition", &p);
    set_any(JOINT, g_shell_joint, "set_LocalRotation", &q);
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
        if (!hand_pose(wp, wr) || !to_weapon_local(wp, wr, lp, &lr)) return;
        write_scale(g_rest_scale);
        write_local(lp, lr);
        return;
    }
    case St::INSERT: {
        const float u = ease(t / cfg::RELOAD_INSERT_SEC);
        write_scale(g_rest_scale);
        write_local(Vec3{g_rest_pos.x, g_w->exit.y + (g_rest_pos.y - g_w->exit.y) * u, g_w->exit.z + (g_rest_pos.z - g_w->exit.z) * u}, g_rest_rot);
        return;
    }
    }
}

// in the left hand: the grip let go drops it; near the magwell it goes in. Fresh hand pose, so only at the late points.
void hand_checks() {
    if (g_st != St::IN_HAND || g_w == nullptr) return;
    Vec3 wp, lp;
    Quat wr;
    if (!hand_pose(wp, wr)) return;
    if (!bridge::held(bridge::S_LGRIP)) {
        g_fall_from = wp;
        g_fall_rot = wr;
        set_state(St::HAND_FALL, "left grip let go");
        return;
    }
    if (!to_weapon_local(wp, wr, lp, nullptr)) return;
    const float d = dist(lp, g_w->exit), need = std::fmax(g_w->dock, cfg::RELOAD_DOCK_MIN_M);
    const float now = now_s();
    if (d > need || now - g_last_dock < cfg::RELOAD_DOCK_COOLDOWN_SEC) return;
    g_last_dock = now;
    set_state(St::INSERT, "brought to the magwell");
    LOGI("%s reload: insert at %.3f m from the magwell (his %.3f, ours at least %.3f)", TAG, d, g_w->dock, cfg::RELOAD_DOCK_MIN_M);
    sfx::play("mag_insert", g_w->sfx, g_w->vol);
    bridge::rumble(bridge::LEFT, cfg::RELOAD_INSERT_BUZZ_AMP, cfg::RELOAD_INSERT_BUZZ_SEC);
}

void put_back_on_swap(const char* why) {
    if (g_st == St::SEATED) return;
    LOGI("%s reload: WP%04d magazine put back as it was (%s) -- its rounds never left the gun", TAG, g_wp, why);
    if (g_joint != nullptr) write_scale(g_rest_scale);
    g_st = St::SEATED;
}

void begin_drop() {
    if (g_joint == nullptr) { LOGW("%s reload: WP%04d joint %s not found on the gun, no drop", TAG, g_wp, g_w->joint); return; }
    Vec3 p, s;
    Quat q;
    if (!get_v3(JOINT, g_joint, "get_LocalPosition", p) || !get_q(JOINT, g_joint, "get_LocalRotation", q)) return;
    if (!get_v3(JOINT, g_joint, "get_LocalScale", s) || s.x < 0.01f) s = Vec3{1, 1, 1};
    g_rest_pos = p;
    g_rest_rot = qnorm(q);
    g_rest_scale = s;
    g_slide_from = p;
    set_state(St::SLIDE_OUT, "right B");
    LOGI("%s reload: drop with %d loaded, %d carried; rest %.3f %.3f %.3f", TAG, loaded(), reserve(), p.x, p.y, p.z);
    sfx::play("mag_drop", g_w->sfx, g_w->vol);
}
} // namespace

// ---- the public side --------------------------------------------------------------------------------------------------
// headset only: without it there is no B, no left hand and no pouch, so the game keeps its own reload
bool managed_now() { return cfg::RELOAD_ON && (g_w != nullptr || g_sw != nullptr) && g_gun != nullptr && bridge::live(); }
bool mag_out() { return managed_now() && g_w != nullptr && g_st != St::SEATED; }
bool session_active() { return mag_out() || g_shell_in_hand; }
int wp_now() { return g_wp; }
const char* sfx_folder_now() { return g_w ? g_w->sfx : g_sw ? g_sw->sfx : "handgun"; }
float sfx_volume_now() { return g_w ? g_w->vol : g_sw ? g_sw->vol : 1.0f; }

void frame() {
    if (!cfg::RELOAD_ON) return;
    const int wp = weapons::current_id();
    if (wp != g_wp) {
        put_back_on_swap(wp < 0 ? "weapon put away" : "weapon changed");
        shell_put_back("weapon changed");
        g_wp = wp;
        g_w = mag_weapon(wp);
        g_sw = shell_weapon(wp);
        g_shell_have_rest = false;
        if (g_sw != nullptr) LOGI("%s reload: WP%04d %s loads single shells (joint %s, part %d)", TAG, wp, weapons::name(wp), g_sw->joint, g_sw->mesh_part);
        if (g_w != nullptr) LOGI("%s reload: WP%04d %s uses the manual magazine reload (joint %s)", TAG, wp, weapons::name(wp), g_w->joint);
    }
    resolve_frame();
    if (g_w == nullptr || g_gun == nullptr) return;

    const float t = now_s() - g_t0;
    switch (g_st) {
    case St::SLIDE_OUT:
        if (t >= cfg::RELOAD_SLIDE_SEC) {
            if (!from_weapon_local(g_w->exit, g_rest_rot, g_fall_from, g_fall_rot)) g_fall_from = Vec3{0, 0, 0};
            set_state(St::FALL, "out of the magwell");
        }
        break;
    case St::FALL:
    case St::HAND_FALL:
        if (t >= cfg::RELOAD_FALL_SEC) {
            set_state(St::OUT, "on the floor");
            sfx::play("mag_floor", g_w->sfx, g_w->vol);
        }
        break;
    case St::INSERT:
        if (t >= cfg::RELOAD_INSERT_SEC) {
            set_state(St::SEATED, "seated");
            g_scale_restore = true;
            top_up();
        }
        break;
    default:
        break;
    }

    // right B in normal play (FirstPerson driving: no menu, no cutscene) drops a seated magazine
    const float fp = bridge::view(bridge::S_FP_USED);
    const bool in_play = bridge::live() && !menu_body::is_menu_open() && bridge::has(fp) && fp > 0.5f;
    if (in_play && bridge::pressed(bridge::S_RB) && g_st == St::SEATED) begin_drop();
}

void late_point() {
    if (!managed_now()) return;
    if (g_w != nullptr) { hand_checks(); pose_joint(); }
    if (g_sw != nullptr) { shell_checks(); shell_pose_joint(); }
}

void render_point() {
    if (!managed_now()) return;
    if (g_w != nullptr) pose_joint();
    if (g_sw != nullptr) shell_pose_joint();
}

bool pouch_grab() {
    if (!managed_now()) return false;
    if (g_sw != nullptr) {   // a shell for the shotgun
        const float now = now_s();
        if (now - g_last_grab < cfg::RELOAD_GRAB_COOLDOWN_SEC || g_shell_in_hand) return true;
        g_last_grab = now;
        if (reserve() <= 0 || tube_space() <= 0) {
            LOGI("%s reload: no shell to take (carried %d, space %d)", TAG, reserve(), tube_space());
            bridge::rumble(bridge::LEFT, cfg::RELOAD_DENY_BUZZ_AMP, cfg::RELOAD_DENY_BUZZ_SEC);
            return true;
        }
        if (g_shell_joint == nullptr) { LOGW("%s reload: shell joint %s not found on WP%04d", TAG, g_sw->joint, g_wp); return true; }
        g_shell_in_hand = true;
        shell_part(true);
        sfx::play("mag_grab", g_sw->sfx, g_sw->vol);
        bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
        LOGI("%s reload: shell in the left hand (carried %d, space %d)", TAG, reserve(), tube_space());
        return true;
    }
    if (g_st != St::OUT) return false;
    const float now = now_s();
    if (now - g_last_grab < cfg::RELOAD_GRAB_COOLDOWN_SEC) return true;
    g_last_grab = now;
    if (!supply()) {
        LOGI("%s reload: pouch is empty (no rounds carried, none in the gun)", TAG);
        bridge::rumble(bridge::LEFT, cfg::RELOAD_DENY_BUZZ_AMP, cfg::RELOAD_DENY_BUZZ_SEC);
        return true;
    }
    set_state(St::IN_HAND, "taken from the pouch");
    sfx::play("mag_grab", g_w->sfx, g_w->vol);
    bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
    return true;
}

} // namespace vn::reload
