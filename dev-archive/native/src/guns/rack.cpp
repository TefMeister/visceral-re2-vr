// rack.cpp -- see rack.h. The pull is measured from the controllers in the room (the bridge), never from the docked wrist
// joint (that would read our own output back); the gun's axis is its own +Z carried into the room, frozen at the start of
// the pull. Andyalpa's motion drive (ext_2 C.7), rewritten; credit: Andyalpa.
#include "rack.h"
#include "ammo.h"
#include "bridge.h"
#include "game_reload.h"
#include "ghosthand.h"
#include "gun_data.h"
#include "gun_frame.h"
#include "guns_api.h"
#include "joints.h"
#include "magazine.h"
#include "menu_body.h"
#include "pump.h"
#include "settings.h"
#include "sfx.h"

#include <cmath>
#include <map>

namespace vn::guns::rack {

using namespace joints;

namespace {
const SlideGun* g_w = nullptr;
int g_wp = -1;
MO* g_joint = nullptr;           // this frame's slide / fore-end joint
Vec3 g_rest{};                   // the joint's own local rest (his numbers are offsets from it)
bool g_have_rest = false;
std::map<int, Vec3> g_rest_by_wp;   // read once per gun: a gun coming back with its slide open must not take that as rest
struct Kept { bool needed, parked; };
std::map<int, Kept> g_kept;

bool g_needed = false;           // a rack is due
bool g_parked = false;           // the slide is locked open
bool g_hand_on = false;          // the hand is on the slide / fore-end
bool g_grip = false;             // a pump gun's fore-end gripped with LG
bool g_armed = false;            // the pull is live
bool g_pulled = false;           // reached the back this cycle
bool g_returning = false;        // the pump handle let go at the bottom, going back up by itself
bool g_spent = false;            // a pump cycle finished on this LT press: nothing until LT is pressed again
bool g_hold_closed = false;      // after a rack the slide is kept closed until the next shot
float g_travel = 0.0f;           // 0 = rest (or parked), 1 = all the way back
float g_base = 0.0f;             // the hands' place along the gun when the pull was armed
float g_pull_m = 0.0f, g_max_pull = 0.0f;
float g_return_t0 = 0.0f;
Vec3 g_axis{0, 0, -1};

bool pump() { return g_w != nullptr && is_pump(g_wp); }
const char* sfx_folder() { return pump() ? pump_sfx(g_wp) : magazine::sfx_folder(); }
float sfx_vol() { return pump() ? 2.0f : magazine::sfx_volume(); }

void find_joint() {
    g_joint = (g_w != nullptr && gf().gun_tf != nullptr) ? joint_by_name(gf().gun_tf, widen(g_w->joint).c_str()) : nullptr;
    if (g_joint == nullptr || g_have_rest) return;
    if (auto it = g_rest_by_wp.find(g_wp); it != g_rest_by_wp.end()) { g_rest = it->second; g_have_rest = true; return; }
    Vec3 p;
    if (get_v3(JOINT, g_joint, "get_LocalPosition", p)) { g_rest = p; g_have_rest = true; g_rest_by_wp[g_wp] = p; }
}

float z_parked() { return g_rest.z + (g_w->parked_z - g_w->rest_z) * cfg::SLIDE_PARK_SCALE; }
float z_from() { return g_parked ? z_parked() : g_rest.z; }
float z_back() { return g_rest.z + (g_w->back_z - g_w->rest_z); }

void write_joint() {
    if (g_joint == nullptr || g_w == nullptr || !g_have_rest) return;
    if (!g_hand_on && !g_parked && g_travel <= 0.0f && !g_hold_closed) return;   // nothing of ours: the game's own animation
    Vec3 p{g_rest.x, g_rest.y, z_from() + (z_back() - z_from()) * g_travel};
    set_any(JOINT, g_joint, "set_LocalPosition", &p);
}

// the drawn left wrist near the slide joint (only "near?": the pull itself reads the controllers)
bool hand_near_slide(float reach) {
    Vec3 jp, hp;
    return get_v3(JOINT, g_joint, "get_Position", jp) && get_v3(JOINT, gf().lwrist, "get_Position", hp) && dist(jp, hp) <= reach;
}

bool gun_axis(Vec3& fwd) {
    Quat gq;
    if (get_q(XFORM, gf().gun_tf, "get_Rotation", gq) && ghosthand::to_room_dir(rotate(qnorm(gq), Vec3{0, 0, 1}), fwd)) return true;
    const Quat rq = bridge::view_quat(bridge::S_RROT);
    if (!bridge::has(rq.x)) return false;
    fwd = rotate(qnorm(rq), Vec3{0, 0, -1});   // OpenXR: a controller points along -Z
    return true;
}
float along_gun() {   // bigger = the left controller further forward along the gun, relative to the right one
    const Vec3 d = bridge::hand_pos(bridge::LEFT) - bridge::hand_pos(bridge::RIGHT);
    return d.x * g_axis.x + d.y * g_axis.y + d.z * g_axis.z;
}

// the left controller on the pump: near the fore-end joint, or anywhere along the barrel ahead of the gun hand
bool hand_on_barrel() {
    Vec3 jp, jr;
    if (g_joint != nullptr && get_v3(JOINT, g_joint, "get_Position", jp) && ghosthand::to_room_point(jp, jr))
        return dist(bridge::hand_pos(bridge::LEFT), jr) <= cfg::PUMP_GRAB_M;
    Vec3 fwd;
    if (!gun_axis(fwd)) return false;
    const Vec3 v = bridge::hand_pos(bridge::LEFT) - bridge::hand_pos(bridge::RIGHT);
    const float ahead = v.x * fwd.x + v.y * fwd.y + v.z * fwd.z;
    const Vec3 off = v - fwd * ahead;
    const float side = std::sqrt(off.x * off.x + off.y * off.y + off.z * off.z);
    return ahead >= cfg::PUMP_ZONE_AHEAD_MIN_M && ahead <= cfg::PUMP_ZONE_AHEAD_MAX_M && side <= cfg::PUMP_ZONE_OFF_AXIS_M;
}

// the cycle completed: a rack that was due lets the gun fire again; one that was not is only for show
void complete() {
    if (gf().gun != nullptr && (pump() || g_needed || g_parked)) ammo::end_reload(pump());
    const bool was_needed = g_needed;
    g_needed = g_parked = false;
    g_hold_closed = true;   // the game's own animation may still hold the slide open
    sfx::play("slide_rack_release", sfx_folder(), sfx_vol());
    bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP, cfg::RACK_BUZZ_SEC);
    game_reload::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
    LOGI("%s rack: WP%04d %s done (%s), %d loaded", TAG, g_wp, pump() ? "pump" : "slide", was_needed ? "the gun fires again" : "for show", ammo::loaded());
}

void reached_back() {
    g_pulled = true;
    sfx::play("slide_rack_pull", sfx_folder(), sfx_vol());
    bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP, cfg::RACK_BUZZ_SEC);
    if (pump()) pump::on_pulled_down();   // the spent shell leaves on the pull, not the return
}

