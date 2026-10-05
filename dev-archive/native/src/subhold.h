// subhold.h -- the sub weapon (knife / grenade) is held out by the left hip, not by holding LG (Tefa 2026-10-06).
// Left hip + right hand RG latches the game's own SUPPORT_HOLD input (what LG does while held); RG there again lets
// go. While it is out: RG does nothing, RT throws (the game's throw is its HOLD input with the sub weapon readied, so
// RT's ATTACK is turned into HOLD). The player's own LG no longer readies it.
// SAFETY (b078 broke menus doing this blindly): nothing here touches input unless the player is in control --
// a player exists and no menu / inventory / map / pause is open. The latch is released whenever that is not so.
#pragma once

namespace vn::subhold {
void toggle();              // the left-hip grab
bool out();
void after_hid();           // UpdateHID post: edit this frame's buttons, after the game has read the pad
} // namespace vn::subhold
