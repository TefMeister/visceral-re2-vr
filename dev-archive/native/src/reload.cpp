// reload.cpp -- see reload.h. Spec: RELOADED 1.0.1 ext_1 (Andyalpa), read as the specification and rewritten.
//
// Space convention (his, kept on purpose): the magazine joint's LOCAL pose is read and written, and world points are
// brought into the WEAPON TRANSFORM's frame to compare with his local numbers (`get_mag_parent_transform` falls back
// to the weapon transform for a joint). World positions are never read back from a joint we wrote the same frame
// (the stale-world-matrix trap, port map §D): the slide's end point is computed from the weapon transform instead.
#include "reload.h"
#include "tune.h"
#include "bridge.h"
#include "ghosthand.h"
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
float hand_to_dock();
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
MO* g_rwrist = nullptr;           // 2026-10-31: the right wrist (the dock socket is in its space)
bool g_claire = false;
float g_t0 = 0.0f;                // when the current state began
Vec3 g_rest_pos{}, g_rest_scale{1, 1, 1};
Quat g_rest_rot{0, 0, 0, 1};
Vec3 g_slide_from{};
Vec3 g_fall_from{};               // world
Quat g_fall_rot{0, 0, 0, 1};      // world
bool g_scale_restore = false;     // the magazine is back: write its rest scale once
float g_last_grab = -10.0f, g_last_dock = -10.0f;

// ---- Village's rules (2026-10-31, from Visceral RE8 reload.cpp; settings.h CHAMBER_ROUND / MAG_GRAB_M) ----------------
// The rounds stay in the game's count while the magazine is out; what the player may do with them is decided here:
int g_chamber = 0;                // the round kept in the chamber while the magazine is out: one shot with it out
int g_loaded_out = -1;            // the game's count when it came out (a lower count later = the chambered round was fired)
bool g_dry = false;               // the chamber is empty: a rack is due once a magazine is back in
bool g_same_mag = false;          // the magazine in the hand is the one pulled from this gun: back in with its own rounds only
bool g_lt_to_seat = false;        // ... and only with LT at the magwell (a pouch magazine seats by itself)
bool g_lt_free = false;           // LT has been let go since the pull-out (the pull's own LT never seats it)
bool g_hand_on_mag = false;       // LG was pressed with the hand at the seated magazine: the hand is on it (B / LT takes it out)
int g_dock_logged = -1;           // the state the hand-on-magazine dock was last logged in (-1 = not docked)
bool g_out_to_hand = false;       // 2026-10-31: this slide-out ends in the left hand (the hand rides out on it), not on the floor
float g_closest = 99.0f;          // the nearest the magazine in the hand came to the magwell (logged when it is let go)

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
    g_gun = g_joint = g_wtf = g_wrist = g_rwrist = nullptr;
    auto* go = player_go();
    if (go == nullptr) return;
    g_claire = read_string(call_ptr(go, "get_Name")).rfind("pl1", 0) == 0;
    g_wrist = joint_by_name(call_ptr(go, "get_Transform"), L"l_arm_wrist");
    g_rwrist = joint_by_name(call_ptr(go, "get_Transform"), L"r_arm_wrist");
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
    const cfg::ShellHold h = cfg::SHELL_HOLD;   // b157: Tefa's numbers; b164: the per-character, per-gun sliders add on top
    pos = hp + rotate(hr, Vec3{h.ox, h.oy, h.oz} + tune::item_move(tune::SHELL));
    rot = qnorm(qmul(qmul(hr, quat_from_ypr(h)), tune::item_turn(tune::SHELL)));
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
    const float need = std::fmin(g_sw->dock, cfg::SHELL_DOCK_MAX_M);
    if (d > need || now - g_shell_last_dock < cfg::RELOAD_DOCK_COOLDOWN_SEC) return;
    g_shell_last_dock = now;
    LOGI("%s reload: shell at the port (%.3f m, need %.3f)", TAG, d, need);
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
    pos = hp + rotate(hr, Vec3{h.ox, h.oy, h.oz} + tune::item_move(tune::MAG));   // b164: the sliders add on top
    rot = qnorm(qmul(qmul(hr, quat_from_ypr(h)), tune::item_turn(tune::MAG)));
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
    if ((before == 0 || g_dry) && loaded() > 0) rack::need(before == 0 ? "a magazine into an empty gun" : "the chamber was fired empty with the magazine out");
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
    if (!bridge::held(bridge::S_LTRIG)) g_lt_free = true;
    if (!bridge::held(bridge::S_LGRIP)) {
        LOGI("%s reload: let go; the magazine came within %.3f m of the magwell (needs %.3f)%s", TAG, g_closest,
             std::fmax(g_w->dock, cfg::RELOAD_DOCK_MIN_M), g_same_mag ? " -- the gun's own magazine, dropped: the pouch gives a new one" : "");
        ghosthand::value("mag_closest_m", g_closest);
        ghosthand::event("mag", "fail");
        g_closest = 99.0f;
        g_same_mag = g_lt_to_seat = false;   // on the floor now: its rounds come back with the next one from the pouch (top_up)
        g_fall_from = wp;
        g_fall_rot = wr;
        set_state(St::HAND_FALL, "left grip let go");
        return;
    }
    if (!to_weapon_local(wp, wr, lp, nullptr)) return;
    const float d = dist(lp, g_w->exit), need = std::fmax(g_w->dock, cfg::RELOAD_DOCK_MIN_M);
    const float now = now_s();
    g_closest = std::fmin(g_closest, d);
    if (d > need || now - g_last_dock < cfg::RELOAD_DOCK_COOLDOWN_SEC) return;
    // RE8 2026-10-25 / 10-31 (Tefa: "LT must be pressed again for it to go back in"; "only the requirement to press LT when i
    // need to re-insert the same mag i just took out"): the gun's own magazine waits at the well for LT, pressed or held,
    // but never the LT that pulled it out
    if (g_lt_to_seat) {
        static float told = -10.0f;
        if (!g_lt_free || !bridge::held(bridge::S_LTRIG) || now - g_t0 < cfg::INSERT_LT_MIN_SEC) {
            if (now - told > 2.0f) { told = now; LOGI("%s reload: the gun's own magazine is at the magwell (%.3f m): LT seats it", TAG, d); }
            return;
        }
        g_lt_to_seat = false;
        LOGI("%s reload: LT at the magwell: the same magazine goes back in", TAG);
    }
    g_last_dock = now;
    set_state(St::INSERT, g_same_mag ? "the same magazine, back in with LT" : "brought to the magwell");
    LOGI("%s reload: insert at %.3f m from the magwell (his %.3f, ours at least %.3f)", TAG, d, g_w->dock, cfg::RELOAD_DOCK_MIN_M);
    ghosthand::value("mag_closest_m", d);
    ghosthand::event("mag", "done");
    g_closest = 99.0f;
    sfx::play("mag_insert", g_w->sfx, g_w->vol);
    bridge::rumble(bridge::LEFT, cfg::RELOAD_INSERT_BUZZ_AMP, cfg::RELOAD_INSERT_BUZZ_SEC);
}

