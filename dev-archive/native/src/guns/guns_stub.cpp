// guns_stub.cpp -- guns_api.h before layer 3 (the clean rebuild, 2026-11-07): nothing of ours is going on, the game's own
// reload and rack stay in charge. Layer 3 replaces this file with the answers of the Village-based reload / rack.
#include "guns_api.h"
#include "weapons.h"

namespace vn::guns {

bool managed_now() { return false; }
bool fire_blocked() { return false; }
bool rack_blocks_fire() { return false; }
bool session_active() { return false; }
bool pouch_grab() { return false; }

void* slide_joint() { return nullptr; }
float rack_travel() { return 0.0f; }
float rack_pull_m() { return 0.0f; }
int rack_wp() { return weapons::current_id(); }
bool rack_active() { return false; }

} // namespace vn::guns
