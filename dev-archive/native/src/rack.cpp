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
#include "ghosthand.h"
#include "tune.h"
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
MO* g_gun_tf = nullptr;   // b163: the gun's transform (its +Z is the barrel)
MO* g_rwrist = nullptr;
Vec3 g_rest{};            // the joint's own LOCAL pose at rest, read once per weapon (his numbers are offsets from it:
bool g_have_rest = false; // b140 wrote them as positions and the W-870's fore-end went to the gun's origin, worn 2026-10-10)

bool g_needed = false;    // a rack is required
bool g_parked = false;    // the slide is locked open (empty gun, or needed)
bool g_hand_on = false;   // the hand is docked on the slide / fore-end
bool g_pulled = false;    // reached the back this cycle
float g_travel = 0.0f;    // 0 = rest/parked, 1 = back
float g_base = 0.0f;      // dot(left - right, forward) at the dock
float g_max_pull = 0.0f;  // 2026-10-13 Ghost Hand: the furthest pull this attempt
float g_pull_m = 0.0f;    // how far back the hand has moved since the dock (metres along the gun)
int g_log_near = 0;
int g_auto = 0;           // button mode: 0 idle, 1 travelling back / held back while LT is held, 2 travelling forward
bool g_grip = false;      // b155: the left hand is gripping the slide / fore-end (LG held near it, or racking)
bool g_socket_frozen = false;   // b157 (unused since b159)
bool g_let_go = false;    // b159: LT let go after a rack: the hand leaves the slide until LG is let go
bool g_armed = false;     // b165: the pull is live (pump: docked on LT; pistol: LT pressed with the hand on)
bool g_spent = false;     // b165: a pump cycle finished on this LT press: nothing until LT is pressed again
bool g_hold_closed = false;   // b159: after a rack, keep the slide closed until the next shot (the magnum stayed open)
float g_auto_t0 = 0.0f;

// the socket handed to the VR layer: the slide joint (plus an offset) in right-wrist space
// v2 (b176, 2026-10-14, Tefa: "the grip ending up in 2 places ... the wrist spot and the actual hand" on the shotgun, and
// "stopping left hand from moving the pistol when hand is on the slider"): two more words the VR layer reads --
//   steer: 1 = the docked left hand may turn the gun (the two-handed aim), 0 = the right hand alone aims while the
//          hand sits on our socket (a pistol slide being racked)
//   owned: 1 = this gun's grip is the plugin's business: when our socket is NOT active the VR layer must not dock the
//          hand on the gun's own aid joint either (a pump gun: one grip spot, ours, instead of two)
struct VisceralDockSocket { int version; int active; float px, py, pz; float qx, qy, qz, qw; int steer; int owned; };
VisceralDockSocket g_socket{2, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0};
} // namespace
extern "C" __declspec(dllexport) VisceralDockSocket* visceral_dock_socket() { return &g_socket; }
namespace {

// b171: proves the gun-slider correction on the log: once per dock, how far it moved the point (0 = already tuned)
float g_tuned_seen = -1.0f;
void note_tuned(float moved, const char* where) {
    if (moved > g_tuned_seen + 0.005f) {
        LOGI("%s rack: gun sliders: the %s point moved %.3f m to the tuned gun (was the untuned one this frame)", TAG, where, moved);
        g_tuned_seen = moved;
    }
}

MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }

void resolve_frame() {
    g_joint = g_gun = g_rwrist = g_gun_tf = nullptr;
    auto* go = player_go();
    auto* gun = field_obj(component(go, "app.ropeway.survivor.Equipment"), "<EquipWeapon>k__BackingField");
    if (gun == nullptr || g_w == nullptr) return;
    g_gun = gun;
    auto* wtf = call_ptr(call_ptr(gun, "get_GameObject"), "get_Transform");
    g_gun_tf = wtf;
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
    note_tuned(tune::to_tuned(jp, nullptr), "grab");   // b171
    d = dist(jp, hp);
    return d <= cfg::RACK_HAND_M;
}

