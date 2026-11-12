// flashlight.cpp -- see flashlight.h.
//
// Tefa 2026-11-12: "manual flashlight, an actual flashlight one can hold in hand ... flashlight is in the left shoulder
// spot with LG ... tap LG in the shoulder holster to bring it out, tap LG again to put it back ... LT switches it on/off
// in both hand and shoulder spot. long weapons cannot be two handed while flashlight is held ... but can reload weapons
// with the flashlight out". The grip: "holding it up above" (overhand) or the ordinary grip, by the wrist ("if wrist angle
// is reliable it is way better option"; LG + LT was the other idea).
//
// Kept from 2026-10-11 (b148-b152, the RE3-style light): the game's flashlight STATE is never switched (b148: that raised
// Leon's hand as if holding the torch); the IES light is switched directly; it shines along its local -Z; its game objects
// must be drawn and updated (b152). The FlashLight component carries the IES light and the torch mesh; its applyTransform
// is where the game and REFramework's FirstPerson place them, so our post-hook and the PrepareRendering point have the
// last word. New: both places every frame (b152 saw applyTransform only on some frames: "comes and goes").
#include "flashlight.h"
#include "bridge.h"
#include "common.h"
#include "guns_api.h"
#include "hand_state.h"
#include "holster.h"
#include "joints.h"
#include "menu_body.h"
#include "settings.h"

