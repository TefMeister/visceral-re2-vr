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
//   fire.cpp     RT fires without RG: no aim stance, no latch (the game's own three switches, found flat 2026-10-07)
//   menu_body.cpp no third-person body in the inventory, map and pause menus (Arcade Controls' menu hide)
//   options.cpp  Run Type Hold, auto reload off, aim assist off: set once through the game's OptionManager (b112)
//   pickup.cpp   item pick-up: logs the GUI drawn and skips the black mask (b110, probe + first try)
//   menu_tint.cpp every menu over the live world: the inventory's colour filter + blur never switched on (b128)
//   subweapon.cpp the knife/grenade from a back spot on RG, held out while held; RG never aims; RT + a swing throws (b134)
//   reload.cpp   manual magazine reload (RELOADED port, bundle 1, b132); reload_block.cpp keeps the game's own off; sfx.cpp sounds
#include <windows.h>

#include <atomic>

#include "bridge.h"
#include "common.h"
#include "fire.h"
#include "holster.h"
#include "ladder.h"
#include "menu_body.h"
#include "menu_probe.h"
#include "menu_tint.h"
#include "options.h"
#include "pickup.h"
#include "reload.h"
#include "reload_block.h"
#include "run.h"
#include "shortcut.h"
#include "spread.h"
#include "subweapon.h"
#include "spreadprobe.h"
#include "suppress.h"

using namespace vn;

namespace {
void on_frame() {
    static std::atomic<bool> installed{false};
    if (!installed.exchange(true)) { bridge::install(); shortcut::install(); suppress::install(); run::install(); fire::install(); pickup::install(); spread::install(); menu_tint::install(); reload_block::install(); }   // hooks need the type database: first game frame
    menu_probe::point("UpdateBehavior.pre");
    bridge::frame_begin();
    reload_block::frame();                         // b132: before anything reads this frame's buttons
    subweapon::frame();                            // b134: RG never aims; the sub weapon on RG at the back
    reload::frame();
    holster::frame();
    suppress::frame();
    spread::frame();                               // b125: bullet spread tiers
    spreadprobe::frame();                          // b124 probe: bullet spread tiers
    run::frame();
    fire::frame();
    menu_body::frame();
    pickup::frame();
    menu_tint::frame();
    options::frame();
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
    // b110: every GUI element passes here before it is drawn; false = not drawn (pick-ups only, see pickup.h)
    param->functions->on_pre_gui_draw_element([](void* e, void* c) { const bool a = pickup::gui_draw(e, c); const bool b = menu_tint::gui_draw(e, c); return a && b; });
    // ladder: the bridge Lua writes the view readings at LateUpdateBehavior PRE; the hold reads them at POST.
    // The climbing body guard puts the body back after FirstPerson turns it (Arcade Controls' two late points).
    param->functions->on_post_application_entry("LateUpdateBehavior", []() { ladder::late_update(); reload::late_point(); menu_body::early_hide(); menu_body::camera_point(false); menu_probe::point("LateUpdateBehavior.post"); });
    // b103: the held menu camera written into the camera's root joint right where the VR layer writes it, after its pass
    param->functions->on_pre_application_entry("BeginRendering", []() { menu_body::render_point(); menu_probe::point("BeginRendering.pre"); });
    param->functions->on_pre_application_entry("UpdateScene", []() { menu_probe::point("UpdateScene.pre"); });
    param->functions->on_post_application_entry("UpdateScene", []() { menu_probe::point("UpdateScene.post"); });
    param->functions->on_pre_application_entry("LockScene", []() { menu_probe::point("LockScene.pre"); ladder::restore(false); menu_body::camera_point(false); menu_probe::point("LockScene.ours-done"); });
    param->functions->on_post_application_entry("LockScene", []() { menu_body::late_write(); menu_probe::point("LockScene.post"); });
    param->functions->on_post_application_entry("PrepareRendering", []() { menu_probe::point("PrepareRendering.post"); ladder::restore(true); reload::render_point(); menu_body::camera_point(true); menu_probe::point("PrepareRendering.ours-done"); });
    param->functions->on_pre_application_entry("UnlockScene", []() { menu_probe::point("UnlockScene.pre"); });
    param->functions->on_post_application_entry("UnlockScene", []() { menu_probe::point("UnlockScene.post"); });
    return true;
}
