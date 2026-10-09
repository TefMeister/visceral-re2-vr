// suppress.h -- button suppressors (Tefa 2026-10-06; Arcade Controls had the first two).
//   RG held first (aiming): the sub weapon cannot come out -- LG's SUPPORT_HOLD is removed (LT: once mapped).
//   LG held first with a knife/grenade in hand: RT's ATTACK is removed, so it cannot drop the grenade; RG still throws.
//   (LG is also the two-handed grip on guns, so with a gun in hand RT is never touched.)
// Whichever button went down first wins, so a throw (LG then RG) never puts the grenade away.
// b082: Arcade Controls' method (force the main weapon back while RG is held) and an isOn(ATTACK) answer for RT;
// no input bits are touched (b081 showed the bits read empty; forcing input broke the menus in b078/b080).
#pragma once

namespace vn::suppress {
void install();             // the isOn(Kind) hooks (once, from the game thread)
void frame();               // once per frame (UpdateBehavior pre)
bool support_forced();      // b109: SUPPORT_HOLD held by setForce right now (for subprobe.cpp)
} // namespace vn::suppress
