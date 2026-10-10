// flashlight.cpp -- see flashlight.h.
//
// Tefa 2026-10-11: "add a flashlight into re2 that works like re3 flashlight ... only the light shows and follows
// head, the actual flashlight in RE2 won't be taken out at all during gameplay. the light source should be coming from
// the left upper chest ... and only in cutscenes is the actual flashlight seen." Switch: LT, left hand at the left
// shoulder.
//
// How the game's flashlight is switched comes from Arcade Controls' re2_vr_holster.lua (LT flashlight, 2026-08):
// SurvivorCondition.set_ManuallyLight + the orderer's PrecedeBits.Accept(64) (the manual switch input), pulsed until
// IsLight matches, with the LightSwitchTimer's limit raised so it does not time out. Where the light sits comes from
// REFramework's FirstPerson::on_pre_flashlight_apply_transform: the FlashLight component carries an IESLight whose
// transform is the beam, and the beam leaves along the light's local +Z (REMath to_quat = lookAtLH).
#include "flashlight.h"
#include "bridge.h"
#include "common.h"
#include "joints.h"
#include "menu_body.h"
#include "settings.h"

namespace vn::flashlight {

using namespace joints;

namespace {
bool g_want = false;          // the switch
int g_pulses = 0;             // switch inputs still allowed before giving up
int g_cooldown = 0;
MO* g_this = nullptr;         // the player's FlashLight component seen in this applyTransform call
int g_logs = 0;

MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }
MO* condition() { return component(player_go(), "app.ropeway.survivor.SurvivorCondition"); }
MO* orderer() { return component(player_go(), "app.ropeway.survivor.player.PlayerActionOrderer"); }

bool is_light_on(MO* cond) { return call_direct<bool>(cond, "get_IsLight", false); }

void keep_switch(MO* cond) {
    call_direct<void*>(cond, "set_ManuallyLight", nullptr, g_want);
    if (g_want) {
        if (auto* timer = field_obj(cond, "<LightSwitchTimer>k__BackingField")) {
            if (auto* lim = field_ptr<float>(timer, "_TimeLimit")) *lim = cfg::FLASHLIGHT_TIMER_S;
        }
    }
}

bool pulse() {   // the game's own manual-switch input (Arcade Controls: PrecedeBits.Accept(64))
    auto* bits = field_obj(orderer(), "<PrecedeBits>k__BackingField");
    if (bits == nullptr) return false;
    call_direct<void*>(bits, "set_Accept", nullptr, (uint32_t)64);
    return true;
}

// gameplay = REFramework's FirstPerson drives the view (bridge slot 32); cutscenes = it does not. Flat runs (no
// headset, no bridge) count as gameplay so the light can be checked on the monitor.
bool gameplay() { return !bridge::live() || bridge::view(bridge::S_FP_USED) > 0.5f; }

bool is_players(MO* flashlight) {
    auto* cond = condition();
    return cond != nullptr && field_obj(flashlight, "<Condition>k__BackingField") == cond;
}

void show_torch(MO* flashlight, bool show) {
    auto* go = call_ptr(flashlight, "get_GameObject");
    auto* mesh = component(go, "via.render.Mesh");
    if (mesh != nullptr && call_direct<bool>(mesh, "get_Enabled", show) != show) call_direct<void*>(mesh, "set_Enabled", nullptr, show);
}

// the beam: at the left upper chest, in the head's left/right turn, pointing where the head looks
void place_beam(MO* flashlight) {
    auto* ies = field_obj(flashlight, "<IESLight>k__BackingField");
    auto* ltf = call_ptr(call_ptr(ies, "get_GameObject"), "get_Transform");
    MO* ctf = (MO*)menu_body::probe_camera_tf();
    Vec3 cp; Quat cq;
    if (ltf == nullptr || !get_v3(XFORM, ctf, "get_Position", cp) || !get_q(XFORM, ctf, "get_Rotation", cq)) return;
    cq = qnorm(cq);
    Vec3 fwd = rotate(cq, Vec3{0, 0, -1});   // RE Engine cameras look down -Z
    fwd.y = 0;
    const float len = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
    fwd = len > 1e-4f ? fwd * (1.0f / len) : Vec3{0, 0, -1};
    const Vec3 right = rotate(cq, Vec3{1, 0, 0});
    const Vec3 pos = cp + right * cfg::FLASHLIGHT_BEAM.side + Vec3{0, cfg::FLASHLIGHT_BEAM.up, 0} + fwd * cfg::FLASHLIGHT_BEAM.fwd;
    const Quat rot = qnorm(qmul(cq, Quat{0, 1, 0, 0}));   // turn the camera's -Z look onto the light's +Z beam
    set_any(XFORM, ltf, "set_Position", &pos);
    set_any(XFORM, ltf, "set_Rotation", &rot);
}

int pre_apply(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    g_this = argc > 1 ? (MO*)argv[1] : nullptr;
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_apply(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    MO* fl = g_this;
    g_this = nullptr;
    if (!cfg::FLASHLIGHT_ON || fl == nullptr || !is_players(fl)) return;
    const bool play = gameplay();
    show_torch(fl, !play);              // the torch only in cutscenes
    if (play) place_beam(fl);           // after the game and REFramework placed it: ours is the last word
    if (g_logs < 5) { ++g_logs; LOGI("%s flashlight: applyTransform (%s): %s", TAG, play ? "play" : "cutscene", play ? "torch hidden, beam at the left chest" : "left to the game"); }
}
} // namespace

void install() {
    auto* m = API::get()->tdb()->find_method("app.ropeway.FlashLight", "applyTransform");
    if (m == nullptr) { LOGE("%s flashlight: FlashLight.applyTransform not found, flashlight off", TAG); return; }
    m->add_hook(pre_apply, post_apply, false);
    LOGI("%s flashlight: applyTransform hook in (beam at the left upper chest, torch hidden in play)", TAG);
}

void toggle() {
    if (!cfg::FLASHLIGHT_ON) return;
    g_want = !g_want;
    g_pulses = cfg::FLASHLIGHT_PULSES;
    g_cooldown = 0;
    if (auto* cond = condition()) keep_switch(cond);
    bridge::rumble(bridge::LEFT, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
    LOGI("%s flashlight: switched %s", TAG, g_want ? "ON" : "OFF");
}

void frame() {
    if (!cfg::FLASHLIGHT_ON) return;
    auto* cond = condition();
    if (cond == nullptr) return;
    if (g_want) keep_switch(cond);
    if (g_pulses <= 0) return;
    if (g_cooldown > 0) { --g_cooldown; return; }
    if (is_light_on(cond) == g_want) { g_pulses = 0; LOGI("%s flashlight: the game's light is %s", TAG, g_want ? "on" : "off"); return; }
    if (pulse()) { --g_pulses; g_cooldown = cfg::FLASHLIGHT_PULSE_GAP; }
    else { g_pulses = 0; LOGW("%s flashlight: the orderer's PrecedeBits not found, cannot switch", TAG); }
}

} // namespace vn::flashlight
