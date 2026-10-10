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
bool mag_out();          // the magazine is not seated: the gun must not fire
bool session_active();   // a reload is under way (the left grip must not ready the sub weapon)
int  wp_now();           // the gun in hand (WP number), -1 if none
const char* sfx_folder_now();
float sfx_volume_now();

} // namespace vn::reload
