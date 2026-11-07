// guns.h -- the gun code's one entry point (layer 3 of the clean rebuild). plugin.cpp calls these at the engine's points;
// the order inside each is the gun code's own business (guns.cpp).
#pragma once

namespace vn::guns {

void install();        // hooks (once, first game frame)
void frame_begin();    // UpdateBehavior pre, right after the bridge: the frame's objects, a gun swap, the game's reload kept off
void frame();          // UpdateBehavior pre, after the sub weapon: the magazine, shells, rack, pump, who has the left hand
void late_point();     // LateUpdateBehavior post: the joints after the gun's own animation
void render_point();   // PrepareRendering post: the joints again
void begin_render();   // BeginRendering pre: the slide once more (the engine moves it in between)
void update_motion();  // UpdateMotion pre: the game's own pump animation cut

} // namespace vn::guns
