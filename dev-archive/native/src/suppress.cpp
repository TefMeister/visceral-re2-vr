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
//
// b109, LG held first: the VR layer sends SUPPORT_HOLD = left grip AND NOT FirstPerson's "left hand docked on the
// weapon" (REFramework VR.cpp openvr_input_to_re2_re3), and the dock needs only IsHold plus the left hand within
// 10 cm of the playing clip's left-wrist spot (FirstPerson.cpp update_player_arm_ik) [inferred-static 2026-10-09].
// With the knife/grenade out the dock can switch on, SUPPORT_HOLD drops, the sub weapon goes away, IsHold drops, the
// dock lets go, SUPPORT_HOLD comes back: the in-and-out loop Tefa saw [hypothesis]. So while LG is held first, the
// game's own InputSystem.setForce(SUPPORT_HOLD, true) holds it, and it is let go the moment LG is.
#include "suppress.h"
#include "bridge.h"
#include "common.h"
#include "holster.h"
#include "menu_body.h"
#include "settings.h"
#include "weapons.h"

#include <atomic>

namespace vn::suppress {

namespace {
constexpr uint64_t KIND_ATTACK = 256;         // InputDefine.Kind.ATTACK (RT)
constexpr uint64_t KIND_SUPPORT_HOLD = 128;   // InputDefine.Kind.SUPPORT_HOLD (LG: ready the sub weapon)
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

// b109: the game's own "hold this button" switch for SUPPORT_HOLD; logs both ways with what was in hand
bool g_sh_forced = false;
int g_sh_count = 0;

bool is_hold_now() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    auto* cond = component(call_ptr(pm, "get_CurrentPlayer"), "app.ropeway.survivor.SurvivorCondition");
    return call_direct<bool>(cond, "get_IsHold", false);
}

void set_support_force(bool on, const char* why = "") {
    if (on == g_sh_forced) return;
    auto* is = API::get()->get_managed_singleton("app.ropeway.InputSystem");
    auto* m = is ? find_method_deep(is->get_type_definition(), "setForce") : nullptr;
    if (m == nullptr) { LOGW("%s LG first: InputSystem.setForce not found, SUPPORT_HOLD left to the VR layer", TAG); return; }
    m->call<void>(API::get()->get_vm_context(), (void*)is, KIND_SUPPORT_HOLD, on);
    g_sh_forced = on;
    if (on) ++g_sh_count;
    LOGI("%s LG first: SUPPORT_HOLD %s (#%d, wp %d, IsHold %d)%s", TAG, on ? "HELD by setForce" : "let go", g_sh_count,
         weapons::current_id(), is_hold_now() ? 1 : 0, why);
}

// b114: setForce did not cover the VR layer's own clear. b113's probe (2026-10-09 16:58, 10 cycles) read the game's
// SUPPORT_HOLD bit at 0 for exactly one frame every 1.5-2.5 s with setForce on and LG held, IsHold dropping in the same
// frame, then Down again the next frame [verified-live 2026-10-09, n=10]: that one-frame release puts the grenade away.
// The bits ARE readable at UpdateBehavior pre (b081's "empty" read was LT, not this). So while we hold SUPPORT_HOLD the
// bit is written back: On set, Down/Up cleared, after the VR layer's write and before the game's behaviour reads it.
constexpr uint32_t OFF_DOWN = 0x10, OFF_ON = 0x18, OFF_UP = 0x20;   // Button fields (RELOADED's offsets)
int g_fills = 0;

void fill_support_hold_bit() {
    auto* bb = call_ptr(API::get()->get_managed_singleton("app.ropeway.InputSystem"), "get_ButtonBits");
    if (!is_managed(bb)) return;
    auto& down = *(uint64_t*)((char*)bb + OFF_DOWN);
    auto& on = *(uint64_t*)((char*)bb + OFF_ON);
    auto& up = *(uint64_t*)((char*)bb + OFF_UP);
    if ((down | on) & KIND_SUPPORT_HOLD) { up &= ~KIND_SUPPORT_HOLD; return; }
    on |= KIND_SUPPORT_HOLD;
    up &= ~KIND_SUPPORT_HOLD;
    if (++g_fills <= cfg::SUBPROBE_MAX_LINES) LOGI("%s LG first: SUPPORT_HOLD bit was cleared by the VR layer, put back (#%d)", TAG, g_fills);
}

bool sub_weapon_in_hand() {
    const int wp = weapons::current_id();
    return wp == 4500 || wp == 4510 || wp == 6200 || wp == 6300;   // knives, hand + flash grenade
}

} // namespace

bool support_forced() { return g_sh_forced; }

void install() {
    // b083-b084 hooked the game's input questions (isOn/isDown) to stop RT: the argument read was wrong (an address,
    // not the button), so unrelated questions were answered NO and walking stopped while LG was held
    // [verified-live 2026-10-06]. Removed; code in archive/suppress-rt-block-b084.cpp. RT with a grenade is open.
}

void frame() {
    if (!bridge::live()) { set_support_force(false); return; }
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

    // b109: LG held first, in play (no menu, a player): the game keeps SUPPORT_HOLD on, whatever the dock does
    const bool menu = menu_body::is_menu_open();
    const bool keep = cfg::KEEP_SUPPORT_HOLD_ON_LG && lg && g_first == First::LG && eq != nullptr && !menu;
    // b113: say why it was let go (the first hold of 16:27:33 let go after 0.7 s with LG still held, per Tefa)
    const char* why = !lg ? " -- LG released" : g_first != First::LG ? (g_first == First::RG ? " -- RG went first" : " -- not LG first")
                    : eq == nullptr ? " -- no player" : menu ? " -- menu open" : "";
    set_support_force(keep, why);
    if (g_sh_forced) fill_support_hold_bit();
}

} // namespace vn::suppress
