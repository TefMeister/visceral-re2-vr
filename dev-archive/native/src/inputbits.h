// inputbits.h -- the game's button record (InputSystem.get_ButtonBits), read and written at UpdateBehavior PRE: after
// the VR layer has written this frame's buttons and before the game's behaviours read them. Proven writable there
// (b114, 2026-10-09: SUPPORT_HOLD put back every frame). Field offsets are RELOADED's (Down 0x10, On 0x18, Up 0x20).
#pragma once
#include "common.h"

namespace vn::inputbits {

constexpr uint64_t HOLD = 64, SUPPORT_HOLD = 128, ATTACK = 256, RELOAD = 512;   // app.ropeway.InputDefine.Kind
constexpr uint32_t OFF_DOWN = 0x10, OFF_ON = 0x18, OFF_UP = 0x20;

inline char* record() {
    auto* bb = call_ptr(API::get()->get_managed_singleton("app.ropeway.InputSystem"), "get_ButtonBits");
    return is_managed(bb) ? (char*)bb : nullptr;
}
inline bool any(uint64_t kind) {
    char* bb = record();
    if (bb == nullptr) return false;
    return ((*(uint64_t*)(bb + OFF_DOWN) | *(uint64_t*)(bb + OFF_ON)) & kind) != 0;
}
// true if the bit was set anywhere (so the caller can count the clears)
inline bool clear(uint64_t kind) {
    char* bb = record();
    if (bb == nullptr) return false;
    auto& down = *(uint64_t*)(bb + OFF_DOWN);
    auto& on = *(uint64_t*)(bb + OFF_ON);
    auto& up = *(uint64_t*)(bb + OFF_UP);
    const bool was = ((down | on | up) & kind) != 0;
    down &= ~kind;
    on &= ~kind;
    up &= ~kind;
    return was;
}
inline void hold_on(uint64_t kind) {   // the button reads as held (On); Down for the first frame is the caller's
    char* bb = record();
    if (bb == nullptr) return;
    *(uint64_t*)(bb + OFF_ON) |= kind;
    *(uint64_t*)(bb + OFF_UP) &= ~kind;
}

} // namespace vn::inputbits
