// subweapon.h -- the knife or grenade from a BACK holster on the right grip; the right grip no longer aims
// (Tefa 2026-10-10: "RG must not be used for 'aim' at all anymore as we have a system now for the accuracies ...
// holding RG at the back holster to bring out the sub weapon, needs to be held to have it equipped, releasing it
// changes it back to the last gun held or no gun ... the combo RG + RT pressed while doing the throwing motion with
// the controller, must throw it in the game").
//
// The game already has the whole mechanism: its SUPPORT_HOLD button readies the sub weapon for as long as it is held
// and puts it away on release, back to what was held before; ATTACK while it is out throws (the knife swings). The VR
// layer sends SUPPORT_HOLD from the LEFT grip and HOLD (= aim) from the RIGHT grip. Here, every frame at
// UpdateBehavior pre, after the VR layer's write:
//   HOLD is cleared (the right grip never aims; firing already works without it, fire.cpp)
//   SUPPORT_HOLD is cleared, unless the right grip was pressed in the back spot and is still held: then it is held ON
//   ATTACK with the sub weapon out is cleared until the right hand swings faster than cfg::THROW_SWING_MPS while RT
//   is held (the throwing motion), then let through for one press
// Every change is logged ("sub: ...").
#pragma once

namespace vn::subweapon {
void frame();      // UpdateBehavior pre, after bridge::frame_begin and before anything reads the buttons
bool active();     // the right grip is holding the sub weapon out (suppress.cpp must not force SUPPORT_HOLD off)
} // namespace vn::subweapon