// the slide's travel in the gun's frame: his offsets from the real rest; parked a little further back (Tefa)
float z_parked() { return g_rest.z + (g_w->parked_z - g_w->rest_z) * cfg::SLIDE_PARK_SCALE; }
float z_from() { return g_parked ? z_parked() : g_rest.z; }
float z_back() { return g_rest.z + (g_w->back_z - g_w->rest_z); }

void write_joint() {
    if (g_joint == nullptr || g_w == nullptr || !g_have_rest) return;
    if (!g_hand_on && !g_parked && g_travel <= 0.0f && !g_hold_closed) return;   // nothing of ours: leave the game's own animation alone
    const float z = z_from() + (z_back() - z_from()) * g_travel;
    Vec3 p{g_rest.x, g_rest.y, z};
    set_any(JOINT, g_joint, "set_LocalPosition", &p);
}

// the socket for the VR layer: where the slide is now, relative to the right wrist
void publish_socket() {
    // b176: the pump gun's grip is ours alone and keeps steering; a pistol on our socket is aimed by the right hand only
    const bool pump = g_wp >= 0 && is_pump(g_wp);
    g_socket.owned = pump ? 1 : 0;
    g_socket.steer = pump ? 1 : 0;
    if (g_socket_frozen && g_grip) return;   // b157: keep the last socket (active) while LG is still held
    g_socket.active = 0;
    if (!(g_hand_on || g_grip) || g_joint == nullptr || g_rwrist == nullptr) return;
    Vec3 jp, rp;
    Quat jq, rq;
    if (!get_v3(JOINT, g_joint, "get_Position", jp) || !get_q(JOINT, g_joint, "get_Rotation", jq)) return;
    if (!get_v3(JOINT, g_rwrist, "get_Position", rp) || !get_q(JOINT, g_rwrist, "get_Rotation", rq)) return;
    note_tuned(tune::to_tuned(jp, &jq), "socket");   // b171: where the TUNED gun has the slide / fore-end
    const Quat inv = qinv(qnorm(rq));
    // b158: where the hand sits on the slide and how it is turned, from the Lua menu sliders while they are live
    // b164: Tefa's numbers built in, the per-character, per-gun sliders (tune.h) added on top
    const Vec3 off = cfg::SLIDE_DOCK_OFF + tune::hand_move(tune::SLIDE);
    const Vec3 add = tune::hand_turn_deg(tune::SLIDE);
    const float ypr[3] = {cfg::SLIDE_DOCK_YPR[0] + add.x, cfg::SLIDE_DOCK_YPR[1] + add.y, cfg::SLIDE_DOCK_YPR[2] + add.z};
    const float h = 3.14159265f / 360.0f;   // half-angle, radians; yaw about Z, then pitch about Y, then roll about X
    const Quat turn = qnorm(qmul(qmul(Quat{0, 0, std::sin(ypr[0] * h), std::cos(ypr[0] * h)}, Quat{0, std::sin(ypr[1] * h), 0, std::cos(ypr[1] * h)}),
                                 Quat{std::sin(ypr[2] * h), 0, 0, std::cos(ypr[2] * h)}));
    const Vec3 world = jp + rotate(qnorm(jq), off);
    const Vec3 local = rotate(inv, world - rp);
    const Quat lrot = qnorm(qmul(inv, qmul(qnorm(jq), turn)));
    g_socket.px = local.x; g_socket.py = local.y; g_socket.pz = local.z;
    g_socket.qx = lrot.x; g_socket.qy = lrot.y; g_socket.qz = lrot.z; g_socket.qw = lrot.w;
    g_socket.active = 1;
}

