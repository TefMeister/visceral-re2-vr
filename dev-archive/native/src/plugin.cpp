// plugin.cpp -- Visceral RE2's new native code (REFramework plugin, API 1.15). Started 2026-10-05.
//
// Tefa, 2026-10-05: no old C++ plugin, only new code. The old visceral_core.dll and its RELOADED port are
// read for ideas only. This file only wires things together; every feature lives in its own file.
//
// Features so far:
//   holster.cpp  step H2 -- holster spots bound to the headset hold the game's 4 shortcut slots (take out / put away)
//   shortcut.cpp the game's shortcut cross: top/left/right = guns, bottom = the equipped sub weapon
#include <windows.h>

#include <atomic>

#include "bridge.h"
#include "common.h"
#include "holster.h"
#include "shortcut.h"

using namespace vn;

namespace {
void on_frame() {
    static std::atomic<bool> installed{false};
    if (!installed.exchange(true)) { bridge::install(); shortcut::install(); }   // hooks need the type database: first game frame
    bridge::frame_begin();
    shortcut::frame();
    holster::frame();
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
    return true;
}
