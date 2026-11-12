// flashlight.h -- the torch you hold (Tefa 2026-11-12; the RE3-style light of 2026-10-11 stays for the torch in its spot).
// The torch lives in the left shoulder spot: tap LG there to take it out, tap LG there again to put it back. LT switches
// the light, in the hand or in the spot. In the hand the game's own torch is drawn in the left hand, the beam leaves its
// lens, and the grip follows the wrist (ordinary, or overhand held up high). In the spot the beam shines from the left
// upper chest where the head looks and the torch is hidden. Cutscenes are left to the game.
#pragma once

namespace vn::flashlight {
void install();        // hooks FlashLight.applyTransform (needs the type database: first game frame)
void frame();          // LT anywhere while the torch is in the hand (UpdateBehavior)
void render_point();   // the torch and the beam, last word before drawing (PrepareRendering post)
void tap_spot();       // holster.cpp: LG tapped in the left shoulder spot (out / back)
bool in_hand();        // the torch is out of its spot
} // namespace vn::flashlight
