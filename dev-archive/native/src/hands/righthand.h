// righthand.h -- the trigger finger, RE2 and RE3 (2026-11-23, Tefa: "right hand index finger on the trigger ... index finger
// moving with RT press on the trigger", every gun in re8, re2 and re3). Village's b152 way: while RT is down with a gun in hand
// the right index finger's three joints are bent a little further on top of whatever pose stands, eased by time (the loader
// gives pressed / not pressed only), written LAST in the frame (BeginRendering pre) so nothing overwrites it.
// The numbers live here (one table): which way a bend goes on these skeletons was measured in the game the same day.
#pragma once

namespace vn::righthand {

constexpr bool  TRIGGER_PULL_ON = true;
constexpr float TRIGGER_PULL_DEG[3] = {4.0f, 20.0f, 10.0f};   // knuckle, middle joint, tip joint (Village's numbers)
constexpr float TRIGGER_PULL_SIGN = 1.0f;                      // +1 = a bend is a positive turn about the joint's own axis below
constexpr int   TRIGGER_PULL_AXIS = 2;                         // 0 x, 1 y, 2 z: the joint's own axis a finger curls about
constexpr float TRIGGER_PULL_S = 0.07f;                        // the ease, seconds
constexpr const wchar_t* INDEX_JOINTS[3] = {L"r_hand_index_0", L"r_hand_index_1", L"r_hand_index_2"};

void begin_render();   // BeginRendering pre, after every other writer (guns::begin_render calls it)

}  // namespace vn::righthand
