// tools.h -- test tools, compiled only with -DVN_TOOLS=ON (CMakeLists.txt); never in a player build. plugin.cpp calls
// late_point() under #ifdef VN_TOOLS.
#pragma once
#include "common.h"

namespace vn::tools {
void magreport();                        // magreport.cpp
void rackreport();                       // rackreport.cpp
void shellreport();                      // shellreport.cpp
inline void late_point() { magreport(); rackreport(); shellreport(); }
} // namespace vn::tools
