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

MO* equipment() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    return component(call_ptr(pm, "get_CurrentPlayer"), "app.ropeway.survivor.Equipment");
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

} // namespace

void install() {
    // b083-b084 hooked the game's input questions (isOn/isDown) to stop RT: the argument read was wrong (an address,
    // not the button), so unrelated questions were answered NO and walking stopped while LG was held
    // [verified-live 2026-10-06]. Removed; code in archive/suppress-rt-block-b084.cpp. RT with a grenade is open.
}

void frame() {
    if (!bridge::live()) return;
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
}

} // namespace vn::suppress
