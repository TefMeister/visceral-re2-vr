// walls.h -- the hands stop at walls, and a gun turns at the wrist away from one (2026-11-18, Tefa: "give re2 and re3 hands,
// walls and guns the same collision treatment", as Visceral RE8 b141; RE8's src/walls.cpp and lua/visceral_re8_walls.lua).
// Our REFramework works out each hand's place and turn from its controller (FirstPerson.cpp update_player_arm_ik) and
// hands them to visceral_hand_wall() below before anything else uses them (reframework-patch 2026-11-18). This file:
//   - writes the hand as the controller put it, and as handed back, into the bridge (lua/visceral_walls.lua reads them);
//   - keeps the hand on the player's side of the wall plane the Lua found in front of it (bridge S_WALL_*);
//   - puts the Lua's turn on the right hand (S_WALL_TILT_*): the gun turns at the wrist so its muzzle stays off the wall.
// The Lua casts the rays (the game's own via.physics.System castRay), finds the muzzle and leaves doors out (doorpush.cpp
// pushes them, now with the gun too).
#pragma once
#include "common.h"

namespace vn::walls {
constexpr bool  ON = true;
constexpr int   STALE_FRAMES = 3;     // a wall or a turn last written longer ago than this many frames is gone
constexpr float MAX_PUSH_M = 1.20f;   // a hand further than this past the plane is not at this wall
} // namespace vn::walls

extern "C" __declspec(dllexport) int visceral_hand_wall(int hand, float* pos, float* quat);
