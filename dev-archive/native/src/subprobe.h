// subprobe.h -- PROBE (b113, 2026-10-09, board row 6d). b109 held SUPPORT_HOLD and the equipped weapon no longer
// changed while LG was held (log 16:27:35-16:27:57, one HELD, no weapon lines), yet Tefa still saw the grenade
// "flipping on and off" when holding LG and looking at it at a certain distance. So something other than the equip
// flips. While LG is held this logs, on change only, the things that could: IsHold, IsReload, the equipped weapon,
// the game's SUPPORT_HOLD / HOLD button bits as the game sees them, whether we hold the setForce, and the real hands'
// distance apart and from the headset (FirstPerson's left-hand dock switches at 0.10 m from its grip spot).
// Lines: "subprobe" in re2_framework_log.txt, at most cfg::SUBPROBE_MAX_LINES per launch. Remove once 6d is solved.
#pragma once

namespace vn::subprobe {
void frame(bool support_forced);   // once per frame, after suppress::frame
} // namespace vn::subprobe
