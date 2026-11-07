// ammo.cpp -- see ammo.h. RELOADED's two rungs for putting rounds in: Inventory.reloadMainSlot first, then
// Equipment.executeReload as the fallback (credit: Andyalpa).
#include "game.h"
#include "ammo.h"
#include "game_reload.h"
#include "gun_frame.h"

#include <algorithm>

namespace vn::guns::ammo {

namespace {
bool add_rounds(int n) {
    const int before = loaded();
    game_reload::Commit c;
    call_direct<bool>(gf().inventory, "reloadMainSlot", false, (int32_t)n);
    if (loaded() > before) return true;
    auto* m = API::get()->tdb()->find_method(game::type("survivor.Equipment").c_str(), ("executeReload(" + game::type("EquipmentDefine.WeaponType") + ", System.Int32)").c_str());
    const int wt = call_direct<int>(gf().gun, "get_WeaponType", -1);
    if (m != nullptr && wt >= 0) m->call<bool>(API::get()->get_vm_context(), (void*)gf().equipment, wt, (int32_t)n);
    return loaded() > before;
}
} // namespace

int loaded() { return gf().gun ? call_direct<int>(gf().gun, "getBulletNumber", -1) : -1; }
int capacity() { return call_direct<int>(call_ptr(gf().inventory, "get_MainSlot"), "get_MaxNumber", -1); }

int reserve() {
    const int a = call_direct<int>(API::get()->get_managed_singleton(game::type("gamemastering.InventoryManager").c_str()), "getMainWeaponReloadableBullet", -1);
    auto* m = API::get()->tdb()->find_method(game::type("survivor.Inventory").c_str(), "getReloadableBulletMainSlot");
    const int b = (m != nullptr && gf().inventory != nullptr) ? m->call<int>(API::get()->get_vm_context(), (void*)gf().inventory, false) : -1;
    return std::max(a, b);
}

int space() {
    const int cap = capacity(), have = loaded();
    return (cap > 0 && have >= 0) ? cap - have : 0;
}

int top_up() {
    const int before = loaded(), carried = reserve();
    const int add = std::min(std::max(0, capacity() - before), std::max(0, carried));
    const bool ok = add <= 0 || add_rounds(add);
    end_reload(false);
    LOGI("%s ammo: WP%04d topped up: loaded %d -> %d, carried %d -> %d%s", TAG, gf().wp, before, loaded(), carried, reserve(), ok ? "" : " -- NOTHING WENT IN");
    return before;
}

bool insert_one() {
    const int before = loaded(), carried = reserve();
    const bool ok = carried > 0 && add_rounds(1);
    end_reload(false);
    LOGI("%s ammo: WP%04d one round in: loaded %d -> %d, carried %d -> %d%s", TAG, gf().wp, before, loaded(), carried, reserve(), ok ? "" : " -- NOTHING WENT IN");
    return ok;
}

void end_reload(bool eject) {
    if (gf().gun == nullptr) return;
    game_reload::Commit c;
    call_direct<void*>(gf().gun, "executeEndReload", nullptr);
    if (eject) call_direct<void*>(gf().gun, "executeEndEject", nullptr);
}

} // namespace vn::guns::ammo
