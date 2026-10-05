// settings.h -- every number of Visceral's new native code, named, in one place (code-shape rule).
#pragma once

namespace vn::cfg {

// ---- holster zones (2026-10-05, Tefa's layout) ---------------------------------------------------
// Bound to the HEADSET, in VR tracking space (the real room), never to the character's body
// (Tefa: "they have to be bound to the hmd position so reaching for something will always be in one spot").
// Offsets in metres from the headset: SIDE = to the right (+) / left (-), UP = up (+) / down (-),
// FWD = in front (+) / behind (-). Side and forward turn with the headset's yaw only (never pitch or roll),
// so looking down at a hip does not swing the hip zone. First guesses; tuned by wearing it.
struct ZoneOffset { float side, up, fwd; };
constexpr ZoneOffset FLASHLIGHT   {-0.14f,  0.10f,  0.00f};   // upper left of the head, left hand + LG
constexpr ZoneOffset RIGHT_HIP    { 0.20f, -0.62f,  0.02f};   // handguns, right hand + RG
constexpr ZoneOffset LEFT_HIP     {-0.20f, -0.62f,  0.02f};   // sub weapon (right hand + RG) / ammo pouch (left hand + LG)
constexpr ZoneOffset RIGHT_SHOULDER{ 0.18f, -0.22f, -0.08f};  // long guns, right hand + RG
constexpr ZoneOffset LEFT_SHOULDER{-0.18f, -0.22f, -0.08f};   // special weapons + the EMF visualizer, right hand + RG

constexpr float ZONE_ENTER_M    = 0.15f;   // hand closer than this = in the zone
constexpr float ZONE_LEAVE_M    = 0.22f;   // and must go further than this to leave (no flicker at the edge)

constexpr float BUZZ_ENTER_AMP  = 0.35f;   // a short tick when a hand enters a zone it can use
constexpr float BUZZ_ENTER_SEC  = 0.05f;
constexpr float BUZZ_GRAB_AMP   = 0.70f;   // a stronger one when the grip is pressed inside it
constexpr float BUZZ_GRAB_SEC   = 0.08f;

constexpr int   STATUS_LOG_EVERY_FRAMES = 600;   // the once-in-a-while status line (about 10 s at 60 fps)

} // namespace vn::cfg
