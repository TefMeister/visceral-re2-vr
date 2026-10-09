// pickup.h -- item pick-up shows black in the headset instead of the game (Tefa 2026-10-08 23:30: "we also need to make
// the black screens go away and be replaced by the game when picking up items"). Board row 6b.
//
// A pick-up is the game's inventory opened in its "get item" mode (GUIMaster.openInventoryGetItemMode, called from the
// fsm action ItemGetMenu) [inferred-static 2026-10-09, type database 2026-09-25]. b110 does two things, only while that
// mode is up and the headset is live:
//   PROBE: logs every GUI element drawn, once per name ("pickup: drawn <name>"), so the black one is named in the log.
//   TRY:   skips the elements named in cfg::PICKUP_HIDE (first guess: GUIBlackMask, which the VR layer lets draw
//          untouched, screen-wide, for the cutscene fades) [hypothesis]. Skips are logged ("pickup: skipped <name>").
// Outside a pick-up nothing is touched, so cutscene fades keep their black.
#pragma once

namespace vn::pickup {
void install();                                    // the openInventoryGetItemMode hook (once, from the game thread)
void frame();                                      // once per frame: ends the pick-up when the inventory closes
bool gui_draw(void* gui_element, void* context);   // REFramework's pre GUI draw: false = do not draw this element
} // namespace vn::pickup
