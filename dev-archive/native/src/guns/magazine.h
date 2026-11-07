// magazine.h -- the magazine by hand (layer 3; Village's rules on RE2's proven mechanics, written clean 2026-11-07).
//   handguns, MQ 11:  right B drops the magazine; with the left hand ON it (an LG press at the seated magazine) B releases
//                     it INTO the left hand
//   long guns (LE 5): LG on the magazine + LT pulls it out into the left hand; B does nothing
//   the same magazine goes back in with LT at the magwell and keeps only its own rounds; a fresh one from the pouch (LG at
//   the left hip) goes in by itself when brought to the magwell and is topped up from the rounds carried
//   a slide gun with rounds keeps one in the chamber while the magazine is out: one shot, then a rack after the next magazine
//   the left hand rides on the magazine going out and coming in; a magazine taken out stays out across a gun swap
#pragma once
#include "common.h"

namespace vn::guns::magazine {

void on_gun_changed(int old_wp, int new_wp);
void frame();            // UpdateBehavior pre: the timers, B / LG / LT while seated, the chambered round
void late_point();       // LateUpdateBehavior post: the magazine in the hand (dropped? at the magwell?), then drawn
void render_point();     // PrepareRendering post: drawn again (the engine rewrites joints in between)

bool ours();             // the gun in hand is a magazine gun we reload by hand
bool mag_out();          // the magazine is not seated
bool fire_blocked();     // the magazine is out and no round is chambered
int  hud_rounds();       // what the counter shows while it is out (0, or 1 chambered); -1 = seated
bool session_active();   // a magazine change is under way
bool hand_docked();      // the drawn left hand sits ON the magazine
// the left hand, as the reload wants it: -1 = no say, 0 = free (a magazine in it: nothing may dock it),
// 1 = docked at (p, q) in the right wrist's space, 2 = no say, but the gun's grip handle is no dock spot
int  hand_socket(Vec3& p, Quat& q);
bool pouch_grab();       // LG went down in the pouch: true if this gun took it (a magazine is now in the hand)
const char* sfx_folder();
float sfx_volume();

} // namespace vn::guns::magazine
