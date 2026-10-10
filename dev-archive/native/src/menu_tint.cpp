// menu_tint.cpp -- see menu_tint.h.
#include "menu_tint.h"
#include "bridge.h"
#include "common.h"
#include "menu_body.h"
#include "settings.h"

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

namespace vn::menu_tint {

namespace {
constexpr int LOG_FIRST = 40;   // calls logged per hook before it goes quiet

std::atomic<int> g_act{0}, g_act_mode{0}, g_deact{0}, g_layer_on{0}, g_layer_off{0};
MO* g_layer = nullptr;          // the InventoryLayer, caught on its first activate (add_ref'd)
bool g_was_open = false;
int g_open_frames = 0;

std::mutex g_mx;                                  // the draw callback may run off the game thread
std::unordered_map<void*, std::string> g_names;   // element -> game object name, read once
std::set<std::string> g_drawn;                    // names logged in this menu
bool g_name_cache_full = false;

bool is_hidden_name(const std::string& n) {
    for (const char* h : cfg::MENU_TINT_HIDE)
        if (n == h) return true;
    return false;
}

bool skip_now() {
    if (!cfg::MENU_TINT_OFF) return false;
    return cfg::MENU_TINT_FLAT_TOO || bridge::live();
}

int open_mode(MO* inv) {
    auto* p = field_ptr<int>(inv, "<OpenMode>k__BackingField");   // CallOpenMode: Normal 0, Map 1, GetMap 2, Map4th 3, GetItem 4, GetItemShortcut 5, UseItem 6, ItemBox 7
    return p ? *p : -1;
}

const char* mode_name(int m) {
    static const char* names[] = {"Normal", "Map", "GetMap", "Map4th", "GetItem", "GetItemShortcut", "UseItem", "ItemBox"};
    return (m >= 0 && m < 8) ? names[m] : "?";
}

void log_layer(const char* when) {
    if (g_layer == nullptr) { LOGI("%s tint: %s -- InventoryLayer not seen yet", TAG, when); return; }
    auto* active = field_ptr<bool>(g_layer, "isActive");
    auto* slot = field_ptr<int>(g_layer, "CurrentSlot");
    LOGI("%s tint: %s -- InventoryLayer isActive=%d slot=%d", TAG, when, active ? (int)*active : -1, slot ? *slot : -1);
}

// NewInventoryBehavior.activatePostEffect(): the dispatcher. Skipped = the use-item path for every mode.
int pre_activate(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    MO* inv = (argc > 1 && is_managed(argv[1])) ? (MO*)argv[1] : nullptr;
    const int mode = open_mode(inv);
    const bool skip = skip_now();
    if (g_act.fetch_add(1) < LOG_FIRST)
        LOGI("%s tint: activatePostEffect, open mode %d (%s), gui %d: %s", TAG, mode, mode_name(mode), menu_body::probe_gui_state(), skip ? "SKIPPED" : "let through");
    return skip ? REFRAMEWORK_HOOK_SKIP_ORIGINAL : REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

// the per-mode entries, in case something calls them without the dispatcher (logged either way)
int pre_activate_mode(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    const bool skip = skip_now();
    if (g_act_mode.fetch_add(1) < LOG_FIRST) LOGI("%s tint: activatePostEffectNormal/Capture: %s", TAG, skip ? "SKIPPED" : "let through");
    return skip ? REFRAMEWORK_HOOK_SKIP_ORIGINAL : REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

int pre_deactivate(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (g_deact.fetch_add(1) < LOG_FIRST) log_layer("deactivatePostEffect (let through)");
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

// InventoryLayer.activate / deactivate: the colour filter itself. Logged only (the dispatcher above is the switch);
// cfg::MENU_TINT_LAYER_SKIP additionally refuses activate, the belt to the braces.
int pre_layer_on(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    MO* layer = (argc > 1 && is_managed(argv[1])) ? (MO*)argv[1] : nullptr;
    if (layer != nullptr && g_layer == nullptr) { layer->add_ref(); g_layer = layer; }
    const bool skip = cfg::MENU_TINT_LAYER_SKIP && skip_now();
    if (g_layer_on.fetch_add(1) < LOG_FIRST) LOGI("%s tint: InventoryLayer.activate: %s", TAG, skip ? "SKIPPED" : "let through");
    return skip ? REFRAMEWORK_HOOK_SKIP_ORIGINAL : REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

int pre_layer_off(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (g_layer_off.fetch_add(1) < LOG_FIRST) LOGI("%s tint: InventoryLayer.deactivate (let through)", TAG);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_nop(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}

void hook(const char* type, const char* method, REFPreHookFn pre) {
    auto* m = API::get()->tdb()->find_method(type, method);
    if (m == nullptr) { LOGE("%s tint: %s.%s not found", TAG, type, method); return; }
    m->add_hook(pre, post_nop, false);
    LOGI("%s tint: hook in: %s.%s", TAG, type, method);
}
} // namespace

void install() {
    const char* INV = "app.ropeway.gui.NewInventoryBehavior";
    hook(INV, "activatePostEffect", pre_activate);
    hook(INV, "activatePostEffectNormal", pre_activate_mode);
    hook(INV, "activatePostEffectCapture", pre_activate_mode);
    hook(INV, "deactivatePostEffect", pre_deactivate);
    const char* LAYER = "app.ropeway.posteffect.cascade.InventoryLayer";
    hook(LAYER, "activate", pre_layer_on);
    hook(LAYER, "deactivate", pre_layer_off);
    LOGI("%s tint: menus over the world: %s (flat too: %d, layer skip: %d)", TAG, cfg::MENU_TINT_OFF ? "ON" : "off (probe only)", (int)cfg::MENU_TINT_FLAT_TOO, (int)cfg::MENU_TINT_LAYER_SKIP);
}

void frame() {
    const bool open = menu_body::is_menu_open();
    if (open && !g_was_open) { g_open_frames = 0; std::lock_guard<std::mutex> lk(g_mx); g_drawn.clear(); }
    if (open) {
        ++g_open_frames;
        if (g_open_frames == 1 || g_open_frames == 5 || g_open_frames == 30) {
            char when[48];
            snprintf(when, sizeof when, "menu open, frame %d", g_open_frames);
            log_layer(when);
        }
    } else if (g_was_open) {
        log_layer("menu closed");
    }
    g_was_open = open;
}

// The pause menu's blur + darkening is a via.gui.BlurFilter INSIDE GUI_Pause (no post effect, no separate element: the
// b129 probe named only GUI_Pause, BlackFade, WhiteFade and the guides). The VR layer draws the whole element on a quad,
// blur and all. So the element's own tree is walked and every BlurFilter is switched invisible (set_Visible false),
// logged once; the walk names the tree the first time, so the next build knows what else sits in there.
std::set<std::string> g_stripped;        // "element/filter" pairs already stripped
std::set<std::string> g_tree_logged;     // elements whose tree has been printed once
int g_tree_lines = 0;

bool wants_hide_node(const std::string& key) {
    for (const char* h : cfg::MENU_TINT_HIDE_NODES)
        if (key == h) return true;
    return false;
}

void strip_blur(const std::string& elem, MO* node, int depth, bool print) {
    int guard = 0;
    for (MO* n = node; n != nullptr && guard < 200 && depth < 12; n = call_ptr(n, "get_Next"), ++guard) {
        const std::string ty = type_name(n);
        std::string nm = read_string(call_ptr(n, "get_Name"));
        if (print && g_tree_lines < 120) { ++g_tree_lines; LOGI("%s tint: %s tree %*s%s (%s)", TAG, elem.c_str(), depth * 2, "", nm.c_str(), ty.c_str()); }
        const std::string key = elem + "/" + nm;
        if (wants_hide_node(key)) {   // a named dark mask / background panel: hidden whole, children included
            if (g_stripped.insert(key).second) {
                call_direct<void*>(n, "set_Visible", nullptr, false);
                LOGI("%s tint: %s: node '%s' (%s) hidden (set_Visible false)", TAG, elem.c_str(), nm.c_str(), ty.c_str());
            } else if (call_direct<bool>(n, "get_Visible", false)) {
                call_direct<void*>(n, "set_Visible", nullptr, false);
            }
            continue;
        }
        if (ty == "via.gui.BlurFilter") {
            if (g_stripped.insert(key).second) {
                call_direct<void*>(n, "set_Visible", nullptr, false);
                LOGI("%s tint: %s: blur filter '%s' hidden (set_Visible false)", TAG, elem.c_str(), nm.c_str());
            } else if (call_direct<bool>(n, "get_Visible", false)) {
                call_direct<void*>(n, "set_Visible", nullptr, false);   // the game switched it back: again, quietly
            }
        }
        strip_blur(elem, call_ptr(n, "get_Child"), depth + 1, print);
    }
}

bool wants_strip(const std::string& n) {
    for (const char* h : cfg::MENU_TINT_STRIP_BLUR)
        if (n == h) return true;
    return false;
}

bool gui_draw(void* gui_element, void*) {
    if (gui_element == nullptr || !menu_body::is_menu_open()) return true;
    std::string name;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        auto it = g_names.find(gui_element);
        if (it != g_names.end()) name = it->second;
    }
    if (name.empty()) {
        name = read_string(call_ptr(call_ptr((MO*)gui_element, "get_GameObject"), "get_Name"));
        if (name.empty()) name = "?";
        std::lock_guard<std::mutex> lk(g_mx);
        if (g_names.size() < (size_t)cfg::MENU_TINT_NAME_CACHE) g_names[gui_element] = name; else g_name_cache_full = true;
    }
    // flat only: the headset's VR layer already refuses GuiBack (REFramework VR.cpp 3112); dropping it flat too makes
    // a flat screenshot show what the headset shows
    if (cfg::MENU_TINT_STRIP_BLUR_ON && wants_strip(name)) {
        auto* view = call_ptr((MO*)gui_element, "get_View");
        const bool print = g_tree_logged.insert(name).second;
        if (view != nullptr) strip_blur(name, view, 0, print);
    }
    const bool hide = cfg::MENU_TINT_HIDE_FLAT && !bridge::live() && is_hidden_name(name);
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_drawn.size() < 80 && g_drawn.insert(name).second) LOGI("%s tint: drawn %s%s", TAG, name.c_str(), hide ? " (skipped, flat)" : "");
    return !hide;
}

} // namespace vn::menu_tint
