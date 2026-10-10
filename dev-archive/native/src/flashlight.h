// flashlight.h -- the flashlight works like RE3's: a light only, never the torch in the hand (Tefa 2026-10-11).
// LT with the left hand at the left shoulder switches it. The light shines from the left upper chest, where the head
// looks; the torch model is hidden during play and left to the game in cutscenes.
#pragma once

namespace vn::flashlight {
void install();      // hooks FlashLight.applyTransform (needs the type database: first game frame)
void frame();        // keeps the game's flashlight state where the switch put it
void toggle();       // the switch (holster.cpp: left hand + LT at the left shoulder)
} // namespace vn::flashlight
