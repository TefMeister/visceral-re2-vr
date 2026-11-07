// rack.h -- the slide and the pump by hand (layer 3, written clean 2026-11-07 from the rules Tefa wore in b165-b186):
//   pistol slide:  any time, a fresh LG press right at the slide puts the hand on it (cosmetic unless a rack is due); when
//                  the slide is locked open or a rack is due, LG near it is enough. LT with the hand on = the pull is live:
//                  the left controller moving back along the gun pulls it; once back it stays back until LG is let go,
//                  then it flies forward and chambers
//   pump:          LG near the fore-end grips it (the pump gun's grip is ours alone); LG + LT arms one cycle; LT held for the
//                  pull down; let go at the bottom and it goes back up by itself; one cycle per LT press
//   a rack due (a magazine into an empty gun, a pump after a shot) blocks the trigger until it is done; each gun keeps a
//   pending rack and a locked-open slide across a swap; no gun fires while LG and LT are both held
#pragma once
#include "common.h"

namespace vn::guns::rack {

void on_gun_changed(int old_wp, int new_wp);
void frame();            // UpdateBehavior pre: the gesture, the timers
void late_point();       // LateUpdateBehavior post: the slide / fore-end joint written
void render_point();     // PrepareRendering post: written again

void need(const char* why);   // a rack / pump is now due (the slide parks open, the trigger is held)
void slide_lock_empty();      // the last round went: a pistol's slide locks open
void on_shot();               // a round was fired (the slide is the game's again)
bool blocks_fire();
bool active();                // the left hand is on the slide / fore-end
bool owns_grip();             // a pump gun: the left hand's only spot on it is the fore-end, ours
// the left hand on the slide / fore-end: its place in the right wrist's space; steer = it may turn the gun (the pump)
bool socket(Vec3& p, Quat& q, bool& steer);

// Ghost Hand's readings
float travel();
float pull_m();
void* slide_joint();

} // namespace vn::guns::rack
