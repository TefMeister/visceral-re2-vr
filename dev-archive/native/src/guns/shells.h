// shells.h -- pump shotgun shells by hand (layer 3; Village b058's way, written clean 2026-11-07):
//   LG at the pouch (left hip): two shells in the left hand (the gun's own hidden shell part, carried at the wrist)
//   brought to the loading port: the hand docks there; LT puts them in one after the other
//   LG let go: the shells left in the hand go back to the pouch
#pragma once
#include "common.h"

namespace vn::guns::shells {

void on_gun_changed(int new_wp);
void late_point();       // LateUpdateBehavior post: let go? at the port? LT? then drawn
void render_point();     // PrepareRendering post: drawn again

bool ours();             // the gun in hand takes shells by hand
bool in_hand();          // shells are in the left hand
// the left hand as the shells want it: -1 = no say, 1 = docked at the port at (p, q) in the right wrist's space
int  hand_socket(Vec3& p, Quat& q);
// LG went down in the pouch: true if this gun took it. A gun with more than one kind of round (rounds.h) gets the kind
// the pouch gives now; LT with the rounds in the hand, away from the gun, steps to the next kind.
bool pouch_grab();

} // namespace vn::guns::shells
