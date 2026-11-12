// magazine.cpp -- see magazine.h. The state machine is RELOADED's (Andyalpa's ext_1, read as the specification and
// rewritten), with Village's rules of 2026-10-31; drawn by magazine_pose.cpp.
#include "magazine_internal.h"
#include "ammo.h"
#include "bridge.h"
#include "game_reload.h"
#include "ghosthand.h"
#include "gun_frame.h"
#include "joints.h"
#include "menu_body.h"
#include "rack.h"
#include "settings.h"
#include "sfx.h"

#include <cmath>
#include <map>

namespace vn::guns::magazine {

using namespace joints;
using namespace detail;

namespace detail {
St g_st = St::SEATED;
const MagGun* g_w = nullptr;
MO* g_joint = nullptr;
float g_t0 = 0.0f;
Vec3 g_rest_pos{}, g_rest_scale{1, 1, 1}, g_slide_from{}, g_fall_from{};
Quat g_rest_rot{0, 0, 0, 1}, g_fall_rot{0, 0, 0, 1};
bool g_scale_restore = false;
bool g_hand_on_mag = false;
bool g_out_to_hand = false;
} // namespace detail

namespace {
int g_chamber = 0;           // the round kept in the chamber while the magazine is out
int g_mag_rounds = 0;        // the rounds in the magazine that is out (magrounds.cpp shows or hides them)
int g_loaded_out = -1;       // the game's count when it came out (lower later = the chambered round was fired)
bool g_dry = false;          // the chamber is empty: a rack is due once a magazine is back in
bool g_same_mag = false;     // the magazine in the hand is the one pulled from this gun
bool g_lt_to_seat = false;   // ... so it goes back in only on a fresh LT at the magwell
bool g_lt_free = false;      // LT has been let go since the pull-out
float g_last_grab = -10.0f, g_last_dock = -10.0f;
float g_closest = 99.0f;     // the nearest the magazine in the hand came to the magwell (logged when it is let go)

struct Kept { Vec3 pos, scale; Quat rot; int chamber, loaded_out; bool dry; int mag_rounds; };
std::map<int, Kept> g_kept;  // guns put away with their magazine out

const char* st_name(St s) {
    switch (s) {
    case St::SEATED: return "seated";       case St::SLIDE_OUT: return "sliding out"; case St::FALL: return "falling";
    case St::OUT: return "out";             case St::IN_HAND: return "in the left hand";
    case St::HAND_FALL: return "dropped from the hand"; case St::INSERT: return "going in";
    }
    return "?";
}

void set_state(St s, const char* why) {
    LOGI("%s magazine: WP%04d %s -> %s (%s)", TAG, gf().wp, st_name(g_st), st_name(s), why);
    g_st = s;
    g_t0 = now_s();
}

bool long_mag() {
    return g_w != nullptr && guns::long_mag(g_w->wp);
}
bool keeps_chamber() { return slide_gun(g_w->wp) != nullptr && !is_pump(g_w->wp); }

void find_joint() {
    g_joint = (g_w != nullptr && gf().gun_tf != nullptr) ? joint_by_name(gf().gun_tf, widen(g_w->joint).c_str()) : nullptr;
}

// the magazine leaves the gun: dropped (it falls) or into the left hand (the same magazine, back in with LT)
void take_out(bool to_hand, const char* why) {
    if (g_joint == nullptr) { LOGW("%s magazine: WP%04d joint %s not found, nothing taken out", TAG, gf().wp, g_w->joint); return; }
    Vec3 p, s;
    Quat q;
    if (!get_v3(JOINT, g_joint, "get_LocalPosition", p) || !get_q(JOINT, g_joint, "get_LocalRotation", q)) return;
    if (!get_v3(JOINT, g_joint, "get_LocalScale", s) || s.x < 0.01f) s = Vec3{1, 1, 1};
    g_rest_pos = g_slide_from = p;
    g_rest_rot = qnorm(q);
    g_rest_scale = s;
    const int n = ammo::loaded();
    g_loaded_out = n;
    g_chamber = (cfg::CHAMBER_ROUND && n > 0 && keeps_chamber()) ? 1 : 0;
    g_dry = n <= 0;
    g_mag_rounds = n > g_chamber ? n - g_chamber : 0;
    g_same_mag = g_lt_to_seat = g_out_to_hand = to_hand;
    g_lt_free = g_hand_on_mag = false;
    g_closest = 99.0f;
    set_state(St::SLIDE_OUT, why);
    LOGI("%s magazine: out with %d loaded (%d chambered, staying), %d carried", TAG, n, g_chamber, ammo::reserve());
    sfx::play("mag_drop", g_w->sfx, g_w->vol);
    if (to_hand) {
        bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
        ghosthand::event("mag", "start");
    }
    game_reload::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
}

// seated: an LG press at the magazine puts the hand on it; B (handguns) or LT (long guns, hand on it) takes it out
void seated_controls() {
    const bool lg = bridge::held(bridge::S_LGRIP);
    if (!lg) g_hand_on_mag = false;
    if (rack::active()) return;
    if (bridge::pressed(bridge::S_LGRIP) && !g_hand_on_mag) {
        const float d = hand_to_dock();
        g_hand_on_mag = d <= cfg::MAG_GRAB_M;
        if (g_hand_on_mag) {
            LOGI("%s magazine: the left hand on it (%.3f m): %s", TAG, d, long_mag() ? "LT pulls it out" : "right B releases it into the hand");
            bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP * 0.5f, cfg::RELOAD_GRAB_BUZZ_SEC);
        }
    }
    if (long_mag()) {
        if (g_hand_on_mag && bridge::pressed(bridge::S_LTRIG)) take_out(true, "pulled out by hand (LG on it + LT)");
        return;
    }
    if (bridge::pressed(bridge::S_RB)) take_out(g_hand_on_mag, g_hand_on_mag ? "right B with the left hand on it" : "right B");
}

// the chambered round, while the magazine is out: fired = the gun is empty until a magazine and a rack
void watch_chamber() {
    if (g_st == St::SEATED || g_chamber <= 0) return;
    const int n = ammo::loaded();
    if (n < 0 || n >= g_loaded_out) return;
    g_chamber = 0;
    g_dry = true;
    g_loaded_out = n;
    LOGI("%s magazine: the chambered round was fired with the magazine out: empty until a magazine and a rack", TAG);
    game_reload::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
}

// seated again: a pouch magazine is topped up from the rounds carried; the gun's own holds just its own rounds
void seat_done() {
    if (!g_same_mag) {
        const int before = ammo::top_up();
        if ((before == 0 || g_dry) && ammo::loaded() > 0) rack::need(before == 0 ? "a magazine into an empty gun" : "the chamber was fired empty");
    } else {
        ammo::end_reload(false);
        if (g_dry && ammo::loaded() > 0) rack::need("its own magazine back in with the chamber empty");
        LOGI("%s magazine: its own magazine back in: %d rounds%s", TAG, ammo::loaded(), g_dry ? ", a rack is due" : "");
    }
    game_reload::show_ammo_counter(cfg::HUD_AFTER_RELOAD_SEC);
    g_chamber = 0;
    g_loaded_out = -1;
    g_dry = g_same_mag = g_lt_to_seat = false;
}

// in the left hand: the grip let go drops it; brought to the magwell it goes in (the gun's own one needs LT)
void hand_checks() {
    if (g_st != St::IN_HAND) return;
    Vec3 wp, lp;
    Quat wr;
    if (!hand_pose(wp, wr)) return;
    if (!bridge::held(bridge::S_LTRIG)) g_lt_free = true;
    if (!bridge::held(bridge::S_LGRIP)) {
        LOGI("%s magazine: let go; it came within %.3f m of the magwell (needs %.3f)", TAG, g_closest, std::fmax(g_w->dock, cfg::RELOAD_DOCK_MIN_M));
        ghosthand::value("mag_closest_m", g_closest);
        ghosthand::event("mag", "fail");
        g_same_mag = g_lt_to_seat = false;   // on the floor now: the next one from the pouch is topped up
        g_fall_from = wp;
        g_fall_rot = wr;
        set_state(St::HAND_FALL, "left grip let go");
        return;
    }
    if (!to_gun_local(wp, wr, lp, nullptr)) return;
    const float d = dist(lp, g_w->exit), need = std::fmax(g_w->dock, cfg::RELOAD_DOCK_MIN_M);
    const float now = now_s();
    g_closest = std::fmin(g_closest, d);
    if (d > need || now - g_last_dock < cfg::RELOAD_DOCK_COOLDOWN_SEC) return;
    if (g_lt_to_seat) {
        if (!g_lt_free || !bridge::held(bridge::S_LTRIG) || now - g_t0 < cfg::INSERT_LT_MIN_SEC) return;
        g_lt_to_seat = false;
    }
    g_last_dock = now;
    set_state(St::INSERT, g_same_mag ? "its own magazine, back in with LT" : "brought to the magwell");
    ghosthand::value("mag_closest_m", d);
    ghosthand::event("mag", "done");
    g_closest = 99.0f;
    sfx::play("mag_insert", g_w->sfx, g_w->vol);
    bridge::rumble(bridge::LEFT, cfg::RELOAD_INSERT_BUZZ_AMP, cfg::RELOAD_INSERT_BUZZ_SEC);
}

void timers() {
    const float t = now_s() - g_t0;
    switch (g_st) {
    case St::SLIDE_OUT:
        if (t < cfg::RELOAD_SLIDE_SEC) return;
        if (g_out_to_hand && bridge::held(bridge::S_LGRIP)) { g_out_to_hand = false; set_state(St::IN_HAND, "out of the magwell, in the left hand"); return; }
        if (g_out_to_hand) { ghosthand::event("mag", "fail"); g_same_mag = g_lt_to_seat = g_out_to_hand = false; }
        if (!from_gun_local(g_w->exit, g_rest_rot, g_fall_from, g_fall_rot)) g_fall_from = Vec3{0, 0, 0};
        set_state(St::FALL, "out of the magwell");
        return;
    case St::FALL:
    case St::HAND_FALL:
        if (t < cfg::RELOAD_FALL_SEC) return;
        set_state(St::OUT, "on the floor");
        sfx::play("mag_floor", g_w->sfx, g_w->vol);
        return;
    case St::INSERT:
        if (t < cfg::RELOAD_INSERT_SEC) return;
        set_state(St::SEATED, "seated");
        g_scale_restore = true;
        seat_done();
        g_hand_on_mag = bridge::held(bridge::S_LGRIP);   // seated with LG still held: the hand stays on it until let go
        return;
    default:
        return;
    }
}
} // namespace

