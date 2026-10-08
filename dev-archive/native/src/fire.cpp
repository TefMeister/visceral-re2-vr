// fire.cpp -- see fire.h.
//
// Proves its own effect: every shot logs the bullets before and after, which of the game's three doorbells rang
// (Equipment.requestFire -> Gun.executeFire -> Equipment.executeFire), how often enableAttack was answered YES, and
// IsHold / the ATTACK input / Precede at the press. "bullets n -> n-1" = it works; a missing doorbell names the gate.
#include "fire.h"
#include "bridge.h"
#include "common.h"
#include "menu_body.h"
#include "settings.h"
#include "weapons.h"

#include <atomic>

namespace vn::fire {

namespace {
constexpr unsigned PRECEDE_ATTACK = 4;        // SurvivorActionOrderer precede action ATTACK (dossier 8k)
constexpr uint64_t KIND_ATTACK = 256;         // InputDefine.Kind.ATTACK (RT)

std::atomic<bool> g_in_flight{false};         // from our press until Equipment.executeFire ran, or the window ran out
std::atomic<int> g_yes{0};                    // enableAttack answers turned to YES during this shot
std::atomic<int> g_request{0}, g_gun_exec{0}, g_eq_exec{0};   // doorbells during this shot
bool g_forcing = false;                       // switch 1 is on
int g_window = 0;                             // frames left for switch 3
int g_bullets_before = -1;
int g_shots = 0;

thread_local MO* t_eq = nullptr;
thread_local int t_type = -1;

struct Player { MO* cond; MO* eq; MO* orderer; MO* updater; };

Player player() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    auto* go = call_ptr(pm, "get_CurrentPlayer");
    auto* cond = component(go, "app.ropeway.survivor.SurvivorCondition");
    return {cond, component(go, "app.ropeway.survivor.Equipment"),
            call_ptr(cond, "get_ActionOrderer"), call_ptr(cond, "get_UserVariablesUpdater")};
}

int bullets(MO* eq) {
    auto* arm = call_ptr(eq, "get_EquipWeapon");
    if (arm == nullptr) arm = call_ptr(eq, "get_MainWeapon");
    return call_direct<int>(arm, "getBulletNumber", -1);
}

bool gun_in_hand() {
    const int wp = weapons::current_id();
    if (wp < 0) return false;
    return weapons::holster_for(wp) != weapons::Holster::SUB;   // knife and grenades go through other actions
}

bool attack_input_on() {
    auto* inp = API::get()->get_managed_singleton("app.ropeway.InputSystem");
    if (inp == nullptr) return false;
    auto* m = find_method_deep(inp->get_type_definition(), "isOn(System.UInt64, System.Boolean)");
    return m != nullptr && m->call<bool>(API::get()->get_vm_context(), (void*)inp, KIND_ATTACK, false);
}

void set_precede(MO* orderer, bool on) {
    auto* m = orderer ? find_method_deep(orderer->get_type_definition(), "setForcePrecede") : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)orderer, on, PRECEDE_ATTACK);
}

