// rack.cpp -- see rack.h.
//
// b143 (Tefa 2026-10-10 20:20): "LT puts the hand on the gun and then I have to actually do the slide racking movement
// with my hands". So: LG held + LT pressed with the left hand near the slide = the hand DOCKS ON THE SLIDE (our socket
// handed to the VR layer: visceral_dock_socket, FirstPerson.cpp b143 patch); then the LEFT controller moving BACK along
// the gun, relative to the right controller, pulls the slide (his motion drive, ext_2 C.7: dot(P_left - P_right, pull
// axis) against a baseline taken at the dock); forward again returns it and chambers. LT released = hand off. The
// controllers are read in room space from the bridge (never the docked wrist joint: that would read our own output
// back, port map §D); the gun's axis = the right controller's forward.
#include "rack.h"
#include "bridge.h"
#include "common.h"
#include "joints.h"
#include "menu_body.h"
#include "pump_native.h"
#include "reload.h"
#include "reload_block.h"
#include "reload_data.h"
#include "settings.h"
#include "sfx.h"
#include "weapons.h"

namespace vn::rack {

using namespace joints;
using namespace reload;   // the weapon tables (reload_data.h)

namespace {
const SlideWeapon* g_w = nullptr;
int g_wp = -1;
MO* g_joint = nullptr;    // this frame's slide / fore-end joint
MO* g_gun = nullptr;
MO* g_rwrist = nullptr;
Vec3 g_rest{};            // the joint's own LOCAL pose at rest, read once per weapon (his numbers are offsets from it:
bool g_have_rest = false; // b140 wrote them as positions and the W-870's fore-end went to the gun's origin, worn 2026-10-10)

bool g_needed = false;    // a rack is required
bool g_parked = false;    // the slide is locked open (empty gun, or needed)
bool g_hand_on = false;   // the hand is docked on the slide / fore-end
bool g_pulled = false;    // reached the back this cycle
float g_travel = 0.0f;    // 0 = rest/parked, 1 = back
float g_base = 0.0f;      // dot(left - right, forward) at the dock
float g_pull_m = 0.0f;    // how far back the hand has moved since the dock (metres along the gun)
int g_log_near = 0;

// the socket handed to the VR layer: the slide joint (plus an offset) in right-wrist space
struct VisceralDockSocket { int version; int active; float px, py, pz; float qx, qy, qz, qw; };
VisceralDockSocket g_socket{1, 0, 0, 0, 0, 0, 0, 0, 1};
} // namespace
extern "C" __declspec(dllexport) VisceralDockSocket* visceral_dock_socket() { return &g_socket; }
namespace {

MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }

void resolve_frame() {
    g_joint = g_gun = g_rwrist = nullptr;
    auto* go = player_go();
    auto* gun = field_obj(component(go, "app.ropeway.survivor.Equipment"), "<EquipWeapon>k__BackingField");
    if (gun == nullptr || g_w == nullptr) return;
    g_gun = gun;
    auto* wtf = call_ptr(call_ptr(gun, "get_GameObject"), "get_Transform");
    g_joint = joint_by_name(wtf, widen(g_w->joint).c_str());
    g_rwrist = joint_by_name(call_ptr(go, "get_Transform"), L"r_arm_wrist");
    if (g_joint != nullptr && !g_have_rest) {
        Vec3 p;
        if (get_v3(JOINT, g_joint, "get_LocalPosition", p)) { g_rest = p; g_have_rest = true; LOGI("%s rack: WP%04d joint %s rests at local %.3f %.3f %.3f (his rest %.3f)", TAG, g_wp, g_w->joint, p.x, p.y, p.z, g_w->rest_z); }
    }
}

// "near?" only: the left wrist joint stands in for the controller (never used for the pull itself)
bool hand_near_joint(float& d) {
    Vec3 jp, hp;
    auto* wrist = joint_by_name(call_ptr(player_go(), "get_Transform"), L"l_arm_wrist");
    if (!get_v3(JOINT, g_joint, "get_Position", jp) || !get_v3(JOINT, wrist, "get_Position", hp)) return false;
    d = dist(jp, hp);
    return d <= cfg::RACK_HAND_M;
}

// the slide's travel in the gun's frame: his offsets from the real rest; parked a little further back (Tefa)
float z_parked() { return g_rest.z + (g_w->parked_z - g_w->rest_z) * cfg::SLIDE_PARK_SCALE; }
float z_from() { return g_parked ? z_parked() : g_rest.z; }
float z_back() { return g_rest.z + (g_w->back_z - g_w->rest_z); }

void write_joint() {
    if (g_joint == nullptr || g_w == nullptr || !g_have_rest) return;
    if (!g_hand_on && !g_parked && g_travel <= 0.0f) return;   // nothing of ours: leave the game's own animation alone
    const float z = z_from() + (z_back() - z_from()) * g_travel;
    Vec3 p{g_rest.x, g_rest.y, z};
    set_any(JOINT, g_joint, "set_LocalPosition", &p);
}

// the socket for the VR layer: where the slide is now, relative to the right wrist
void publish_socket() {
    g_socket.active = 0;
    if (!g_hand_on || g_joint == nullptr || g_rwrist == nullptr) return;
    Vec3 jp, rp;
    Quat jq, rq;
    if (!get_v3(JOINT, g_joint, "get_Position", jp) || !get_q(JOINT, g_joint, "get_Rotation", jq)) return;
    if (!get_v3(JOINT, g_rwrist, "get_Position", rp) || !get_q(JOINT, g_rwrist, "get_Rotation", rq)) return;
    const Quat inv = qinv(qnorm(rq));
    const Vec3 world = jp + rotate(qnorm(jq), cfg::SLIDE_DOCK_OFF);
    const Vec3 local = rotate(inv, world - rp);
    const Quat lrot = qnorm(qmul(inv, qnorm(jq)));
    g_socket.px = local.x; g_socket.py = local.y; g_socket.pz = local.z;
    g_socket.qx = lrot.x; g_socket.qy = lrot.y; g_socket.qz = lrot.z; g_socket.qw = lrot.w;
    g_socket.active = 1;
}

// his motion drive: the left controller's place along the gun, relative to the right controller (room space)
float along_gun() {
    const Quat rq = bridge::view_quat(bridge::S_RROT);
    if (!bridge::has(rq.x)) return 0.0f;
    const Vec3 fwd = rotate(qnorm(rq), Vec3{0, 0, -1});   // OpenXR: a controller points along -Z
    const Vec3 d = bridge::hand_pos(bridge::LEFT) - bridge::hand_pos(bridge::RIGHT);
    return d.x * fwd.x + d.y * fwd.y + d.z * fwd.z;        // bigger = further forward along the gun
}

void complete() {
    const bool pump = is_pump(g_wp);
    if (g_gun != nullptr) {
        reload_block::Commit c;
        call_direct<void*>(g_gun, "executeEndReload", nullptr);
        if (pump) call_direct<void*>(g_gun, "executeEndEject", nullptr);
    }
    const bool was_needed = g_needed;
    g_needed = false;
    g_parked = false;
    sfx::play("slide_rack_release", pump ? pump_sfx(g_wp) : reload::sfx_folder_now(), pump ? 2.0f : reload::sfx_volume_now());
    bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP, cfg::RACK_BUZZ_SEC);
    reload_block::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
    LOGI("%s rack: WP%04d %s cycle complete (%s), %d rounds loaded", TAG, g_wp, pump ? "pump" : "slide", was_needed ? "was needed: fire unblocked" : "cosmetic", g_gun ? call_direct<int>(g_gun, "getBulletNumber", -1) : -1);
}

