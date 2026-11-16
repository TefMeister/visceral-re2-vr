// sidestep.h -- a quick side step, RE2 and RE3 (2026-11-16, Tefa: "yes and if this is possible to do, then this type of
// dodge would be preferred in re3 as well").
//
// The left stick's direction + the right stick CLICK (the shared Visceral layout): the player slides about a metre that way
// in about a third of a second, slow-fast-slow, and cannot be hurt for the first quarter second; stick centred = a step
// back. Built from what RE3's own dodge was seen to do (modding-notes/2026-11-16-how-re3s-dodge-works-and-a-side-step-for-re2.md,
// RE2's repo): there it is one animation whose root motion carries Jill ~2 m in ~1.3 s, with a ~0.23 s perfect-dodge window.
// Leon and Claire have no such animation, so here the player's Transform is nudged a little every UpdateBehavior frame;
// the game's own collision stops her at walls (measured on the simulator: pushed into a wall she stops and slides along it).
// Not being hit: HitPointController.Invincible, on for the window only, and left alone when the game had it on already.
// Only while REFramework's FirstPerson drives the view (not in menus, cutscenes, ladders or grabs), not with the left hand
// on the gun, and not again until the cool-down has passed.
// In RE3 it takes the right stick click over from the game's own dodge (our REFramework patch asks visceral_own_sidestep()
// in core/exports.cpp), and the game's own roll is switched off altogether (Tefa 2026-11-16: "please remove the original
// dodge roll if possible"; it was on right A when nothing was there to use).
#pragma once

namespace vn::sidestep {

constexpr bool  ON = true;
constexpr float DISTANCE_M = 1.0f;        // how far one step goes (in the open)
constexpr float DURATION_S = 0.30f;       // and over how long; eased in and out (smoothstep), like RE3's slow-fast-slow
constexpr float INVINCIBLE_S = 0.25f;     // the first part of it in which nothing hurts (RE3's perfect-dodge window ~0.23 s)
constexpr float COOLDOWN_S = 0.80f;       // from the start of one step to the earliest next one
constexpr float STICK_DEADZONE = 0.30f;   // under this the stick counts as centred: the step goes back
constexpr float BUZZ_AMP = 0.35f, BUZZ_S = 0.06f;
constexpr float MAX_FRAME_S = 0.1f;       // a frame longer than this (a hitch) moves no further than this much time

void frame();       // UpdateBehavior: start a step on the click, move the player along it, the invincible window
bool owns_click();  // the right stick click is the side step's (RE3: not the game's dodge)

} // namespace vn::sidestep
