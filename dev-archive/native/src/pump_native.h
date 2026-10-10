// pump_native.h -- the shotgun's OWN pump: what the game does after each shot, and what we take over (bundle 2,
// 2026-10-10; spec: Andyalpa's ext_4 C.3-C.6 and the delayed shell eject, rewritten; credit: Andyalpa).
//   after a shot with another shell still in the gun: the pump is NEEDED (rack.cpp parks the fore-end, fire is blocked)
//   after the last shell: no pump until shells are put in; then it is needed
//   the game's own pump animation (weapon + player motion layers whose clip name says pump/cycle/reload/eject/rack on a
//   shotgun) is cut short for 2.5 s after a shot: the layer is sent to its last frame once it has played 8-20 %
//   the spent shell's eject (ShellCartridgeController.generate) is held back and replayed when OUR pull-down happens
//   the game's own pump SOUNDS: the Wwise trigger IDs heard in the window are logged (no name lookup in this build);
//   blocking them is the next step once the IDs are known
#pragma once
#include "common.h"

namespace vn::pump_native {
void install();                       // hooks (once, from the game thread)
void frame();                         // UpdateBehavior pre
void update_motion();                 // UpdateMotion pre: cut the game's pump animation
void on_pre_fire(MO* gun);            // from reload_block's requestFire hook: remember the rounds
void on_post_fire(MO* gun);           // decide whether a pump is needed
void on_pulled_down();                // rack.cpp: the fore-end reached the back -- replay the held shell eject
void on_shells_inserted(bool was_empty);   // reload.cpp: a shell went in
} // namespace vn::pump_native
