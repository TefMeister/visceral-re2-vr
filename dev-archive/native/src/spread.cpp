// spread.cpp -- see spread.h.
#include "spread.h"
#include "bridge.h"
#include "common.h"
#include "settings.h"
#include "weapons.h"

#include <atomic>

namespace vn::spread {

namespace {
std::atomic<bool> g_live{false};
std::atomic<int> g_shots{0};

struct Player { MO* eq; MO* cond; };

Player player() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    auto* go = call_ptr(pm, "get_CurrentPlayer");
    return {component(go, "app.ropeway.survivor.Equipment"), component(go, "app.ropeway.survivor.SurvivorCondition")};
}

bool is_gun(int wp) { return wp >= 0 && wp != 4500 && wp != 4510 && wp != 6200 && wp != 6300; }

struct Choice { float share; const char* why; };   // share of the gun's best accuracy; < 0 = leave the game's value

Choice choose(const Player& p) {
    const int wp = weapons::current_id();
    if (!g_live || p.eq == nullptr || p.cond == nullptr || !is_gun(wp)) return {-1.0f, "not a gun"};
    const bool running = call_direct<bool>(p.cond, "get_IsJog", false);
    const bool walking = call_direct<bool>(p.cond, "get_IsWalk", false);
    const bool long_gun = weapons::holster_for(wp) == weapons::Holster::LONG || weapons::holster_for(wp) == weapons::Holster::SPECIAL;
    const bool two_hands = bridge::held(bridge::S_LGRIP) &&
                           dist(bridge::hand_pos(bridge::LEFT), bridge::hand_pos(bridge::RIGHT)) < cfg::SPREAD_DOCK_HANDS_M;
    if (running) return {cfg::SPREAD_RUNNING, "running"};
    if (walking) {
        if (long_gun) return two_hands ? Choice{cfg::SPREAD_WALKING_LONG_TWO_HANDS, "walking, long gun two hands"} : Choice{cfg::SPREAD_WALKING, "walking, long gun one hand"};
        return two_hands ? Choice{cfg::SPREAD_WALKING_HANDGUN_TWO_HANDS, "walking, handgun two hands"} : Choice{cfg::SPREAD_WALKING_HANDGUN, "walking, handgun one hand"};
    }
    if (two_hands) return {cfg::SPREAD_STILL_TWO_HANDS, "still, two hands"};
    return long_gun ? Choice{cfg::SPREAD_STILL_LONG_ONE_HAND, "still, long gun one hand"} : Choice{cfg::SPREAD_STILL_HANDGUN_ONE_HAND, "still, handgun one hand"};
}

// returns the fit after the write
float apply(const Player& p, const Choice& c) {
    auto* fit = field_ptr<float>(p.eq, "_ReticleFitPoint");
    if (fit == nullptr || c.share < 0.0f) return fit ? *fit : -1.0f;
    float best = 100.0f;
    if (auto* rp = call_ptr(p.eq, "get_ReticleParam")) {
        if (auto* mx = field_ptr<float>(rp, "MAX_POINT")) best = *mx;
        if (auto* range = field_ptr<float>(rp, "_PointRange"); range != nullptr && range[1] > 0.0f) best = range[1];
    }
    *fit = best * c.share;
    if (auto* isfit = field_ptr<bool>(p.eq, "_IsReticleFit")) *isfit = c.share >= 1.0f;
    return *fit;
}

int pre_request_fire(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    const Player p = player();
    const Choice c = choose(p);
    const float after = apply(p, c);
    const int n = g_shots.fetch_add(1) + 1;
    if (n <= cfg::SPREAD_LOG_SHOTS) LOGI("%s spread: shot #%d, %s, fit %.1f", TAG, n, c.why, after);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_request_fire(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}
} // namespace

void install() {
    if (!cfg::SPREAD_TIERS_ON) return;
    auto* m = API::get()->tdb()->find_method("app.ropeway.survivor.Equipment", "requestFire");
    if (m == nullptr) { LOGW("%s spread: Equipment.requestFire not found, tiers only per frame", TAG); return; }
    m->add_hook(pre_request_fire, post_request_fire, false);
    LOGI("%s spread: tiers on (running %.0f%%, walking long gun %.0f%%, walking handgun %.0f%%, still long gun one hand %.0f%%, still handgun one hand %.0f%%, still two hands %.0f%%)",
         TAG, cfg::SPREAD_RUNNING * 100, cfg::SPREAD_WALKING * 100, cfg::SPREAD_WALKING_HANDGUN * 100, cfg::SPREAD_STILL_LONG_ONE_HAND * 100,
         cfg::SPREAD_STILL_HANDGUN_ONE_HAND * 100, cfg::SPREAD_STILL_TWO_HANDS * 100);
}

void frame() {
    g_live = bridge::live();
    if (!cfg::SPREAD_TIERS_ON) return;
    const Player p = player();
    apply(p, choose(p));
}

} // namespace vn::spread
