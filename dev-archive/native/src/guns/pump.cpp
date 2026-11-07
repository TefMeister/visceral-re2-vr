// pump.cpp -- see pump.h.
#include "game.h"
#include "pump.h"
#include "bridge.h"
#include "gun_data.h"
#include "gun_frame.h"
#include "joints.h"
#include "rack.h"
#include "settings.h"

#include <cctype>
#include <string>

namespace vn::guns::pump {

using namespace joints;

namespace {
int g_pre_chamber = -1, g_pre_in_gun = -1;
float g_window_until = -1.0f;      // the game's own pump animation and eject are ours until then
bool g_eject_pending = false;      // a spent shell is waiting for our pull-down
float g_allow_eject_until = -1.0f; // our own request's generate() passes until then

bool pump_gun() { return is_pump(gf().wp) && bridge::live(); }
bool window() { return g_window_until > 0.0f && now_s() < g_window_until; }

std::string lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
bool has(const std::string& s, const char* tok) { return s.find(tok) != std::string::npos; }

// a pump clip? (his classifiers, C.5)
bool is_pump_clip(const std::string& name, bool player) {
    const std::string n = lower(name);
    if (player) return has(n, "pump") && (!has(n, "idle") || has(n, "hold_pump"));
    if (has(n, "idle") || has(n, "ready")) return false;
    if (has(n, "wp4300") && (has(n, "hold") || has(n, "pump"))) return true;
    const bool shotgun = has(n, "wp1000") || has(n, "wp1001") || has(n, "wp1100") || has(n, "wp1200") || has(n, "wp1500") || has(n, "sg02") || has(n, "_sg0");
    return shotgun && (has(n, "blowback") || has(n, "pump") || has(n, "cycle") || has(n, "reload") || has(n, "eject") || has(n, "rack"));
}

// a pump clip once it has played a little is sent to its last frame
void cut(MO* motion, bool player) {
    if (motion == nullptr) return;
    auto* get_layer = find_method_deep(motion->get_type_definition(), "getLayer");
    if (get_layer == nullptr) return;
    const int layers = (int)call_direct<uint32_t>(motion, "getLayerCount", 0u);
    for (int i = 0; i < layers && i < 16; ++i) {
        auto* layer = (MO*)get_layer->call<void*>(API::get()->get_vm_context(), (void*)motion, (uint32_t)i);
        auto* node = layer ? call_ptr(layer, "get_HighestWeightMotionNode") : nullptr;
        if (node == nullptr) continue;
        const std::string name = read_string(call_ptr(node, "get_MotionName"));
        if (name.empty() || !is_pump_clip(name, player) || call_direct<float>(node, "get_Weight", 0.0f) < 0.05f) continue;
        const float f = call_direct<float>(node, "get_Frame", 0.0f), end = call_direct<float>(node, "get_EndFrame", 0.0f);
        const float min_played = player ? 0.0f : (has(lower(name), "blowback") ? 0.08f : 0.20f);
        if (end <= 0.0f || f / end < min_played) continue;
        if (auto* sf = find_method_deep(layer->get_type_definition(), "set_Frame")) sf->call<void>(API::get()->get_vm_context(), (void*)layer, end);
    }
}

int pre_generate(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (!cfg::RELOAD_ON || !pump_gun() || now_s() < g_allow_eject_until) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    g_eject_pending = true;
    return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
}
int pre_wwise(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (!cfg::RELOAD_ON || !pump_gun() || !window() || argc < 3) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    const auto id = (uint32_t)(uintptr_t)argv[2];
    for (uint32_t b : cfg::PUMP_SOUND_BLOCK) if (b == id) return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_nop(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}
void hook(const char* type, const char* method, REFPreHookFn pre, const char* what) {
    auto* m = API::get()->tdb()->find_method(type, method);
    if (m == nullptr) { LOGW("%s pump: %s.%s not found (%s)", TAG, type, method, what); return; }
    m->add_hook(pre, post_nop, false);
}
} // namespace

void install() {
    if (!cfg::RELOAD_ON) return;
    hook(game::type("weapon.shell.ShellCartridgeController").c_str(), "generate", pre_generate, "the spent shell on our pull-down");
    hook(game::type("WwiseContainerApp").c_str(), "trigger(System.UInt32)", pre_wwise, "the game's pump sounds");
}

void before_shot() {
    g_pre_chamber = g_pre_in_gun = -1;
    if (!cfg::RELOAD_ON || !pump_gun() || gf().gun == nullptr) return;
    g_pre_chamber = call_direct<int>(gf().gun, "getBulletNumber", -1);
    g_pre_in_gun = call_direct<int>(API::get()->get_managed_singleton(game::type("gamemastering.InventoryManager").c_str()), "getMainWeaponRemainingBullet", -1);
}

void after_shot() {
    if (g_pre_chamber < 0) return;
    g_window_until = now_s() + cfg::PUMP_WINDOW_SEC;
    if (g_pre_chamber > 0) g_eject_pending = true;
    if (g_pre_chamber > 0 && g_pre_in_gun > 1) rack::need("a shot with another shell in the gun");
    g_pre_chamber = -1;
}

void on_pulled_down() {
    if (!g_eject_pending) return;
    g_eject_pending = false;
    auto* shells = call_ptr(gf().gun, "get_ShellCartridgeController");
    if (shells == nullptr) return;
    g_allow_eject_until = now_s() + cfg::PUMP_EJECT_ALLOW_SEC;   // request() only; its own coroutine calls generate()
    call_direct<void*>(shells, "request", nullptr);
}

void on_shell_in(bool was_empty) {
    if (was_empty) rack::need("shells put into an empty gun");
}

void frame() {
    if (cfg::RELOAD_ON && pump_gun() && window()) cut(call_ptr(gf().gun, "get_Motion"), false);
}

void update_motion() {
    if (!cfg::RELOAD_ON || !pump_gun() || !window()) return;
    cut(call_ptr(gf().gun, "get_Motion"), false);
    cut(component(gf().player, "via.motion.Motion"), true);
}

} // namespace vn::guns::pump
