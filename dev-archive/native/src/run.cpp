// run.cpp -- see run.h.
#include "run.h"
#include "bridge.h"
#include "common.h"
#include "settings.h"

#include <atomic>

namespace vn::run {

namespace {
std::atomic<bool> g_armed{false};
std::atomic<bool> g_active{false};    // only while the headset bridge is live; flat play is left alone
std::atomic<int> g_logged{0};
std::atomic<int> g_refused{0};
thread_local MO* t_orderer = nullptr;
thread_local int t_wanted = -1;

// instance method: argv[0] = thread context, argv[1] = this (the orderer), argv[2] = the bool the game wants.
// Proven layout for a static method in shortcut.cpp (argv[1] = the first argument) [verified-live 2026-10-05];
// the instance layout is the same shifted by one. The value is checked, and every first call is logged.
int pre_set_jog(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    t_orderer = nullptr;
    if (!g_active || argc < 3 || !is_managed(argv[1])) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    const auto raw = (uintptr_t)argv[2];
    if ((raw & 0xFF) > 1) {   // not a bool where we expect one: touch nothing
        if (g_refused.fetch_add(1) < cfg::RUN_LOG_FIRST)
            LOGW("%s run: set_JogMode argument is not a bool (raw 0x%llx): left alone", TAG, (unsigned long long)raw);
        return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    }
    t_wanted = (int)(raw & 1);
    argv[2] = (void*)(uintptr_t)(g_armed ? 1 : 0);
    t_orderer = (MO*)argv[1];
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

// proves the write: read the flag back after the game's setter ran
void post_set_jog(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (t_orderer == nullptr || g_logged.load() >= cfg::RUN_LOG_FIRST) return;
    static int last = -2;
    const int now = (int)call_direct<bool>(t_orderer, "get_JogMode", false);
    if (now == last) return;
    last = now;
    g_logged.fetch_add(1);
    LOGI("%s run: game wanted jog=%d, we set %d, the flag reads %d", TAG, t_wanted, (int)g_armed.load(), now);
}
} // namespace

void install() {
    auto* m = API::get()->tdb()->find_method("app.ropeway.survivor.player.PlayerActionOrderer", "set_JogMode");
    if (m == nullptr) { LOGE("%s run: set_JogMode not found, running stop off", TAG); return; }
    m->add_hook(pre_set_jog, post_set_jog, false);
    LOGI("%s run: set_JogMode hook in (left stick click runs; stick to the middle or a second click walks)", TAG);
}

void frame() {
    const bool live = bridge::live();
    g_active = live;
    if (!live) { g_armed = false; return; }
    const bool was = g_armed;
    if (bridge::pressed(bridge::S_LCLICK)) g_armed = !g_armed;                         // click: run / walk
    if (g_armed && bridge::view(bridge::S_LSTICK_MAG) <= cfg::RUN_STICK_DEADZONE) g_armed = false;   // stick let go
    if (was != g_armed) LOGI("%s run: %s", TAG, g_armed ? "RUN" : "walk");
}

} // namespace vn::run
