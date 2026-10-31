// exposure.h -- the game's automatic brightness (eye adaptation) kept off, written every frame before rendering
// (2026-10-28). See exposure.cpp.
#pragma once

namespace vn::exposure {
void render_point();   // BeginRendering pre: after the game's tone-map controller, before the frame is drawn
void install();        // 2026-10-31: the post-hook on the game's ToneMapController.lateUpdate (first game frame)
} // namespace vn::exposure