void hand_off(const char* why) {
    const bool armed = g_armed;
    g_hand_on = g_armed = g_returning = false;
    if (g_pulled) { g_pulled = false; g_travel = 0.0f; complete(); ghosthand::event("rack", "done"); return; }
    if (armed) {
        ghosthand::value("rack_max_pull_m", g_max_pull);
        ghosthand::event("rack", g_max_pull > 0.01f ? "fail" : "cancel");
    }
    LOGI("%s rack: hand off the %s (%s)", TAG, pump() ? "fore-end" : "slide", why);
}

// LG (and LT for the pump) puts the hand on the slide / fore-end
void take_hold(bool lg, bool menu) {
    const bool p = pump();
    const bool any_time = !p && !g_parked && !g_needed;   // a cosmetic rack: only on a fresh LG press right at the slide
    g_grip = p && lg && !menu && !guns::session_active() && (g_grip || hand_on_barrel());
    if (g_hand_on || menu || guns::session_active() || magazine::hand_docked() || g_spent) return;
    const bool want = p ? (lg && bridge::pressed(bridge::S_LTRIG)) : any_time ? bridge::pressed(bridge::S_LGRIP) : lg;
    if (!want) return;
    const bool near = p ? (g_grip || hand_on_barrel()) : hand_near_slide(any_time ? cfg::RACK_ANYTIME_HAND_M : cfg::RACK_HAND_M);
    if (!near) return;
    g_hand_on = true;
    g_pulled = g_armed = false;
    bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP * 0.5f, cfg::RACK_BUZZ_SEC);
}