namespace vn::flashlight {

using namespace joints;

namespace {
bool g_light = false;         // LT's switch
bool g_hand = false;          // the torch is out of its spot
bool g_reverse = false;       // the overhand grip (the lens at the little-finger end)
float g_flip = 0.0f;          // 0 = ordinary grip .. 1 = overhand, eased
float g_flip_t = -1.0f;       // now_s() of the last frame the ease ran
MO* g_this = nullptr;         // the FlashLight seen in this applyTransform call
MO* g_fl = nullptr;           // the player's FlashLight, held with a reference (add_ref) so it is never a dangling pointer
int g_logs = 0;
bool g_report = false;        // log the beam's numbers once after each change
bool g_axes_logged = false;

MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }
MO* condition() { return component(player_go(), "app.ropeway.survivor.SurvivorCondition"); }

// gameplay = REFramework's FirstPerson drives the view (bridge slot 32); cutscenes = it does not. Flat runs (no
// headset, no bridge) count as gameplay so the light can be checked on the monitor.
bool gameplay() { return !bridge::live() || bridge::view(bridge::S_FP_USED) > 0.5f; }

bool is_players(MO* flashlight) {
    auto* cond = condition();
    return cond != nullptr && flashlight != nullptr && field_obj(flashlight, "<Condition>k__BackingField") == cond;
}

void keep(MO* fl) {
    if (fl == g_fl) return;
    if (fl != nullptr) fl->add_ref();
    if (g_fl != nullptr) g_fl->release();
    g_fl = fl;
    LOGI("%s flashlight: the player's FlashLight is %p", TAG, (void*)fl);
}

void show_torch(MO* flashlight, bool show) {
    auto* go = call_ptr(flashlight, "get_GameObject");
    auto* mesh = component(go, "via.render.Mesh");
    if (mesh != nullptr && call_direct<bool>(mesh, "get_Enabled", !show) != show) call_direct<void*>(mesh, "set_Enabled", nullptr, show);
}

// the turn that makes local -Z (the way the IES light and the camera face) point along dir, world up kept up
Quat facing(Vec3 dir) {
    const float l = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (l < 1e-5f) return {0, 0, 0, 1};
    const Vec3 z = dir * (-1.0f / l);                       // local +Z = -dir
    Vec3 up{0, 1, 0};
    if (std::fabs(z.y) > 0.98f) up = {0, 0, 1};
    auto cross = [](Vec3 a, Vec3 b) { return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; };
    Vec3 x = cross(up, z);
    const float xl = std::sqrt(x.x * x.x + x.y * x.y + x.z * x.z);
    x = x * (1.0f / xl);
    const Vec3 y = cross(z, x);
    // rotation matrix columns x, y, z -> quaternion
    const float tr = x.x + y.y + z.z;
    Quat q;
    if (tr > 0) { const float s = std::sqrt(tr + 1.0f) * 2; q = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s}; }
    else if (x.x > y.y && x.x > z.z) { const float s = std::sqrt(1.0f + x.x - y.y - z.z) * 2; q = {0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s}; }
    else if (y.y > z.z) { const float s = std::sqrt(1.0f + y.y - x.x - z.z) * 2; q = {(y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s}; }
    else { const float s = std::sqrt(1.0f + z.z - x.x - y.y) * 2; q = {(z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s}; }
    return qnorm(q);
}

Quat ypr_deg(float yaw, float pitch, float roll) {   // the same convention as guns/gun_frame.cpp
    const float h = 3.14159265f / 360.0f;
    const Quat qz{0, 0, std::sin(yaw * h), std::cos(yaw * h)};
    const Quat qy{0, std::sin(pitch * h), 0, std::cos(pitch * h)};
    const Quat qx{std::sin(roll * h), 0, 0, std::cos(roll * h)};
    return qnorm(qmul(qmul(qz, qy), qx));
}

bool camera(Vec3& cp, Quat& cq) {
    MO* ctf = (MO*)menu_body::probe_camera_tf();
    if (ctf == nullptr || !get_v3(XFORM, ctf, "get_Position", cp) || !get_q(XFORM, ctf, "get_Rotation", cq)) return false;
    cq = qnorm(cq);
    return true;
}

Vec3 flat_forward(Quat cq) {
    Vec3 f = rotate(cq, Vec3{0, 0, -1});   // RE Engine cameras look down -Z
    f.y = 0;
    const float len = std::sqrt(f.x * f.x + f.z * f.z);
    return len > 1e-4f ? f * (1.0f / len) : Vec3{0, 0, -1};
}

// the light itself: on / off as switched, its game objects drawn and updated while on (b152), at pos facing rot
void light_at(MO* flashlight, Vec3 pos, Quat rot, const char* where) {
    auto* ies = field_obj(flashlight, "<IESLight>k__BackingField");
    auto* lgo = call_ptr(ies, "get_GameObject");
    auto* ltf = call_ptr(lgo, "get_Transform");
    if (ies == nullptr || ltf == nullptr) {
        if (g_report) { g_report = false; LOGW("%s flashlight: beam NOT placed (ies %p, light transform %p)", TAG, (void*)ies, (void*)ltf); }
        return;
    }
    const bool was_on = call_direct<bool>(ies, "get_Enabled", false);
    if (was_on != g_light) call_direct<void*>(ies, "set_Enabled", nullptr, g_light);
    if (g_light) {
        auto* fgo = call_ptr(flashlight, "get_GameObject");
        if (!call_direct<bool>(lgo, "get_DrawSelf", true)) call_direct<void*>(lgo, "set_DrawSelf", nullptr, true);
        if (!call_direct<bool>(lgo, "get_UpdateSelf", true)) call_direct<void*>(lgo, "set_UpdateSelf", nullptr, true);
        if (!call_direct<bool>(fgo, "get_DrawSelf", true)) call_direct<void*>(fgo, "set_DrawSelf", nullptr, true);
        if (!call_direct<bool>(fgo, "get_UpdateSelf", true)) call_direct<void*>(fgo, "set_UpdateSelf", nullptr, true);
    }
    set_any(XFORM, ltf, "set_Position", &pos);
    set_any(XFORM, ltf, "set_Rotation", &rot);
    if (g_report) {
        g_report = false;
        const float inten = call_direct<float>(ies, "get_Intensity", -1.0f);
        LOGI("%s flashlight: light %s %s, intensity %.1f, at %.2f %.2f %.2f", TAG, g_light ? "ON" : "OFF", where, inten, pos.x, pos.y, pos.z);
    }
}

// in the spot: the beam at the left upper chest, in the head's left/right turn, where the head looks (2026-10-11)
void place_in_spot(MO* flashlight) {
    Vec3 cp; Quat cq;
    if (!camera(cp, cq)) return;
    const Vec3 fwd = flat_forward(cq);
    const Vec3 right = rotate(cq, Vec3{1, 0, 0});
    const Vec3 pos = cp + right * cfg::FLASHLIGHT_BEAM.side + Vec3{0, cfg::FLASHLIGHT_BEAM.up, 0} + fwd * cfg::FLASHLIGHT_BEAM.fwd;
    show_torch(flashlight, false);
    light_at(flashlight, pos, cq, "at the left chest (torch in its spot)");
}

// in the hand: the torch on the left wrist in the ordinary or overhand grip, the beam out of its lens
void place_in_hand(MO* flashlight) {
    MO* body = call_ptr(player_go(), "get_Transform");
    MO* wrist = body != nullptr ? joint_by_name(body, L"l_arm_wrist") : nullptr;
    Vec3 wp; Quat wq; Vec3 cp; Quat cq;
    if (wrist == nullptr || !get_v3(JOINT, wrist, "get_Position", wp) || !get_q(JOINT, wrist, "get_Rotation", wq) || !camera(cp, cq)) {
        if (g_logs < 8) { ++g_logs; LOGW("%s flashlight: no left wrist or camera this frame (wrist %p)", TAG, (void*)wrist); }
        return;
    }
    wq = qnorm(wq);
    const Vec3 lens{cfg::TORCH_LENS_AXIS[0], cfg::TORCH_LENS_AXIS[1], cfg::TORCH_LENS_AXIS[2]};
    const Quat hold = qnorm(qmul(wq, ypr_deg(cfg::TORCH_HOLD_YPR[0], cfg::TORCH_HOLD_YPR[1], cfg::TORCH_HOLD_YPR[2])));

    // the grip follows the wrist: the lens goes to whichever end of the fist points forward
    const Vec3 fwd = flat_forward(cq);
    const Vec3 o = rotate(hold, lens);   // the lens in the ordinary grip
    const float dot = o.x * fwd.x + o.y * fwd.y + o.z * fwd.z;
    const bool want = g_reverse ? !(dot > cfg::TORCH_FLIP_DOT) : dot < -cfg::TORCH_FLIP_DOT;
    if (want != g_reverse) {
        g_reverse = want;
        LOGI("%s flashlight: grip -> %s (lens end . forward %.2f)", TAG, g_reverse ? "OVERHAND (lens at the little finger)" : "ORDINARY (lens at the thumb)", dot);
    }
    const float t = now_s();
    const float dt = g_flip_t < 0 ? 0.0f : std::fmin(0.1f, t - g_flip_t);
    g_flip_t = t;
    const float step = cfg::TORCH_FLIP_SEC > 0 ? dt / cfg::TORCH_FLIP_SEC : 1.0f;
    g_flip = g_reverse ? std::fmin(1.0f, g_flip + step) : std::fmax(0.0f, g_flip - step);

    // the overhand grip = the torch turned end for end about its grip centre (an axis across the lens axis), eased
    Vec3 across = std::fabs(lens.x) < 0.9f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    const Vec3 a{lens.y * across.z - lens.z * across.y, lens.z * across.x - lens.x * across.z, lens.x * across.y - lens.y * across.x};
    const float al = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    const float ang = 3.14159265f * ease(g_flip);
    const float s = std::sin(ang * 0.5f) / (al > 1e-5f ? al : 1.0f);
    const Quat flip = qnorm(Quat{a.x * s, a.y * s, a.z * s, std::cos(ang * 0.5f)});

    const Vec3 pos = wp + rotate(wq, Vec3{cfg::TORCH_HOLD_POS[0], cfg::TORCH_HOLD_POS[1], cfg::TORCH_HOLD_POS[2]});
    const Quat rot = qnorm(qmul(hold, flip));
    auto* fgo = call_ptr(flashlight, "get_GameObject");
    auto* ftf = call_ptr(fgo, "get_Transform");
    if (ftf != nullptr) {
        set_any(XFORM, ftf, "set_Position", &pos);
        set_any(XFORM, ftf, "set_Rotation", &rot);
    }
    show_torch(flashlight, true);

    const Vec3 dir = rotate(rot, lens);
    light_at(flashlight, pos + dir * cfg::TORCH_LENS_M, facing(dir), g_reverse ? "from the lens (overhand)" : "from the lens (ordinary)");

    if (!g_axes_logged) {   // once: the wrist's axes in the world, for tuning TORCH_HOLD_* on the first run
        g_axes_logged = true;
        const Vec3 ax = rotate(wq, Vec3{1, 0, 0}), ay = rotate(wq, Vec3{0, 1, 0}), az = rotate(wq, Vec3{0, 0, 1});
        LOGI("%s flashlight: left wrist axes X %.2f %.2f %.2f, Y %.2f %.2f %.2f, Z %.2f %.2f %.2f; camera forward %.2f %.2f %.2f",
             TAG, ax.x, ax.y, ax.z, ay.x, ay.y, ay.z, az.x, az.y, az.z, fwd.x, fwd.y, fwd.z);
    }
}

void place(MO* fl) {
    if (!gameplay()) { show_torch(fl, true); return; }   // cutscenes: the game's own torch, as it decides
    if (g_hand) place_in_hand(fl);
    else place_in_spot(fl);
}

int pre_apply(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    g_this = argc > 1 ? (MO*)argv[1] : nullptr;
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_apply(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    MO* fl = g_this;
    g_this = nullptr;
    if (!cfg::FLASHLIGHT_ON || fl == nullptr || !is_players(fl)) return;
    keep(fl);
    place(fl);   // after the game and REFramework placed it: ours is the last word
}
} // namespace

void install() {
    auto* m = API::get()->tdb()->find_method("app.ropeway.FlashLight", "applyTransform");
    if (m == nullptr) { LOGE("%s flashlight: FlashLight.applyTransform not found, flashlight off", TAG); return; }
    m->add_hook(pre_apply, post_apply, false);
    LOGI("%s flashlight: applyTransform hook in (the torch lives in the left shoulder spot; LG taps it out and back, LT lights it)", TAG);
}

bool in_hand() { return g_hand; }

void tap_spot() {
    if (!cfg::FLASHLIGHT_ON) return;
    g_hand = !g_hand;
    hand_state::torch_in_left_hand = g_hand;
    g_report = true;
    g_flip_t = -1.0f;
    bridge::rumble(bridge::LEFT, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
    LOGI("%s flashlight: torch %s (light %s)", TAG, g_hand ? "TAKEN OUT into the left hand" : "PUT BACK in the left shoulder spot", g_light ? "on" : "off");
}

void frame() {
    if (!cfg::FLASHLIGHT_ON || !bridge::live()) return;
    // LT switches the light: anywhere while the torch is in the hand, or with the hand in the torch's spot. Never with LG
    // held (LG + LT is the rack and the reload) or while a reload has the left hand (its LT puts the round in).
    if (!bridge::pressed(bridge::S_LTRIG) || bridge::held(bridge::S_LGRIP)) return;
    if (g_hand ? guns::session_active() : !holster::left_hand_at_torch_spot()) return;
    g_light = !g_light;
    g_report = true;
    bridge::rumble(bridge::LEFT, cfg::BUZZ_GRAB_AMP, cfg::BUZZ_GRAB_SEC);
    LOGI("%s flashlight: light switched %s (torch %s)", TAG, g_light ? "ON" : "OFF", g_hand ? "in the hand" : "in its spot");
}

void render_point() {
    if (!cfg::FLASHLIGHT_ON || g_fl == nullptr) return;
    if (!is_players(g_fl)) { keep(nullptr); return; }   // a new area / character: wait for applyTransform to name it again
    place(g_fl);
}

} // namespace vn::flashlight
