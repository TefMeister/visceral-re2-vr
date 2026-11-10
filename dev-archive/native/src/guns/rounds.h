// rounds.h -- guns that take more than one kind of round (2026-11-10, Tefa: "left hip for normal ammo, left hand on right
// hip for alternate ... insert different ammo into the same grenade launcher drum and have the game recognize what ammo is
// in the barrel"; then: "rounds fire follow the drums chambers if at all possible"). The game keeps ONE kind per gun (the
// inventory slot's BulletID) and a count; changing the kind keeps the count and the next shot is of that kind (RE3's MGL,
// fired, 2026-11-10). The game has no chambers (its drum joint never turns). So the drum is OURS: one kind per chamber, a
// round goes into the empty chamber nearest the hand, a shot fires the chamber at the barrel and the drum turns one on, and
// the game's kind is kept on the chamber that fires next. A gun with no chamber joints in its table is a plain list, first
// in first out.
#pragma once
#include "common.h"

namespace vn::guns::rounds {

void on_gun_changed(int old_wp, int new_wp);
void late_point();           // the drum in step with the gun's count; the game's kind = the round coming up; the drum drawn
void render_point();         // the drum drawn again
bool ours();                 // the gun in hand takes more than one kind of round (gun_data.h: round_gun)
void install();              // the hook that puts the game's kind back on the chamber coming up before a shot
// ONE pouch for every kind (Tefa 2026-11-10: "left holster ammo pouch must be used for all of them, LG+LT press chooses
// next ammo type"): the kind the pouch gives now (0 = none carried), and the step to the next kind carried (the game's
// ammo counter shows it for a moment)
int  selected();
int  cycle();
int  carried(int item);      // rounds of that kind carried
bool chambered();            // the gun in hand has chambers the hand loads one by one
// the empty chamber nearest a point in the gun's own space: its distance; false = no chamber is empty
bool nearest_empty(Vec3 in_gun, float& d);
// one round of that kind into the gun (a chambered gun: into the empty chamber nearest in_gun, or the next empty one when
// in_gun is null); true = it went in
bool insert(int item, const Vec3* in_gun);
int  dump_spent();           // the spent cases out of their chambers (cylinder.h: the open gun tipped up); how many
// n rounds drawn in the hand at (world, wrot): empty chambers lend their rounds; called every drawn frame while held
void show_hand(int n, int item, Vec3 world, Quat wrot);

} // namespace vn::guns::rounds
