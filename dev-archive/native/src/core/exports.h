// exports.h -- the dock socket our patched REFramework reads (exports.cpp). Same layout as the patch's own declaration.
#pragma once

namespace vn::exports {
struct VisceralDockSocket { int version; int active; float px, py, pz; float qx, qy, qz, qw; int steer; int owned; };
VisceralDockSocket& dock_socket();
} // namespace vn::exports
