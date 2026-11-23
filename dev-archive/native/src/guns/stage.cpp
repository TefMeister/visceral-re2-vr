// stage.cpp -- see stage.h.
#include "stage.h"
#include "bridge.h"
#include "cylinder.h"
#include "gun_data.h"
#include "gun_frame.h"
#include "magazine.h"
#include "rack.h"
#include "shells.h"
#include "weapons.h"

namespace vn::guns::stage {

namespace {
int g_last = -1, g_last_wp = -2;

bool back_has_own_pose() {   // Village: a pistol's hand stays "slide" at the back; long guns and pumps change
    const auto kind = weapons::holster_for(gf().wp);
    return is_pump(gf().wp) || kind == weapons::Holster::LONG || kind == weapons::Holster::SPECIAL;
}
} // namespace

int left() {
    if (gf().gun == nullptr || gf().wp < 0 || !bridge::live()) return 0;
    Vec3 p{};
    Quat q{0, 0, 0, 1};
    if (shells::hand_socket(p, q) == 1) return 20;       // docked at the port
    if (shells::in_hand()) return 9;
    float pull = 0.0f;
    const int m = magazine::left_stage(pull);             // the reload has the last word over the rack (left_hand.cpp)
    if (m != 0) return m;
    if (rack::active()) return rack::at_back() && back_has_own_pose() ? 11 : 3;
    if (rack::gripping()) return 5;
    if (cylinder::hand_on_drum()) return 6;
    return 0;
}

void publish() {
    if (!bridge::live()) return;
    const int wp = gf().gun != nullptr && gf().wp >= 0 ? gf().wp : -1;
    float pull = 0.0f;
    const int st = left();
    if (wp >= 0) magazine::left_stage(pull);
    bridge::put(bridge::S_WHO, (float)wp);
    bridge::put((bridge::Slot)(bridge::S_WHO + 1), gf().claire ? 1.0f : 0.0f);
    bridge::put(bridge::S_LEFT_STAGE, (float)st);
    bridge::put(bridge::S_MAG_PULL, pull);
    if (st != g_last || wp != g_last_wp) {
        LOGI("%s stage: WP%04d left hand %d", TAG, wp, st);
        g_last = st; g_last_wp = wp;
    }
}

} // namespace vn::guns::stage
