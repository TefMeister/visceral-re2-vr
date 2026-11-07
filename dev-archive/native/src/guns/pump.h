// pump.h -- a pump gun's OWN pump, taken over (Andyalpa's ext_4 C.3-C.6 and the delayed shell eject, rewritten; credit:
// Andyalpa):
//   a shot with another shell in the gun: a pump is due (rack.cpp parks the fore-end, the trigger is held)
//   the last shell fired: no pump until shells are put in; then one is due
//   the game's own pump animation is cut for a moment after each shot, and its pump sound triggers are refused
//   the spent shell's eject is held back and played when OUR pull-down happens
#pragma once
#include "common.h"

namespace vn::guns::pump {

void install();                 // hooks (once, first game frame)
void frame();                   // UpdateBehavior pre: the gun's own animation cut
void update_motion();           // UpdateMotion pre: the gun's and the player's animation cut
void before_shot();             // dryfire.cpp: a shot is about to happen (the rounds counted)
void after_shot();              // ... it happened: is a pump due?
void on_pulled_down();          // rack.cpp: the fore-end reached the back: the spent shell flies
void on_shell_in(bool was_empty);   // shells.cpp: a shell went in

} // namespace vn::guns::pump
