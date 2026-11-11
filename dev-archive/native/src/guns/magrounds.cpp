// magrounds.cpp -- see magrounds.h.
#include "magrounds.h"
#include "gun_data.h"
#include "gun_frame.h"
#include "joints.h"
#include "magazine.h"
#include "settings.h"

namespace vn::guns::magrounds {

namespace {
int g_wp = -1, g_said = -2;   // the gun and the look last logged (1 shown, 0 hidden)
float g_fight_said = -10.0f;

bool part_on(MO* mesh, API::Method* get, int part) {
    return get != nullptr && get->call<bool>(API::get()->get_vm_context(), (void*)mesh, (uint64_t)part);
}
} // namespace

void point() {
    if (!cfg::MAG_ROUNDS_ON || !magazine::ours()) return;
    const MagRounds* r = mag_rounds(gf().wp);
    if (r == nullptr) return;
    const int rounds = magazine::rounds_in_mag();
    if (rounds < 0) return;
    auto* mesh = call_ptr(gf().gun, "get_Mesh");
    if (mesh == nullptr) return;
    auto* td = mesh->get_type_definition();
    auto* get = find_method_deep(td, "getPartsEnable(System.UInt64)");
    auto* set = find_method_deep(td, "setPartsEnable(System.UInt64, System.Boolean)");
    if (get == nullptr || set == nullptr) return;
    if (gf().wp != g_wp) { g_wp = gf().wp; g_said = -2; }
    const int pairs[2][2] = {{r->mag_a, r->rounds_a}, {r->mag_b, r->rounds_b}};
    int shown = 0;
    bool wrote = false;
    for (const auto& p : pairs) {
        if (p[1] < 0) continue;
        // a gun with two magazines (standard / extended): only the rounds of the magazine the gun wears
        const bool want = rounds > 0 && (p[0] < 0 || part_on(mesh, get, p[0]));
        if (part_on(mesh, get, p[1]) != want) {
            set->call<void>(API::get()->get_vm_context(), (void*)mesh, (uint64_t)p[1], want);
            wrote = true;
        }
        shown += want ? 1 : 0;
    }
    if (shown != g_said) {
        LOGI("%s magrounds: WP%04d the magazine holds %d (%s): its rounds %s", TAG, gf().wp, rounds,
             magazine::mag_out() ? "out of the gun" : "seated", shown ? "SHOWN" : "HIDDEN");
        g_said = shown;
    } else if (wrote && joints::now_s() - g_fight_said > 1.0f) {   // the same look written again: the game switched the part back
        g_fight_said = joints::now_s();
        LOGI("%s magrounds: WP%04d the game switched the rounds part back: written %s again", TAG, gf().wp, shown ? "on" : "off");
    }
}

} // namespace vn::guns::magrounds
