// hand_state.h -- what each hand is busy with, written by the body files and read by the gun code (shared RE2 / RE3).
// 2026-11-12, Tefa: "long weapons cannot be two handed while flashlight is held - it has to go back to shoulder spot
// before putting another hand on the gun, but can reload weapons with the flashlight out".
#pragma once

namespace vn::hand_state {
inline bool torch_in_left_hand = false;   // body/flashlight.cpp: the torch is out of its spot, in the left hand
} // namespace vn::hand_state