void on_gun_changed(int old_wp, int new_wp) {
    g_hand_on_mag = g_out_to_hand = false;
    if (g_w != nullptr && g_st != St::SEATED) {
        if (cfg::MAG_KEEP_OUT_ON_SWAP) {
            g_kept[old_wp] = Kept{g_rest_pos, g_rest_scale, g_rest_rot, g_chamber, g_loaded_out, g_dry, g_mag_rounds};
            if (g_joint != nullptr) write_scale(Vec3{0, 0, 0});
            LOGI("%s magazine: WP%04d put away with its magazine out: it stays out", TAG, old_wp);
        } else if (g_joint != nullptr) {
            write_scale(g_rest_scale);
        }
    }
    g_st = St::SEATED;
    g_chamber = 0;
    g_loaded_out = -1;
    g_dry = g_same_mag = g_lt_to_seat = false;
    g_w = mag_gun(new_wp);
    if (auto it = g_kept.find(new_wp); g_w != nullptr && it != g_kept.end()) {
        const Kept k = it->second;
        g_kept.erase(it);
        g_rest_pos = k.pos; g_rest_scale = k.scale; g_rest_rot = k.rot;
        g_chamber = k.chamber; g_loaded_out = k.loaded_out; g_dry = k.dry; g_mag_rounds = k.mag_rounds;
        g_st = St::OUT;
        g_t0 = now_s();
        LOGI("%s magazine: WP%04d back with its magazine still out", TAG, new_wp);
    }
}

