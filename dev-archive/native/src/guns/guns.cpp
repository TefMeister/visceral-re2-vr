// guns.cpp -- see guns.h, and the answers of guns_api.h (what the body files ask the gun code).
#include "guns.h"
#include "guns_api.h"
#include "cylinder.h"
#include "dryfire.h"
#include "game_reload.h"
#include "gun_frame.h"
#include "left_hand.h"
#include "magazine.h"
#include "pump.h"
#include "rack.h"
#include "rounds.h"
#include "settings.h"
#include "shells.h"

namespace vn::guns {

namespace {
int g_wp = -2;   // the gun last seen (-2 = never)
} // namespace

void install() {
    game_reload::install();
    dryfire::install();
    pump::install();
    rounds::install();
}

void frame_begin() {
    refresh();
    if (gf().wp != g_wp) {
        const int old = g_wp < 0 ? -1 : g_wp;
        magazine::on_gun_changed(old, gf().wp);
        rounds::on_gun_changed(old, gf().wp);
        cylinder::on_gun_changed(gf().wp);
        shells::on_gun_changed(gf().wp);
        rack::on_gun_changed(old, gf().wp);
        g_wp = gf().wp;
    }
    game_reload::frame();   // before anything reads this frame's buttons
    dryfire::frame();
}

void frame() {
    cylinder::frame();
    magazine::frame();
    rack::frame();
    pump::frame();
    left_hand::frame();
}

void late_point() { magazine::late_point(); cylinder::late_point(); rounds::late_point(); shells::late_point(); rack::late_point(); }
void render_point() { magazine::render_point(); cylinder::render_point(); rounds::render_point(); shells::render_point(); rack::render_point(); }
void begin_render() { rack::render_point(); }
void update_motion() { pump::update_motion(); }

// ---- guns_api.h ---------------------------------------------------------------------------------------------------
bool managed_now() { return magazine::ours() || shells::ours(); }
bool fire_blocked() { return magazine::fire_blocked() || cylinder::blocks_fire(); }
bool rack_blocks_fire() { return rack::blocks_fire(); }
bool session_active() { return magazine::session_active() || shells::in_hand(); }
bool pouch_grab() { return shells::pouch_grab() || magazine::pouch_grab(); }
void* slide_joint() { return rack::slide_joint(); }
float rack_travel() { return rack::travel(); }
float rack_pull_m() { return rack::pull_m(); }
int rack_wp() { return gf().wp; }
bool rack_active() { return rack::active(); }

} // namespace vn::guns
