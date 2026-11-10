// rounds.h -- guns that take more than one kind of round (2026-11-10, Tefa: "left hip for normal ammo, left hand on right
// hip for alternate ... insert different ammo into the same grenade launcher drum and have the game recognize what ammo is
// in the barrel"). The game keeps ONE kind per gun (the inventory slot's BulletID) and a count; changing the kind keeps the
// count and the next shot is of that kind (RE3's MGL, fired, 2026-11-10). So the drum is OUR list of what went in, in order,
// and the game's kind is kept on the round that fires next.
#pragma once

namespace vn::guns::rounds {

void on_gun_changed(int old_wp, int new_wp);
void late_point();           // the list in step with the gun's count; the game's kind = the round coming up
bool ours();                 // the gun in hand takes more than one kind of round (gun_data.h: round_gun)
int  hip_item(bool right);   // the round the left hip (false) or the right hip (true) holds for this gun now; 0 = none
int  carried(int item);      // rounds of that kind carried
bool insert(int item);       // one round of that kind into the gun; true = it went in

} // namespace vn::guns::rounds
