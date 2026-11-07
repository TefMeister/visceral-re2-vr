// game_reload.h -- keeps the game's OWN reload off for the guns we reload by hand, and keeps the ammo counter honest
// (RELOADED's suppression layers, read in ext/reload.lua B.5 and rewritten; credit: Andyalpa). Only for our guns, only with
// the headset live, never while a menu is open (combining ammo with a gun in the inventory still reloads it the game's way):
//   the player's action orderer never starts a RELOAD; the RELOAD input bit is cleared before the game reads it
//   Equipment.requestReload / executeReload, Gun.executeReload, Inventory.reloadMainSlot are skipped
//   the HUD's loaded count reads 0 while the magazine is out (1 with a round chambered); the counter is shown for a few
//   seconds after one of our reloads or racks
// Our own ammo writes pass through inside a Commit.
#pragma once

namespace vn::guns::game_reload {

void install();                          // the hooks (once, first game frame)
void frame();                            // UpdateBehavior pre, before anything reads this frame's buttons
void show_ammo_counter(float seconds);   // the HUD's ammo counter kept up this long

struct Commit {   // our own reload calls go through the blocks while one of these is alive (game thread only)
    Commit();
    ~Commit();
};

} // namespace vn::guns::game_reload
