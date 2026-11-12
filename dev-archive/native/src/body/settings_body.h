// settings_body.h -- every number of this game's body files (src/body/), named; part of vn::cfg (core/settings.h).
#pragma once
#include "common.h"

namespace vn::cfg {

// ---- holster zones (2026-10-05, Tefa's layout) ---------------------------------------------------
// Bound to the HEADSET, in VR tracking space (the real room), never to the character's body
// (Tefa: "they have to be bound to the hmd position so reaching for something will always be in one spot").
// Offsets in metres from the headset: SIDE = to the right (+) / left (-), UP = up (+) / down (-),
// FWD = in front (+) / behind (-). The set turns left/right with the headset but never tilts
// (Tefa 2026-10-05: "no tilt please, just left and right turn"). First guesses; tuned by wearing it.
// b077: shoulders moved 10 cm further out each side (Tefa: "quite close to the headset").
struct ZoneOffset { float side, up, fwd; };
constexpr ZoneOffset RIGHT_HIP    { 0.20f, -0.62f,  0.02f};   // shortcut RIGHT, right hand + RG
constexpr ZoneOffset LEFT_HIP     {-0.20f, -0.62f,  0.02f};   // shortcut DOWN (right hand + RG) / ammo pouch (left hand + LG)
constexpr ZoneOffset RIGHT_SHOULDER{ 0.28f, -0.22f, -0.08f};  // shortcut UP, right hand + RG
constexpr ZoneOffset LEFT_SHOULDER{-0.28f, -0.22f, -0.08f};   // shortcut LEFT, right hand + RG
constexpr ZoneOffset BACK_RIGHT   { 0.12f, -0.62f, -0.30f};   // the sub weapon (knife/grenade), right hand + RG: lower back, as low as the pistol, a little right and out (Tefa 2026-10-10, b135; was behind the head in b134)
constexpr ZoneOffset BACK_LEFT    {-0.12f, -0.62f, -0.30f};   // 2026-11-12: the mirror of BACK_RIGHT; medicine, left hand + LG (a later step: buzzes for now)

// Six spots, the same places on every character and in every game (Tefa 2026-11-12: "6 holster spots on all my
// charachters. hips, shoulders, back, 1 set on left and 1 set on right ... just LG and RG in these spots do different
// stuff"), "as big as possible so they won't interfere with each other but also don't get in the way of gameplay": a
// hand counts as in the NEAREST spot within reach, so two overlapping spots never both answer.
constexpr float ZONE_ENTER_M    = 0.20f;   // hand closer than this = in the zone (was 0.15 until 2026-11-12)
constexpr float ZONE_LEAVE_M    = 0.26f;   // and must go further than this to leave (no flicker at the edge)

// ---- the flashlight in the hand (2026-11-12, Tefa; flashlight.cpp) ------------------------------------------------
// Lives in the left shoulder spot: tap LG there to take it out, tap LG there again to put it back (no holding LG). LT
// switches the light, in the hand or in the spot. In the spot it shines from the left upper chest where the head looks
// (2026-10-11's RE3-style light); in the hand the game's own torch is drawn in the left hand and the beam leaves its lens.
// The grip follows the wrist: whichever end of the fist points forward gets the lens (the thumb end = the ordinary grip,
// the little-finger end = the overhand grip held up high). In the hand: no two-handed long gun (core/hand_state.h).
constexpr bool  FLASHLIGHT_ON = true;
constexpr int   RAINHOLD_CTX_ALLOC_DEPTH = 0x78;   // via.clr thread context: allocation-nesting counter (re2.exe 0x141f5d750; scratch cleanup when it returns to 0), rainhold.h
constexpr ZoneOffset FLASHLIGHT_BEAM {-0.12f, -0.25f, 0.05f};   // the beam's start from the camera: left, down, forward (m)
// The torch in the left wrist's space (l_arm_wrist): position (m) and turn (yaw pitch roll, degrees, gun_frame's ypr_deg)
// for the ordinary grip. FIRST GUESSES, to be tuned on the first run (the log prints the wrist's axes once).
constexpr float TORCH_HOLD_POS[3] = {0.0f, -0.03f, 0.08f};
constexpr float TORCH_HOLD_YPR[3] = {0.0f, 0.0f, 0.0f};
constexpr float TORCH_LENS_AXIS[3] = {0.0f, 0.0f, 1.0f};   // the lens end, in the torch's own space (unit)
constexpr float TORCH_LENS_M      = 0.09f;                 // grip centre to lens
constexpr float TORCH_FLIP_DOT    = 0.25f;   // the lens end must point this far past sideways (cos) before the grip flips
constexpr float TORCH_FLIP_SEC    = 0.15f;   // the flip eases over this long (Village's pose changes ease 0.15 s)

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
constexpr int   SNAP_FIRST_WAIT_FRAMES = 3;   // 2026-10-29: no snap in a jack's first frames (the rendered-view reading is a frame or
                                              // two behind; a snap taken from a stale reading over-turned by 7 deg on 2026-10-28 23:54)
constexpr bool  JACK_PRE_TURN      = true;    // 2026-10-29: take the head's own turn out of the hold at the capture (see ladder.cpp)
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
constexpr bool  PICKUP_KEEP_WORLD_CAMERA = true;           // b144: skip the game's item camera in a pick-up (the world stays behind it)
constexpr bool  INVENTORY_KEEP_WORLD_CAMERA = false;       // b182: OFF again, Tefa 2026-10-17 23:40: the jump got about twice as far with it (probe: ~32 deg vs ~24). b181: with the headset live, EVERY inventory open keeps the world camera (the inventory's own camera switch is the closing jump; pickup.cpp)
constexpr int   PICKUP_COMING_FRAMES = 30;                 // b145: after ItemGetMenu.start, the next item camera this many frames on is a pick-up's

// ---- every menu over the live game world: no tint, no blur (b128, 2026-10-10, Tefa's screenshot) -------------
// The inventory's post effect (colour filter + blur) is never switched on: every mode takes the use-item path. See
// menu_tint.h. The camera hold (menu_body) and the VR layer's GuiBack skip do the rest.
constexpr bool  MENU_TINT_OFF = true;          // false = probe only: everything logged, nothing skipped
constexpr bool  MENU_TINT_FLAT_TOO = true;     // also without the headset (a flat screenshot proves the effect)
constexpr bool  MENU_TINT_LAYER_SKIP = false;  // additionally refuse InventoryLayer.activate itself (belt and braces)
constexpr bool  MENU_TINT_HIDE_FLAT = true;    // flat only: do not draw the elements below (the VR layer already drops GuiBack)
constexpr const char* MENU_TINT_HIDE[] = {"GuiBack"};   // the inventory's captured-screen backdrop (dark panel + grain, flat)
constexpr int   MENU_TINT_NAME_CACHE = 1024;   // element names remembered
constexpr bool  MENU_TINT_STRIP_BLUR_ON = true; // hide every via.gui.BlurFilter inside the elements below (headset and flat)
constexpr const char* MENU_TINT_STRIP_BLUR[] = {"GUI_Pause"};   // the pause menu's blur + darkening lives inside it (b129 probe)
// nodes inside those elements hidden whole ("element/node"): the pause menu's full-screen dark mask and its two dark
// background panels (b130 tree: main > mask_all (Texture), c_blur (Rect + Texture + blur), c_bg (two Rects))
constexpr const char* MENU_TINT_HIDE_NODES[] = {"GUI_Pause/mask_all", "GUI_Pause/c_blur", "GUI_Pause/c_bg"};

// ---- game options set once (b112, 2026-10-09, board row 5) -------------------------------------------------------
constexpr unsigned OPTIONS_AIM_ASSIST_LEVEL = 0;   // OptionManager.CameraAimAssistLevel wanted [hypothesis: 0 = off]
constexpr int   OPTIONS_SETTLE_FRAMES = 120;       // frames in play (headset live) before the options are touched
constexpr int   OPTIONS_TRIES = 3;                 // set + read back at most this many times per launch

// ---- bullet spread by stance and hands (b125, 2026-10-10, Tefa's tiers; see spread.h) --------------------------
// Reticle fit 0 = widest, 100 = dead on (Matilda's range; the game clamps per gun).
constexpr bool  SPREAD_TIERS_ON = true;
// Share of each gun's best accuracy (1.0 = dead on). Tefa 2026-10-10 02:30 + 02:35 ("75% for walking with pistols").
// Walking with two hands: long gun 75%, handgun 90% (Tefa 02:50).
constexpr float SPREAD_RUNNING = 0.25f;                // running, one or two hands
constexpr float SPREAD_WALKING = 0.50f;                // walking with a long gun, one hand
constexpr float SPREAD_WALKING_HANDGUN = 0.75f;        // walking with a handgun, one hand
constexpr float SPREAD_WALKING_LONG_TWO_HANDS = 0.75f;     // walking with a long gun, two hands (Tefa 02:50)
constexpr float SPREAD_WALKING_HANDGUN_TWO_HANDS = 0.90f;  // walking with a handgun, two hands (Tefa 02:50)
constexpr float SPREAD_STILL_LONG_ONE_HAND = 0.75f;    // standing still, long gun, one hand
constexpr float SPREAD_STILL_HANDGUN_ONE_HAND = 1.0f;  // standing still, handgun, one hand
constexpr float SPREAD_STILL_TWO_HANDS = 1.0f;         // standing still, any gun, two hands
constexpr float SPREAD_DOCK_HANDS_M = 0.75f;      // left grip held + controllers closer than this = two hands on the gun
constexpr int   SPREAD_LOG_SHOTS = 200;           // shots logged per launch

// ---- running stop (2026-10-06, ported from Arcade Controls' re2_vr_run_toggle_fix.lua) -------------------------
constexpr float RUN_STICK_DEADZONE = 0.05f;   // left stick nearer the middle than this = let go: running stops
constexpr int   RUN_LOG_FIRST      = 10;      // log the first few jog-flag changes, to prove the write lands

// ---- no body in menus (2026-10-06, ported from Arcade Controls' re2_vr_menu_hide_player.lua) -------------------
constexpr int   MENU_WALK_DEPTH    = 12;      // how deep the player's transform tree is searched for meshes
constexpr int   MENU_WALK_CHILDREN = 500;     // and at most this many children under one transform
constexpr float MENU_CAM_BACK_M    = 0.10f;   // after a menu closes, the camera counts as back this close to the held spot
constexpr float MENU_CAM_BACK_DEG  = 2.0f;    // and within this many degrees of the held turn
constexpr int   MENU_CAM_RELEASE_FRAMES = 20; // and is held at most this many frames while it comes back
constexpr int   MENU_CAM_SETTLE_FRAMES  = 3;
constexpr int   MENU_CAM_CLOSE_RENDER_FRAMES = 0; // b180: after a menu closes, the held view is also written at BeginRendering PRE for this many frames; b181: 0, it wrote 3 times per close and the jump stayed (FirstPerson rewrites the camera from the render thread after submit, VR.cpp ~1684)  // b102: released only once FirstPerson's camera has sat at the held view this many frames in a row
constexpr bool  JACK_CAM_HOLD = true;         // b174: a jack start (ladder, cupboard, switch) with FirstPerson aside: the camera is written stripped (menu_body.cpp "jack")
constexpr int   JACK_CAM_HOLD_FRAMES_MAX = 6; // ... for at most this many frames; FirstPerson aside for longer = left to the ladder hold, and logged

// ---- RT fires without RG (2026-10-08, detour step 2; the three switches found flat 2026-10-07) -----------------
constexpr int   FIRE_WINDOW_FRAMES = 20;      // the gun's "may fire" answer stays YES at most this long after the press
constexpr int   FIRE_LOG_FIRST     = 30;
constexpr bool  FIRE_EMPTY_SILENT  = true;    // b174: an empty gun (0 rounds, magazine out, rack needed) gets NO Fire trigger on RT (fire.cpp)
// automatics fire while RT is held (WP numbers): MQ 11, LE 5, flamethrower, the miniguns. Everything else: one per pull
constexpr int   FIRE_AUTOMATIC_WP[] = {2000, 2200, 4200, 4700, 8700, 4520, 4900};      // log the first shots in full (press + bullets before/after + doorbells)

// ---- the reload sounds (body/sfx.cpp: Andyalpa's RELOADED sound pack, played by miniaudio) ----------------------
constexpr int   SFX_VOICES = 16;
constexpr float SFX_DEBOUNCE_SEC = 0.10f;         // his ext_5 debounce per kind
constexpr float SFX_MASTER = 1.0f;

// ---- Ghost Hand BeG0nE (hands/ghosthand.cpp) ----------------------------------------------------------------------
constexpr bool  GHOST_HAND_ON = true;             // record the hands while the headset is live (Lanes PROTOCOL 20)

// ---- the sub weapon from the back holster on RG; RG never aims (b134, 2026-10-10; subweapon.h) ------------------
constexpr bool  SUB_ON_RG = true;            // false = the game's way (left grip readies the sub weapon, right grip aims)
constexpr bool  RG_NEVER_AIMS = true;        // the HOLD (aim) button is never sent from the right grip
constexpr float THROW_SWING_MPS = 1.6f;      // OURS: the right controller must move this fast (room metres/s) for the throw
constexpr float THROW_SWING_SMOOTH = 0.35f;  // speed smoothing per frame (1 = raw)
constexpr int   THROW_HOLD_FRAMES = 12;      // the grenade throw = HOLD sent this many frames on the swing (b137)

// ---- the gun code's per-game tuning (guns/shells.cpp) ------------------------------------------------------------
constexpr float SHELLS_STEP_S = 0.5f;             // one shell after another while LT is pressed at the port
constexpr float SHELL_PORT_DOCK_M = 0.10f;        // the shells this close to the port: the hand docks there

} // namespace vn::cfg
