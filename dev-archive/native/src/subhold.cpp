// subhold.cpp -- see subhold.h. New code 2026-10-06 (b078's subweapon.cpp is archived; ideas reused, code not).
// Game pieces used:
//   InputSystem.setForce(InputDefine.Kind, bool)            the latch (proven for HOLD by the old plugin, 2026-08-27)
//   InputSystem.get_ButtonBits() -> Button { u64 Down @0x10, On @0x18, Up @0x20 }
//                                                           RELOADED edits these by offset at UpdateHID post
//   gui.GUIMaster.get_IsOpenInventory / Map / Pause / PauseForEvent   "a menu is up" (RELOADED's check)
//   survivor.Inventory.get_SubSlot().get_IsEmpty             a sub weapon is equipped ("E")
#include "subhold.h"
#include "common.h"

namespace vn::subhold {

namespace {
constexpr uint64_t KIND_HOLD = 64;            // InputDefine.Kind.HOLD          (RG)
constexpr uint64_t KIND_SUPPORT_HOLD = 128;   // InputDefine.Kind.SUPPORT_HOLD  (LG: ready the sub weapon)
constexpr uint64_t KIND_ATTACK = 256;         // InputDefine.Kind.ATTACK        (RT)
constexpr uint32_t OFF_DOWN = 0x10, OFF_ON = 0x18, OFF_UP = 0x20;   // Button fields (RELOADED's offsets)

bool g_out = false;
bool g_forced = false;

MO* input_system() { return API::get()->get_managed_singleton("app.ropeway.InputSystem"); }

MO* player() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    return call_ptr(pm, "get_CurrentPlayer");
}

bool menu_open() {
    auto* gm = API::get()->get_managed_singleton("app.ropeway.gui.GUIMaster");
    if (gm == nullptr) return true;          // no GUI master yet: treat as not in control
    for (const char* g : {"get_IsOpenInventory", "get_IsOpenMap", "get_IsOpenPause", "get_IsOpenPauseForEvent"})
        if (call_direct<bool>(gm, g, false)) return true;
    return false;
}

bool in_control() { return player() != nullptr && !menu_open(); }

bool has_sub_weapon() {
    auto* slot = call_ptr(component(player(), "app.ropeway.survivor.Inventory"), "get_SubSlot");
    return slot != nullptr && !call_direct<bool>(slot, "get_IsEmpty", true);
}

void set_force(bool on) {
    if (on == g_forced) return;
    auto* is = input_system();
    auto* m = is ? find_method_deep(is->get_type_definition(), "setForce") : nullptr;
    if (m == nullptr) return;
    m->call<void>(API::get()->get_vm_context(), (void*)is, KIND_SUPPORT_HOLD, on);
    g_forced = on;
    LOGI("%s SUPPORT_HOLD force %s", TAG, on ? "ON" : "off");
}

uint64_t& field(MO* bb, uint32_t off) { return *(uint64_t*)((char*)bb + off); }
} // namespace

void toggle() {
    if (!g_out && !has_sub_weapon()) { LOGI("%s left hip: no sub weapon equipped (E), nothing to take out", TAG); return; }
    g_out = !g_out;
    LOGI("%s left hip: sub weapon %s", TAG, g_out ? "OUT" : "PUT BACK");
}

bool out() { return g_out; }

void after_hid() {
    if (!in_control()) { set_force(false); return; }        // menus, inventory, title screen: hands off
    if (g_out && !has_sub_weapon()) { g_out = false; LOGI("%s sub weapon used up or unequipped: let go", TAG); }
    set_force(g_out);
    auto* is = input_system();
    if (!is_managed(is)) return;
    auto* bb = call_ptr(is, "get_ButtonBits");
    if (!is_managed(bb)) return;
    for (uint32_t off : {OFF_DOWN, OFF_ON, OFF_UP}) {
        uint64_t& v = field(bb, off);
        if (g_out) {
            const bool attack = (v & KIND_ATTACK) != 0;      // RT
            v &= ~(KIND_HOLD | KIND_ATTACK);                  // RG does nothing, RT does not drop it
            if (attack) v |= KIND_HOLD;                       // RT becomes the game's throw
        } else {
            v &= ~KIND_SUPPORT_HOLD;                          // LG no longer readies it
        }
    }
}

} // namespace vn::subhold
