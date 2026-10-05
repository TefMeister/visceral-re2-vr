// settings.h -- every number of Visceral's new native code, named, in one place (code-shape rule).
#pragma once

namespace vn::cfg {

// ---- holster zones (2026-10-05, Tefa's layout) ---------------------------------------------------
// Bound to the HEADSET, in VR tracking space (the real room), never to the character's body
// (Tefa: "they have to be bound to the hmd position so reaching for something will always be in one spot").
// Offsets in metres from the headset: SIDE = to the right (+) / left (-), UP = up (+) / down (-),
// FWD = in front (+) / behind (-). The set turns left/right with the headset but never tilts
// (Tefa 2026-10-05: "no tilt please, just left and right turn"). First guesses; tuned by wearing it.
// b077: shoulders moved 10 cm further out each side (Tefa: "quite close to the headset").
struct ZoneOffset { float side, up, fwd; };
constexpr ZoneOffset FLASHLIGHT   {-0.14f,  0.10f,  0.00f};   // upper left of the head, left hand + LG
constexpr ZoneOffset RIGHT_HIP    { 0.20f, -0.62f,  0.02f};   // shortcut RIGHT, right hand + RG
constexpr ZoneOffset LEFT_HIP     {-0.20f, -0.62f,  0.02f};   // shortcut DOWN (right hand + RG) / ammo pouch (left hand + LG)
constexpr ZoneOffset RIGHT_SHOULDER{ 0.28f, -0.22f, -0.08f};  // shortcut UP, right hand + RG
constexpr ZoneOffset LEFT_SHOULDER{-0.28f, -0.22f, -0.08f};   // shortcut LEFT, right hand + RG

constexpr float ZONE_ENTER_M    = 0.15f;   // hand closer than this = in the zone
constexpr float ZONE_LEAVE_M    = 0.22f;   // and must go further than this to leave (no flicker at the edge)

constexpr float BUZZ_ENTER_AMP  = 0.35f;   // a short tick when a hand enters a zone it can use
constexpr float BUZZ_ENTER_SEC  = 0.05f;
constexpr float BUZZ_GRAB_AMP   = 0.70f;   // a stronger one when the grip is pressed inside it
constexpr float BUZZ_GRAB_SEC   = 0.08f;

constexpr int   SLOT_CHECK_EVERY_FRAMES = 30;    // how often the shortcut cross is re-read for the log (twice a second)

} // namespace vn::cfg
