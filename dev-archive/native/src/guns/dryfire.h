// dryfire.h -- the trigger on one of our guns (layer 3): a shot is refused while the gun is empty, the magazine is out with
// nothing chambered, or a rack is due, and the empty click plays instead (the game makes no sound of its own then); a shot
// that happens tells the pump and the slide.
#pragma once

namespace vn::guns::dryfire {
void install();   // the requestFire hook (once, first game frame)
void frame();     // UpdateBehavior pre: RT pressed on a gun that cannot fire = the click
} // namespace vn::guns::dryfire
