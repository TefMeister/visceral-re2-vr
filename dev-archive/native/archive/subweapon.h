// subweapon.h -- the sub weapon (knife / grenade) comes out ONLY from the left hip (Tefa 2026-10-06).
// The bottom of the shortcut cross is the sub-weapon box: left hip + right hand RG readies the equipped sub
// weapon (the game's own SUPPORT_HOLD input, latched on), RG there again puts it back. While it is out, RT uses
// it (the game's own attack) and RG does nothing. The player's own LG no longer readies it.
#pragma once

namespace vn::subweapon {
void toggle();          // called by the left-hip holster
bool out();             // latched right now
void frame();           // once per frame, before the game reads input
} // namespace vn::subweapon