// ---- switch 3: Equipment.enableAttack(WeaponType), instance: argv[1] = this, argv[2] = the WeaponType ----------
int pre_enable_attack(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    t_eq = argc > 2 && is_managed(argv[1]) ? (MO*)argv[1] : nullptr;
    t_type = argc > 2 ? (int)(intptr_t)argv[2] : -1;
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_enable_attack(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (ret_val == nullptr || t_eq == nullptr || !g_in_flight) return;
    if (((uintptr_t)*ret_val & 0xFF) != 0) return;                                   // the game already said yes
    if (call_direct<bool>(t_eq, "checkEmpty", true, t_type)) return;                  // empty gun: let it click
    *ret_val = (void*)(uintptr_t)1;
    g_yes.fetch_add(1);
}

// ---- observe-only doorbells -------------------------------------------------------------------------------------
int pre_request(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) { if (g_in_flight) g_request.fetch_add(1); return REFRAMEWORK_HOOK_CALL_ORIGINAL; }
int pre_gun_exec(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) { if (g_in_flight) g_gun_exec.fetch_add(1); return REFRAMEWORK_HOOK_CALL_ORIGINAL; }
int pre_eq_exec(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) { if (g_in_flight) g_eq_exec.fetch_add(1); return REFRAMEWORK_HOOK_CALL_ORIGINAL; }
// switch 3 ends once the shot has really gone (the next frame, so the post-hooks of this one still see it)
void post_eq_exec(void**, REFrameworkTypeDefinitionHandle, unsigned long long) { if (g_in_flight && g_window > 1) g_window = 1; }

void hook(const char* type, const char* method, REFPreHookFn pre, REFPostHookFn post, const char* what) {
    auto* m = API::get()->tdb()->find_method(type, method);
    if (m == nullptr) { LOGE("%s fire: %s.%s not found: %s off", TAG, type, method, what); return; }
    m->add_hook(pre, post, false);
    LOGI("%s fire: hook in %s.%s (%s)", TAG, type, method, what);
}

void end_shot(const Player& p) {
    const int after = bullets(p.eq);
    if (g_shots <= cfg::FIRE_LOG_FIRST)
        LOGI("%s fire: shot #%d bullets %d -> %d | requestFire %d, Gun.executeFire %d, Equipment.executeFire %d, enableAttack YES %d",
             TAG, g_shots, g_bullets_before, after, g_request.load(), g_gun_exec.load(), g_eq_exec.load(), g_yes.load());
    g_in_flight = false;
}
} // namespace

void install() {
    hook("app.ropeway.survivor.Equipment", "enableAttack", pre_enable_attack, post_enable_attack, "switch 3: the gun may fire unaimed");
    hook("app.ropeway.survivor.Equipment", "requestFire", pre_request, nullptr, "doorbell, observe only");
    hook("app.ropeway.implement.Gun", "executeFire", pre_gun_exec, nullptr, "doorbell, observe only");
    hook("app.ropeway.survivor.Equipment", "executeFire", pre_eq_exec, post_eq_exec, "doorbell, observe only");
}

void frame() {
    const bool rt = bridge::live() && bridge::held(bridge::S_RTRIG);
    const bool rg = bridge::held(bridge::S_RGRIP);
    Player p{};
    const bool want = rt && !rg && !menu_body::is_menu_open() && gun_in_hand() && (p = player()).cond != nullptr;

    // switch 1, for as long as the trigger is down; the latch is cleared the frame it is let go
    if (want && bridge::pressed(bridge::S_RTRIG) && !call_direct<bool>(p.cond, "get_IsHold", false)) {
        g_forcing = true;
        ++g_shots;
        g_request = 0; g_gun_exec = 0; g_eq_exec = 0; g_yes = 0;
        g_bullets_before = bullets(p.eq);
        g_window = cfg::FIRE_WINDOW_FRAMES;
        g_in_flight = true;
        set_precede(p.orderer, true);
        auto* set_fire = p.updater ? find_method_deep(p.updater->get_type_definition(), "set_Fire") : nullptr;
        if (set_fire != nullptr) set_fire->call<void>(API::get()->get_vm_context(), (void*)p.updater, true);   // switch 2
        if (g_shots <= cfg::FIRE_LOG_FIRST)
            LOGI("%s fire: RT without RG, shot #%d | bullets %d IsHold %d ATTACK input %d Precede %d", TAG, g_shots,
                 g_bullets_before, (int)call_direct<bool>(p.cond, "get_IsHold", false), (int)attack_input_on(),
                 call_direct<int>(p.orderer, "get_Precede", -1));
    } else if (g_forcing && want && g_in_flight && g_eq_exec == 0) {
        set_precede(p.orderer, true);
    } else if (g_forcing) {
        // b097 kept the order on for as long as RT was held: one round went, then the shoot clip replayed with no
        // round, the slide cycling until RT was let go (Tefa 2026-10-08). So the order ends once the round has gone:
        // one shot per pull. Holding RT does nothing more until it is pulled again.
        g_forcing = false;
        set_precede(want ? p.orderer : player().orderer, false);
    }

    if (g_in_flight && --g_window <= 0) end_shot(p.eq != nullptr ? p : player());
}

} // namespace vn::fire