bool ours() { return cfg::RELOAD_ON && g_w != nullptr && gf().gun != nullptr && bridge::live(); }
bool mag_out() { return ours() && g_st != St::SEATED; }
bool fire_blocked() { return mag_out() && g_chamber <= 0; }
int rounds_in_mag() {
    if (!ours()) return -1;
    if (g_st != St::SEATED) return g_mag_rounds;
    const int n = ammo::loaded();
    if (n <= 0) return 0;
    // seated: a slide gun holds one of its rounds in the chamber, unless a rack is still due (then all are in the magazine)
    return (keeps_chamber() && !rack::blocks_fire()) ? n - 1 : n;
}
int chambered() {
    if (!ours()) return -1;
    if (g_st != St::SEATED) return g_chamber > 0 ? 1 : 0;
    return (ammo::loaded() > 0 && keeps_chamber() && !rack::blocks_fire()) ? 1 : 0;
}
int hud_rounds() { return mag_out() ? g_chamber : -1; }
bool session_active() { return mag_out(); }
bool hand_docked() {
    if (!ours() || g_joint == nullptr) return false;
    return (g_st == St::SEATED && g_hand_on_mag) || (g_st == St::SLIDE_OUT && g_out_to_hand) || g_st == St::INSERT;
}
const char* sfx_folder() { return g_w ? g_w->sfx : "handgun"; }
float sfx_volume() { return g_w ? g_w->vol : 1.0f; }

