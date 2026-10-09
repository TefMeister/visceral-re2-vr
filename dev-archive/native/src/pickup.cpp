// pickup.cpp -- see pickup.h.
#include "pickup.h"
#include "bridge.h"
#include "common.h"
#include "menu_body.h"
#include "settings.h"

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

namespace vn::pickup {

namespace {
std::atomic<bool> g_on{false};        // a pick-up is up (set by the hook, cleared when the inventory closes again)
std::atomic<bool> g_live{false};      // the headset bridge is live; flat play is left alone
bool g_seen_open = false;             // the inventory has read open since the hook fired
int g_frames = 0;                     // frames since the hook fired (gives up if the inventory never opens)
std::atomic<int> g_count{0};          // pick-ups so far, for the log

std::mutex g_mx;                      // the draw callback may run off the game thread
std::unordered_map<void*, std::string> g_names;   // element -> game object name, read once per element
std::set<std::string> g_logged;       // names already logged in this pick-up
std::set<std::string> g_skip_logged;

int pre_open_get_item(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (!g_live) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_logged.clear();
        g_skip_logged.clear();
        g_names.clear();   // element addresses can be reused by other elements between pick-ups
    }
    g_seen_open = false;
    g_frames = 0;
    g_on = true;
    LOGI("%s pickup #%d: get-item inventory opening", TAG, g_count.fetch_add(1) + 1);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_open_get_item(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}

bool is_hidden_name(const std::string& n) {
    for (const char* h : cfg::PICKUP_HIDE)
        if (n == h) return true;
    return false;
}
} // namespace

void install() {
    auto* m = API::get()->tdb()->find_method("app.ropeway.gui.GUIMaster", "openInventoryGetItemMode");
    if (m == nullptr) { LOGE("%s pickup: openInventoryGetItemMode not found, pick-up probe off", TAG); return; }
    m->add_hook(pre_open_get_item, post_open_get_item, false);
    LOGI("%s pickup: openInventoryGetItemMode hook in (probe + skip %s)", TAG, cfg::PICKUP_HIDE_ON ? "on" : "off");
}

void frame() {
    g_live = bridge::live();
    if (!g_on) return;
    const bool open = menu_body::is_menu_open();
    if (open) g_seen_open = true;
    ++g_frames;
    if ((g_seen_open && !open) || (!g_seen_open && g_frames > cfg::PICKUP_GIVE_UP_FRAMES) || !g_live) {
        g_on = false;
        LOGI("%s pickup: get-item inventory closed (%d frames, opened %d)", TAG, g_frames, g_seen_open ? 1 : 0);
    }
}

bool gui_draw(void* gui_element, void*) {
    if (!g_on || gui_element == nullptr) return true;
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
        if (g_names.size() < (size_t)cfg::PICKUP_NAME_CACHE) g_names[gui_element] = name;
    }
    const bool hide = cfg::PICKUP_HIDE_ON && is_hidden_name(name);
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_logged.insert(name).second) LOGI("%s pickup: drawn %s%s", TAG, name.c_str(), hide ? " (skipped)" : "");
    if (hide && g_skip_logged.insert(name).second) LOGI("%s pickup: skipped %s", TAG, name.c_str());
    return !hide;
}

} // namespace vn::pickup
