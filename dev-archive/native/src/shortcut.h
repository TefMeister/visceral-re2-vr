// shortcut.h -- the game's own 4-way weapon shortcut (the d-pad cross), used by the holsters (step H2).
// Tefa 2026-10-05: each holster spot holds whatever sits in one shortcut slot; the knife and grenades may be put
// into the cross too (the game's own "enableShortcut" rule is told yes for them).
#pragma once

namespace vn::shortcut {

// EquipmentDefine.Shortcut values (from the game's enum)
enum Dir : int { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3 };

void install();                // hooks EquipmentDefine.enableShortcut (call once, from the game thread)
int weapon_in(Dir d);          // WP number in that slot, -1 = empty / unknown
bool take_out(Dir d);          // the game's own Inventory.equipMainSlot(Shortcut)
bool put_away(int wp);         // Inventory.unequipEquipedWeapon(type), Equipment.requestHolster as a fallback
const char* dir_name(Dir d);
void log_slots();              // one line per slot, on demand

} // namespace vn::shortcut
