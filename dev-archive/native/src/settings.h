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

// ---- ladder + cupboard view hold (2026-10-06, ported from Arcade Controls' v12.2, staging 028a678) ----------
// Values are Arcade Controls' own, tuned in the headset on 2026-08-22; change only after wearing it.
constexpr const char* CLIMB_NAME_PARTS[] = {"LADDER", "CLIMB", "HASHIGO"};   // motion names that mean "climbing"
constexpr float CLIMB_HOLD_S       = 0.35f;   // stay "climbing" this long after the last match (no flicker at the ends)
constexpr float HOLD_REAIM_S       = 0.30f;   // re-aim the held view for this long after a jack starts, then freeze
constexpr float SERVO_GAIN         = 0.25f;   // share of the measured view error corrected per frame
constexpr float SERVO_MAX_DEG_S    = 180.0f;  // fastest the held view may be turned
constexpr float SERVO_DEADBAND_DEG = 2.0f;    // closer than this = leave it alone
constexpr float WATCHDOG_GROW_RAD  = 0.02f;   // the error growing by more than this per frame...
constexpr float WATCHDOG_S         = 0.6f;    // ...for this long = the turn direction is inverted: flip it once
constexpr float K_LEARN_AFTER_S    = 0.5f;    // learn the view constant only once a hold is this old
constexpr float K_LEARN_ERR_DEG    = 5.0f;    // and only while the view is this close to the body facing
constexpr float K_RELOG_RAD        = 0.09f;   // log a new constant only if it moved this much
constexpr int   MAX_UPDATE_HOOKS   = 6;       // camera-controller update methods given a post-hook
constexpr float STATUS_LOG_EVERY_S = 0.5f;    // status line while held (twice a second)
constexpr float MOTION_LOG_EVERY_S = 0.25f;   // fastest the player's animation name is logged
constexpr int   MOTION_LOG_MAX     = 400;     // and at most this many times per game run

// ---- running stop (2026-10-06, ported from Arcade Controls' re2_vr_run_toggle_fix.lua) -------------------------
constexpr float RUN_STICK_DEADZONE = 0.05f;   // left stick nearer the middle than this = let go: running stops
constexpr int   RUN_LOG_FIRST      = 10;      // log the first few jog-flag changes, to prove the write lands

// ---- no body in menus (2026-10-06, ported from Arcade Controls' re2_vr_menu_hide_player.lua) -------------------
constexpr int   MENU_WALK_DEPTH    = 12;      // how deep the player's transform tree is searched for meshes
constexpr int   MENU_WALK_CHILDREN = 500;     // and at most this many children under one transform

} // namespace vn::cfg
