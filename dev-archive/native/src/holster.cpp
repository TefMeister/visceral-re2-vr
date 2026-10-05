// holster.cpp -- see holster.h
#include "holster.h"
#include "bridge.h"
#include "settings.h"
#include "shortcut.h"
#include "weapons.h"

namespace vn::holster {

namespace {
using bridge::Hand;
constexpr int NO_SHORTCUT = -1;

struct Zone {
    const char* name;
    cfg::ZoneOffset off;
    Hand hand;              // the hand that uses it
    bridge::Slot grip;      // the button that acts in it
    int dir;                // the shortcut slot it holds (Tefa's mapping), or NO_SHORTCUT
    bool inside = false;
};

// Tefa 2026-10-05: right = right hip, bottom = left hip, left = left shoulder, top = right shoulder.
// 2026-10-06: the bottom is the sub-weapon box: shortcut.cpp keeps the equipped knife/grenade in it.
Zone g_zones[] = {
    {"flashlight (upper left of the head)", cfg::FLASHLIGHT,     bridge::LEFT,  bridge::S_LGRIP, NO_SHORTCUT},
    {"ammo pouch (left hip)",                cfg::LEFT_HIP,       bridge::LEFT,  bridge::S_LGRIP, NO_SHORTCUT},
    {"right hip",                            cfg::RIGHT_HIP,      bridge::RIGHT, bridge::S_RGRIP, shortcut::RIGHT},
    {"left hip (sub weapon)",                cfg::LEFT_HIP,       bridge::RIGHT, bridge::S_RGRIP, shortcut::DOWN},
    {"left shoulder",                        cfg::LEFT_SHOULDER,  bridge::RIGHT, bridge::S_RGRIP, shortcut::LEFT},
    {"right shoulder",                       cfg::RIGHT_SHOULDER, bridge::RIGHT, bridge::S_RGRIP, shortcut::UP},
};

int g_frame = 0;
int g_last_weapon = -2;
int g_last_slots[4] = {-2, -2, -2, -2};

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

void grab(const Zone& z) {
    const auto d = (shortcut::Dir)z.dir;
    const int in_slot = shortcut::weapon_in(d);
    const int held = weapons::current_id();
    if (in_slot < 0) { LOGI("%s GRAB at %s: shortcut %s is empty, nothing to do", TAG, z.name, shortcut::dir_name(d)); return; }
    bridge::rumble(z.hand, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
    if (held == in_slot) {
        const bool ok = shortcut::put_away(held);
        LOGI("%s GRAB at %s: PUT AWAY %s -> %s", TAG, z.name, weapons::name(held), ok ? "called" : "FAILED");
    } else {
        const bool ok = shortcut::take_out(d);
        LOGI("%s GRAB at %s: TAKE OUT %s (shortcut %s; was holding %s) -> %s", TAG, z.name, weapons::name(in_slot),
             shortcut::dir_name(d), held < 0 ? "nothing" : weapons::name(held), ok ? "game said yes" : "game said no");
    }
}
} // namespace

void frame() {
    ++g_frame;
    track_weapon();
    track_slots();
    if (!bridge::live()) return;
    for (auto& z : g_zones) {
        const float d = dist(bridge::hand_pos(z.hand), zone_pos(z.off));
        if (!z.inside && d < cfg::ZONE_ENTER_M) {
            z.inside = true;
            bridge::rumble(z.hand, cfg::BUZZ_ENTER_AMP, cfg::BUZZ_ENTER_SEC);
        } else if (z.inside && d > cfg::ZONE_LEAVE_M) {
            z.inside = false;
        }
        if (!z.inside || !bridge::pressed(z.grip)) continue;
        if (z.dir == NO_SHORTCUT) {
            bridge::rumble(z.hand, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
            LOGI("%s GRAB at %s (does nothing yet: a later step)", TAG, z.name);
        } else {
            grab(z);
        }
    }
}

} // namespace vn::holster
