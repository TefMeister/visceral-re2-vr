// subprobe.cpp -- see subprobe.h.
#include "subprobe.h"
#include "bridge.h"
#include "common.h"
#include "settings.h"
#include "weapons.h"

#include <cstdint>

namespace vn::subprobe {

namespace {
constexpr uint64_t KIND_HOLD = 64, KIND_SUPPORT_HOLD = 128;
constexpr uint32_t OFF_DOWN = 0x10, OFF_ON = 0x18;   // Button fields (RELOADED's offsets, see archive/subhold.cpp)

int g_lines = 0;
int g_frame = 0;
struct State { int hold = -1, reload = -1, wp = -2, sh = -1, h = -1, forced = -1; } g_last;

MO* condition() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    return component(call_ptr(pm, "get_CurrentPlayer"), "app.ropeway.survivor.SurvivorCondition");
}

// 0 = off, 1 = down this frame, 2 = held on
int bit_state(MO* bb, uint64_t kind) {
    if (!is_managed(bb)) return -1;
    const uint64_t down = *(uint64_t*)((char*)bb + OFF_DOWN), on = *(uint64_t*)((char*)bb + OFF_ON);
    return (down & kind) ? 1 : (on & kind) ? 2 : 0;
}
} // namespace

void frame(bool support_forced) {
    ++g_frame;
    if (g_lines >= cfg::SUBPROBE_MAX_LINES || !bridge::live() || !bridge::held(bridge::S_LGRIP)) { g_last = State{}; return; }
    auto* cond = condition();
    auto* bb = call_ptr(API::get()->get_managed_singleton("app.ropeway.InputSystem"), "get_ButtonBits");
    State s;
    s.hold = call_direct<bool>(cond, "get_IsHold", false) ? 1 : 0;
    s.reload = call_direct<bool>(cond, "get_IsReload", false) ? 1 : 0;
    s.wp = weapons::current_id();
    s.sh = bit_state(bb, KIND_SUPPORT_HOLD);
    s.h = bit_state(bb, KIND_HOLD);
    s.forced = support_forced ? 1 : 0;
    // the down-edge (1) becomes held (2) next frame on its own; only log real changes
    const auto norm = [](int v) { return v == 1 ? 2 : v; };
    if (s.hold == g_last.hold && s.reload == g_last.reload && s.wp == g_last.wp && norm(s.sh) == norm(g_last.sh) &&
        norm(s.h) == norm(g_last.h) && s.forced == g_last.forced)
        return;
    g_last = s;
    ++g_lines;
    const Vec3 l = bridge::hand_pos(bridge::LEFT), r = bridge::hand_pos(bridge::RIGHT), hm = bridge::hmd_pos();
    LOGI("%s subprobe f%d: IsHold %d IsReload %d wp %d | game bits SUPPORT_HOLD %d HOLD %d | setForce %d | hands %.3f m apart, "
         "left %.3f m right %.3f m from the headset",
         TAG, g_frame, s.hold, s.reload, s.wp, s.sh, s.h, s.forced, dist(l, r), dist(l, hm), dist(r, hm));
}

} // namespace vn::subprobe
