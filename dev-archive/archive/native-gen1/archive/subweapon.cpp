// subweapon.cpp -- see subweapon.h. Game calls used (type dump, 2026-10-06):
//   InputSystem.setForce(InputDefine.Kind, bool)  -- the latch the old plugin proved for HOLD on 2026-08-27
//   InputSystem.get_ButtonBits() -> Button { u64 Down, On, Up }  -- cleared bits = the game never sees the press
//                                                                  (RELOADED strips SUPPORT_HOLD the same way)
//   survivor.Inventory.get_SubSlot().get_IsEmpty  -- nothing equipped as sub weapon: let go of the latch
#include "subweapon.h"
#include "common.h"

namespace vn::subweapon {

namespace {
constexpr uint64_t KIND_HOLD = 64;            // InputDefine.Kind.HOLD (RG: aim)
constexpr uint64_t KIND_SUPPORT_HOLD = 128;   // InputDefine.Kind.SUPPORT_HOLD (ready the sub weapon)

bool g_out = false;
bool g_forced = false;      // what setForce(SUPPORT_HOLD) was last told

MO* input_system() { return API::get()->get_managed_singleton("app.ropeway.InputSystem"); }

void set_force(bool on) {
    auto* is = input_system();
    auto* m = is ? find_method_deep(is->get_type_definition(), "setForce") : nullptr;
    if (m == nullptr) return;
    m->call<void>(API::get()->get_vm_context(), (void*)is, KIND_SUPPORT_HOLD, on);
    g_forced = on;
}

// clear the given input bits from this frame's buttons, so the game's own logic never sees them
void strip(uint64_t mask) {
    auto* bb = call_ptr(input_system(), "get_ButtonBits");
    if (bb == nullptr) return;
    auto* td = bb->get_type_definition();
    for (const char* name : {"Down", "On", "Up"}) {
        auto* f = td ? td->find_field(name) : nullptr;
        if (f == nullptr) continue;
        auto& v = f->get_data<uint64_t>(bb);
        v &= ~mask;
    }
}

bool has_sub_weapon() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    auto* inv = component(call_ptr(pm, "get_CurrentPlayer"), "app.ropeway.survivor.Inventory");
    auto* slot = call_ptr(inv, "get_SubSlot");
    return slot != nullptr && !call_direct<bool>(slot, "get_IsEmpty", true);
}
} // namespace

void toggle() {
    if (!g_out && !has_sub_weapon()) { LOGI("%s left hip: no sub weapon equipped, nothing to take out", TAG); return; }
    g_out = !g_out;
    LOGI("%s left hip: sub weapon %s", TAG, g_out ? "OUT (SUPPORT_HOLD latched)" : "PUT BACK");
}

bool out() { return g_out; }

void frame() {
    if (g_out && !has_sub_weapon()) { g_out = false; LOGI("%s sub weapon used up: latch released", TAG); }
    if (g_out != g_forced) set_force(g_out);
    if (g_out) strip(KIND_HOLD);              // Tefa: RG does nothing while the sub weapon is out
    else strip(KIND_SUPPORT_HOLD);            // Tefa: holding LG must not ready the sub weapon
}

} // namespace vn::subweapon
