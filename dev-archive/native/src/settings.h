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
constexpr float SNAP_WINDOW_S      = 0.5f;    // a jack's first moments: correct the whole measured error at once...
constexpr int   SNAP_SETTLE_FRAMES = 2;       // ...then wait this many frames for the view to show it before measuring again
constexpr float WATCHDOG_GROW_RAD  = 0.02f;   // the error growing by more than this per frame...
constexpr float WATCHDOG_S         = 0.6f;    // ...for this long = the turn direction is inverted: flip it once
constexpr float K_LEARN_AFTER_S    = 0.5f;    // learn the view constant only once a hold is this old
constexpr float K_LEARN_ERR_DEG    = 5.0f;    // and only while the view is this close to the body facing
constexpr float K_RELOG_RAD        = 0.09f;   // log a new constant only if it moved this much
constexpr int   MAX_UPDATE_HOOKS   = 6;       // camera-controller update methods given a post-hook
constexpr float STATUS_LOG_EVERY_S = 0.5f;    // status line while held (twice a second)
constexpr float MOTION_LOG_EVERY_S = 0.25f;   // fastest the player's animation name is logged
constexpr int   MOTION_LOG_MAX     = 400;     // and at most this many times per game run

// ---- LG keeps the sub weapon out (b109, 2026-10-09, board row 6d) ----------------------------------------------
// The VR layer sends the game's SUPPORT_HOLD only while FirstPerson is NOT docking the left hand on a weapon, and the
// dock can switch on with the knife/grenade itself in hand: it went in and out in a loop (Tefa 2026-10-09 00:15).
// true = while LG is held first, the game's own InputSystem.setForce(SUPPORT_HOLD) keeps it held; false = old way.
constexpr bool  KEEP_SUPPORT_HOLD_ON_LG = false;  // b116: OFF -- b115 worn: grenade still drops once, then menus dead (Tefa 2026-10-09 22:25)
constexpr bool  NO_FORBID_AIM_WITH_LG = false;   // b116: off with it (never fired in b115)   // b115: the game's gun-lowers-at-something rule off while LG holds the sub weapon
constexpr int   SUBPROBE_MAX_LINES = 400;   // b113 probe: change lines logged per launch while LG is held

// ---- item pick-up without the black screen (b110, 2026-10-09, board row 6b) -----------------------------------
// Only while the game's get-item inventory is up (a pick-up) and the headset is live. See pickup.h.
constexpr bool  PICKUP_HIDE_ON = true;                     // false = probe only, nothing skipped
constexpr const char* PICKUP_HIDE[] = {"GUIBlackMask"};    // GUI elements not drawn during a pick-up (first guess)
constexpr int   PICKUP_GIVE_UP_FRAMES = 300;               // the inventory never read open this long after the call: stop
constexpr int   PICKUP_NAME_CACHE = 512;                   // element names remembered per pick-up

// ---- game options set once (b112, 2026-10-09, board row 5) -------------------------------------------------------
constexpr unsigned OPTIONS_AIM_ASSIST_LEVEL = 0;   // OptionManager.CameraAimAssistLevel wanted [hypothesis: 0 = off]
constexpr int   OPTIONS_SETTLE_FRAMES = 120;       // frames in play (headset live) before the options are touched
constexpr int   OPTIONS_TRIES = 3;                 // set + read back at most this many times per launch

// ---- bullet spread by stance and hands (b125, 2026-10-10, Tefa's tiers; see spread.h) --------------------------
// Reticle fit 0 = widest, 100 = dead on (Matilda's range; the game clamps per gun).
constexpr bool  SPREAD_TIERS_ON = true;
// Share of each gun's best accuracy (1.0 = dead on). Tefa 2026-10-10 02:30 + 02:35 ("75% for walking with pistols").
// Walking with two hands on a long gun was not named: set to the long-gun walking value.
constexpr float SPREAD_RUNNING = 0.25f;                // running, one or two hands
constexpr float SPREAD_WALKING = 0.50f;                // walking with a long gun (one or two hands)
constexpr float SPREAD_WALKING_HANDGUN = 0.75f;        // walking with a handgun (one or two hands)
constexpr float SPREAD_STILL_LONG_ONE_HAND = 0.75f;    // standing still, long gun, one hand
constexpr float SPREAD_STILL_HANDGUN_ONE_HAND = 1.0f;  // standing still, handgun, one hand
constexpr float SPREAD_STILL_TWO_HANDS = 1.0f;         // standing still, any gun, two hands
constexpr float SPREAD_DOCK_HANDS_M = 0.75f;      // left grip held + controllers closer than this = two hands on the gun
constexpr int   SPREAD_LOG_SHOTS = 200;           // shots logged per launch

// ---- spread probe (b124, 2026-10-10) ---------------------------------------------------------------------------
constexpr int   SPREADPROBE_MAX_LINES = 600;   // log lines per launch
constexpr float SPREADPROBE_FIT_STEP = 0.05f;  // log the fit point when it moved more than this since the last line

// ---- running stop (2026-10-06, ported from Arcade Controls' re2_vr_run_toggle_fix.lua) -------------------------
constexpr float RUN_STICK_DEADZONE = 0.05f;   // left stick nearer the middle than this = let go: running stops
constexpr int   RUN_LOG_FIRST      = 10;      // log the first few jog-flag changes, to prove the write lands

// ---- no body in menus (2026-10-06, ported from Arcade Controls' re2_vr_menu_hide_player.lua) -------------------
constexpr int   MENU_WALK_DEPTH    = 12;      // how deep the player's transform tree is searched for meshes
constexpr int   MENU_WALK_CHILDREN = 500;     // and at most this many children under one transform
constexpr float MENU_CAM_BACK_M    = 0.10f;   // after a menu closes, the camera counts as back this close to the held spot
constexpr float MENU_CAM_BACK_DEG  = 2.0f;    // and within this many degrees of the held turn
constexpr int   MENU_CAM_RELEASE_FRAMES = 20; // and is held at most this many frames while it comes back
constexpr int   MENU_CAM_SETTLE_FRAMES  = 3;  // b102: released only once FirstPerson's camera has sat at the held view this many frames in a row

// ---- RT fires without RG (2026-10-08, detour step 2; the three switches found flat 2026-10-07) -----------------
constexpr int   FIRE_WINDOW_FRAMES = 20;      // the gun's "may fire" answer stays YES at most this long after the press
constexpr int   FIRE_LOG_FIRST     = 30;
// automatics fire while RT is held (WP numbers): MQ 11, LE 5, flamethrower, the miniguns. Everything else: one per pull
constexpr int   FIRE_AUTOMATIC_WP[] = {2000, 2200, 4200, 4700, 8700, 4520, 4900};      // log the first shots in full (press + bullets before/after + doorbells)

} // namespace vn::cfg
