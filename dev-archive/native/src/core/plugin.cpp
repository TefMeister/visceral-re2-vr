// plugin.cpp -- Visceral RE2, the clean rebuild (2026-11-07). This file only wires things together: every feature lives in
// its own file. Design: modding-notes/2026-11-07-clean-rebuild-design.md; plan: 2026-11-07-clean-rebuild-plan.md.
//
// Layer 2, the body: the gen-1 body files moved over unchanged (src/body/), in the gen-1 order of calls, minus the parts that
// are dropped (tune, handpose, menu_probe, spreadprobe, reload_block) or come back rebuilt off Village from layer 3 on
// (reload, rack, breech, pump_native, shellclip). The body asks the gun code through guns/guns_api.h; until layer 3 the
// stub answers "nothing of ours is going on". The four functions our patched REFramework looks up: core/exports.cpp
// (visceral_dock_socket, visceral_hands_frozen) and body/subweapon.cpp (visceral_vr_buttons, visceral_sub_weapon_out).
#include <windows.h>

#include <atomic>

#include "bridge.h"
#include "common.h"
#include "exposure.h"
#include "fire.h"
#include "flashlight.h"
#include "ghosthand.h"
#include "holster.h"
#include "ladder.h"
#include "menu_body.h"
#include "menu_tint.h"
#include "options.h"
#include "pickup.h"
#include "rainhold.h"
#include "run.h"
#include "shadowhead.h"
#include "shortcut.h"
#include "spread.h"
#include "subweapon.h"
#include "suppress.h"

using namespace vn;

namespace {
void on_frame() {
    static std::atomic<bool> installed{false};
    if (!installed.exchange(true)) {   // hooks need the type database: first game frame
        bridge::install(); shortcut::install(); suppress::install(); run::install(); fire::install(); pickup::install();
        spread::install(); menu_tint::install(); flashlight::install(); rainhold::install(); exposure::install();
        LOGI("%s layer 2 (the body): hooks installed", TAG);
    }
    bridge::frame_begin();
    subweapon::frame();     // RG never aims; the sub weapon on RG at the back
    holster::frame();
    flashlight::frame();
    suppress::frame();
    spread::frame();        // bullet spread tiers
    run::frame();
    fire::frame();
    menu_body::frame();
    pickup::frame();
    menu_tint::frame();
    options::frame();
    shadowhead::frame();    // the shadow-only head copy
}
} // namespace

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    g_param = param;
    try {
        API::initialize(param);
    } catch (...) {
        param->functions->log_error("%s API init failed, nothing will run", TAG);
        return true;
    }
    param->functions->log_info("%s loaded: the clean rebuild, layer 2 (the body)", TAG);
    // the bridge Lua writes buttons at UpdateHID pre; reading after it, at UpdateBehavior pre, sees this frame's presses
    param->functions->on_pre_application_entry("UpdateBehavior", []() { on_frame(); });
    // every GUI element passes here before it is drawn; false = not drawn
    param->functions->on_pre_gui_draw_element([](void* e, void* c) { const bool a = pickup::gui_draw(e, c); const bool b = menu_tint::gui_draw(e, c); return a && b; });
    // the bridge Lua writes the view readings at LateUpdateBehavior PRE; the ladder hold reads them at POST
    param->functions->on_post_application_entry("LateUpdateBehavior", []() { ladder::late_update(); menu_body::early_hide(); menu_body::camera_point(false); });
    // the held menu camera written right where the VR layer writes it, after its pass
    param->functions->on_pre_application_entry("BeginRendering", []() { exposure::render_point(); ghosthand::render_point(); menu_body::render_point(); });
    param->functions->on_pre_application_entry("LockScene", []() { ladder::restore(false); menu_body::camera_point(false); });
    param->functions->on_post_application_entry("LockScene", []() { menu_body::late_write(); });
    param->functions->on_post_application_entry("PrepareRendering", []() { ladder::restore(true); menu_body::camera_point(true); });
    return true;
}
