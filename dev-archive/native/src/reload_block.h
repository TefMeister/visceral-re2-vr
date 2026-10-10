// reload_block.h -- keeps the game's own reload off for the guns reload.cpp reloads by hand (RELOADED's four suppression
// layers, read in ext/reload.lua B.5 and rewritten; credit: Andyalpa), plus the dry-fire click. Only for those guns, and
// never while a menu is open (combining ammo with a gun in the inventory still reloads it the game's way).
//   layer 1  the RELOAD input bit is cleared before the game reads it
//   layer 2  Equipment.requestReload / executeReload, Gun.executeReload, Inventory.reloadMainSlot are skipped
//   layer 3  while the magazine is out: Equipment.requestFire is skipped (the trigger clicks instead) and the HUD's
//            loaded count reads 0
// Our own ammo writes pass through inside a Commit. Every block is counted and logged ("block: ...").
#pragma once

namespace vn::reload_block {
void install();   // the hooks (once, from the game thread)
void frame();     // UpdateBehavior pre: clears the input bits (RELOAD; SUPPORT_HOLD while a reload is under way)

void show_ammo_counter(float seconds);   // the HUD's ammo counter kept up this long (a reload or rack just happened)

struct Commit {   // our own reload calls go through the blocks while one of these is alive (game thread only)
    Commit();
    ~Commit();
};
} // namespace vn::reload_block
