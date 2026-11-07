// plugin.cpp -- Visceral RE2, the clean rebuild (2026-11-07). This file only wires things together: every feature lives in
// its own file. Design: modding-notes/2026-11-07-clean-rebuild-design.md; plan: 2026-11-07-clean-rebuild-plan.md.
//
// Layer 2, the body: the gen-1 body files moved over unchanged (src/body/), in the gen-1 order of calls, minus the parts that
// are dropped (tune, handpose, menu_probe, spreadprobe). Layer 3, the guns (src/guns/, written clean 2026-11-07): the magazine,
// shells, the slide and the pump by hand, the game's own reload kept off, the empty click; one entry point, guns.h. The body
// asks the gun code through guns/guns_api.h. The four functions our patched REFramework looks up: core/exports.cpp
// (visceral_dock_socket, visceral_hands_frozen) and body/subweapon.cpp (visceral_vr_buttons, visceral_sub_weapon_out).
#include <windows.h>

#include <atomic>

#include "bridge.h"
#include "common.h"
#include "exposure.h"
#include "fire.h"
#include "flashlight.h"
#include "ghosthand.h"
#include "guns.h"
#ifdef VN_TOOLS
#include "tools.h"   // test tools: only in a VN_TOOLS build
#endif
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
        guns::install();
        LOGI("%s layers 2-3 (the body, the guns): hooks installed", TAG);
    }
    bridge::frame_begin();
    guns::frame_begin();    // the frame's gun, a swap, the game's own reload kept off (before anything reads the buttons)
    subweapon::frame();     // RG never aims; the sub weapon on RG at the back
    guns::frame();          // the magazine, shells, the slide and the pump, who has the left hand
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
    param->functions->log_info("%s loaded: the clean rebuild, layers 2-3 (the body, the guns)", TAG);
    // the bridge Lua writes buttons at UpdateHID pre; reading after it, at UpdateBehavior pre, sees this frame's presses
    param->functions->on_pre_application_entry("UpdateBehavior", []() { on_frame(); });
    // every GUI element passes here before it is drawn; false = not drawn
    param->functions->on_pre_gui_draw_element([](void* e, void* c) { const bool a = pickup::gui_draw(e, c); const bool b = menu_tint::gui_draw(e, c); return a && b; });
    // the bridge Lua writes the view readings at LateUpdateBehavior PRE; the ladder hold reads them at POST
    param->functions->on_post_application_entry("LateUpdateBehavior", []() {
        ladder::late_update(); guns::late_point(); menu_body::early_hide(); menu_body::camera_point(false);
#ifdef VN_TOOLS
        tools::late_point();
#endif
    });
    // the held menu camera written right where the VR layer writes it, after its pass
    param->functions->on_pre_application_entry("BeginRendering", []() { exposure::render_point(); guns::begin_render(); ghosthand::render_point(); menu_body::render_point(); });
    param->functions->on_pre_application_entry("UpdateMotion", []() { guns::update_motion(); });   // the game's own pump animation cut
    param->functions->on_pre_application_entry("LockScene", []() { ladder::restore(false); menu_body::camera_point(false); });
    param->functions->on_post_application_entry("LockScene", []() { menu_body::late_write(); });
    param->functions->on_post_application_entry("PrepareRendering", []() { ladder::restore(true); guns::render_point(); menu_body::camera_point(true); });
    return true;
}
