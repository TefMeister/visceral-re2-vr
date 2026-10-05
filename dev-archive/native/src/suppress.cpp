// suppress.cpp -- see suppress.h.
//
// b081 cleared input bits (InputSystem.get_ButtonBits) and did nothing: the record read 0 on every press
// [verified-live 2026-10-06, 7 LT presses]. Arcade Controls had already learned that clearing SUPPORT_HOLD "was a
// losing race against native per-frame recomputation" (re2_vr_suppress_supporthold.lua) and instead CORRECTED THE
// RESULT: while RG is held (RG first), it writes the main weapon into Equipment.<ForceEquipType>k__BackingField and
// <ForceEquipParts>k__BackingField (Nullable structs) every frame, then clears them 3 frames after release. That
// method, re-written here in C++.
//
// RT while a knife/grenade is out (LG first): Arcade Controls had no rule for it. Here the game's own question "is
// ATTACK on?" (InputSystem/InputUnit.isOn(InputDefine.Kind)) is answered NO for ATTACK while that is the case.
// [hypothesis: the sub-weapon throw asks isOn; the first answers are logged to check].
#include "suppress.h"
#include "bridge.h"
#include "common.h"
#include "holster.h"
#include "weapons.h"

#include <atomic>

namespace vn::suppress {

namespace {
constexpr uint64_t KIND_ATTACK = 256;         // InputDefine.Kind.ATTACK (RT)
constexpr int CLEAR_DELAY_FRAMES = 3;         // Arcade Controls' pulse-then-clear delay

enum class First { NONE, RG, LG, HOLSTER };   // HOLSTER = RG pressed inside a holster spot: never forced
First g_first = First::NONE;
bool g_forcing = false;
int g_clear_in = 0;
bool g_rg_at_holster = false;    // RG went down inside a holster spot while a sub weapon was out

MO* equipment() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    return component(call_ptr(pm, "get_CurrentPlayer"), "app.ropeway.survivor.Equipment");
}

API::Field* find_field_deep(API::TypeDefinition* td, const char* name) {
    for (int i = 0; td != nullptr && i < 12; ++i) {
        if (auto* f = td->find_field(name)) return f;
        td = td->get_parent_type();
    }
    return nullptr;
}

// write a System.Nullable`1<enum> field of obj: HasValue + Value
bool write_nullable(MO* obj, const char* field, bool has, int value) {
    auto* f = find_field_deep(obj->get_type_definition(), field);
    if (f == nullptr) return false;
    auto* nt = f->get_type();
    auto* fh = nt ? nt->find_field("_HasValue") : nullptr;
    auto* fv = nt ? nt->find_field("_Value") : nullptr;
    if (fh == nullptr || fv == nullptr) return false;
    auto* base = (char*)f->get_data_raw(obj, false);
    if (base == nullptr) return false;
    *(bool*)(base + fh->get_offset_from_fieldptr()) = has;
    *(int32_t*)(base + fv->get_offset_from_fieldptr()) = value;
    return true;
}

void force_main_weapon(MO* eq) {
    auto* arm = call_ptr(eq, "get_MainWeapon");
    if (arm == nullptr) return;
    const int type = call_direct<int>(arm, "get_WeaponType", -1);
    const int parts = call_direct<int>(arm, "get_WeaponParts", 0);   // parts too, or attachments are stripped (AC)
    if (type < 0) return;
    const bool a = write_nullable(eq, "<ForceEquipType>k__BackingField", true, type);
    const bool b = write_nullable(eq, "<ForceEquipParts>k__BackingField", true, parts);
    if (!g_forcing) LOGI("%s RG first: holding the main weapon in hand (type %d, parts %d, writes %d/%d)", TAG, type, parts, a, b);
    g_forcing = true;
}

void clear_force(MO* eq) {
    write_nullable(eq, "<ForceEquipType>k__BackingField", false, 0);
    write_nullable(eq, "<ForceEquipParts>k__BackingField", false, 0);
    LOGI("%s force-equip cleared", TAG);
}

bool sub_weapon_in_hand() {
    const int wp = weapons::current_id();
    return wp == 4500 || wp == 4510 || wp == 6200 || wp == 6300;   // knives, hand + flash grenade
}

// The game's input questions, all instance methods (argv[1] = this, argv[2] = the asked Kind or bit mask):
//   isOn(Kind) / isDown(Kind) / isOn(UInt64, Boolean) / isDown(UInt64, Boolean), on InputSystem and InputUnit.
// b082 hooked only isOn(Kind): never asked for ATTACK during a throw [verified-live 2026-10-06]. Each is now hooked
// and the first answers per method are logged, so the next build can keep only the one the throw really asks.
constexpr uint64_t KIND_HOLD = 64;            // InputDefine.Kind.HOLD (RG)
std::atomic<uint64_t> g_block_mask{0};
thread_local uint64_t t_asked = 0;

int pre_ask(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    t_asked = argc > 2 ? (uint64_t)(uintptr_t)argv[2] : 0;
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
template <int N>
void post_ask(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    static std::atomic<int> logged{0};
    static const char* names[] = {"InputSystem.isOn(Kind)", "InputSystem.isDown(Kind)", "InputSystem.isOn(bits)", "InputSystem.isDown(bits)",
                                  "InputUnit.isOn(Kind)", "InputUnit.isDown(Kind)", "InputUnit.isOn(bits)", "InputUnit.isDown(bits)"};
    const uint64_t mask = g_block_mask.load();
    if (ret_val == nullptr || mask == 0 || (t_asked & mask) == 0) return;
    if (((uintptr_t)*ret_val & 0xFF) != 0 && logged.fetch_add(1) < 3)
        LOGI("%s blocked: the game asked %s for 0x%llx while a sub weapon is out, answered NO", TAG, names[N], (unsigned long long)t_asked);
    *ret_val = (void*)(uintptr_t)0;
}

void hook_ask(const char* type, const char* method, REFPostHookFn post) {
    auto* m = API::get()->tdb()->find_method(type, method);
    if (m == nullptr) { LOGW("%s %s.%s not found", TAG, type, method); return; }
    m->add_hook(pre_ask, post, false);
}
} // namespace

void install() {
    const char* S = "app.ropeway.InputSystem";
    const char* U = "app.ropeway.InputUnit";
    hook_ask(S, "isOn(app.ropeway.InputDefine.Kind)", post_ask<0>);
    hook_ask(S, "isDown(app.ropeway.InputDefine.Kind)", post_ask<1>);
    hook_ask(S, "isOn(System.UInt64, System.Boolean)", post_ask<2>);
    hook_ask(S, "isDown(System.UInt64, System.Boolean)", post_ask<3>);
    hook_ask(U, "isOn(app.ropeway.InputDefine.Kind)", post_ask<4>);
    hook_ask(U, "isDown(app.ropeway.InputDefine.Kind)", post_ask<5>);
    hook_ask(U, "isOn(System.UInt64, System.Boolean)", post_ask<6>);
    hook_ask(U, "isDown(System.UInt64, System.Boolean)", post_ask<7>);
    LOGI("%s input-question hooks in (RT, and RG at a holster, blocked while a sub weapon is out)", TAG);
}

void frame() {
    if (!bridge::live()) { g_block_mask = 0; return; }
    const bool rg = bridge::held(bridge::S_RGRIP), lg = bridge::held(bridge::S_LGRIP);
    // whichever went down first wins (Arcade Controls' "engagement order"), so LG-then-RG (a throw) is left alone
    if (bridge::pressed(bridge::S_RGRIP)) g_first = lg ? First::LG : holster::right_hand_in_zone() ? First::HOLSTER : First::RG;
    else if (bridge::pressed(bridge::S_LGRIP) && !rg) g_first = First::LG;
    if (!rg && !lg) g_first = First::NONE;
    if ((g_first == First::RG || g_first == First::HOLSTER) && !rg) g_first = lg ? First::LG : First::NONE;

    auto* eq = equipment();
    const bool suppress_sub = g_first == First::RG && rg;
    if (suppress_sub && eq != nullptr) { force_main_weapon(eq); g_clear_in = 0; }
    else if (g_forcing) { g_forcing = false; g_clear_in = CLEAR_DELAY_FRAMES; }
    if (g_clear_in > 0 && --g_clear_in == 0 && eq != nullptr) clear_force(eq);

    const bool sub_out = g_first == First::LG && lg && sub_weapon_in_hand();
    if (bridge::pressed(bridge::S_RGRIP)) g_rg_at_holster = sub_out && holster::right_hand_in_zone();
    if (!rg) g_rg_at_holster = false;
    // RT never drops it; RG pressed at a holster spot does not throw it (Tefa 2026-10-06)
    g_block_mask = sub_out ? (KIND_ATTACK | (g_rg_at_holster ? KIND_HOLD : 0)) : 0;
}

} // namespace vn::suppress