int hand_socket(Vec3& p, Quat& q) {
    if (!ours()) return -1;
    if (g_st == St::IN_HAND) return 0;
    if (!hand_docked()) return long_mag() ? -1 : 2;   // handguns: the magazine is the only left-hand spot, never the grip
    Vec3 lp, wp, rp;
    Quat lq, wq, rq;
    const float down = g_st == St::INSERT ? ease((now_s() - g_t0) / cfg::RELOAD_INSERT_SEC) : 1.0f;
    if (!mag_local_now(lp, lq) || !dock_wrist(lp, lq, down, wp, wq) || !get_v3(JOINT, gf().rwrist, "get_Position", rp) ||
        !get_q(JOINT, gf().rwrist, "get_Rotation", rq))
        return 0;
    const Quat inv = qinv(qnorm(rq));
    p = rotate(inv, wp - rp);
    q = qnorm(qmul(inv, wq));
    return 1;
}

bool pouch_grab() {
    if (!ours() || g_st != St::OUT) return false;
    const float now = now_s();
    if (now - g_last_grab < cfg::RELOAD_GRAB_COOLDOWN_SEC) return true;
    g_last_grab = now;
    if (ammo::reserve() <= 0 && ammo::loaded() <= 0) {
        LOGI("%s magazine: the pouch is empty (no rounds carried)", TAG);
        bridge::rumble(bridge::LEFT, cfg::RELOAD_DENY_BUZZ_AMP, cfg::RELOAD_DENY_BUZZ_SEC);
        return true;
    }
    g_same_mag = g_lt_to_seat = false;   // a fresh one: topped up, seats at the magwell by itself
    g_mag_rounds = ammo::reserve() > 0 ? ammo::reserve() : 0;   // what it will bring (the top-up takes at most the capacity)
    g_closest = 99.0f;
    set_state(St::IN_HAND, "taken from the pouch");
    ghosthand::event("mag", "start");
    sfx::play("mag_grab", g_w->sfx, g_w->vol);
    bridge::rumble(bridge::LEFT, cfg::RELOAD_GRAB_BUZZ_AMP, cfg::RELOAD_GRAB_BUZZ_SEC);
    return true;
}

void frame() {
    find_joint();
    if (!ours()) return;
    timers();
    watch_chamber();
    const float fp = bridge::view(bridge::S_FP_USED);   // normal play: FirstPerson driving, no menu, no cutscene
    const bool in_play = !menu_body::is_menu_open() && bridge::has(fp) && fp > 0.5f;
    if (g_st != St::SEATED) return;
    if (in_play) seated_controls();
    else g_hand_on_mag = false;
}

void late_point() {
    find_joint();
    if (!ours()) return;
    hand_checks();
    pose_joint();
}

void render_point() {
    find_joint();
    if (ours()) pose_joint();
}

} // namespace vn::guns::magazine
