// reload.h -- manual magazine reload (bundle 1 of the RELOADED port, 2026-10-10; Tefa: "implement the manual reloads").
// The spec is Andyalpa's RE2VRMODRELOADED 1.0.1 (ext_1 magazine state machine, read, not copied; credit: Andyalpa);
// the numbers per weapon are his data (reload_data.h).
//
// The player's side, for every magazine gun in reload_data.h:
//   right B          the magazine slides out of the gun and falls (the gun cannot fire while it is out; the trigger clicks)
//   left grip at the ammo pouch (the left-hip spot of holster.cpp)   a new magazine appears in the left hand
//   bring it to the magwell   it slides in; the gun is reloaded the game's way: the rounds that were in the dropped
//                    magazine are kept, and the rest is topped up from the ammo you carry (no ammo is ever lost)
//   let go of the left grip with a magazine in hand   it drops; take another from the pouch
// The game's own reload is switched off for these guns only (reload_block.cpp); every other gun keeps it.
//
// States: SEATED -> SLIDE_OUT (0.19 s) -> FALL (0.5 s, 1.2 m) -> OUT -> IN_HAND -> INSERT (0.19 s) -> SEATED,
// and IN_HAND -> HAND_FALL -> OUT when the grip is let go. Each change is logged ("reload: ...").
#pragma once
#include "common.h"

namespace vn::reload {

void frame();            // UpdateBehavior pre: the B press, the timers, the ammo
void late_point();       // LateUpdateBehavior post: pose the magazine joint (after the weapon's own animation)
void render_point();     // PrepareRendering post: pose it again (the engine rewrites joints in between)
bool pouch_grab();       // holster.cpp: the left grip went down in the pouch spot; true if a magazine was taken

bool managed_now();      // the gun in hand is one of ours (its game reload is off)
bool mag_out();          // the magazine is not seated
bool fire_blocked();     // the magazine is out and no round is left in the chamber: the gun must not fire (2026-10-31)
int  hud_rounds();       // what the ammo counter shows while the magazine is out (0 or the chambered 1); -1 = seated
bool session_active();   // a reload is under way (the left grip must not ready the sub weapon)
bool mag_in_hand();      // a magazine is in the left hand (taken from the pouch, or going in)
bool shell_in_hand();    // a shotgun shell is in the left hand
bool hand_docked();      // 2026-10-31: the drawn left hand sits ON the magazine (seated, sliding out into the hand, or going in)
// 2026-10-31, for rack.cpp's dock socket: 1 = dock the left wrist at (p, q), right-wrist space (steer: the left hand may
// turn the gun); 0 = the left hand must stay free (a magazine is in it: no grip dock); -1 = not the reload's business
int  hand_socket(Vec3& p, Quat& q, bool& steer);
int  wp_now();           // the gun in hand (WP number), -1 if none
const char* sfx_folder_now();
float sfx_volume_now();

} // namespace vn::reload