void hand_off(const char* why) {
    g_hand_on = false;
    g_socket.active = 0;
    if (g_pulled) { g_pulled = false; g_travel = 0.0f; complete(); LOGI("%s rack: hand off while pulled: released (%s)", TAG, why); }
    else LOGI("%s rack: hand off the %s (%s)", TAG, is_pump(g_wp) ? "fore-end" : "slide", why);
}
} // namespace

bool blocks_fire() { return cfg::RELOAD_ON && g_w != nullptr && g_needed; }
bool active() { return g_hand_on; }
int wp_now() { return g_wp; }

void need(const char* why) {
    if (g_w == nullptr) return;
    if (!g_needed) LOGI("%s rack: WP%04d %s NEEDED (%s): the gun will not fire until then", TAG, g_wp, is_pump(g_wp) ? "pump" : "slide rack", why);
    g_needed = true;
    g_parked = true;
}

void slide_lock_empty() {
    if (g_w == nullptr || is_pump(g_wp)) return;
    if (!g_parked) LOGI("%s rack: WP%04d empty: the slide locks open", TAG, g_wp);
    g_parked = true;
}

void frame() {
    if (!cfg::RELOAD_ON) return;
    const int wp = weapons::current_id();
    if (wp != g_wp) {
        if (g_needed) LOGI("%s rack: WP%04d put away with a rack pending -- forgotten (the game's own state rules)", TAG, g_wp);
        g_wp = wp;
        g_w = slide_weapon(wp);
        g_needed = g_parked = g_hand_on = g_pulled = false;
        g_travel = 0.0f;
        g_have_rest = false;
        g_socket.active = 0;
        if (g_w != nullptr) LOGI("%s rack: WP%04d %s: %s on joint %s (rest %.3f, parked %.3f, back %.3f)", TAG, wp, weapons::name(wp), is_pump(wp) ? "pump" : "slide rack", g_w->joint, g_w->rest_z, g_w->parked_z, g_w->back_z);
    }
    resolve_frame();
    if (g_w == nullptr || g_gun == nullptr || !bridge::live()) { if (g_hand_on) hand_off("no gun / no headset"); return; }
    if (!is_pump(g_wp) && !g_parked && !reload::mag_out() && call_direct<int>(g_gun, "getBulletNumber", -1) == 0) slide_lock_empty();   // b142: polled
    const bool menu = menu_body::is_menu_open();
    const bool lg = bridge::held(bridge::S_LGRIP), lt = bridge::held(bridge::S_LTRIG);

    // the hand goes on the slide / fore-end: LT pressed with LG held and the hand near it; LT or LG let go = off
    if (!g_hand_on && !menu && lg && bridge::pressed(bridge::S_LTRIG) && !reload::session_active()) {
        float d = 0.0f;
        const bool near = hand_near_joint(d);
        if (g_log_near < 30) { ++g_log_near; LOGI("%s rack: LG+LT, left wrist %.3f m from the %s joint (need %.2f): %s", TAG, d, is_pump(g_wp) ? "fore-end" : "slide", cfg::RACK_HAND_M, near ? "HAND ON" : "too far"); }
        if (near) {
            g_hand_on = true;
            g_pulled = false;
            g_base = along_gun();
            g_pull_m = 0.0f;
            bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP * 0.5f, cfg::RACK_BUZZ_SEC);
        }
    } else if (g_hand_on && (!lt || !lg || menu)) {
        hand_off(!lt ? "LT let go" : !lg ? "LG let go" : "menu");
    }

    // the pull: the left controller moving back along the gun, relative to the right one
    if (g_hand_on) {
        g_pull_m = g_base - along_gun();   // positive = pulled back
        const float want = std::fmax(0.0f, std::fmin(1.0f, g_pull_m / cfg::RACK_PULL_M));
        g_travel = g_travel + (want - g_travel) * cfg::RACK_FOLLOW;
        if (!g_pulled && g_travel >= 0.97f) {
            g_pulled = true;
            sfx::play("slide_rack_pull", is_pump(g_wp) ? pump_sfx(g_wp) : reload::sfx_folder_now(), is_pump(g_wp) ? 2.0f : reload::sfx_volume_now());
            bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP, cfg::RACK_BUZZ_SEC);
            if (is_pump(g_wp)) pump_native::on_pulled_down();   // the spent shell leaves on the pull, not the return
            LOGI("%s rack: pulled back (%.3f m)", TAG, g_pull_m);
        }
        if (g_pulled && g_travel <= 0.10f) { g_pulled = false; g_travel = 0.0f; complete(); }
    } else if (g_travel > 0.0f) {
        g_travel = std::fmax(0.0f, g_travel - cfg::RACK_RETURN_PER_SEC / 72.0f);   // let go mid-way: it springs back
    }
    publish_socket();
}

void late_point() { if (g_w != nullptr) write_joint(); }
void render_point() { if (g_w != nullptr) write_joint(); }

} // namespace vn::rack
