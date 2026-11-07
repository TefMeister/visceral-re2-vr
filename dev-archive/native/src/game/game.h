// game.h -- THE PER-GAME LAYER (the clean rebuild, 2026-11-07). Everything the shared files (src/core, guns, hands, tools)
// need that differs between RE2 and RE3 is asked here. Each game's repo has its own src/game/game.cpp answering it, and its
// own src/game/guns_table.cpp holding the per-gun numbers that gun_data.h declares. This header itself is shared (RE2's repo
// owns it; tools/sync_shared.py copies it to RE3).
#pragma once
#include "common.h"

#include <string>

namespace vn::game {

const char* ns();                    // the game's type namespace: "app.ropeway." (RE2), "offline." (RE3)
std::string type(const char* rest);  // ns() + rest, e.g. type("survivor.Equipment")
const char* project();               // the repo's name, for the recorders' file names: "visceral-re2-vr"
bool second_character(MO* player);   // the gun holds' second set (Andyalpa's "claire" numbers): Claire in RE2

} // namespace vn::game
