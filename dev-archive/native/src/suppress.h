// suppress.h -- button suppressors (Tefa 2026-10-06; Arcade Controls had the first two).
//   RG held first (aiming): the sub weapon cannot come out -- LG's SUPPORT_HOLD is removed (LT: once mapped).
//   LG held first with a knife/grenade in hand: RT's ATTACK is removed, so it cannot drop the grenade; RG still throws.
//   (LG is also the two-handed grip on guns, so with a gun in hand RT is never touched.)
// Whichever button went down first wins, so a throw (LG then RG) never puts the grenade away.
// SAFETY: acts only while the real controller button is held AND the player is in control (no menu open); it only
// ever CLEARS a bit of this frame's input, never forces one (forcing broke the menus twice, b078 and b080).
#pragma once

namespace vn::suppress {
void after_hid();           // UpdateHID post
} // namespace vn::suppress
