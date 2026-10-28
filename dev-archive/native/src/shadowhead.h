// shadowhead.h -- a shadow-only copy of the game's own face mesh, placed on the hidden head joint, so the head keeps
// its shadow while REFramework's Hide Joint Mesh keeps the neck (2026-10-28). See shadowhead.cpp.
#pragma once

namespace vn::shadowhead {
void frame();   // once per frame (UpdateBehavior pre)
} // namespace vn::shadowhead
