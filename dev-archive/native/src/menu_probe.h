// menu_probe.h -- PROBE (2026-10-06 night): the inventory still shows one frame from inside Leon's head on opening and
// one frame from the menu's camera spot on closing (Tefa: "the exact same thing"), although the camera is held at
// LockScene and PrepareRendering. This logs, at every frame step we can hook, whether the menu reads open, whether the
// body is hidden, which camera is primary and where it sits, for the few frames around each open and close (first six
// changes only), so the frame where it goes wrong and the step that moves it can be read off the log.
// Lines: "menuprobe" in re2_framework_log.txt.
#pragma once

namespace vn::menu_probe {
void point(const char* where);
} // namespace vn::menu_probe
