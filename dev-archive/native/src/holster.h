// holster.h -- the holster zones, bound to the headset (Tefa 2026-10-05).
// Step H2: each spot holds one slot of the game's own 4-way shortcut cross (Tefa's mapping in holster.cpp).
// Right hand + RG there takes that weapon out, or puts it away if it is already in hand. Flashlight and ammo
// pouch only buzz for now.
#pragma once

namespace vn::holster {
void frame();
bool right_hand_in_zone();   // the right hand is inside a holster spot right now
} // namespace vn::holster
