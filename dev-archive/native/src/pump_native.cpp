// pump_native.cpp -- see pump_native.h.
#include "pump_native.h"
#include "bridge.h"
#include "joints.h"
#include "rack.h"
#include "reload_data.h"
#include "settings.h"
#include "weapons.h"

#include <atomic>
#include <string>
#include <vector>

namespace vn::pump_native {

using namespace joints;
using namespace reload;   // the weapon tables (reload_data.h)

namespace {
int g_pre_chamber = -1, g_pre_in_gun = -1;
float g_window_until = -1.0f;      // the game's pump animation + eject are ours until then
bool g_eject_pending = false;      // a shell eject was held back
int g_eject_wp = -1;
std::atomic<int> g_ejects_held{0}, g_kills{0}, g_wwise{0};
std::vector<std::string> g_killed_names;

bool pump_gun() { return is_pump(weapons::current_id()); }
bool window() { return g_window_until > 0.0f && now_s() < g_window_until; }
int loaded(MO* gun) { return call_direct<int>(gun, "getBulletNumber", -1); }
int in_gun() {   // the HUD's loaded count (chamber + tube)
    const int n = call_direct<int>(API::get()->get_managed_singleton("app.ropeway.gamemastering.InventoryManager"), "getMainWeaponRemainingBullet", -1);
    return n;
}

MO* player_go() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }
MO* gun_now() { return field_obj(component(player_go(), "app.ropeway.survivor.Equipment"), "<EquipWeapon>k__BackingField"); }

bool lower_has(const std::string& s, const char* tok) { return s.find(tok) != std::string::npos; }
std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

// a shotgun / Spark Shot pump clip? (his classifiers, C.5)
bool is_pump_clip(const std::string& name, bool player) {
    const std::string n = lower(name);
    if (player) return lower_has(n, "pump") && (!lower_has(n, "idle") || lower_has(n, "hold_pump"));
    if (lower_has(n, "idle") || lower_has(n, "ready")) return false;
    const bool gun = lower_has(n, "wp1000") || lower_has(n, "wp1001") || lower_has(n, "wp1100") || lower_has(n, "wp1200") || lower_has(n, "wp1300") || lower_has(n, "wp1500") || lower_has(n, "sg02") || lower_has(n, "_sg0");
    const bool spark = lower_has(n, "wp4300") && (lower_has(n, "hold") || lower_has(n, "pump"));
    if (spark) return true;
    return gun && (lower_has(n, "blowback") || lower_has(n, "pump") || lower_has(n, "cycle") || lower_has(n, "reload") || lower_has(n, "eject") || lower_has(n, "rack"));
}

void scrub(MO* motion, bool player) {
    if (motion == nullptr) return;
    const int layers = (int)call_direct<uint32_t>(motion, "getLayerCount", 0u);
    for (int i = 0; i < layers && i < 16; ++i) {
        auto* m = find_method_deep(motion->get_type_definition(), "getLayer");
        if (m == nullptr) return;
        auto* layer = (MO*)m->call<void*>(API::get()->get_vm_context(), (void*)motion, (uint32_t)i);
        if (layer == nullptr) continue;
        auto* node = call_ptr(layer, "get_HighestWeightMotionNode");
        if (node == nullptr) continue;
        const std::string name = read_string(call_ptr(node, "get_MotionName"));
        if (name.empty() || !is_pump_clip(name, player)) continue;
        const float w = call_direct<float>(node, "get_Weight", 0.0f);
        if (w < 0.05f) continue;
        const float f = call_direct<float>(node, "get_Frame", 0.0f), ef = call_direct<float>(node, "get_EndFrame", 0.0f);
        const float min_p = player ? 0.0f : (lower_has(lower(name), "blowback") ? 0.08f : 0.20f);
        if (ef > 0.0f && f / ef < min_p) continue;
        if (ef > 0.0f) {
            auto* sf = find_method_deep(layer->get_type_definition(), "set_Frame");
            if (sf != nullptr) sf->call<void>(API::get()->get_vm_context(), (void*)layer, ef);
        }
        const int n = g_kills.fetch_add(1) + 1;
        if (n <= 30) LOGI("%s pump: the game's %s clip '%s' cut at %.0f/%.0f (layer %d, weight %.2f)", TAG, player ? "player" : "weapon", name.c_str(), f, ef, i, w);
    }
}

// ---- the shell eject, held back and replayed on our pull ---------------------------------------------------------
int pre_generate(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (!cfg::PUMP_NATIVE_ON || !pump_gun() || !bridge::live()) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    if (g_eject_pending && g_eject_wp == weapons::current_id() && rack::active()) return REFRAMEWORK_HOOK_CALL_ORIGINAL;   // our replay
    g_eject_pending = true;
    g_eject_wp = weapons::current_id();
    const int n = g_ejects_held.fetch_add(1) + 1;
    if (n <= 20) LOGI("%s pump: shell eject held back until the pull-down (#%d)", TAG, n);
    return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
}
void post_nop(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}

// the game's own pump sounds: log the trigger IDs heard in the window (no name lookup on this build)
int pre_wwise(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (!cfg::PUMP_NATIVE_ON || !pump_gun() || !window() || argc < 3) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    const auto id = (uint32_t)(uintptr_t)argv[2];
    const int n = g_wwise.fetch_add(1) + 1;
    if (n <= 60) LOGI("%s pump: Wwise trigger %u heard in the pump window (#%d)", TAG, id, n);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void hook(const char* type, const char* method, REFPreHookFn pre, const char* what) {
    auto* m = API::get()->tdb()->find_method(type, method);
    if (m == nullptr) { LOGW("%s pump: %s.%s not found (%s)", TAG, type, method, what); return; }
    m->add_hook(pre, post_nop, false);
    LOGI("%s pump: hook in: %s.%s (%s)", TAG, type, method, what);
}
} // namespace

void install() {
    if (!cfg::PUMP_NATIVE_ON) return;
    hook("app.ropeway.weapon.shell.ShellCartridgeController", "generate", pre_generate, "shell eject on our pull-down");
    hook("app.ropeway.WwiseContainerApp", "trigger(System.UInt32)", pre_wwise, "pump sound IDs logged");
}

void on_pre_fire(MO* gun) {
    g_pre_chamber = g_pre_in_gun = -1;
    if (!cfg::PUMP_NATIVE_ON || !pump_gun() || gun == nullptr) return;
    g_pre_chamber = loaded(gun);
    g_pre_in_gun = in_gun();
}

void on_post_fire(MO*) {
    if (!cfg::PUMP_NATIVE_ON || !pump_gun() || g_pre_chamber < 0) return;
    g_window_until = now_s() + cfg::PUMP_WINDOW_SEC;
    if (g_pre_chamber > 0 && g_pre_in_gun > 1) rack::need("a shot with another shell in the gun");
    else LOGI("%s pump: shot with chamber %d, in gun %d: no pump needed (empty until shells go in)", TAG, g_pre_chamber, g_pre_in_gun);
    g_pre_chamber = -1;
}

void on_pulled_down() {
    if (!g_eject_pending) return;
    auto* gun = gun_now();
    auto* scc = call_ptr(gun, "get_ShellCartridgeController");
    g_eject_pending = false;
    if (scc == nullptr) { LOGW("%s pump: no ShellCartridgeController to replay the eject", TAG); return; }
    g_eject_pending = true;   // the replay's generate must pass: pre_generate lets it through while rack::active()
    call_direct<void*>(scc, "request", nullptr);
    g_eject_pending = false;
    LOGI("%s pump: spent shell ejected on the pull-down", TAG);
}

void on_shells_inserted(bool was_empty) {
    if (was_empty) rack::need("shells put into an empty gun");
}

void frame() {
    if (!cfg::PUMP_NATIVE_ON || !pump_gun() || !window()) return;
    scrub(call_ptr(gun_now(), "get_Motion"), false);
}

void update_motion() {
    if (!cfg::PUMP_NATIVE_ON || !pump_gun() || !window()) return;
    scrub(call_ptr(gun_now(), "get_Motion"), false);
    scrub(component(player_go(), "via.motion.Motion"), true);
}

} // namespace vn::pump_native