// his motion drive: the left controller's place along the gun, relative to the right controller (room space)
// b162 (Ghost Hand, 2026-10-13): the gun's axis is frozen at the dock, so the gun hand turning mid-rack no longer
// moves the starting point (Tefa: the pump's push back "sometimes needing me to extend the left hand far out")
Vec3 g_axis{0, 0, -1};
// b163 (worn b162 00:15: "the pump handle didn't move at all"; log: 0.349 m off the line): the controller's -Z is
// tilted well off the barrel on Touch controllers, so the axis is now the GUN's own forward (+Z, the slide and the
// fore-end travel along it) carried into the room; the controller's -Z only if that fails
bool gun_axis(Vec3& fwd) {
    Quat gq;
    Vec3 gp{};
    if (g_gun_tf != nullptr && get_q(XFORM, g_gun_tf, "get_Rotation", gq) && (tune::to_tuned(gp, &gq), true) &&
        ghosthand::to_room_dir(rotate(qnorm(gq), Vec3{0, 0, 1}), fwd)) return true;
    const Quat rq = bridge::view_quat(bridge::S_RROT);
    if (!bridge::has(rq.x)) return false;
    fwd = rotate(qnorm(rq), Vec3{0, 0, -1});   // OpenXR: a controller points along -Z
    return true;
}
float along_gun() {
    const Vec3 fwd = g_axis;
    const Vec3 d = bridge::hand_pos(bridge::LEFT) - bridge::hand_pos(bridge::RIGHT);
    return d.x * fwd.x + d.y * fwd.y + d.z * fwd.z;        // bigger = further forward along the gun
}

// b162 (Tefa 2026-10-13: "the pump handle area is off, so i had to draw the left motion controller very close to my
// right one"): for a pump gun the left CONTROLLER anywhere along the barrel ahead of the gun hand docks, not the game's
// wrist near the fore-end joint
bool hand_on_barrel(float& d) {
    // b163: the left controller near the fore-end joint itself, both in the room
    Vec3 jp, jr;
    if (g_joint != nullptr && get_v3(JOINT, g_joint, "get_Position", jp) && (note_tuned(tune::to_tuned(jp, nullptr), "grab"), true) &&
        ghosthand::to_room_point(jp, jr)) {
        d = dist(bridge::hand_pos(bridge::LEFT), jr);
        return d <= cfg::PUMP_GRAB_M;
    }
    Vec3 fwd;
    if (!gun_axis(fwd)) return false;
    const Vec3 v = bridge::hand_pos(bridge::LEFT) - bridge::hand_pos(bridge::RIGHT);
    const float ahead = v.x * fwd.x + v.y * fwd.y + v.z * fwd.z;
    const Vec3 off = v - fwd * ahead;
    d = std::sqrt(off.x * off.x + off.y * off.y + off.z * off.z);
    return ahead >= cfg::PUMP_ZONE_AHEAD_MIN_M && ahead <= cfg::PUMP_ZONE_AHEAD_MAX_M && d <= cfg::PUMP_ZONE_OFF_AXIS_M;
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
    g_hold_closed = true;   // b159: the game's own animation may still hold the slide open (magnum, worn b158)
    sfx::play("slide_rack_release", pump ? pump_sfx(g_wp) : reload::sfx_folder_now(), pump ? 2.0f : reload::sfx_volume_now());
    bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP, cfg::RACK_BUZZ_SEC);
    reload_block::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
    LOGI("%s rack: WP%04d %s cycle complete (%s), %d rounds loaded", TAG, g_wp, pump ? "pump" : "slide", was_needed ? "was needed: fire unblocked" : "cosmetic", g_gun ? call_direct<int>(g_gun, "getBulletNumber", -1) : -1);
}

void hand_off(const char* why) {
    g_hand_on = false;
    g_tuned_seen = -1.0f;   // b171: log the correction again at the next dock
    g_socket.active = 0;
    const bool armed = g_armed;
    g_armed = false;
    if (!cfg::RACK_ON_BUTTON) g_auto = 0;
    if (g_pulled) { g_pulled = false; g_travel = 0.0f; complete(); ghosthand::event("rack", "done"); LOGI("%s rack: hand off while pulled: released (%s)", TAG, why); }
    else if (!armed) LOGI("%s rack: hand off the %s (%s)", TAG, is_pump(g_wp) ? "fore-end" : "slide", why);
    else {
        ghosthand::value("rack_max_pull_m", g_max_pull);
        ghosthand::event("rack", g_max_pull > 0.01f ? "fail" : "cancel");
        LOGI("%s rack: hand off the %s (%s), furthest pull %.3f m", TAG, is_pump(g_wp) ? "fore-end" : "slide", why, g_max_pull);
    }
}
} // namespace

