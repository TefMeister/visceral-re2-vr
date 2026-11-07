// flashlight.cpp -- see flashlight.h.
//
// Tefa 2026-10-11: "add a flashlight into re2 that works like re3 flashlight ... only the light shows and follows
// head, the actual flashlight in RE2 won't be taken out at all during gameplay. the light source should be coming from
// the left upper chest ... and only in cutscenes is the actual flashlight seen." Switch: LT, left hand at the left
// shoulder.
//
// b148 switched the GAME's flashlight (Arcade Controls' set_ManuallyLight + PrecedeBits.Accept(64)): it worked, but Leon
// raised his hand as if holding the torch and no beam was seen (Tefa, worn 00:50). b150 leaves the game's state alone
// and switches the IES light itself. Where the light sits comes from
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
MO* g_this = nullptr;         // the player's FlashLight component seen in this applyTransform call
int g_logs = 0;
bool g_report = false;      // log the beam's numbers once after each switch

MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }
MO* condition() { return component(player_go(), "app.ropeway.survivor.SurvivorCondition"); }

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

// the beam: at the left upper chest, in the head's left/right turn, pointing where the head looks. The IES light is
// switched on and off here directly (b150), never through the game's flashlight state (that raises Leon's hand).
void place_beam(MO* flashlight) {
    auto* ies = field_obj(flashlight, "<IESLight>k__BackingField");
    auto* ltf = call_ptr(call_ptr(ies, "get_GameObject"), "get_Transform");
    MO* ctf = (MO*)menu_body::probe_camera_tf();
    Vec3 cp; Quat cq;
    if (ies == nullptr || ltf == nullptr || !get_v3(XFORM, ctf, "get_Position", cp) || !get_q(XFORM, ctf, "get_Rotation", cq)) {
        if (g_report) { g_report = false; LOGW("%s flashlight: beam NOT placed (ies %p, light transform %p, camera transform %p)", TAG, (void*)ies, (void*)ltf, (void*)ctf); }
        return;
    }
    const bool was_on = call_direct<bool>(ies, "get_Enabled", false);
    if (was_on != g_want) call_direct<void*>(ies, "set_Enabled", nullptr, g_want);
    // b152: the light also needs its game objects drawn and updated; with the game's own flashlight off they may be
    // switched off (b151 worn 00:56: enabled, intensity 300000, at the chest, facing forward, still no beam)
    auto* lgo = call_ptr(ies, "get_GameObject");
    auto* fgo = call_ptr(flashlight, "get_GameObject");
    const bool l_draw = call_direct<bool>(lgo, "get_DrawSelf", true), l_upd = call_direct<bool>(lgo, "get_UpdateSelf", true);
    const bool f_draw = call_direct<bool>(fgo, "get_DrawSelf", true), f_upd = call_direct<bool>(fgo, "get_UpdateSelf", true);
    if (g_want) {
        if (!l_draw) call_direct<void*>(lgo, "set_DrawSelf", nullptr, true);
        if (!l_upd) call_direct<void*>(lgo, "set_UpdateSelf", nullptr, true);
        if (!f_draw) call_direct<void*>(fgo, "set_DrawSelf", nullptr, true);
        if (!f_upd) call_direct<void*>(fgo, "set_UpdateSelf", nullptr, true);
    }
    if (g_report) LOGI("%s flashlight: game objects before: light draw %d update %d, torch draw %d update %d", TAG, l_draw, l_upd, f_draw, f_upd);
    cq = qnorm(cq);
    Vec3 fwd = rotate(cq, Vec3{0, 0, -1});   // RE Engine cameras look down -Z
    fwd.y = 0;
    const float len = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
    fwd = len > 1e-4f ? fwd * (1.0f / len) : Vec3{0, 0, -1};
    const Vec3 right = rotate(cq, Vec3{1, 0, 0});
    const Vec3 pos = cp + right * cfg::FLASHLIGHT_BEAM.side + Vec3{0, cfg::FLASHLIGHT_BEAM.up, 0} + fwd * cfg::FLASHLIGHT_BEAM.fwd;
    // the IES light's Direction reads (0, 0, -1): it shines along its local -Z, the same way the camera looks, so the
    // camera's turn is the beam's turn (b150 turned it 180 degrees and the beam went out behind the head, 2026-10-11)
    const Quat rot = cq;
    set_any(XFORM, ltf, "set_Position", &pos);
    set_any(XFORM, ltf, "set_Rotation", &rot);
    if (g_report) {
        g_report = false;
        Vec3 dir{0, 0, 0}; get_v3("via.render.IESLight", ies, "get_Direction", dir);
        Vec3 now{0, 0, 0}; get_v3(XFORM, ltf, "get_Position", now);
        const float inten = call_direct<float>(ies, "get_Intensity", -1.0f);
        LOGI("%s flashlight: beam %s (was %s), type %s, Direction %.2f %.2f %.2f, intensity %.1f, at %.2f %.2f %.2f (camera %.2f %.2f %.2f, %.2f m away)",
             TAG, g_want ? "ON" : "OFF", was_on ? "on" : "off", type_name(ies).c_str(), dir.x, dir.y, dir.z, inten, now.x, now.y, now.z, cp.x, cp.y, cp.z, dist(now, cp));
    }
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
    g_report = true;
    bridge::rumble(bridge::LEFT, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
    LOGI("%s flashlight: switched %s (our beam only: the game's flashlight state is left off, so no hand pose)", TAG, g_want ? "ON" : "OFF");
}

void frame() {}

} // namespace vn::flashlight
