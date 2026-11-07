#pragma once
// breech.h -- the GM 79's break action, ported from Visceral RE8 (2026-11-01; Tefa: "grenade launcher and revolvers must not
// open with a swing, only LG + LT, but close with LG + LT and the motion swing").
//   Opens ONLY with the left hand on the front grip, LG held, LT pressed; closes the same way or with a flick of the gun up.
//   The barrel (joint _01) swings BREAK_OPEN_DEG about its local x; the gun does not fire while it is open; rounds go in only
//   while it is open (reload.cpp's single-shell loading, with the GM 79 in SHELL_WEAPONS).
namespace vn::breech {
void frame();            // UpdateBehavior: buttons, the flick, the open state
void pose_point();       // LateUpdateBehavior post and PrepareRendering post: the barrel's turn
bool here();             // the gun in hand has a break action (the GM 79)
bool is_open();          // here() and open
bool blocks_fire();      // the barrel is open
bool loading_allowed();  // not a break action, or open
bool port_local(float out[3]);
// 2026-11-01 (Tefa, worn: "the left hand doesn't travel with the motion and stay on the front end of it"): while the barrel is
// open or swinging and the left hand took it at the front (LG still held), the hand rides on the barrel: its place and turn
// in the right wrist's space, for the VR layer's dock socket
bool hand_socket(float p[3], float q[4]);   // the chamber (joint _100) in the weapon transform's frame, for the round's insert point
}
