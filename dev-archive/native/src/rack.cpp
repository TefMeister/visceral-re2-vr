// rack.cpp -- see rack.h.
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
Vec3 g_rest{};            // the joint's own LOCAL pose at rest, read once per weapon (his numbers are offsets from it:
bool g_have_rest = false; // b140 wrote them as positions and the W-870's fore-end went to the gun's origin, worn 2026-10-10)

bool g_needed = false;    // a rack is required
bool g_parked = false;    // the slide is locked open (empty gun, or needed)
bool g_hand_on = false;   // LG held with the hand near the joint
bool g_pulling = false;   // LT held: travelling back
bool g_pulled = false;    // reached the back
float g_travel = 0.0f;    // 0 = rest/parked, 1 = back
float g_last_t = -1.0f;
int g_log_near = 0;

float dt() {
    const float t = now_s();
    const float d = g_last_t < 0.0f ? 1.0f / 60.0f : std::fmin(0.1f, t - g_last_t);
    g_last_t = t;
    return d;
}

MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }

void resolve_frame() {
    g_joint = g_gun = nullptr;
    auto* go = player_go();
    auto* gun = field_obj(component(go, "app.ropeway.survivor.Equipment"), "<EquipWeapon>k__BackingField");
    if (gun == nullptr || g_w == nullptr) return;
    g_gun = gun;
    auto* wtf = call_ptr(call_ptr(gun, "get_GameObject"), "get_Transform");
    g_joint = joint_by_name(wtf, widen(g_w->joint).c_str());
    if (g_joint != nullptr && !g_have_rest) {
        Vec3 p;
        if (get_v3(JOINT, g_joint, "get_LocalPosition", p)) { g_rest = p; g_have_rest = true; LOGI("%s rack: WP%04d joint %s rests at local %.3f %.3f %.3f (his rest %.3f)", TAG, g_wp, g_w->joint, p.x, p.y, p.z, g_w->rest_z); }
    }
}

// the left hand (its wrist joint, the real controller's hand) within reach of the slide / fore-end
bool hand_near_joint() {
    Vec3 jp, hp;
    auto* wrist = joint_by_name(call_ptr(player_go(), "get_Transform"), L"l_arm_wrist");
    if (!get_v3(JOINT, g_joint, "get_Position", jp) || !get_v3(JOINT, wrist, "get_Position", hp)) return false;
    const float d = dist(jp, hp);
    if (g_log_near < 20 && bridge::pressed(bridge::S_LGRIP)) { ++g_log_near; LOGI("%s rack: LG pressed %.3f m from the %s joint (need %.2f)", TAG, d, is_pump(g_wp) ? "fore-end" : "slide", cfg::RACK_HAND_M); }
    return d <= cfg::RACK_HAND_M;
}

// the joint's local Z: its real rest + his offset (parked - rest, back - rest)
float z_from() { return g_rest.z + ((g_parked ? g_w->parked_z : g_w->rest_z) - g_w->rest_z); }
float z_back() { return g_rest.z + (g_w->back_z - g_w->rest_z); }

void write_joint() {
    if (g_joint == nullptr || g_w == nullptr || !g_have_rest) return;
    if (!g_hand_on && !g_parked && g_travel <= 0.0f) return;   // nothing of ours: leave the game's own animation alone
    const float z = z_from() + (z_back() - z_from()) * ease(g_travel);
    Vec3 p{g_rest.x, g_rest.y, z};
    set_any(JOINT, g_joint, "set_LocalPosition", &p);
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
    LOGI("%s rack: WP%04d %s cycle complete (%s), %d rounds loaded", TAG, g_wp, pump ? "pump" : "slide", was_needed ? "was needed: fire unblocked" : "cosmetic", g_gun ? call_direct<int>(g_gun, "getBulletNumber", -1) : -1);
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
        g_needed = g_parked = g_hand_on = g_pulling = g_pulled = false;
        g_travel = 0.0f;
        g_have_rest = false;
        if (g_w != nullptr) LOGI("%s rack: WP%04d %s: %s on joint %s (rest %.3f, parked %.3f, back %.3f)", TAG, wp, weapons::name(wp), is_pump(wp) ? "pump" : "slide rack", g_w->joint, g_w->rest_z, g_w->parked_z, g_w->back_z);
    }
    resolve_frame();
    if (g_w == nullptr || g_gun == nullptr || !bridge::live()) { g_hand_on = false; return; }
    if (!is_pump(g_wp) && !g_parked && !reload::mag_out() && call_direct<int>(g_gun, "getBulletNumber", -1) == 0) slide_lock_empty();   // b142: polled, the post-fire read was too early
    const float d = dt();
    const bool menu = menu_body::is_menu_open();
    const bool lg = bridge::held(bridge::S_LGRIP), lt = bridge::held(bridge::S_LTRIG);

    // the hand goes on the slide / fore-end: LG pressed near it; it leaves when LG is let go
    if (!g_hand_on && !menu && bridge::pressed(bridge::S_LGRIP) && !reload::session_active() && hand_near_joint()) {
        g_hand_on = true;
        g_pulling = g_pulled = false;
        LOGI("%s rack: hand on the %s", TAG, is_pump(g_wp) ? "fore-end" : "slide");
    } else if (g_hand_on && !lg) {
        g_hand_on = false;
        g_pulling = false;
        if (g_pulled) { g_pulled = false; g_travel = 0.0f; complete(); LOGI("%s rack: hand off while pulled: released", TAG); }
        else LOGI("%s rack: hand off the %s", TAG, is_pump(g_wp) ? "fore-end" : "slide");
    }

    // LT pulls back, letting go returns; the cycle completes when it is back at rest
    if (g_hand_on && !menu) {
        if (lt && !g_pulling) g_pulling = true;
        if (!lt && g_pulling) g_pulling = false;
    }
    const float target = (g_hand_on && g_pulling) ? 1.0f : 0.0f;
    const float speed = target > g_travel ? cfg::RACK_PULL_PER_SEC : cfg::RACK_PUSH_PER_SEC;
    if (g_travel < target) g_travel = std::fmin(target, g_travel + speed * d);
    else if (g_travel > target) g_travel = std::fmax(target, g_travel - speed * d);
    if (g_pulling && !g_pulled && g_travel >= 0.999f) {
        g_pulled = true;
        sfx::play("slide_rack_pull", is_pump(g_wp) ? pump_sfx(g_wp) : reload::sfx_folder_now(), is_pump(g_wp) ? 2.0f : reload::sfx_volume_now());
        bridge::rumble(bridge::LEFT, cfg::RACK_BUZZ_AMP, cfg::RACK_BUZZ_SEC);
        if (is_pump(g_wp)) pump_native::on_pulled_down();   // the spent shell leaves on the pull, not the return
        LOGI("%s rack: pulled back", TAG);
    }
    if (g_pulled && !g_pulling && g_travel <= 0.001f) { g_pulled = false; complete(); }
}

void late_point() { if (g_w != nullptr) write_joint(); }
void render_point() { if (g_w != nullptr) write_joint(); }

} // namespace vn::rack
