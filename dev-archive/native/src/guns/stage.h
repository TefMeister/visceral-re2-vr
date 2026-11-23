// stage.h -- what the left hand is doing on the gun, as ONE number for Hand Poser (2026-11-23, Tefa: "no to a plan for re2/re3,
// just do your thing": build what Village has). Village's plugin has told Hand Poser the left hand's spot since its b084, so
// every reload and rack stage has its own finger pose; here the hand's fingers were the game's animation through all of it.
//
// The numbers are Village's (hand-poser-vr reframework/autorun/hand_poser.lua current_spot), so one script names them alike:
//   0 nothing in particular (the gun's own pose)   3 on the slide / bolt / fore-end, LT held or not
//   4 on the seated magazine (and riding it in)    5 a pump gun's fore-end held with LG alone (the two-handed hold)
//   6 on the drum (RE3's MGL)                      9 shells / rounds from the pouch in the hand
//   11 the slide / fore-end at the back (long guns and pumps; a pistol keeps 3, Village's rule)
//   13 a magazine in the hand (and sliding out into it)
//   20 shells docked at the loading port (new here: Village sends 8 there, which the script names "slide from the left")
// Also written: the gun (bridge S_WHO, the WP number; -1 = no gun: the knife and grenades are not guns here), the character
// beside it, and how far a magazine riding the hand is out (S_MAG_PULL: 0 = not riding, else 1 + 0..1).
#pragma once

namespace vn::guns::stage {
int left();          // the number above, this frame
void publish();      // write the three slots; called after the frame's gesture code and again after the late point
}
