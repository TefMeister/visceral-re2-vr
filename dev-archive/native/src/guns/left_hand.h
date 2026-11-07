// left_hand.h -- who has the left hand, told to our patched REFramework through the dock socket (core/exports.h) once a
// frame: the slide or the pump (rack.cpp) first; the reload (shells, then the magazine) has the last word while it has
// something in or for the hand. Also tells Hand Poser whether the left hand is on the gun (bridge S_LEFT_ON_GUN).
#pragma once

namespace vn::guns::left_hand {
void frame();   // UpdateBehavior pre, after the rack and the reload have had their frame
} // namespace vn::guns::left_hand
