// suppress.cpp -- see suppress.h. Input bits: InputSystem.get_ButtonBits() -> Button { u64 Down @0x10, On @0x18,
// Up @0x20 } (RELOADED edits them by offset at UpdateHID post). Menu check: gui.GUIMaster.get_IsOpen*.
#include "suppress.h"
#include "bridge.h"
#include "common.h"
#include "weapons.h"

namespace vn::suppress {

namespace {
constexpr uint64_t KIND_SUPPORT_HOLD = 128;   // InputDefine.Kind.SUPPORT_HOLD (LG: ready the sub weapon)
constexpr uint64_t KIND_ATTACK = 256;         // InputDefine.Kind.ATTACK       (RT)
constexpr uint32_t OFF_DOWN = 0x10, OFF_ON = 0x18, OFF_UP = 0x20;

enum class First { NONE, RG, LG };
First g_first = First::NONE;
bool g_prev_lt = false;

bool in_control() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    if (call_ptr(pm, "get_CurrentPlayer") == nullptr) return false;
    auto* gm = API::get()->get_managed_singleton("app.ropeway.gui.GUIMaster");
    if (gm == nullptr) return false;
    for (const char* g : {"get_IsOpenInventory", "get_IsOpenMap", "get_IsOpenPause", "get_IsOpenPauseForEvent"})
        if (call_direct<bool>(gm, g, false)) return false;
    return true;
}

MO* button_bits() {
    auto* is = API::get()->get_managed_singleton("app.ropeway.InputSystem");
    auto* bb = call_ptr(is, "get_ButtonBits");
    return is_managed(bb) ? bb : nullptr;
}

bool sub_weapon_in_hand() {
    const int wp = weapons::current_id();
    return wp == 4500 || wp == 4510 || wp == 6200 || wp == 6300;   // knives, hand + flash grenade (forum ID list)
}

uint64_t& field(MO* bb, uint32_t off) { return *(uint64_t*)((char*)bb + off); }

void clear(MO* bb, uint64_t mask) {
    for (uint32_t off : {OFF_DOWN, OFF_ON, OFF_UP}) field(bb, off) &= ~mask;
}
} // namespace

void after_hid() {
    if (!bridge::live()) return;
    const bool rg = bridge::held(bridge::S_RGRIP), lg = bridge::held(bridge::S_LGRIP), lt = bridge::held(bridge::S_LTRIG);

    // who went down first
    if (!rg && !lg) g_first = First::NONE;
    else if (g_first == First::NONE) g_first = rg ? First::RG : First::LG;
    else if (g_first == First::RG && !rg) g_first = lg ? First::LG : First::NONE;
    else if (g_first == First::LG && !lg) g_first = rg ? First::RG : First::NONE;

    if (!in_control()) return;
    auto* bb = button_bits();
    if (bb == nullptr) return;

    // LT's game action is not known yet: log the input bits when the left trigger goes down, once per press
    if (lt && !g_prev_lt) LOGI("%s left trigger pressed: game input bits On=0x%llx", TAG, (unsigned long long)field(bb, OFF_ON));
    g_prev_lt = lt;

    if (g_first == First::RG) clear(bb, KIND_SUPPORT_HOLD);     // aiming: no sub weapon
    // LG is also the two-handed grip on guns: RT is only blocked when a knife or grenade is really in the hand
    if (g_first == First::LG && sub_weapon_in_hand()) clear(bb, KIND_ATTACK);   // RT cannot drop it
}

} // namespace vn::suppress
