// exposure.cpp -- the game's automatic brightness ("adaptive lighting") switched off (2026-10-28, Opus; Tefa: "please
// see if you can just turn it off and i'll check in the headset").
//
// Found live, flat [verified-live 2026-10-28, n=1]: the main camera ("Main Camera", via.SceneManager -> MainView ->
// PrimaryCamera) carries a via.render.ToneMapping whose AutoExposure is Enable (0); it adapts towards bright slowly
// (BrightAdaptationRate 0.01) and towards dark fast (DarkAdaptationRate 0.7) between EV -8 and +16, around the scene's
// authored EV (4.2 in the RPD hall). The game's own app.ropeway.posteffect.ToneMapController copies its blended
// parameters (CurrentParam, AutoExposure = Enable) onto that component in its lateUpdate, so a one-off write is undone
// the next frame: a setAutoExposure through the live link read back Enable at once. So: write it every frame, after the
// game's late update and before rendering (BeginRendering pre). With auto exposure off the picture uses the area's
// authored EV, the brightness the game's lighting artists set for each place.
//
// Which number is "Disable": the enum's names are Disable / Enable / FixedEnable; Enable = 0 (read live). The other
// two are tried at start: the first value after which get_UseAutoExposure reads false is kept and logged.
// On by default. Off when: the Lua menu's tick box says so (bridge slot S_AUTO_EXPOSURE_ON = 1, headset only), or NUM7
// toggles it (flat or headset; logged). [compile-verified] at most until it has run.
//
// b192, 2026-10-28 in the headset (Tefa: "the adaptive lighting is still on in re2", then "the colours change to a lot
// more blue hue in the dark"). Two things, read live (Opus, home PC):
//   1. The AutoExposure write above reads back Disable, yet the renderer still adapts: read from outside it shows Enable
//      most of the time (the game's ToneMapController writes Enable in its late update, our write lands only at
//      BeginRendering). The controller never writes the adaptation's limits, AutoExposureMinEV / MaxEV (-8 / +16), so
//      both are now pinned to the area's own EV every frame: the adaptation has no room left, whoever switches it on.
//      A live write of 4 / 4 held `[verified-live 2026-10-28, n=1]`.
//   2. The blue is the game's colour grading, a colour table per area (st4: ColorCube_15 in a bright spot,
//      ColorCube_13 in a dark one, EV 4.2 vs 4.0), applied by via.render.LDRColorCorrect and re-enabled every frame
//      by app.ropeway.posteffect.ColorCorrectController. With the controller and the component both off, live: "the
//      blue is gone, looks much better, so much better" `[verified-live 2026-10-28, headset, n=1]`. Kept off;
//      NUM6 gives the game's grading back (and takes it away again) to compare.
#include "exposure.h"
#include "bridge.h"
#include "common.h"
#include <windows.h>

using namespace vn;

namespace {

constexpr int AE_ENABLE = 0;           // via.render.ToneMapping.AutoExposure.Enable [verified-live 2026-10-28]
constexpr int AE_CANDIDATES[] = {2, 1};   // Disable is one of these; FixedEnable the other [inferred-static: Vignetting's Disable = 2]
constexpr float REPORT_S = 10.0f;
constexpr float GAME_MIN_EV = -8.0f, GAME_MAX_EV = 16.0f;   // the limits the game leaves on the component (read live), put back when ours is off

struct State {
    int disable_value = -1;            // learned: the value that makes get_UseAutoExposure false
    bool tried[3] = {false, false, false};
    bool off = true;                   // our switch: true = auto exposure kept off
    bool key7 = false, key6 = false;
    bool grading_off = true;           // b192: the game's per-area colour grading kept off
    bool limits_pinned = false;        // b192: we have the adaptation limits pinned (so they are put back once)
    MO* last_tm = nullptr;
    MO* last_cc = nullptr;
    int writes = 0;
    float last_report = 0.0f;
} g;

MO* main_camera_go() {
    static API::Method* main_view = API::get()->tdb()->find_method("via.SceneManager", "get_MainView");
    static API::Method* primary = API::get()->tdb()->find_method("via.SceneView", "get_PrimaryCamera");
    void* sm = API::get()->get_native_singleton("via.SceneManager");
    if (main_view == nullptr || primary == nullptr || sm == nullptr) return nullptr;
    auto* ctx = API::get()->get_vm_context();
    void* view = main_view->call<void*>(ctx, sm);
    auto* cam = view != nullptr ? (MO*)primary->call<void*>(ctx, view) : nullptr;
    return cam != nullptr ? call_ptr(cam, "get_GameObject") : nullptr;
}

void set_float(MO* o, const char* name, float v) {
    auto* m = o ? find_method_deep(o->get_type_definition(), name) : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)o, v);
}
void set_enabled(MO* o, bool v) {
    auto* m = o ? find_method_deep(o->get_type_definition(), "set_Enabled") : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)o, v);
}

