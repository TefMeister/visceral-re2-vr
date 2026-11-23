// cylinder.h -- a drum that swings out to be loaded (RE3's MGL; design D5, Tefa's rule of 2026-11-01): it opens ONLY with the
// left grip on the drum + the left trigger, closes the same way, the gun cannot fire while it is open, rounds only go in
// while it is open, and the spent cases stay in their chambers until the open gun is tipped muzzle-up (the revolver rule).
// 2026-11-23 (Tefa): moved BY THE HAND: LG + LT on the drum take it, it follows the hand sideways and clicks open at the end;
// pushed back with LG + LT it clicks shut. The hand rides along (every game, every weapon).
// The joint that swings is the gun table's `crane` (gun_data.h: RoundGun); the chambers themselves are rounds.h's.
#pragma once

namespace vn::guns::cylinder {

void on_gun_changed(int new_wp);
void frame();            // the drum taken by LG + LT and moved by the hand, the clicks, the tip-out
void late_point();       // the crane joint written (before rounds.h draws the chambers)
void render_point();
bool ours();             // the gun in hand has a drum that swings out
bool hand_on_drum();     // LG held with the left wrist at the drum (stage.cpp)
bool is_open();          // swung out far enough to load
bool blocks_fire();      // not fully closed

} // namespace vn::guns::cylinder