void put_back_on_swap(const char* why) {
    g_hand_on_mag = g_out_to_hand = false;
    if (g_st == St::SEATED) return;
    LOGI("%s reload: WP%04d magazine put back as it was (%s) -- its rounds never left the gun", TAG, g_wp, why);
    if (g_joint != nullptr) write_scale(g_rest_scale);
    g_st = St::SEATED;
    g_chamber = 0;
    g_loaded_out = -1;
    g_dry = g_same_mag = g_lt_to_seat = false;
}

bool long_mag() {
    for (int wp : cfg::MAG_LONG_WP) if (wp == g_wp) return true;
    return false;
}

// the drawn left wrist to the seated magazine joint (where the tuned gun has it), metres; 99 = not known
float hand_to_mag() {
    Vec3 mp, hp;
    if (g_joint == nullptr || g_wrist == nullptr || !get_v3(JOINT, g_joint, "get_Position", mp) || !get_v3(JOINT, g_wrist, "get_Position", hp)) return 99.0f;
    tune::to_tuned(mp, nullptr);   // b171: the gun is moved by the hand tuning after the joints are read
    return dist(mp, hp);
}

// the magazine leaves the gun: dropped (falls to the floor) or into the left hand (the same magazine, back in with LT).
// RE8's real-gun rule (2026-10-31): a slide gun with rounds keeps one in the chamber; the game's count is not touched, the
// fire gate (fire_blocked) and the HUD read (hud_rounds) let only that one round be seen and fired.
void take_out(bool to_hand, const char* why) {
    if (g_joint == nullptr) { LOGW("%s reload: WP%04d joint %s not found on the gun, no drop", TAG, g_wp, g_w->joint); return; }
    Vec3 p, s;
    Quat q;
    if (!get_v3(JOINT, g_joint, "get_LocalPosition", p) || !get_q(JOINT, g_joint, "get_LocalRotation", q)) return;
    if (!get_v3(JOINT, g_joint, "get_LocalScale", s) || s.x < 0.01f) s = Vec3{1, 1, 1};
    g_rest_pos = p;
    g_rest_rot = qnorm(q);
    g_rest_scale = s;
    g_slide_from = p;
    const int n = loaded();
    g_loaded_out = n;
    g_chamber = (cfg::CHAMBER_ROUND && n > 0 && slide_weapon(g_wp) != nullptr && !is_pump(g_wp)) ? 1 : 0;
    g_dry = n <= 0;
    g_same_mag = g_lt_to_seat = to_hand;
    g_lt_free = false;
    g_hand_on_mag = false;
    g_out_to_hand = to_hand;   // 2026-10-31: it slides out either way; into the hand, the hand rides out on it (hand_socket)
    g_closest = 99.0f;
    set_state(St::SLIDE_OUT, why);
    LOGI("%s reload: %s with %d loaded (%d of them chambered, staying), %d carried; rest %.3f %.3f %.3f", TAG, to_hand ? "into the left hand" : "drop",
         n, g_chamber, reserve(), p.x, p.y, p.z);
    sfx::play("mag_drop", g_w->sfx, g_w->vol);
    if (to_hand) {
        bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
        ghosthand::event("mag", "start");
    }
    reload_block::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);   // RE8 b016: the counter says what is left at once
}

