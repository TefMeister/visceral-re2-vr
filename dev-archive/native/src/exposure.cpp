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
#include "exposure.h"
#include "bridge.h"
#include "common.h"
#include <windows.h>

using namespace vn;

namespace {

constexpr int AE_ENABLE = 0;           // via.render.ToneMapping.AutoExposure.Enable [verified-live 2026-10-28]
constexpr int AE_CANDIDATES[] = {2, 1};   // Disable is one of these; FixedEnable the other [inferred-static: Vignetting's Disable = 2]
constexpr float REPORT_S = 10.0f;

struct State {
    int disable_value = -1;            // learned: the value that makes get_UseAutoExposure false
    bool tried[3] = {false, false, false};
    bool off = true;                   // our switch: true = auto exposure kept off
    bool key7 = false;
    MO* last_tm = nullptr;
    int writes = 0;
    float last_report = 0.0f;
} g;

MO* main_tonemapping() {
    static API::Method* main_view = API::get()->tdb()->find_method("via.SceneManager", "get_MainView");
    static API::Method* primary = API::get()->tdb()->find_method("via.SceneView", "get_PrimaryCamera");
    void* sm = API::get()->get_native_singleton("via.SceneManager");
    if (main_view == nullptr || primary == nullptr || sm == nullptr) return nullptr;
    auto* ctx = API::get()->get_vm_context();
    void* view = main_view->call<void*>(ctx, sm);
    auto* cam = view != nullptr ? (MO*)primary->call<void*>(ctx, view) : nullptr;
    return cam != nullptr ? component(call_ptr(cam, "get_GameObject"), "via.render.ToneMapping") : nullptr;
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
    const float menu = bridge::view(bridge::S_AUTO_EXPOSURE_ON);
    const bool keep_off = g.off && !(menu > 0.5f && menu < bridge::NO_VALUE - 1.0f);

    MO* tm = main_tonemapping();
    if (tm == nullptr) return;
    if (tm != g.last_tm) { g.last_tm = tm; LOGI("%s exposure: main camera ToneMapping %p: AutoExposure %d, UseAutoExposure %d", TAG, (void*)tm, get_ae(tm), (int)use_ae(tm)); }
    if (!keep_off) return;   // the game's own value stands (its controller writes Enable every frame)

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
        LOGI("%s exposure: kept off (%d writes; AutoExposure %d, UseAutoExposure %d, EV %.2f)", TAG, g.writes, get_ae(tm),
             (int)use_ae(tm), call_direct<float>(tm, "get_EV", -99.0f));
    }
}

}  // namespace vn::exposure