bool blocks_fire() {
    // b165 (Tefa 2026-10-13): "guns can't fire during LG + LT held, that has to be a rule"
    if (cfg::NO_FIRE_WHILE_LG_LT && bridge::live() && bridge::held(bridge::S_LGRIP) && bridge::held(bridge::S_LTRIG)) return true;
    return cfg::RELOAD_ON && g_w != nullptr && g_needed;
}
bool active() { return g_hand_on; }
bool grip_pose() { return g_grip; }
void on_shot() { if (g_hold_closed) LOGI("%s rack: first shot after the rack: the slide is the game's again", TAG); g_hold_closed = false; }
int wp_now() { return g_wp; }
float travel() { return g_travel; }
float pull_m() { return g_hand_on ? g_pull_m : 0.0f; }
void* slide_joint() { return g_joint; }
// b173: the dock's own zone test, for the opening hand (d = how far, metres)
bool in_grab_zone(float& d) {
    d = 99.0f;
    if (g_w == nullptr || g_joint == nullptr || g_gun == nullptr) return false;
    return is_pump(g_wp) ? hand_on_barrel(d) : hand_near_joint(d);
}
// b172: the game's left wrist to the slide / fore-end joint (where the tuned gun has it), metres; 99 = no such gun
float hand_dist_m() {
    if (g_w == nullptr || g_joint == nullptr) return 99.0f;
    Vec3 jp, hp;
    auto* wrist = joint_by_name(call_ptr(player_go(), "get_Transform"), L"l_arm_wrist");
    if (wrist == nullptr || !get_v3(JOINT, g_joint, "get_Position", jp) || !get_v3(JOINT, wrist, "get_Position", hp)) return 99.0f;
    tune::to_tuned(jp, nullptr);
    return dist(jp, hp);
}

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
        g_needed = g_parked = g_hand_on = g_pulled = g_grip = g_let_go = g_hold_closed = g_armed = g_spent = false;
        g_auto = 0;
        g_travel = 0.0f;
        g_have_rest = false;
        g_log_near = 0;
        g_socket.active = 0;
        if (g_w != nullptr) LOGI("%s rack: WP%04d %s: %s on joint %s (rest %.3f, parked %.3f, back %.3f)", TAG, wp, weapons::name(wp), is_pump(wp) ? "pump" : "slide rack", g_w->joint, g_w->rest_z, g_w->parked_z, g_w->back_z);
    }
    resolve_frame();
    if (g_w == nullptr || g_gun == nullptr || !bridge::live()) { g_grip = false; if (g_hand_on) hand_off("no gun / no headset"); return; }
    if (!is_pump(g_wp) && !g_parked && !reload::mag_out() && call_direct<int>(g_gun, "getBulletNumber", -1) == 0) slide_lock_empty();   // b142: polled
    const bool menu = menu_body::is_menu_open();
    const bool lg = bridge::held(bridge::S_LGRIP), lt = bridge::held(bridge::S_LTRIG);

    // 2026-10-11 the release way: LG held + LT pressed = the whole rack / pump cycle, run by itself (no hand tracking)
    if (cfg::RACK_ON_BUTTON) {
        const float now = now_s();
        if (g_auto == 0 && !menu && lg && bridge::pressed(bridge::S_LTRIG) && !reload::session_active()) {
            g_auto = 1;
            g_auto_t0 = now;
            bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP * 0.5f, cfg::RACK_BUZZ_SEC);
            LOGI("%s rack: LG+LT: WP%04d %s on the button", TAG, g_wp, is_pump(g_wp) ? "pump" : "slide rack");
        }
        // b155 (Tefa 2026-10-11): LG + LT held pulls the slide back and HOLDS it there; LT let go (LG still held) =
        // the slide flies forward on its own, the hand stays. Let go before it reached the back = it returns, no round.
        if (g_auto == 1) {
            const float t = (now - g_auto_t0) / cfg::RACK_BUTTON_PULL_SEC;
            g_travel = std::fmin(1.0f, t);
            if (t >= 1.0f && !g_pulled) {
                g_pulled = true;
                sfx::play("slide_rack_pull", is_pump(g_wp) ? pump_sfx(g_wp) : reload::sfx_folder_now(), is_pump(g_wp) ? 2.0f : reload::sfx_volume_now());
                bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP, cfg::RACK_BUZZ_SEC);
                if (is_pump(g_wp)) pump_native::on_pulled_down();   // the spent shell leaves on the pull, not the return
                LOGI("%s rack: held back (LT held)", TAG);
            }
            if (!lt || !lg || menu) {
                LOGI("%s rack: %s %s", TAG, !lt ? "LT let go" : !lg ? "LG let go" : "menu", g_pulled ? "-> the slide goes forward" : "before the back -> it returns, no round");
                g_let_go = true;   // b159 (Tefa: "my hand still remains held on the gun"): the hand comes off the slide
                g_auto = 2;
                g_auto_t0 = now;
            }
        } else if (g_auto == 2) {
            const float t = (now - g_auto_t0) / cfg::RACK_BUTTON_RETURN_SEC;
            g_travel = std::fmax(0.0f, 1.0f - t);
            if (t >= 1.0f) { g_travel = 0.0f; g_auto = 0; if (g_pulled) { g_pulled = false; complete(); } }
        }
        // the grip pose: LG held with the left hand near the slide / fore-end, or a rack under way
        float d = 0.0f;
        const bool near = lg && !menu && !reload::session_active() && hand_near_joint(d);
        if (!lg) g_let_go = false;
        const bool grip = (g_auto == 1 || near) && !g_let_go;
        if (grip != g_grip) LOGI("%s rack: left hand %s the %s (wrist %.3f m, need %.2f)", TAG, grip ? "grips" : "lets go of", is_pump(g_wp) ? "fore-end" : "slide", d, cfg::RACK_HAND_M);
        // b157 (Tefa: "hand still doesn't move onto the slide"): the hand docks on the slide while it grips; once on,
        // it stays on until LG is let go (the wrist is then AT the slide, so the near test would flicker)
        if (g_grip && lg && !menu && !grip && !g_let_go) { /* keep gripping while LG is held */ } else g_grip = grip;
        if (!lg) g_socket_frozen = false;
        static bool was_docked = false;
        if (g_grip != was_docked) LOGI("%s rack: hand %s the %s (socket)", TAG, g_grip ? "docked on" : "off", is_pump(g_wp) ? "fore-end" : "slide");
        was_docked = g_grip;
        publish_socket();
        return;
    }

    // b165 (Tefa 2026-10-13, after b163/b164 worn: "letting go of LT after each rack, then holding it again - the racking
    // works really well! so this has to be built into the reloading system"):
    //   pump:   LG + LT near the fore-end docks the hand and arms ONE cycle; LT held for the pull down; once down,
    //           letting LT go sends the handle (and the hand on it) back up by itself and chambers (b166, Tefa: "because
    //           it's a two handed weapon and LG has to be held at all times - releasing LT makes the hand with the pump
    //           handle jump back up"). Pushing it up by hand with LT still held works too. Then nothing until LT again.
    //   pistol: only when the gun is completely empty (slide locked open) or a rack is needed. LG near the slide = the
    //           hand on the slide; LT (with the hand on) = the pull is live and must be held until the slide is all the way
    //           back; once back, LT does nothing and letting LG go makes it jump forward (b166, Tefa: "LT is just a latch
    //           to make the slide and pump handle movable").
    const bool pump = is_pump(g_wp);
    const bool may_rack = pump || g_parked || g_needed;   // pistols: only an empty / locked-open gun (Tefa)
    if (!lt) g_spent = false;
    if (!g_hand_on && !menu && !reload::session_active() && may_rack && !g_spent) {
        float d = 0.0f;
        const bool want = pump ? (lg && bridge::pressed(bridge::S_LTRIG)) : lg;
        if (want) {
            const bool near = pump ? hand_on_barrel(d) : hand_near_joint(d);
            if ((pump || bridge::pressed(bridge::S_LGRIP)) && g_log_near < 30) { ++g_log_near; LOGI("%s rack: %s, left hand %.3f m from the %s (need %.2f): %s", TAG, pump ? "LG+LT" : "LG", d, pump ? "fore-end" : "slide", pump ? cfg::PUMP_GRAB_M : cfg::RACK_HAND_M, near ? "HAND ON" : "too far"); }
            if (near) {
                g_hand_on = true;
                g_pulled = false;
                g_armed = false;
                bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP * 0.5f, cfg::RACK_BUZZ_SEC);
            } else if (pump) {
                ghosthand::value("dock_miss_m", d);   // pressed too far from the fore-end: the hand never docked
            }
        }
    }
    if (g_hand_on) {
        // arm the pull: the pump at once (it docked on the LT press), the pistol on an LT press with the hand on
        if (!g_armed && lt && (pump || bridge::pressed(bridge::S_LTRIG))) {
            g_armed = true;
            if (!gun_axis(g_axis)) g_axis = Vec3{0, 0, -1};
            g_base = along_gun();
            g_pull_m = g_max_pull = 0.0f;
            ghosthand::event("rack", "start");
        }
        if (menu) hand_off("menu");
        else if (pump && g_pulled && !lt && g_auto != 2) {   // b166: down and LT let go: the handle goes back up by itself
            g_auto = 2;
            g_auto_t0 = now_s();
            LOGI("%s rack: LT let go with the handle down: it goes back up", TAG);
        }
        else if (pump && !g_pulled && !lt) hand_off("LT let go before the handle was down");
        else if (pump && !lg) hand_off("LG let go");
        else if (!pump && !lg) hand_off(g_pulled ? "LG let go: the slide jumps forward" : "LG let go");
        else if (!pump && !g_pulled && g_armed && !lt) {   // the pull let go before the back: it springs back, the hand stays
            g_armed = false;
            ghosthand::value("rack_max_pull_m", g_max_pull);
            ghosthand::event("rack", g_max_pull > 0.01f ? "fail" : "cancel");
        }
    }

    // the pull: the left controller moving back along the gun, relative to the right one
    if (g_hand_on && g_armed && g_auto == 2) {   // b166: the pump handle on its way back up by itself
        const float t = (now_s() - g_auto_t0) / cfg::RACK_BUTTON_RETURN_SEC;
        g_travel = std::fmax(0.0f, 1.0f - t);
        if (t >= 1.0f) { g_auto = 0; g_spent = lt; hand_off("pump cycle done"); }
    } else if (g_hand_on && g_armed) {
        g_pull_m = g_base - along_gun();   // positive = pulled back
        g_max_pull = std::fmax(g_max_pull, g_pull_m);
        // b162: on the way forward the travel is measured from the furthest-back point, not from the dock spot
        const float want = !g_pulled ? std::fmax(0.0f, std::fmin(1.0f, g_pull_m / cfg::RACK_PULL_M))
                                     : std::fmax(0.0f, std::fmin(1.0f, 1.0f - (g_max_pull - g_pull_m) / cfg::RACK_RETURN_M));
        if (pump || !g_pulled) g_travel = g_travel + (want - g_travel) * cfg::RACK_FOLLOW;
        else g_travel = 1.0f;   // b165: the pistol's slide stays back until LG and LT are both let go
        if (!g_pulled && g_travel >= 0.97f) {
            g_pulled = true;
            sfx::play("slide_rack_pull", pump ? pump_sfx(g_wp) : reload::sfx_folder_now(), pump ? 2.0f : reload::sfx_volume_now());
            bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP, cfg::RACK_BUZZ_SEC);
            if (pump) pump_native::on_pulled_down();   // the spent shell leaves on the pull, not the return
            LOGI("%s rack: pulled back (%.3f m)%s", TAG, g_pull_m, pump ? "" : ": held back until LG and LT are let go");
        }
        if (pump && g_pulled && lt && g_travel <= 0.10f) {   // pushed back up by hand: one cycle per LT press
            g_spent = true;
            hand_off("pump cycle done: LT again for the next one");
        }
    } else if (!g_pulled && g_travel > 0.0f) {
        g_travel = std::fmax(0.0f, g_travel - cfg::RACK_RETURN_PER_SEC / 72.0f);   // let go mid-way: it springs back
    }
    publish_socket();
}

void late_point() { if (g_w != nullptr) write_joint(); }
void render_point() { if (g_w != nullptr) write_joint(); }

} // namespace vn::rack
