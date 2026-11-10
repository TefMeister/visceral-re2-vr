// rounds.cpp -- see rounds.h. Each gun's drum list is kept while the session lasts (a weapon swap does not lose it); after a
// save is loaded every round in the gun is of the kind the game saved.
#include "game.h"
#include "rounds.h"
#include "ammo.h"
#include "gun_data.h"
#include "gun_frame.h"

#include <map>
#include <string>
#include <vector>

namespace vn::guns::rounds {

namespace {
const RoundGun* g_w = nullptr;
std::map<int, std::vector<int>> g_drums;   // per WP: the kinds in the gun, front = fires next

MO* slot() { return call_ptr(gf().inventory, "get_MainSlot"); }
int current() { return call_direct<int>(slot(), "get_BulletID", 0); }
void set_current(int item) { call_direct<void*>(slot(), "set_BulletID", nullptr, (int32_t)item); }

std::string drum_text(const std::vector<int>& d) {
    std::string s;
    for (int it : d) s += (s.empty() ? "" : " ") + std::to_string(it);
    return s.empty() ? "empty" : s;
}

bool known(int item) {
    for (int it : g_w->items) if (it != 0 && it == item) return true;
    return false;
}
} // namespace

void on_gun_changed(int, int new_wp) { g_w = round_gun(new_wp); }

bool ours() { return g_w != nullptr && gf().gun != nullptr && gf().inventory != nullptr; }

int carried(int item) {
    if (!ours() || item == 0) return 0;
    if (item == current()) return ammo::reserve();
    auto* im = API::get()->get_managed_singleton(game::type("gamemastering.InventoryManager").c_str());
    auto* m = API::get()->tdb()->find_method(game::type("gamemastering.InventoryManager").c_str(),
                                             ("getMainWeaponChangeBulletNumber(" + game::type("gamemastering.Item.ID") + ")").c_str());
    return (im != nullptr && m != nullptr) ? m->call<int>(API::get()->get_vm_context(), (void*)im, (int32_t)item) : 0;
}

// the left hip: the gun's normal round (the first of its list) if any is carried, else the first kind that is; the right
// hip: the first carried kind that is not the left hip's
int hip_item(bool right) {
    if (!ours()) return 0;
    int left = 0;
    for (int it : g_w->items) if (it != 0 && carried(it) > 0) { left = it; break; }
    if (!right) return left;
    for (int it : g_w->items) if (it != 0 && it != left && carried(it) > 0) return it;
    return 0;
}

bool insert(int item) {
    if (!ours() || !known(item)) return false;
    auto& drum = g_drums[gf().wp];
    set_current(item);                       // the game takes the round from this kind's stack
    const bool ok = ammo::insert_one();
    if (ok) drum.push_back(item);
    if (!drum.empty()) set_current(drum.front());
    LOGI("%s rounds: WP%04d %s item %d; the drum: %s", TAG, gf().wp, ok ? "took" : "DID NOT TAKE", item, drum_text(drum).c_str());
    return ok;
}

void late_point() {
    if (!ours()) return;
    const int n = ammo::loaded();
    if (n < 0) return;
    auto& drum = g_drums[gf().wp];
    const size_t before = drum.size();
    while ((int)drum.size() > n) drum.erase(drum.begin());        // fired: the front rounds are gone
    while ((int)drum.size() < n) drum.push_back(current());       // put in by the game itself (a save, its own reload)
    if (!drum.empty() && current() != drum.front()) {
        set_current(drum.front());
        LOGI("%s rounds: WP%04d next up is item %d; the drum: %s", TAG, gf().wp, drum.front(), drum_text(drum).c_str());
    } else if (drum.size() != before) {
        LOGI("%s rounds: WP%04d the drum: %s", TAG, gf().wp, drum_text(drum).c_str());
    }
}

} // namespace vn::guns::rounds
