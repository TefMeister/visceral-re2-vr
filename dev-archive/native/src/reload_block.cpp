// reload_block.cpp -- see reload_block.h.
#include "reload_block.h"
#include "bridge.h"
#include "common.h"
#include "menu_body.h"
#include "pump_native.h"
#include "rack.h"
#include "reload.h"
#include "settings.h"
#include "sfx.h"

#include <atomic>
#include <chrono>

namespace vn::reload_block {

namespace {
int g_commit = 0;                       // >0 while our own reload call runs
std::atomic<int> g_blocked{0}, g_fire_blocked{0}, g_bits_cleared{0};
constexpr uint64_t KIND_SUPPORT_HOLD = 128, KIND_RELOAD = 512;     // app.ropeway.InputDefine.Kind
constexpr uint32_t OFF_DOWN = 0x10, OFF_ON = 0x18, OFF_UP = 0x20;  // InputSystem ButtonBits fields (as suppress.cpp)
constexpr uint32_t PRECEDE_RELOAD = 8;  // SurvivorDefine.ActionOrder.Precede.RELOAD (type database 2026-10-10)

// b133: Tefa saw the game's reload animation play on B anyway (b132): the RELOAD action is started by the player's
// action orderer, not by Equipment.requestReload (never called), and our input-bit clear came too late for it. So the
// orderer is told not to start a reload (RELOADED's layer 1, setInhibitPrecede) and the reload state reads false
// (his layer 3, get_IsReload). Both only while one of our guns is in hand with the headset on.
bool g_inhibited = false;
MO* orderer() {
    auto* go = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    return call_ptr(component(go, "app.ropeway.survivor.SurvivorCondition"), "get_ActionOrderer");
}
void inhibit_reload(bool on) {
    auto* o = orderer();
    auto* m = o ? find_method_deep(o->get_type_definition(), "setInhibitPrecede") : nullptr;
    if (m == nullptr) return;
    m->call<void>(API::get()->get_vm_context(), (void*)o, on, PRECEDE_RELOAD);
    if (on != g_inhibited) LOGI("%s block: the orderer's RELOAD action %s", TAG, on ? "INHIBITED" : "allowed again");
    g_inhibited = on;
}
std::atomic<int> g_isreload_turned{0};
void post_is_reload(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (ret_val == nullptr || g_commit > 0 || !cfg::RELOAD_ON || !reload::managed_now()) return;
    if (((uintptr_t)*ret_val & 0xFF) == 0) return;
    *ret_val = (void*)(uintptr_t)0;
    const int n = g_isreload_turned.fetch_add(1) + 1;
    if (n <= 10 || n % 200 == 0) LOGI("%s block: the game said it is reloading, answered no (#%d)", TAG, n);
}
void post_orderer_update(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (cfg::RELOAD_ON && reload::managed_now()) inhibit_reload(true);
}

float now_s() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
}

float g_hud_until = -1.0f;
void post_hud_request(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {   // the ammo counter stays up
    if (ret_val == nullptr || g_hud_until < 0.0f || now_s() > g_hud_until) return;
    *ret_val = (void*)(uintptr_t)1;
}


bool blocking() { return cfg::RELOAD_ON && g_commit == 0 && reload::managed_now() && !menu_body::is_menu_open(); }

int pre_skip_reload(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (!blocking()) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    const int n = g_blocked.fetch_add(1) + 1;
    if (n <= 10 || n % 100 == 0) LOGI("%s block: the game's own reload stopped for WP%04d (#%d)", TAG, reload::wp_now(), n);
    return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
}

// the trigger on an empty gun, or with the magazine out: Andyalpa's dry-fire click, once per pull
float g_last_dry = -10.0f;
void dry_click() {
    const float now = now_s();
    if (now - g_last_dry < cfg::RELOAD_DRY_FIRE_GAP_SEC && !bridge::pressed(bridge::S_RTRIG)) return;
    g_last_dry = now;
    sfx::play("dry_fire", reload::sfx_folder_now(), reload::sfx_volume_now());
}

int pre_fire(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (!cfg::RELOAD_ON || !reload::managed_now() || menu_body::is_menu_open()) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    if (reload::mag_out() || rack::blocks_fire()) {
        const int n = g_fire_blocked.fetch_add(1) + 1;
        if (n <= 10 || n % 100 == 0) LOGI("%s block: shot stopped, %s (#%d)", TAG, reload::mag_out() ? "the magazine is out" : "a rack / pump is needed", n);
        dry_click();
        return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
    }
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_nop(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}

// an empty but seated gun: the game still asks to fire (port step 2, 2026-10-03), so the click is played after the
// game has had its go (getBulletNumber read in the post, when the shot, if any, has already taken its round)
int g_rounds_at_pre = -1;
MO* t_gun = nullptr;
int pre_fire_count(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    g_rounds_at_pre = -1;
    if (!cfg::RELOAD_ON || !reload::managed_now() || reload::mag_out() || argc < 2) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    auto* eq = is_managed(argv[1]) ? (MO*)argv[1] : nullptr;
    t_gun = field_obj(eq, "<EquipWeapon>k__BackingField");
    g_rounds_at_pre = call_direct<int>(t_gun, "getBulletNumber", -1);
    pump_native::on_pre_fire(t_gun);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_fire_count(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (g_rounds_at_pre == 0) dry_click();
    if (g_rounds_at_pre > 0 && t_gun != nullptr) {
        pump_native::on_post_fire(t_gun);
        if (call_direct<int>(t_gun, "getBulletNumber", -1) == 0) rack::slide_lock_empty();   // the last round: the slide locks open
    }
}

// the HUD's loaded count while the magazine is out
void post_hud_rounds(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (ret_val == nullptr || !cfg::RELOAD_HUD_ZERO_WHEN_OUT || g_commit > 0 || !reload::mag_out()) return;
    *ret_val = (void*)(uintptr_t)0;
}
int pre_nop(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) { return REFRAMEWORK_HOOK_CALL_ORIGINAL; }

void hook(const char* type, const char* method, REFPreHookFn pre, REFPostHookFn post, const char* what) {
    auto* m = API::get()->tdb()->find_method(type, method);
    if (m == nullptr) { LOGW("%s block: %s.%s not found (%s)", TAG, type, method, what); return; }
    m->add_hook(pre, post, false);
    LOGI("%s block: hook in: %s.%s (%s)", TAG, type, method, what);
}

void clear_bit(char* bb, uint64_t kind) {
    auto& down = *(uint64_t*)(bb + OFF_DOWN);
    auto& on = *(uint64_t*)(bb + OFF_ON);
    auto& up = *(uint64_t*)(bb + OFF_UP);
    if (((down | on | up) & kind) == 0) return;
    down &= ~kind;
    on &= ~kind;
    up &= ~kind;
    const int n = g_bits_cleared.fetch_add(1) + 1;
    if (n <= 10 || n % 200 == 0) LOGI("%s block: input %s cleared (#%d)", TAG, kind == KIND_RELOAD ? "RELOAD" : "SUPPORT_HOLD", n);
}
} // namespace

void show_ammo_counter(float seconds) { g_hud_until = now_s() + seconds; }

Commit::Commit() { ++g_commit; }
Commit::~Commit() { --g_commit; }

void install() {
    if (!cfg::RELOAD_ON) { LOGI("%s block: manual reload is OFF (cfg::RELOAD_ON), the game reloads as usual", TAG); return; }
    const char* EQ = "app.ropeway.survivor.Equipment";
    hook(EQ, "requestReload", pre_skip_reload, post_nop, "reload request");
    hook(EQ, "executeReload(app.ropeway.EquipmentDefine.WeaponType, System.Int32)", pre_skip_reload, post_nop, "reload by weapon");
    hook(EQ, "executeReload(app.ropeway.EquipmentDefine.EquipCategory, System.Int32)", pre_skip_reload, post_nop, "reload by category");
    hook("app.ropeway.implement.Gun", "executeReload", pre_skip_reload, post_nop, "gun reload");
    hook("app.ropeway.survivor.Inventory", "reloadMainSlot", pre_skip_reload, post_nop, "rounds into the gun");
    hook(EQ, "requestFire", pre_fire, post_nop, "no shot with the magazine out");
    hook(EQ, "requestFire", pre_fire_count, post_fire_count, "dry-fire click on an empty gun");
    hook("app.ropeway.gamemastering.InventoryManager", "getMainWeaponRemainingBullet", pre_nop, post_hud_rounds, "HUD reads 0 with the magazine out");
    hook("app.ropeway.gui.RemainingBulletBehavior", "get_RequestDraw", pre_nop, post_hud_request, "the ammo counter shown after a reload (Tefa: 5 s)");
    // b141: the get_IsReload spoof is OUT. Worn 2026-10-10 (b140): guns shot with sound but no flash and no counter, and a
    // shotgun sat in the game's shell-loading hand pose forever -- the game's own reload state had started and, told it
    // was not reloading while its inner steps were skipped, never left it [hypothesis]. The orderer inhibit alone is what
    // stopped the animation on B (b133). The state is now WATCHED instead (frame()).
    hook("app.ropeway.survivor.player.PlayerActionOrderer", "doSurvivorActionOrdererUpdate", pre_nop, post_orderer_update, "no reload action for our guns");
}

bool g_game_reload = false;
float g_game_reload_t0 = 0.0f;
void watch_game_reload() {   // the game's own reload state, which our guns should never enter
    auto* go = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    auto* cond = component(go, "app.ropeway.survivor.SurvivorCondition");
    const bool r = call_direct<bool>(cond, "get_IsReload", false);
    if (r && !g_game_reload) { g_game_reload_t0 = now_s(); LOGW("%s block: THE GAME ENTERED ITS OWN RELOAD STATE on WP%04d (ours; this is the stuck-hand case if it stays)", TAG, reload::wp_now()); }
    else if (!r && g_game_reload) LOGI("%s block: the game's reload state ended after %.2f s", TAG, now_s() - g_game_reload_t0);
    g_game_reload = r;
}

void frame() {
    if (!cfg::RELOAD_ON) return;
    const bool ours = reload::managed_now(), session = reload::session_active();
    if (ours) watch_game_reload();
    // b142: the trigger with the magazine out or a rack needed: the click (fire.cpp no longer forces the shot, so
    // requestFire is never reached and the click has to come from the press itself)
    if (ours && bridge::pressed(bridge::S_RTRIG) && (reload::mag_out() || rack::blocks_fire()) && !menu_body::is_menu_open()) dry_click();
    if (ours) inhibit_reload(true);
    else if (g_inhibited) inhibit_reload(false);   // a shotgun, revolver or no headset: the game's reload is back
    if (menu_body::is_menu_open()) return;
    if (!ours && !session) return;
    auto* bb = call_ptr(API::get()->get_managed_singleton("app.ropeway.InputSystem"), "get_ButtonBits");
    if (!is_managed(bb)) return;
    if (ours) clear_bit((char*)bb, KIND_RELOAD);
    if (session) clear_bit((char*)bb, KIND_SUPPORT_HOLD);   // the left grip takes a magazine, not the knife or grenade
}

} // namespace vn::reload_block
