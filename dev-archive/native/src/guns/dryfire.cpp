// dryfire.cpp -- see dryfire.h. RT fires without aiming (body/fire.cpp), so the press itself is where an empty gun clicks;
// requestFire is still hooked because the game can ask to fire on its own (automatics held).
#include "game.h"
#include "dryfire.h"
#include "bridge.h"
#include "guns_api.h"
#include "gun_frame.h"
#include "joints.h"
#include "magazine.h"
#include "menu_body.h"
#include "pump.h"
#include "rack.h"
#include "settings.h"
#include "sfx.h"

namespace vn::guns::dryfire {

using namespace joints;

namespace {
float g_last_click = -10.0f;
int g_rounds_before = -1;
MO* g_shot_gun = nullptr;

int rounds(MO* gun) { return call_direct<int>(gun, "getBulletNumber", -1); }
bool cannot_fire(MO* gun) { return (!magazine::mag_out() && rounds(gun) == 0) || magazine::fire_blocked() || rack::blocks_fire(); }

void click() {
    const float now = now_s();
    if (now - g_last_click < cfg::DRY_CLICK_GAP_SEC && !bridge::pressed(bridge::S_RTRIG)) return;
    g_last_click = now;
    sfx::play(cfg::DRY_CLICK_KIND, cfg::DRY_CLICK_FOLDER, cfg::DRY_CLICK_VOL);
}

bool ours() { return cfg::RELOAD_ON && guns::managed_now() && !menu_body::is_menu_open(); }

int pre_fire(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    g_rounds_before = -1;
    if (!ours() || argc < 2 || !is_managed(argv[1])) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    auto* gun = field_obj((MO*)argv[1], "<EquipWeapon>k__BackingField");
    if (cannot_fire(gun)) { click(); return REFRAMEWORK_HOOK_SKIP_ORIGINAL; }
    g_rounds_before = rounds(gun);
    g_shot_gun = gun;
    pump::before_shot();
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_fire(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (g_rounds_before <= 0 || g_shot_gun == nullptr) return;
    g_rounds_before = -1;
    pump::after_shot();
    rack::on_shot();
    if (rounds(g_shot_gun) == 0) rack::slide_lock_empty();   // the last round: a pistol's slide locks open
}
} // namespace

void install() {
    if (!cfg::RELOAD_ON) return;
    auto* m = API::get()->tdb()->find_method(game::type("survivor.Equipment").c_str(), "requestFire");
    if (m == nullptr) { LOGW("%s trigger: Equipment.requestFire not found: no empty click, no shot gate", TAG); return; }
    m->add_hook(pre_fire, post_fire, false);
}

void frame() {
    if (ours() && bridge::pressed(bridge::S_RTRIG) && cannot_fire(gf().gun)) click();
}

} // namespace vn::guns::dryfire
