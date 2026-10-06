// run.h -- running stops at once (Tefa 2026-10-06), ported to C++ from Arcade Controls' re2_vr_run_toggle_fix.lua.
//
// Tefa: "Running and then letting go of LS to stop running immediately, same with pressing the run button again to
// stop running and start walking." Left stick click arms running; it disarms the instant the stick returns to the
// middle, or on the next click. Arcade Controls' method, worn and shipped since 2026-08-09 [verified-live 2026-08-09]:
// the game asks PlayerActionOrderer.set_JogMode(bool) every frame (true while it wants a jog); a pre-hook on that
// setter replaces the value with ours, so every call lands on it however often and from wherever it is made.
// (Setting the jog flag once, or blocking the JOG order, did nothing there; writing the animation flag IsJog froze
// movement. Only the setter works.)
#pragma once

namespace vn::run {
void install();   // the set_JogMode hook (once, from the game thread)
void frame();     // once per frame, UpdateBehavior pre: read the click and the stick
} // namespace vn::run