// b192: the colour grading. The controller has to stop first, or it switches the component straight back on.
void grading(MO* cam_go) {
    auto* ctl = component(cam_go, "app.ropeway.posteffect.ColorCorrectController");
    auto* ldr = component(cam_go, "via.render.LDRPostProcess");
    auto* cc = call_ptr(ldr, "get_ColorCorrect");
    if (ctl == nullptr || cc == nullptr) return;
    const bool ctl_on = call_direct<bool>(ctl, "get_Enabled", true), cc_on = call_direct<bool>(cc, "get_Enabled", true);
    if (g.grading_off) {
        if (ctl_on) set_enabled(ctl, false);
        if (cc_on) set_enabled(cc, false);
        if (cc != g.last_cc) { g.last_cc = cc; LOGI("%s exposure: colour grading OFF (ColorCorrectController %p and LDRColorCorrect %p switched off)", TAG, (void*)ctl, (void*)cc); }
    } else if (!ctl_on) {
        set_enabled(ctl, true);   // the controller switches its component back on by itself
        g.last_cc = nullptr;
        LOGI("%s exposure: colour grading back to the game's own", TAG);
    }
}

void set_ae(MO* tm, int v) {
    static API::Method* m = API::get()->tdb()->find_method("via.render.ToneMapping", "setAutoExposure");
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)tm, v);
}
int get_ae(MO* tm) {
    static API::Method* m = API::get()->tdb()->find_method("via.render.ToneMapping", "getAutoExposure");
    return m != nullptr ? m->call<int>(API::get()->get_vm_context(), (void*)tm) : -1;
}
bool use_ae(MO* tm) { return call_direct<bool>(tm, "get_UseAutoExposure", true); }

float now_s() {
    static const DWORD t0 = GetTickCount();
    return (GetTickCount() - t0) / 1000.0f;
}

}  // namespace

namespace vn::exposure {

void render_point() {
    const bool k7 = (GetAsyncKeyState(VK_NUMPAD7) & 0x8000) != 0;
    if (k7 && !g.key7) { g.off = !g.off; LOGI("%s exposure: NUM7 -> the game's automatic brightness %s", TAG, g.off ? "OFF (ours)" : "ON (the game's own)"); }
    g.key7 = k7;
    const bool k6 = (GetAsyncKeyState(VK_NUMPAD6) & 0x8000) != 0;
    if (k6 && !g.key6) { g.grading_off = !g.grading_off; LOGI("%s exposure: NUM6 -> the game's colour grading %s", TAG, g.grading_off ? "OFF (ours)" : "ON (the game's own)"); }
    g.key6 = k6;
    const float menu = bridge::view(bridge::S_AUTO_EXPOSURE_ON);
    const bool keep_off = g.off && !(menu > 0.5f && menu < bridge::NO_VALUE - 1.0f);

    MO* cam_go = main_camera_go();
    if (cam_go == nullptr) return;
    grading(cam_go);
    MO* tm = component(cam_go, "via.render.ToneMapping");
    if (tm == nullptr) return;
    if (tm != g.last_tm) { g.last_tm = tm; g.limits_pinned = false; LOGI("%s exposure: main camera ToneMapping %p: AutoExposure %d, UseAutoExposure %d", TAG, (void*)tm, get_ae(tm), (int)use_ae(tm)); }
    if (!keep_off) {   // the game's own value stands (its controller writes Enable every frame); its limits come back once
        if (g.limits_pinned) { set_float(tm, "set_AutoExposureMinEV", GAME_MIN_EV); set_float(tm, "set_AutoExposureMaxEV", GAME_MAX_EV); g.limits_pinned = false; }
        return;
    }
    // b192: the adaptation's limits pinned to the area's own EV, so it cannot move even while the game has it on
    const float ev = call_direct<float>(tm, "get_EV", 0.0f);
    set_float(tm, "set_AutoExposureMinEV", ev);
    set_float(tm, "set_AutoExposureMaxEV", ev);
    g.limits_pinned = true;

    if (g.disable_value < 0) {
        for (int v : AE_CANDIDATES) {
            if (g.tried[v]) continue;
            g.tried[v] = true;
            set_ae(tm, v);
            const bool still = use_ae(tm);
            LOGI("%s exposure: tried AutoExposure %d: reads back %d, UseAutoExposure %d", TAG, v, get_ae(tm), (int)still);
            if (!still) { g.disable_value = v; LOGI("%s exposure: Disable = %d, kept: the game's automatic brightness is OFF", TAG, v); }
            break;   // one try per frame
        }
        if (g.disable_value < 0 && g.tried[1] && g.tried[2]) {
            g.disable_value = AE_CANDIDATES[0];
            LOGW("%s exposure: neither value read back as off; keeping %d anyway", TAG, g.disable_value);
        }
        return;
    }
    set_ae(tm, g.disable_value);
    ++g.writes;
    const float t = now_s();
    if (t - g.last_report >= REPORT_S) {
        g.last_report = t;
        LOGI("%s exposure: kept off (%d writes; AutoExposure %d, UseAutoExposure %d, EV %.2f, limits %.2f..%.2f, grading %s)", TAG, g.writes, get_ae(tm),
             (int)use_ae(tm), call_direct<float>(tm, "get_EV", -99.0f), call_direct<float>(tm, "get_AutoExposureMinEV", -99.0f),
             call_direct<float>(tm, "get_AutoExposureMaxEV", -99.0f), g.grading_off ? "off" : "the game's");
    }
}

}  // namespace vn::exposure
