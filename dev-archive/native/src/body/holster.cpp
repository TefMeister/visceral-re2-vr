// holster.cpp -- see holster.h
//
// 2026-11-12, Tefa: six spots on every character (both hips, both shoulders, back left and back right), the same places
// for both hands; LG and RG in a spot do different things. A hand is in the NEAREST spot within reach, so two spots that
// overlap never both answer, and the spots can be big.
//
//   spot             LG (left hand)                      RG (right hand)
//   left shoulder    the torch: tap out / tap back; LT   shortcut LEFT
//   right shoulder   -                                   shortcut UP
//   left hip         the ammo pouch                      shortcut DOWN
//   right hip        -                                   shortcut RIGHT
//   back left        medicine (a later step: buzzes)     -
//   back right       -                                   the sub weapon (subweapon.cpp reads the press itself)
#include "holster.h"
#include "bridge.h"
#include "guns_api.h"
#include "settings.h"
#include "shortcut.h"
#include "flashlight.h"
#include "weapons.h"

namespace vn::holster {

namespace {
using bridge::Hand;
constexpr int NONE = -1;

enum class Act { NOTHING, TORCH, POUCH, MEDICINE, SHORTCUT, SUB_WEAPON };

struct Spot {
    const char* name;
    cfg::ZoneOffset off;
    Act left;               // what LG does here
    Act right;              // what RG does here
    int dir;                // the shortcut slot RG takes out here (Tefa's mapping), or NONE
};

// Tefa 2026-10-05: right = right hip, bottom = left hip, left = left shoulder, top = right shoulder
const Spot SPOTS[] = {
    {"left shoulder",  cfg::LEFT_SHOULDER,  Act::TORCH,    Act::SHORTCUT,   shortcut::LEFT},
    {"right shoulder", cfg::RIGHT_SHOULDER, Act::NOTHING,  Act::SHORTCUT,   shortcut::UP},
    {"left hip",       cfg::LEFT_HIP,       Act::POUCH,    Act::SHORTCUT,   shortcut::DOWN},
    {"right hip",      cfg::RIGHT_HIP,      Act::NOTHING,  Act::SHORTCUT,   shortcut::RIGHT},
    {"back left",      cfg::BACK_LEFT,      Act::MEDICINE, Act::NOTHING,    NONE},
    {"back right",     cfg::BACK_RIGHT,     Act::NOTHING,  Act::SUB_WEAPON, NONE},
};
constexpr int N_SPOTS = (int)(sizeof(SPOTS) / sizeof(SPOTS[0]));
constexpr int LEFT_SHOULDER_SPOT = 0;
constexpr int BACK_RIGHT_SPOT = 5;

int g_in[2] = {NONE, NONE};   // the spot each hand is in (index bridge::LEFT / bridge::RIGHT)
int g_frame = 0;
int g_last_weapon = -2;
int g_last_slots[4] = {-2, -2, -2, -2};

Act act_of(int spot, Hand h) { return spot < 0 ? Act::NOTHING : (h == bridge::LEFT ? SPOTS[spot].left : SPOTS[spot].right); }

// headset position + offset turned by the headset's YAW only: the spots turn left and right with the head and never
// tilt (Tefa 2026-10-05: "no tilt please, just left and right turn"). OpenXR space: +X right, +Y up, -Z forward.
Vec3 zone_pos(const cfg::ZoneOffset& o) {
    Vec3 f = rotate(bridge::hmd_rot(), Vec3{0, 0, -1});
    f.y = 0;
    const float len = std::sqrt(f.x * f.x + f.z * f.z);
    if (len < 1e-4f) f = {0, 0, -1}; else f = f * (1.0f / len);
    const Vec3 right{-f.z, 0, f.x};
    return bridge::hmd_pos() + right * o.side + Vec3{0, o.up, 0} + f * o.fwd;
}

// the nearest spot within reach; the one the hand is in keeps it out to ZONE_LEAVE_M unless another is within reach and
// nearer
void update_hand(Hand h) {
    const Vec3 hp = bridge::hand_pos(h);
    float d[N_SPOTS];
    int best = 0;
    for (int i = 0; i < N_SPOTS; ++i) {
        d[i] = dist(hp, zone_pos(SPOTS[i].off));
        if (d[i] < d[best]) best = i;
    }
    const int was = g_in[h];
    int now = NONE;
    if (was != NONE && d[was] < cfg::ZONE_LEAVE_M && (best == was || d[best] >= cfg::ZONE_ENTER_M)) now = was;
    else if (d[best] < cfg::ZONE_ENTER_M) now = best;
    if (now != was) {
        g_in[h] = now;
        if (now != NONE && act_of(now, h) != Act::NOTHING) bridge::rumble(h, cfg::BUZZ_ENTER_AMP, cfg::BUZZ_ENTER_SEC);
    }
}

void track_weapon() {
    const int wp = weapons::current_id();
    if (wp == g_last_weapon) return;
    g_last_weapon = wp;
    if (wp < 0) LOGI("%s weapon: bare hands", TAG);
    else LOGI("%s weapon: WP%04d %s", TAG, wp, weapons::name(wp));
}

void track_slots() {
    if (g_frame % cfg::SLOT_CHECK_EVERY_FRAMES != 0) return;
    bool changed = false;
    for (int d = 0; d < 4; ++d) {
        const int wp = shortcut::weapon_in((shortcut::Dir)d);
        if (wp != g_last_slots[d]) { g_last_slots[d] = wp; changed = true; }
    }
    if (changed) shortcut::log_slots();
}

void grab_shortcut(const Spot& z) {
    // a knife or grenade in hand: RG is the game's throw, so the holster stays out of it (Tefa 2026-10-06)
    const int held = weapons::current_id();
    if (held == 4500 || held == 4510 || held == 6200 || held == 6300) return;
    const auto d = (shortcut::Dir)z.dir;
    const int in_slot = shortcut::weapon_in(d);
    if (in_slot < 0) { LOGI("%s GRAB at %s: shortcut %s is empty, nothing to do", TAG, z.name, shortcut::dir_name(d)); return; }
    bridge::rumble(bridge::RIGHT, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
    if (held == in_slot) {
        const bool ok = shortcut::put_away(held);
        LOGI("%s GRAB at %s: PUT AWAY %s -> %s", TAG, z.name, weapons::name(held), ok ? "called" : "FAILED");
    } else {
        const bool ok = shortcut::take_out(d);
        LOGI("%s GRAB at %s: TAKE OUT %s (shortcut %s; was holding %s) -> %s", TAG, z.name, weapons::name(in_slot),
             shortcut::dir_name(d), held < 0 ? "nothing" : weapons::name(held), ok ? "game said yes" : "game said no");
    }
}

void left_press(int spot) {
    const Spot& z = SPOTS[spot];
    switch (z.left) {
    case Act::POUCH: guns::pouch_grab(); return;   // b132: a magazine / round from the pouch
    case Act::TORCH: flashlight::tap_spot(); return;
    case Act::MEDICINE:
        bridge::rumble(bridge::LEFT, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
        LOGI("%s GRAB at %s: medicine (does nothing yet: a later step)", TAG, z.name);
        return;
    default: return;
    }
}
} // namespace

bool right_hand_in_back_zone() { return g_in[bridge::RIGHT] == BACK_RIGHT_SPOT; }
bool right_hand_in_zone() { return act_of(g_in[bridge::RIGHT], bridge::RIGHT) != Act::NOTHING; }
bool left_hand_at_torch_spot() { return g_in[bridge::LEFT] == LEFT_SHOULDER_SPOT; }

void frame() {
    ++g_frame;
    track_weapon();
    track_slots();
    if (!bridge::live()) { g_in[0] = g_in[1] = NONE; return; }
    update_hand(bridge::LEFT);
    update_hand(bridge::RIGHT);
    if (g_in[bridge::LEFT] != NONE && bridge::pressed(bridge::S_LGRIP)) left_press(g_in[bridge::LEFT]);
    const int r = g_in[bridge::RIGHT];
    if (r != NONE && bridge::pressed(bridge::S_RGRIP) && SPOTS[r].right == Act::SHORTCUT) grab_shortcut(SPOTS[r]);
}

} // namespace vn::holster
