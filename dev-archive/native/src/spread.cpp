// spread.cpp -- see spread.h.
#include "spread.h"
#include "bridge.h"
#include "common.h"
#include "settings.h"
#include "weapons.h"

#include <atomic>

namespace vn::spread {

namespace {
enum class Tier { GAME, ONE_HAND_STILL, TWO_HANDS_STILL };
std::atomic<bool> g_live{false};
std::atomic<int> g_shots{0};

struct Player { MO* eq; MO* cond; };

Player player() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    auto* go = call_ptr(pm, "get_CurrentPlayer");
    return {component(go, "app.ropeway.survivor.Equipment"), component(go, "app.ropeway.survivor.SurvivorCondition")};
}

bool is_gun(int wp) { return wp >= 0 && wp != 4500 && wp != 4510 && wp != 6200 && wp != 6300; }

Tier tier(const Player& p) {
    if (!g_live || p.eq == nullptr || p.cond == nullptr || !is_gun(weapons::current_id())) return Tier::GAME;
    const bool moving = call_direct<bool>(p.cond, "get_IsWalk", false) || call_direct<bool>(p.cond, "get_IsJog", false);
    if (moving) return Tier::GAME;   // the vanilla walk-aim value
    const bool two_hands = bridge::held(bridge::S_LGRIP) &&
                           dist(bridge::hand_pos(bridge::LEFT), bridge::hand_pos(bridge::RIGHT)) < cfg::SPREAD_DOCK_HANDS_M;
    return two_hands ? Tier::TWO_HANDS_STILL : Tier::ONE_HAND_STILL;
}

const char* name(Tier t) { return t == Tier::TWO_HANDS_STILL ? "two hands still" : t == Tier::ONE_HAND_STILL ? "one hand still" : "moving (game's value)"; }

// returns the fit after the write (or the game's, untouched)
float apply(const Player& p, Tier t) {
    auto* fit = field_ptr<float>(p.eq, "_ReticleFitPoint");
    if (fit == nullptr) return -1.0f;
    if (t == Tier::TWO_HANDS_STILL) {
        *fit = cfg::SPREAD_TWO_HANDS_STILL;
        if (auto* isfit = field_ptr<bool>(p.eq, "_IsReticleFit")) *isfit = true;
    } else if (t == Tier::ONE_HAND_STILL && *fit < cfg::SPREAD_ONE_HAND_STILL) {
        *fit = cfg::SPREAD_ONE_HAND_STILL;   // aiming still with RG climbs above this on its own: never pulled down
    }
    return *fit;
}

int pre_request_fire(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    const Player p = player();
    const Tier t = tier(p);
    const float after = apply(p, t);
    const int n = g_shots.fetch_add(1) + 1;
    if (n <= cfg::SPREAD_LOG_SHOTS) LOGI("%s spread: shot #%d, %s, fit %.1f", TAG, n, name(t), after);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_request_fire(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}
} // namespace

void install() {
    if (!cfg::SPREAD_TIERS_ON) return;
    auto* m = API::get()->tdb()->find_method("app.ropeway.survivor.Equipment", "requestFire");
    if (m == nullptr) { LOGW("%s spread: Equipment.requestFire not found, tiers only per frame", TAG); return; }
    m->add_hook(pre_request_fire, post_request_fire, false);
    LOGI("%s spread: tiers on (one hand still >= %.0f, two hands still = %.0f, moving = the game's)", TAG,
         cfg::SPREAD_ONE_HAND_STILL, cfg::SPREAD_TWO_HANDS_STILL);
}

void frame() {
    g_live = bridge::live();
    if (!cfg::SPREAD_TIERS_ON) return;
    const Player p = player();
    apply(p, tier(p));
}

} // namespace vn::spread