// the magazine is seated: B and the left hand (RE8 2026-10-25 and 10-30: "B only ejects the mag on pistols"; "rifles and long
// guns with mag in the middle of the gun only need LG + LT"; "just LG held and the mag just slides out after pressing B with
// hand on the mag"). 2026-10-31 (RE2 first wear, "hold on to the mag that is in the gun and not the grip itself"): an LG press
// at the magazine puts the hand ON it, every gun (hand_socket docks it there, not on the grip).
void seated_controls() {
    const bool lg = bridge::held(bridge::S_LGRIP);
    if (!lg && g_hand_on_mag) { g_hand_on_mag = false; LOGI("%s reload: LG let go: the left hand leaves the magazine", TAG); }
    if (rack::active()) return;
    if (bridge::pressed(bridge::S_LGRIP) && !g_hand_on_mag) {
        // the spot the hand would take on it (the VR layer docks within 0.15 m of that spot, so it is measured, not the joint)
        const float d = cfg::MAG_HAND_DOCK ? hand_to_dock() : hand_to_mag();
        g_hand_on_mag = d <= cfg::MAG_GRAB_M;
        LOGI("%s reload: LG pressed with the magazine seated: the hand %.3f m from %s (%.3f from the joint; takes it within %.2f)%s", TAG, d,
             cfg::MAG_HAND_DOCK ? "its spot on the magazine" : "it", hand_to_mag(), cfg::MAG_GRAB_M,
             !g_hand_on_mag ? "" : long_mag() ? ": the hand on the magazine, LT pulls it out" : ": the hand on the magazine, right B releases it into the hand");
        if (g_hand_on_mag) bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP * 0.5f, cfg::RELOAD_GRAB_BUZZ_SEC);
    }
    if (long_mag()) {
        if (g_hand_on_mag && bridge::pressed(bridge::S_LTRIG)) take_out(true, "pulled out by hand (LG on it + LT)");
        else if (bridge::pressed(bridge::S_RB)) LOGI("%s reload: WP%04d: B does nothing on this gun: LG on the magazine, then LT, pulls it out", TAG, g_wp);
        return;
    }
    if (!bridge::pressed(bridge::S_RB)) return;
    LOGI("%s reload: right B%s", TAG, g_hand_on_mag ? " with the left hand on the magazine" : lg ? " with LG held away from the magazine" : "");
    if (g_hand_on_mag) take_out(true, "right B with the left hand on the magazine");
    else take_out(false, "right B");
}

