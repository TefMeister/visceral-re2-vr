// holster.h -- the six holster spots, bound to the headset (Tefa 2026-10-05; six spots 2026-11-12).
// Both hips, both shoulders, back left and back right, the same for both hands; LG and RG do different things in each
// (the table in holster.cpp). A hand is in the nearest spot within reach, never in two.
#pragma once

namespace vn::holster {
void frame();
bool right_hand_in_zone();        // the right hand is in a spot where RG does something
bool right_hand_in_back_zone();   // ... in the back right spot (the sub weapon, b134)
bool left_hand_at_torch_spot();   // the left hand is in the left shoulder spot (the torch's home)
} // namespace vn::holster
