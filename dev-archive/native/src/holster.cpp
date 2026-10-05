// holster.cpp -- see holster.h
#include "holster.h"
#include "bridge.h"
#include "settings.h"
#include "weapons.h"

namespace vn::holster {

namespace {
using bridge::Hand;
using weapons::Holster;

struct Zone {
    const char* name;
    cfg::ZoneOffset off;
    Hand hand;              // the hand that uses it
    bridge::Slot grip;      // the button that acts in it
    Holster holster;        // the weapons it holds (NONE = not a weapon holster)
    bool inside = false;
};

Zone g_zones[] = {
    {"flashlight (upper left of the head)", cfg::FLASHLIGHT,     bridge::LEFT,  bridge::S_LGRIP, Holster::NONE},
    {"ammo pouch (left hip)",                cfg::LEFT_HIP,       bridge::LEFT,  bridge::S_LGRIP, Holster::NONE},
    {"right hip",                            cfg::RIGHT_HIP,      bridge::RIGHT, bridge::S_RGRIP, Holster::HANDGUN},
    {"right shoulder",                       cfg::RIGHT_SHOULDER, bridge::RIGHT, bridge::S_RGRIP, Holster::LONG},
    {"left shoulder",                        cfg::LEFT_SHOULDER,  bridge::RIGHT, bridge::S_RGRIP, Holster::SPECIAL},
    {"left hip (sub weapon)",                cfg::LEFT_HIP,       bridge::RIGHT, bridge::S_RGRIP, Holster::SUB},
};

int g_frame = 0;
int g_last_weapon = -2;

// headset position + offset turned by the headset's YAW only (OpenXR space: +X right, +Y up, -Z forward)
Vec3 zone_pos(const cfg::ZoneOffset& o) {
    const Vec3 head = bridge::hmd_pos();
    Vec3 f = rotate(bridge::hmd_rot(), Vec3{0, 0, -1});
    f.y = 0;
    const float len = std::sqrt(f.x * f.x + f.z * f.z);
    if (len < 1e-4f) f = {0, 0, -1}; else f = f * (1.0f / len);
    const Vec3 right{-f.z, 0, f.x};
    return head + right * o.side + Vec3{0, o.up, 0} + f * o.fwd;
}

void track_weapon() {
    const int wp = weapons::current_id();
    if (wp == g_last_weapon) return;
    g_last_weapon = wp;
    if (wp < 0) { LOGI("%s weapon: bare hands", TAG); return; }
    LOGI("%s weapon: WP%04d %s -> %s", TAG, wp, weapons::name(wp), weapons::holster_name(weapons::holster_for(wp)));
}
} // namespace

void frame() {
    ++g_frame;
    track_weapon();
    if (!bridge::live()) return;
    for (auto& z : g_zones) {
        const float d = dist(bridge::hand_pos(z.hand), zone_pos(z.off));
        if (!z.inside && d < cfg::ZONE_ENTER_M) {
            z.inside = true;
            bridge::rumble(z.hand, cfg::BUZZ_ENTER_AMP, cfg::BUZZ_ENTER_SEC);
            LOGI("%s %s hand ENTERED %s (%.2f m)", TAG, z.hand == bridge::LEFT ? "left" : "right", z.name, d);
        } else if (z.inside && d > cfg::ZONE_LEAVE_M) {
            z.inside = false;
        }
        if (z.inside && bridge::pressed(z.grip)) {
            bridge::rumble(z.hand, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
            const int wp = weapons::current_id();
            const bool holds_this_kind = wp >= 0 && z.holster != Holster::NONE && weapons::holster_for(wp) == z.holster;
            LOGI("%s GRAB in %s -- would %s (holding WP%04d %s)", TAG, z.name,
                 z.holster == Holster::NONE ? "use it (later step)" : holds_this_kind ? "PUT AWAY the weapon in hand" : "TAKE OUT the weapon kept here",
                 wp < 0 ? 9999 : wp, wp < 0 ? "bare hands" : weapons::name(wp));
        }
    }
    if (g_frame % cfg::STATUS_LOG_EVERY_FRAMES == 0) {
        const Vec3 h = bridge::hmd_pos(), l = bridge::hand_pos(bridge::LEFT), r = bridge::hand_pos(bridge::RIGHT);
        LOGI("%s status: head %.2f %.2f %.2f | left hand %.2f %.2f %.2f | right hand %.2f %.2f %.2f", TAG,
             h.x, h.y, h.z, l.x, l.y, l.z, r.x, r.y, r.z);
    }
}

} // namespace vn::holster
