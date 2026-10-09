// options.h -- ship with the right game options already set (Tefa 2026-10-06): Run Type = Hold, aim assist OFF,
// auto reload OFF. Board row 5. Tefa: "as long as they can be set automatically, that's perfect"; set ONCE.
//
// The options live in the game's system save, so they cannot be shipped as a file. This sets them through the game's
// own OptionManager setters (what the options menu calls), the first time the player is in the game with the headset
// live, reads them back, saves the system data the PC way (saveSystemSaveData_PC), and writes a marker file
// (reframework/data/visceral_options_set.txt) so it never happens again and the player's later choices stand.
// Values [inferred-static 2026-10-09, type database 2026-09-25]: OptionManager.OnOff is ON = 0, OFF = 1;
// InputSystem.setOptionToggleRunType(OnOff): ON = toggle, so Hold = OFF; auto reload OFF = 1; aim assist =
// CameraAimAssistLevel 0 [hypothesis: 0 is "off"]. Every launch logs the three values as the game has them.
#pragma once

namespace vn::options {
void frame();   // once per frame (UpdateBehavior pre)
} // namespace vn::options
