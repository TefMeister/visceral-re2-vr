// weapons.h -- which weapon the player holds, its name, and which holster it belongs in.
#pragma once
#include "common.h"

namespace vn::weapons {

enum class Holster { NONE, HANDGUN, LONG, SPECIAL, SUB };   // right hip, right shoulder, left shoulder, left hip

int current_id();                  // WP number (0 = WP0000 Matilda), -1 = bare hands / unknown
const char* name(int wp);          // display name, "unknown" if not in the table
Holster holster_for(int wp);       // NONE = not sorted yet (logged so the next build can sort it)
const char* holster_name(Holster h);

} // namespace vn::weapons
