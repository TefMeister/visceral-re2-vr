// rack.h -- the slide rack (pistols, SMGs, the magnum) and the pump (shotguns, Spark Shot): bundle 2 of the RELOADED
// port (2026-10-10). Spec: Andyalpa's ext_2 / ext_4 (credit: Andyalpa), rewritten; the shipped gesture is his:
// "hold LG to put your hand on the slider and press LT to pull it back, let go of LT to release the slider" and
// "RG + LG to 2-hand the shotgun, then LT to pull the pump handle down, release LT to push it up".
//
// Here: with the left grip held and the left hand near the slide / fore-end joint, LT pulls the joint back along its
// travel (his per-weapon numbers, reload_data.h), letting LT go returns it and CHAMBERS the round (executeEndReload,
// + executeEndEject for a pump). The joint is written at LateUpdateBehavior post, PrepareRendering post and
// BeginRendering pre (the engine rewrites joints in between; one write alternates = Tefa's "jittery").
//
// When a rack is NEEDED (the gun will not fire until then; reload_block asks `blocks_fire()`):
//   pistol: a magazine was put into an EMPTY gun (reload.cpp says so); the slide sits locked open (parked) until racked
//   pump:   a shot was fired with another shell still in the gun (pump_native.cpp says so), or shells were put into an
//           empty gun. The pump sits back (parked) until pumped.
// A cosmetic rack (nothing needed) is always allowed; it still chambers, which never hurts.
#pragma once

namespace vn::rack {
void frame();            // UpdateBehavior pre: the gesture, the timers
void late_point();       // LateUpdateBehavior post: write the joint
void render_point();     // PrepareRendering post / BeginRendering pre: write it again

void need(const char* why);        // a rack/pump is now required (the joint parks, fire is blocked)
void slide_lock_empty();           // the last round went: the slide locks open (pistols; cosmetic only)
bool blocks_fire();                // a rack/pump is required and has not happened
bool active();                     // the hand is on the slide / fore-end right now (the dock must leave the left hand alone)
void on_shot();                    // b159: a round was fired (the slide is left to the game again)
bool grip_pose();                  // b155: the fingers should grip the slide / fore-end (LG held near it, or racking)
int  wp_now();
float travel();           // 2026-10-13 Ghost Hand: the slide's travel 0..1
float pull_m();           // how far back the hand has pulled since the dock (metres along the gun)
void* slide_joint();      // this frame's slide / fore-end joint (or nullptr)
float hand_dist_m();      // b172: the game's left wrist to that joint, metres (the tuned gun); 99 = none
} // namespace vn::rack
