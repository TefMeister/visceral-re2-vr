// doorpush.h -- doors pushed open by the hand and the body, RE2 and RE3 (2026-11-16, Tefa: "touching a door opens it just
// a little, like pressing A does ... and after that the door follows hand collision, like it does follow leons body";
// "can the gap be set to 1 degree, so it really does just register as opened and then the rest is hand and body collision
// that pushes it open?"; A keeps opening doors too).
//
// What the game does (RE3, read live 2026-11-16, visceral-re3-vr modding-notes/2026-11-16-doors-by-hand-sh2-vs-re.md): a
// door is a gimmick whose leaf turns on its hinge joint "_00"; A swings it to ~61 degrees and the game keeps turning it
// with the player's body, then closes it itself once she is past; the next room loads as it opens; the game's own
// collision lets the player through whatever angle we DRAW. So: the game opens the door (A, or the hand touching it, which
// presses A for it through our REFramework patch, visceral_action_pulse() in core/exports.cpp); we draw the leaf: 1 degree
// at first, then as far as the hand or the body has pushed it, never less than the body needs to pass, and back down with
// the game when it closes the door.
#pragma once

namespace vn::doorpush {

constexpr bool  ON = true;
constexpr bool  TOUCH_OPENS = true;        // the hand pressing on a shut door presses A for it
constexpr float INITIAL_DEG = 1.0f;        // how far the door is drawn open when the game opens it (Tefa: "1 degree")
constexpr float MAX_DEG = 92.0f;           // never drawn further than this
constexpr float LEAF_M = 1.0f;             // the leaf's width from the hinge (a hand or body further out does not touch it)
constexpr float LEAF_SIDE = 1.0f;          // the leaf lies along the door's own +X from the hinge (-1 = -X)
constexpr float HAND_R = 0.10f;
constexpr bool  GUN_PUSHES = true;         // 2026-11-18 (Tefa, Village first): the gun's muzzle and barrel push doors too
constexpr float GUN_R = 0.05f;             // their reach            // the hand's reach around the wrist, metres
constexpr float BODY_R = 0.32f;            // the body's, around the player's position
constexpr float SCAN_S = 0.5f;             // how often the nearest door is looked for
constexpr float SCAN_M = 3.0f;             // and how close it must be, across the floor
constexpr float SCAN_UP_M = 1.5f;          // and up or down (a door on another floor is not ours)
constexpr int   TOUCH_PULSE_FRAMES = 6;    // frames A is held for the game when the hand touches a shut door
constexpr float TOUCH_COOLDOWN_S = 1.5f;
constexpr float OPEN_SEEN_DEG = 0.5f;      // the game's own angle above this = the game has the door open
constexpr const wchar_t* HINGE = L"_00";

void frame();         // UpdateBehavior: the nearest door, the touch that opens it
void after_motion();  // UpdateMotion (post): read the game's angle, push with the hands and the body, draw ours
void render_point();  // BeginRendering (pre): draw ours again, after anything that moved it since
bool action_pulse();  // A held for the game this frame (the touch), read by our REFramework through core/exports.cpp

} // namespace vn::doorpush
