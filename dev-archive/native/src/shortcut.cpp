// shortcut.cpp -- see shortcut.h. Every call here is the game's own (found in the type dump, 2026-10-05):
//   survivor.Inventory.get_ShortcutSlots()                     -> List<inventory.Slot>, read per direction
//   survivor.Inventory.equipMainSlot(EquipmentDefine.Shortcut) -> what the d-pad itself does
//   survivor.Inventory.unequipEquipedWeapon(WeaponType)        -> put the held weapon away (RELOADED used it)
//   EquipmentDefine.enableShortcut(WeaponType)                 -> the rule "may this weapon go in the cross"
#include "shortcut.h"
#include "common.h"
#include "weapons.h"

#include <atomic>

namespace vn::shortcut {

namespace {
// knife, unbreakable knife, hand grenade, flash grenade (RE Modding forum ID list, 2026-10-05)
constexpr int SUB_WEAPONS[] = {4500, 4510, 6200, 6300};

thread_local int t_asked_type = -1;     // enableShortcut's argument, carried from the pre to the post hook
std::atomic<int> g_logged_calls{0};
constexpr int LOG_FIRST_CALLS = 8;

MO* inventory() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    return component(call_ptr(pm, "get_CurrentPlayer"), "app.ropeway.survivor.Inventory");
}

MO* slot_at(Dir d) {
    auto* list = call_ptr(inventory(), "get_ShortcutSlots");
    if (list == nullptr) return nullptr;
    const int n = call_direct<int>(list, "get_Count", 0);
    if ((int)d >= n) return nullptr;
    return call_ptr(list, "get_Item", {(void*)(intptr_t)d});
}

bool is_sub_weapon(int enum_value) {
    const int wp = weapons::wp_of_enum(enum_value);
    for (int s : SUB_WEAPONS) if (s == wp) return true;
    return false;
}

int pre_enable(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    // static method: argv[0] is the thread context, argv[1] the WeaponType (checked by the first log lines)
    t_asked_type = argc > 1 ? (int)(intptr_t)argv[1] : -1;
    if (g_logged_calls.fetch_add(1) < LOG_FIRST_CALLS)
        LOGI("%s enableShortcut asked: argc=%d type=%d (WP%04d %s)", TAG, argc, t_asked_type,
             weapons::wp_of_enum(t_asked_type), weapons::name(weapons::wp_of_enum(t_asked_type)));
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_enable(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (ret_val == nullptr || !is_sub_weapon(t_asked_type)) return;
    if (((uintptr_t)*ret_val & 0xFF) == 0) {
        static std::atomic<bool> said{false};
        if (!said.exchange(true)) LOGI("%s enableShortcut: the game said NO to a sub weapon, answering YES", TAG);
    }
    *ret_val = (void*)(uintptr_t)1;
}
} // namespace

void install() {
    auto* m = API::get()->tdb()->find_method("app.ropeway.EquipmentDefine", "enableShortcut");
    if (m == nullptr) { LOGE("%s EquipmentDefine.enableShortcut not found: knife/grenades stay out of the cross", TAG); return; }
    m->add_hook(pre_enable, post_enable, false);
    LOGI("%s enableShortcut hook in (knife and grenades allowed in the shortcut cross)", TAG);
}

int weapon_in(Dir d) {
    auto* s = slot_at(d);
    if (s == nullptr || call_direct<bool>(s, "get_IsEmpty", true)) return -1;
    return weapons::wp_of_enum(call_direct<int>(s, "get_WeaponType", -1));
}

bool take_out(Dir d) {
    return call_direct<bool>(inventory(), "equipMainSlot(app.ropeway.EquipmentDefine.Shortcut)", false, (int)d);
}

bool put_away(int wp) {
    const int e = weapons::enum_of_wp(wp);
    if (e >= 0 && call_direct<bool>(inventory(), "unequipEquipedWeapon", false, e)) return true;
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    auto* eq = component(call_ptr(pm, "get_CurrentPlayer"), "app.ropeway.survivor.Equipment");
    auto* m = eq ? find_method_deep(eq->get_type_definition(), "requestHolster") : nullptr;
    if (m == nullptr) return false;
    m->call<void>(API::get()->get_vm_context(), (void*)eq);
    return true;
}

const char* dir_name(Dir d) {
    switch (d) { case UP: return "up"; case DOWN: return "down"; case LEFT: return "left"; case RIGHT: return "right"; }
    return "?";
}

void log_slots() {
    for (Dir d : {UP, DOWN, LEFT, RIGHT}) {
        const int wp = weapon_in(d);
        LOGI("%s shortcut %s: %s", TAG, dir_name(d), wp < 0 ? "empty" : weapons::name(wp));
    }
}

} // namespace vn::shortcut
