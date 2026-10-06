// ladder.h -- the ladder + cupboard view hold, ported to C++ from Arcade Controls (Tefa 2026-10-06).
//
// Two parts, both from Arcade Controls' re2_vr_ladder_body_yaw_fix.lua v12.2 (staging 028a678, worn and liked
// 2026-08-22: "view locks to the ladder/cupboard every time, clean 180 at the top, seamless exits"):
//   1. VIEW HOLD during every "jack" (the game taking the body over: ladders, cupboard pushes, switches). The
//      game's player camera controller is pinned so the view faces what the body faces, follows the body's own
//      turns (the 180 at the top of a ladder), and is handed back seamlessly at the end.
//   2. BODY GUARD while climbing: FirstPerson turns the body toward the headset every frame; during a climb the
//      game's own body rotation is put back after it.
// Not solved in Arcade Controls either (why Tefa calls it unfinished): a short snap-then-turn as a jack STARTS.
// That one is FirstPerson's own smoothing inside REFramework (its snap check never fires), out of reach here.
#pragma once

namespace vn::ladder {
void late_update();          // LateUpdateBehavior POST, once per frame
void restore(bool final);    // LockScene PRE (false) and PrepareRendering POST (true): put the body rotation back
} // namespace vn::ladder
