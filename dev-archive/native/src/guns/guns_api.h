// guns_api.h -- what the body files ask the gun code (the clean rebuild, 2026-11-07).
//
// The body files (fire, suppress, holster) and Ghost Hand used to call reload.cpp and rack.cpp directly. In the rebuild the
// gun code is Village's (layer 3 on), so they ask through these few questions instead. Until layer 3, guns_stub.cpp answers
// "nothing of ours is going on": the game's own reload and rack stay in charge.
#pragma once

namespace vn::guns {

bool managed_now();       // the gun in hand is one of ours (its game reload is off)
bool fire_blocked();      // the magazine is out and no round is chambered: the gun must not fire
bool rack_blocks_fire();  // a rack / pump is required and has not happened
bool session_active();    // a reload is under way (the left grip must not ready the sub weapon)
bool pouch_grab();        // holster.cpp: the left grip went down in the pouch spot and took a magazine / round

// Ghost Hand's readings of the rack (zeros until layer 3)
void* slide_joint();      // this frame's slide / fore-end joint, or nullptr
float rack_travel();      // 0..1
float rack_pull_m();      // metres pulled along the gun since the dock
int   rack_wp();          // the gun in hand (WP number), -1 if none
bool  rack_active();      // the left hand is on the slide / fore-end

} // namespace vn::guns
