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
std::atomic<bool> g_block_attack{false};
std::atomic<int> g_attack_answers{0};

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

// isOn(InputDefine.Kind) is an instance method: argv[1] = this, argv[2] = Kind
thread_local bool t_asked_attack = false;
int pre_is_on(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    t_asked_attack = argc > 2 && ((uint64_t)(uintptr_t)argv[2] == KIND_ATTACK);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_is_on(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (!t_asked_attack || ret_val == nullptr || !g_block_attack.load()) return;
    if (((uintptr_t)*ret_val & 0xFF) != 0 && g_attack_answers.fetch_add(1) < 5)
        LOGI("%s RT while the sub weapon is out: the game asked isOn(ATTACK), answered NO", TAG);
    *ret_val = (void*)(uintptr_t)0;
}
} // namespace

void install() {
    for (const char* type : {"app.ropeway.InputSystem", "app.ropeway.InputUnit"}) {
        auto* m = API::get()->tdb()->find_method(type, "isOn(app.ropeway.InputDefine.Kind)");
        if (m == nullptr) { LOGE("%s %s.isOn(Kind) not found", TAG, type); continue; }
        m->add_hook(pre_is_on, post_is_on, false);
        LOGI("%s hook in: %s.isOn(Kind) (RT blocked while a sub weapon is out)", TAG, type);
    }
}

void frame() {
    if (!bridge::live()) { g_block_attack = false; return; }
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

    g_block_attack = g_first == First::LG && lg && sub_weapon_in_hand();
}

} // namespace vn::suppress
