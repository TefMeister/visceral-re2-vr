// plugin.cpp -- Visceral RE2's new native code (REFramework plugin, API 1.15). Started 2026-10-05.
//
// Tefa, 2026-10-05: no old C++ plugin, only new code. The old visceral_core.dll and its RELOADED port are
// read for ideas only. This file only wires things together; every feature lives in its own file.
//
// Features so far:
//   holster.cpp  step H2 -- holster spots bound to the headset hold the game's 4 shortcut slots (take out / put away)
//   shortcut.cpp the game's shortcut cross; the knife and grenades are allowed into it
//   suppress.cpp RG first keeps the gun in hand (Arcade Controls' force-equip); LG first with a knife/grenade: RT ignored
//   ladder.cpp   ladder + cupboard view hold and the climbing body guard (Arcade Controls' v12.2, in C++)
//   run.cpp      running stops at once: stick let go or a second click (Arcade Controls' set_JogMode override)
//   menu_body.cpp no third-person body in the inventory, map and pause menus (Arcade Controls' menu hide)
#include <windows.h>

#include <atomic>

#include "bridge.h"
#include "common.h"
#include "holster.h"
#include "ladder.h"
#include "menu_body.h"
#include "menu_probe.h"
#include "run.h"
#include "shortcut.h"
#include "suppress.h"

using namespace vn;

namespace {
void on_frame() {
    static std::atomic<bool> installed{false};
    if (!installed.exchange(true)) { bridge::install(); shortcut::install(); suppress::install(); run::install(); }   // hooks need the type database: first game frame
    menu_probe::point("UpdateBehavior.pre");
    bridge::frame_begin();
    holster::frame();
    suppress::frame();
    run::frame();
    menu_body::frame();
    menu_probe::point("UpdateBehavior.ours-done");
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
    param->functions->log_info("%s loaded: step H2 holsters = the shortcut cross", TAG);
    // the bridge Lua writes buttons at UpdateHID pre; reading after it, at UpdateBehavior pre, sees this frame's presses
    param->functions->on_pre_application_entry("UpdateBehavior", []() { on_frame(); });
    // ladder: the bridge Lua writes the view readings at LateUpdateBehavior PRE; the hold reads them at POST.
    // The climbing body guard puts the body back after FirstPerson turns it (Arcade Controls' two late points).
    param->functions->on_post_application_entry("LateUpdateBehavior", []() { ladder::late_update(); menu_body::early_hide(); menu_body::camera_point(false); menu_probe::point("LateUpdateBehavior.post"); });
    param->functions->on_pre_application_entry("UpdateScene", []() { menu_probe::point("UpdateScene.pre"); });
    param->functions->on_post_application_entry("UpdateScene", []() { menu_probe::point("UpdateScene.post"); });
    param->functions->on_pre_application_entry("LockScene", []() { menu_probe::point("LockScene.pre"); ladder::restore(false); menu_body::camera_point(false); menu_probe::point("LockScene.ours-done"); });
    param->functions->on_post_application_entry("PrepareRendering", []() { menu_probe::point("PrepareRendering.post"); ladder::restore(true); menu_body::camera_point(true); menu_probe::point("PrepareRendering.ours-done"); });
    param->functions->on_pre_application_entry("UnlockScene", []() { menu_probe::point("UnlockScene.pre"); });
    param->functions->on_post_application_entry("UnlockScene", []() { menu_probe::point("UnlockScene.post"); });
    return true;
}