// the chambered round, while the magazine is out: fired = the gun is empty until a magazine and a rack
void watch_chamber() {
    if (g_st == St::SEATED || g_chamber <= 0) return;
    const int n = loaded();
    if (n < 0 || n >= g_loaded_out) return;
    g_chamber = 0;
    g_dry = true;
    LOGI("%s reload: WP%04d the chambered round was fired with the magazine out (%d -> %d): the gun is empty until a magazine and a rack", TAG, g_wp, g_loaded_out, n);
    g_loaded_out = n;
    reload_block::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
}

// seated again: a pouch magazine is topped up from the carried ammo; the gun's own magazine holds just its own rounds
void seat_done() {
    if (!g_same_mag) {
        top_up();
    } else {
        if (g_dry && loaded() > 0) rack::need("the gun's own magazine back in with the chamber empty");
        reload_block::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
        LOGI("%s reload: WP%04d the same magazine back in: %d rounds (no top-up)%s", TAG, g_wp, loaded(), g_dry ? ", waiting for a rack" : "");
    }
    g_chamber = 0;
    g_loaded_out = -1;
    g_dry = g_same_mag = g_lt_to_seat = false;
}

// ---- 2026-10-31: the left hand ON the magazine (Tefa: "so when inserted or ejected, hand travels with the mag") ----------
// The magazine's place in the weapon frame this frame: as the game has it while seated, as pose_joint writes it while it
// slides out or goes in.
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
    case St::INSERT: {
        const float u = ease(t / cfg::RELOAD_INSERT_SEC);
        p = Vec3{g_rest_pos.x, g_w->exit.y + (g_rest_pos.y - g_w->exit.y) * u, g_w->exit.z + (g_rest_pos.z - g_w->exit.z) * u};
        q = g_rest_rot;
        return true;
    }
    default:
        return false;
    }
}

// where the left wrist goes so the hand holds the magazine standing at (lp, lq): the magazine-in-the-hand hold undone
// (magazine = wrist * hold), moved `down` x MAG_DOCK_DOWN_M toward where it comes out, plus the MAG_DOCK sliders
bool dock_wrist(Vec3 lp, Quat lq, float down, Vec3& wp, Quat& wq) {
    Vec3 mp, a, b;
    Quat mq, qa, qb;
    const Vec3 rest = g_st == St::SEATED ? lp : g_rest_pos;
    if (!from_weapon_local(lp, lq, mp, mq) || !from_weapon_local(rest, lq, a, qa) || !from_weapon_local(g_w->exit, lq, b, qb)) return false;
    tune::to_tuned(mp, &mq);   // b171: where the TUNED gun has it
    tune::to_tuned(a, nullptr);
    tune::to_tuned(b, nullptr);
    const float len = dist(a, b);
    const Vec3 out = len > 1e-4f ? (b - a) * (1.0f / len) : Vec3{0, 0, 0};
    const Hold& h = g_claire ? g_w->claire : g_w->leon;
    const Quat hq = qnorm(qmul(quat_from_ypr(h), tune::item_turn(tune::MAG)));
    const Vec3 ho = Vec3{h.ox, h.oy, h.oz} + tune::item_move(tune::MAG);
    wq = qnorm(qmul(mq, qinv(hq)));
    wp = mp - rotate(wq, ho) + out * (cfg::MAG_DOCK_DOWN_M * down) + rotate(wq, tune::hand_move(tune::MAG_DOCK));
    wq = qnorm(qmul(wq, tune::ypr(tune::hand_turn_deg(tune::MAG_DOCK))));
    return true;
}

// the drawn left wrist to the spot it would take on the seated magazine, metres; 99 = not known
float hand_to_dock() {
    Vec3 lp, wp, hp;
    Quat lq, wq;
    if (g_wrist == nullptr || !mag_local_now(lp, lq) || !dock_wrist(lp, lq, 1.0f, wp, wq) || !get_v3(JOINT, g_wrist, "get_Position", hp)) return 99.0f;
    return dist(wp, hp);
}
} // namespace

