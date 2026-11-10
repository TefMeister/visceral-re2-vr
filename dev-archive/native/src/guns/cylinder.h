// cylinder.h -- a drum that swings out to be loaded (RE3's MGL; design D5, Tefa's rule of 2026-11-01): it opens ONLY with the
// left grip on the drum + the left trigger, closes the same way, the gun cannot fire while it is open, rounds only go in
// while it is open, and the spent cases stay in their chambers until the open gun is tipped muzzle-up (the revolver rule).
// The joint that swings is the gun table's `crane` (gun_data.h: RoundGun); the chambers themselves are rounds.h's.
#pragma once

namespace vn::guns::cylinder {

void on_gun_changed(int new_wp);
void frame();            // the open / close press, the swing, the tip-out
void late_point();       // the crane joint written (before rounds.h draws the chambers)
void render_point();
bool ours();             // the gun in hand has a drum that swings out
bool is_open();          // swung out far enough to load
bool blocks_fire();      // not fully closed

} // namespace vn::guns::cylinder
