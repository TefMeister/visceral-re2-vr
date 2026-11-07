// settings_guns.h -- every number of the gun code (layer 3 of the clean rebuild, 2026-11-07), part of vn::cfg (included by
// core/settings.h). The per-gun numbers (which joint is the magazine, the slide's travel...) are in gun_data.h.
// Times and distances marked "his" are Andyalpa's (RELOADED's re2_vr_reload.json); the rest were set by wearing it.
#pragma once
#include "common.h"

namespace vn::cfg {

// ---- the reload by hand ------------------------------------------------------------------------------------------
constexpr bool  RELOAD_ON = true;                 // false = the game's own reload everywhere, nothing of ours runs
constexpr float RELOAD_SLIDE_SEC = 0.19f;         // his: the magazine slides out of the magwell
constexpr float RELOAD_FALL_SEC = 0.5f;           // his: then falls
constexpr float RELOAD_FALL_M = 1.2f;             // his: this far, then it is hidden
constexpr float RELOAD_INSERT_SEC = 0.19f;        // his: a magazine slides in
constexpr float RELOAD_DOCK_MIN_M = 0.06f;        // a magazine goes in at least this close to the magwell (his are 2-10 cm)
constexpr float RELOAD_DOCK_COOLDOWN_SEC = 0.5f;
constexpr float RELOAD_GRAB_COOLDOWN_SEC = 0.6f;  // between two pouch grabs
constexpr float RELOAD_GRAB_BUZZ_AMP = 0.99f, RELOAD_GRAB_BUZZ_SEC = 0.057f;   // his grab rumble
constexpr float RELOAD_INSERT_BUZZ_AMP = 0.7f, RELOAD_INSERT_BUZZ_SEC = 0.06f;
constexpr float RELOAD_DENY_BUZZ_AMP = 1.0f, RELOAD_DENY_BUZZ_SEC = 0.25f;     // the pouch has nothing for this gun
constexpr float HUD_AFTER_RELOAD_SEC = 5.0f;      // the ammo counter stays up this long after a magazine / shell / rack (Tefa)

// ---- Village's real-gun rules (2026-10-31) -----------------------------------------------------------------------
// A slide gun with rounds keeps one in the chamber when its magazine comes out: it fires once with the magazine out. The
// rounds stay in the game's count; the trigger gate and the HUD decide what can be seen and fired, so the same magazine
// going back in holds exactly its own rounds, and a pouch magazine is topped up from the ammo carried.
constexpr bool  CHAMBER_ROUND = true;
constexpr float MAG_GRAB_M = 0.15f;               // an LG press this close to the seated magazine puts the hand ON it
constexpr int   MAG_LONG_WP[] = {2200};           // the magazine sits ahead of the grip: LG on it + LT pulls it out (no B)
constexpr float INSERT_LT_MIN_SEC = 0.25f;        // the gun's own magazine back in needs a fresh LT; the pull's own LT never counts
constexpr float MAG_DOCK_DOWN_M = 0.05f;          // the hand on the magazine holds its bottom, this far toward where it comes out
constexpr bool  MAG_KEEP_OUT_ON_SWAP = true;      // a magazine taken out stays out when the gun is put away and comes back

// ---- shotgun shells ----------------------------------------------------------------------------------------------
constexpr int   SHELLS_PER_GRAB = 2;              // two shells per pouch grab
constexpr float SHELLS_STEP_S = 0.5f;             // one shell after another while LT is pressed at the port
constexpr float SHELL_PORT_DOCK_M = 0.10f;        // the shells this close to the port: the hand docks there
struct ShellHold { float ox, oy, oz, yaw, pitch, roll; };
constexpr ShellHold SHELL_HOLD = {0.108f, -0.042f, 0.032f, 13.0f, 86.0f, 0.0f};   // a shell in the left hand (Tefa set, b157)

// ---- the slide and the pump, by hand -----------------------------------------------------------------------------
constexpr float RACK_HAND_M = 0.20f;              // the left hand this close to the slide when it locked open / a rack is due
constexpr float RACK_ANYTIME_HAND_M = 0.08f;      // ... and this close for a cosmetic rack any time (a two-handed grip never gets pulled on)
constexpr float RACK_PULL_M = 0.05f;              // his: the left controller this far back along the gun = pulled
constexpr float RACK_RETURN_M = 0.03f;            // forward this far from the furthest-back point = home and chambered
constexpr float PUMP_GRAB_M = 0.20f;              // the left controller this close to the fore-end grabs the pump
constexpr float PUMP_ZONE_AHEAD_MIN_M = 0.10f, PUMP_ZONE_AHEAD_MAX_M = 0.70f;   // or anywhere this far ahead of the gun hand
constexpr float PUMP_ZONE_OFF_AXIS_M = 0.15f;     // ... and within this of the gun's line
constexpr float RACK_FOLLOW = 0.5f;               // the slide follows the hand with this much smoothing per frame (1 = raw)
constexpr float RACK_RETURN_PER_SEC = 5.0f;       // let go mid-way: it springs back at this rate (0..1 per second)
constexpr float RACK_AUTO_RETURN_SEC = 0.10f;     // the pump handle let go at the bottom goes back up in this long
constexpr float SLIDE_PARK_SCALE = 1.5f;          // the locked-open slide sits this much further back than his parked (Tefa)
constexpr float SLIDE_DOCK_YPR[3] = {173.0f, 18.0f, 180.0f};   // the hand on the slide, turned (Tefa's numbers, b164)
constexpr Vec3  SLIDE_DOCK_OFF = {0.078f, 0.026f, 0.012f};     // ... and placed, in the slide joint's axes (Tefa's numbers, b164)
constexpr float RACK_BUZZ_AMP = 0.7f, RACK_BUZZ_SEC = 0.06f;
constexpr bool  NO_FIRE_WHILE_LG_LT = true;       // no gun fires while LG and LT are both held (Tefa, b165)

// ---- the pump gun's own animation and spent shell (pump.cpp) -----------------------------------------------------
constexpr float PUMP_WINDOW_SEC = 2.5f;           // his: the game's pump animation after a shot is cut for this long
constexpr uint32_t PUMP_SOUND_BLOCK[] = {1474380250u, 3160867172u};   // the game's own pump sound triggers (b166)
constexpr float PUMP_EJECT_ALLOW_SEC = 0.3f;      // the spent shell's generate() passes this long after our request()

// ---- the trigger on an empty gun (dryfire.cpp) -------------------------------------------------------------------
// The game makes no sound of its own when an empty shot is refused, so ours plays: Village's recorded hammer.
constexpr const char* DRY_CLICK_KIND = "dry_hammer";
constexpr const char* DRY_CLICK_FOLDER = "common";
constexpr float DRY_CLICK_VOL = 1.0f;
constexpr float DRY_CLICK_GAP_SEC = 0.3f;         // one click per pull on automatics

// ---- the left hand on the gun, told to Hand Poser (left_hand.cpp) ------------------------------------------------
constexpr float LEFT_ON_GUN_NEAR_M = 0.45f;       // LG held this close to the gun hand counts as on the gun [hypothesis]

} // namespace vn::cfg
