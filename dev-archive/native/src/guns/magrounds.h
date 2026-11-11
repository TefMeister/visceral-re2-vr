// magrounds.h -- the rounds SEEN in a handgun's magazine follow what the magazine really holds (Tefa 2026-11-09 / 11-11:
// "bullet only shows in the mag when it needs to, and doesn't show when it can't be there"; Village's magdisp.cpp is the
// model). Some guns' models carry their magazine's top rounds as a mesh part of their own (gun_data.h MagRounds, read from
// the model files 2026-11-11): that part is switched on while the magazine holds a round and off when it holds none,
// seated, in the hand or on the floor. One round left in the chamber is NOT in the magazine: the part is off.
// A gun whose rounds are part of the magazine's own mesh (most of RE2's) has no entry and is left alone.
#pragma once

namespace vn::guns::magrounds {

void point();   // LateUpdateBehavior post and PrepareRendering post: the part written when it differs

} // namespace vn::guns::magrounds