// ---- the public side --------------------------------------------------------------------------------------------------
// headset only: without it there is no B, no left hand and no pouch, so the game keeps its own reload
bool managed_now() { return cfg::RELOAD_ON && (g_w != nullptr || g_sw != nullptr) && g_gun != nullptr && bridge::live(); }
bool mag_out() { return managed_now() && g_w != nullptr && g_st != St::SEATED; }
bool fire_blocked() { return mag_out() && g_chamber <= 0; }
int hud_rounds() { return mag_out() ? g_chamber : -1; }
bool session_active() { return mag_out() || g_shell_in_hand; }
int wp_now() { return g_wp; }
bool mag_in_hand() { return managed_now() && g_w != nullptr && (g_st == St::IN_HAND || g_st == St::INSERT); }
bool shell_in_hand() { return managed_now() && g_shell_in_hand; }
bool hand_docked() {
    if (!cfg::MAG_HAND_DOCK || !managed_now() || g_w == nullptr || g_joint == nullptr) return false;
    return (g_st == St::SEATED && g_hand_on_mag) || (g_st == St::SLIDE_OUT && g_out_to_hand) || g_st == St::INSERT;
}
int hand_socket(Vec3& p, Quat& q, bool& steer) {
    if (!cfg::MAG_HAND_DOCK || !managed_now() || g_w == nullptr) return -1;
    if (g_st == St::IN_HAND) return 0;   // the magazine is in the hand: the hand is the controller's, the grip must not catch it
    if (!hand_docked()) { g_dock_logged = -1; return cfg::MAG_ONLY_DOCK && !long_mag() ? 2 : -1; }   // b184: no grip-handle dock
    Vec3 lp, wp, rp;
    Quat lq, wq, rq;
    const float down = g_st == St::INSERT ? ease((now_s() - g_t0) / cfg::RELOAD_INSERT_SEC) : 1.0f;
    if (!mag_local_now(lp, lq) || !dock_wrist(lp, lq, down, wp, wq) || g_rwrist == nullptr || !get_v3(JOINT, g_rwrist, "get_Position", rp) ||
        !get_q(JOINT, g_rwrist, "get_Rotation", rq))
        return 0;
    const Quat inv = qinv(qnorm(rq));
    p = rotate(inv, wp - rp);
    q = qnorm(qmul(inv, wq));
    steer = false;   // b184 (Tefa: "docking hand on the mag turns the weapon"): the right hand alone aims, always
    if (g_dock_logged != static_cast<int>(g_st)) {
        g_dock_logged = static_cast<int>(g_st);
        LOGI("%s reload: the left hand docked on the magazine (%s): %.3f %.3f %.3f from the right wrist", TAG, st_name(g_st), p.x, p.y, p.z);
    }
    return 1;
}
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
            if (g_out_to_hand && bridge::held(bridge::S_LGRIP)) {   // 2026-10-31: out, the hand lets go of the gun and carries it
                g_out_to_hand = false;
                set_state(St::IN_HAND, "out of the magwell, in the left hand");
                break;
            }
            if (g_out_to_hand) {   // LG let go while it slid out: it falls, and the pouch gives a new one
                LOGI("%s reload: LG let go while the magazine slid out: it falls", TAG);
                ghosthand::event("mag", "fail");
                g_same_mag = g_lt_to_seat = g_out_to_hand = false;
            }
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
            seat_done();
            // 2026-10-31 (RE8 2026-10-24: "the hand stay on the bottom of the mag and follow it fully until it's in the gun"):
            // seated with LG still held, the hand stays on the magazine until LG is let go
            g_hand_on_mag = cfg::MAG_HAND_DOCK && bridge::held(bridge::S_LGRIP);
        }
        break;
    default:
        break;
    }
    watch_chamber();

    // in normal play (FirstPerson driving: no menu, no cutscene): right B / the left hand take a seated magazine out
    const float fp = bridge::view(bridge::S_FP_USED);
    const bool in_play = bridge::live() && !menu_body::is_menu_open() && bridge::has(fp) && fp > 0.5f;
    if (in_play && g_st == St::SEATED) seated_controls();
    else if (g_st == St::SEATED) g_hand_on_mag = false;
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
    g_same_mag = g_lt_to_seat = false;   // a fresh one: topped up from the carried ammo, seats at the magwell by itself
    g_closest = 99.0f;
    set_state(St::IN_HAND, "taken from the pouch");
    ghosthand::event("mag", "start");
    sfx::play("mag_grab", g_w->sfx, g_w->vol);
    bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
    return true;
}

} // namespace vn::reload
