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
// The inventory menu's "Shortcut" option asks Inventory.enableSetShortcut(Slot), not enableShortcut (b076: the
// latter was never called while Tefa browsed the menu) [verified-live 2026-10-05]. Instance method: argv[1] = this,
// argv[2] = the Slot [hypothesis; the first calls are logged].
thread_local MO* t_asked_slot = nullptr;
std::atomic<int> g_logged_set{0};

int pre_enable_set(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    t_asked_slot = argc > 2 && is_managed(argv[2]) ? (MO*)argv[2] : nullptr;
    if (g_logged_set.fetch_add(1) < LOG_FIRST_CALLS)
        LOGI("%s enableSetShortcut asked: argc=%d slot=%s sub=%d", TAG, argc, type_name(t_asked_slot).c_str(),
             t_asked_slot ? (int)call_direct<bool>(t_asked_slot, "get_IsSubWeapon", false) : -1);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_enable_set(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (ret_val == nullptr || t_asked_slot == nullptr) return;
    if (!call_direct<bool>(t_asked_slot, "get_IsSubWeapon", false)) return;
    *ret_val = (void*)(uintptr_t)1;
}

// b077 skipped PlayerActionOrderer.set_RequestSubShortcut to stop LG; it never fired, so it is not the LG path
// [verified-live 2026-10-05] and is gone. LG is now stopped in subweapon.cpp (SUPPORT_HOLD bits cleared).
// Every call into Inventory.equipSubSlot* is still LOGGED (not blocked) to see who equips sub weapons.
std::atomic<int> g_sub_equips{0};
int pre_log_sub_equip(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (g_sub_equips.fetch_add(1) < 20) LOGI("%s Inventory.equipSubSlot* called (not blocked yet)", TAG);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void hook(const char* type, const char* method, REFPreHookFn pre, REFPostHookFn post, const char* what) {
    auto* m = API::get()->tdb()->find_method(type, method);
    if (m == nullptr) { LOGE("%s %s.%s not found: %s off", TAG, type, method, what); return; }
    m->add_hook(pre, post, false);
    LOGI("%s hook in: %s.%s (%s)", TAG, type, method, what);
}
} // namespace

void install() {
    hook("app.ropeway.survivor.Inventory", "enableSetShortcut", pre_enable_set, post_enable_set, "sub weapons get the menu's Shortcut option");
    hook("app.ropeway.survivor.Inventory", "equipSubSlot(app.ropeway.inventory.Slot)", pre_log_sub_equip, nullptr, "log");
    hook("app.ropeway.survivor.Inventory", "equipSubSlot(app.ropeway.EquipmentDefine.Shortcut)", pre_log_sub_equip, nullptr, "log");
    hook("app.ropeway.survivor.Inventory", "equipSubSlotLastWeapon", pre_log_sub_equip, nullptr, "log");
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
    // BOTH calls, like RELOADED's native_holster_stow: b076 showed unequipEquipedWeapon alone returns true and
    // leaves the gun in hand [verified-live 2026-10-05, n=3]; requestHolster is what plays the put-away.
    const int e = weapons::enum_of_wp(wp);
    if (e >= 0) call_direct<bool>(inventory(), "unequipEquipedWeapon", false, e);
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
