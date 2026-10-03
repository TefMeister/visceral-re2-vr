// rld_dryfire.cpp -- the click on an empty trigger (2026-10-03, port step 2). RELOADED: reload.lua 2807-2937
// (install_dry_fire_hook): a pre-hook on requestFire of implement.Gun and survivor.Equipment; when the drawn weapon
// has no round, play `dry_fire` for that weapon. RELOADED also requires its manual-reload mode to be on; the port has
// no blocking layer yet (step 3), so this step plays on any empty requestFire and LOGS the round count of the first
// calls -- one run shows whether the stock game calls requestFire at all on an empty gun (it may reload instead).
// It never blocks the call (no SKIP_ORIGINAL): blocking is step 3's job.
#include "../visceral.h"
#include "port.h"
#include "port_settings.h"

namespace visceral::port {

namespace {
std::atomic<int> g_calls{0};
int g_selftest_left = SFX_SELFTEST_PRESSES;
bool g_rb_prev = false;

int rounds_now() {
    // getBulletNumber on the equipped weapon: RELOADED's `sc(weapon, "getBulletNumber")` (reload.lua 3045). -1 = unknown.
    return call_direct<int>(g.weapon, "getBulletNumber", -1);
}

int pre_request_fire(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    const int n = rounds_now();
    const std::string wp = current_wp();
    const int k = g_calls.fetch_add(1) + 1;
    if (k <= DRYFIRE_LOG_FIRST) LOGI("%s dryfire: requestFire #%d, weapon %s, rounds %d", TAG, k, wp.empty() ? "?" : wp.c_str(), n);
    if (n == 0) sfx_play("dry_fire", wp);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
} // namespace

std::string current_wp() {
    if (g.weapon == nullptr) return {};
    return sysstr(inv_ptr(inv_ptr(g.weapon, "get_GameObject"), "get_Name"));
}

void dryfire_install() {
    rld_data_load();
    sfx_init();
    auto& api = API::get();
    int ok = 0;
    for (const char* type : DRYFIRE_HOOK_TYPES) {
        API::Method* m = api->tdb()->find_method(type, DRYFIRE_HOOK_METHOD);
        if (m == nullptr) if (auto* td = api->tdb()->find_type(type); td != nullptr) m = find_method_deep(td, DRYFIRE_HOOK_METHOD);
        if (m == nullptr) { LOGW("%s dryfire: %s.%s not found", TAG, type, DRYFIRE_HOOK_METHOD); continue; }
        const auto id = m->add_hook(pre_request_fire, nullptr, false);
        LOGI("%s dryfire: hooked %s.%s id=%u", TAG, type, DRYFIRE_HOOK_METHOD, id);
        ++ok;
    }
    LOGI("%s dryfire: %d of 2 requestFire hooks in; the first %d right-B presses play dry_fire as a sound self-test", TAG, ok, SFX_SELFTEST_PRESSES);
}

void dryfire_frame() {
    if (g_selftest_left <= 0 || !bridge_live()) return;
    const bool rb = bridge_down(S_RB);
    if (rb && !g_rb_prev) {
        --g_selftest_left;
        std::string wp = current_wp();
        if (wp.empty()) wp = SFX_SELFTEST_FALLBACK_WP;
        const bool played = sfx_play("dry_fire", wp);
        LOGI("%s sfx self-test (right B) %d of %d: %s", TAG, SFX_SELFTEST_PRESSES - g_selftest_left, SFX_SELFTEST_PRESSES, played ? "played" : "NOT played (see the line above)");
    }
    g_rb_prev = rb;
}

} // namespace visceral::port