// with the hand on: arm the pull, follow the hand, let go
void hand_on(bool lg, bool lt, bool menu) {
    const bool p = pump();
    if (!g_armed && lt && (p || bridge::pressed(bridge::S_LTRIG))) {
        g_armed = true;
        if (!gun_axis(g_axis)) g_axis = Vec3{0, 0, -1};
        g_base = along_gun();
        g_pull_m = g_max_pull = 0.0f;
        ghosthand::event("rack", "start");
    }
    if (menu) { hand_off("menu"); return; }
    if (p && g_pulled && !lt && !g_returning) { g_returning = true; g_return_t0 = now_s(); }
    else if (p && !g_pulled && !lt) { hand_off("LT let go before the handle was down"); return; }
    else if (!lg) { hand_off(g_pulled ? "LG let go: the slide flies forward" : "LG let go"); return; }
    else if (!p && !g_pulled && g_armed && !lt) {   // let go before the back: it springs back, the hand stays
        g_armed = false;
        ghosthand::value("rack_max_pull_m", g_max_pull);
        ghosthand::event("rack", g_max_pull > 0.01f ? "fail" : "cancel");
    }
    if (g_returning) {   // the pump handle on its way back up by itself
        const float t = (now_s() - g_return_t0) / cfg::RACK_AUTO_RETURN_SEC;
        g_travel = std::fmax(0.0f, 1.0f - t);
        if (t >= 1.0f) { g_spent = lt; hand_off("pump cycle done"); }
        return;
    }
    if (!g_armed) return;
    g_pull_m = g_base - along_gun();   // positive = pulled back
    g_max_pull = std::fmax(g_max_pull, g_pull_m);
    // forward again is measured from the furthest-back point
    const float want = !g_pulled ? std::fmax(0.0f, std::fmin(1.0f, g_pull_m / cfg::RACK_PULL_M))
                                 : std::fmax(0.0f, std::fmin(1.0f, 1.0f - (g_max_pull - g_pull_m) / cfg::RACK_RETURN_M));
    if (p || !g_pulled) g_travel += (want - g_travel) * cfg::RACK_FOLLOW;
    else g_travel = 1.0f;   // a pistol's slide stays back until LG is let go
    if (!g_pulled && g_travel >= 0.97f) reached_back();
    if (p && g_pulled && g_travel <= 0.10f) { g_spent = true; hand_off("pump cycle done: LT again for the next"); }   // pushed up by hand
}
} // namespace

void on_gun_changed(int old_wp, int new_wp) {
    if (g_w != nullptr && old_wp >= 0) g_kept[old_wp] = Kept{g_needed, g_parked};
    g_wp = new_wp;
    g_w = slide_gun(new_wp);
    g_needed = g_parked = g_hand_on = g_grip = g_armed = g_pulled = g_returning = g_spent = g_hold_closed = false;
    if (auto it = g_kept.find(new_wp); g_w != nullptr && it != g_kept.end()) {
        g_needed = it->second.needed;
        g_parked = it->second.parked;
        g_kept.erase(it);
    }
    g_travel = 0.0f;
    g_have_rest = false;
}

void frame() {
    find_joint();
    if (!cfg::RELOAD_ON || g_w == nullptr || gf().gun == nullptr || !bridge::live()) {
        g_grip = false;
        if (g_hand_on) hand_off("no gun / no headset");
        return;
    }
    if (!pump() && !g_parked && !magazine::mag_out() && ammo::loaded() == 0) slide_lock_empty();
    const bool menu = menu_body::is_menu_open();
    const bool lg = bridge::held(bridge::S_LGRIP), lt = bridge::held(bridge::S_LTRIG);
    if (!lt) g_spent = false;
    take_hold(lg, menu);
    if (g_hand_on) hand_on(lg, lt, menu);
    const bool pulling = g_hand_on && (g_armed || g_returning);
    if (!pulling && !g_pulled && g_travel > 0.0f) g_travel = std::fmax(0.0f, g_travel - cfg::RACK_RETURN_PER_SEC / 72.0f);   // let go mid-way: springs back
}

void late_point() { find_joint(); write_joint(); }
void render_point() { find_joint(); write_joint(); }

void need(const char* why) {
    if (g_w == nullptr) return;
    if (!g_needed) LOGI("%s rack: WP%04d %s due (%s): the trigger waits for it", TAG, g_wp, pump() ? "a pump" : "a rack", why);
    g_needed = g_parked = true;
}
void slide_lock_empty() { if (g_w != nullptr && !pump()) g_parked = true; }
void on_shot() { g_hold_closed = false; }
bool blocks_fire() {
    if (cfg::NO_FIRE_WHILE_LG_LT && bridge::live() && bridge::held(bridge::S_LGRIP) && bridge::held(bridge::S_LTRIG)) return true;
    return cfg::RELOAD_ON && g_w != nullptr && g_needed;
}
bool active() { return g_hand_on; }
bool owns_grip() { return pump(); }

bool socket(Vec3& p, Quat& q, bool& steer) {
    steer = pump();
    if (!(g_hand_on || g_grip) || g_joint == nullptr) return false;
    Vec3 jp, rp;
    Quat jq, rq;
    if (!get_v3(JOINT, g_joint, "get_Position", jp) || !get_q(JOINT, g_joint, "get_Rotation", jq) ||
        !get_v3(JOINT, gf().rwrist, "get_Position", rp) || !get_q(JOINT, gf().rwrist, "get_Rotation", rq))
        return false;
    const Quat inv = qinv(qnorm(rq));
    const Quat turn = ypr_deg(cfg::SLIDE_DOCK_YPR[0], cfg::SLIDE_DOCK_YPR[1], cfg::SLIDE_DOCK_YPR[2]);
    p = rotate(inv, jp + rotate(qnorm(jq), cfg::SLIDE_DOCK_OFF) - rp);
    q = qnorm(qmul(inv, qmul(qnorm(jq), turn)));
    return true;
}

float travel() { return g_travel; }
float pull_m() { return g_hand_on ? g_pull_m : 0.0f; }
void* slide_joint() { return g_joint; }

} // namespace vn::guns::rack
