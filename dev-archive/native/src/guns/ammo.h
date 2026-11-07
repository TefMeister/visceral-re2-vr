// ammo.h -- the game's own rounds for the gun in hand: what is loaded, what fits, what is carried, and our ways of putting
// rounds in (the game's own calls, inside a game_reload::Commit). RE2's model: the gun's getBulletNumber is the loaded count,
// the inventory's main slot holds the capacity, the carried rounds come from two readings that can lag after a save load.
#pragma once

namespace vn::guns::ammo {

int loaded();      // rounds in the gun now (-1 = no gun)
int capacity();    // rounds the gun holds
int reserve();     // rounds carried that fit this gun
int space();       // rounds the gun can still take (0 when unknown)
// tops the gun up to capacity from the carried rounds (the game's reload result, done by our hand); returns the loaded
// count before
int top_up();
bool insert_one();        // one round in (a shotgun shell); true = it went in
void end_reload(bool eject);   // the gun ready to fire again (executeEndReload, and executeEndEject for a pump)

} // namespace vn::guns::ammo
