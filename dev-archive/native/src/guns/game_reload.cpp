// game_reload.cpp -- see game_reload.h.
#include "game_reload.h"
#include "bridge.h"
#include "gun_frame.h"
#include "guns_api.h"
#include "joints.h"
#include "magazine.h"
#include "menu_body.h"
#include "settings.h"

#include <algorithm>
#include <atomic>

namespace vn::guns::game_reload {

using namespace joints;

namespace {
int g_commit = 0;                                                  // >0 while our own reload call runs
constexpr uint64_t KIND_SUPPORT_HOLD = 128, KIND_RELOAD = 512;     // app.ropeway.InputDefine.Kind
constexpr uint32_t OFF_DOWN = 0x10, OFF_ON = 0x18, OFF_UP = 0x20;  // InputSystem ButtonBits fields
constexpr uint32_t PRECEDE_RELOAD = 8;                             // SurvivorDefine.ActionOrder.Precede.RELOAD
std::atomic<int> g_blocked{0};

bool blocking() { return cfg::RELOAD_ON && g_commit == 0 && guns::managed_now() && !menu_body::is_menu_open(); }

// the RELOAD action is started by the player's action orderer, not by Equipment.requestReload: the orderer is told not to
bool g_inhibited = false;
void inhibit_reload(bool on) {
    auto* o = call_ptr(component(gf().player, "app.ropeway.survivor.SurvivorCondition"), "get_ActionOrderer");
    auto* m = o ? find_method_deep(o->get_type_definition(), "setInhibitPrecede") : nullptr;
    if (m == nullptr) return;
    m->call<void>(API::get()->get_vm_context(), (void*)o, on, PRECEDE_RELOAD);
    if (on != g_inhibited) LOGI("%s reload: the game's own reload %s", TAG, on ? "OFF for this gun" : "back on");
    g_inhibited = on;
}
void post_orderer_update(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (cfg::RELOAD_ON && guns::managed_now()) inhibit_reload(true);
}

int pre_skip_reload(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (!blocking()) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    const int n = g_blocked.fetch_add(1) + 1;
    if (n <= 5 || n % 100 == 0) LOGI("%s reload: the game's own reload stopped for WP%04d (#%d)", TAG, gf().wp, n);
    return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
}

// the HUD: the counter shown after one of ours, the loaded count while the magazine is out
float g_hud_until = -1.0f;
MO* g_hud = nullptr;   // the RemainingBulletBehavior, caught from its own getter
int pre_hud_request(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (argc > 1 && is_managed(argv[1]) && g_hud != (MO*)argv[1]) { g_hud = (MO*)argv[1]; g_hud->add_ref(); }
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_hud_request(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (ret_val != nullptr && g_hud_until >= 0.0f && now_s() <= g_hud_until) *ret_val = (void*)(uintptr_t)1;
}
void post_hud_rounds(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (ret_val == nullptr || g_commit > 0 || !magazine::mag_out()) return;
    *ret_val = (void*)(uintptr_t)std::max(0, magazine::hud_rounds());
}
void hud_tick() {
    if (g_hud == nullptr || g_hud_until < 0.0f || now_s() > g_hud_until) return;
    call_direct<void*>(g_hud, "set_RequestDraw", nullptr, true);
    call_direct<void*>(g_hud, "set_IsDraw", nullptr, true);
}

int pre_nop(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) { return REFRAMEWORK_HOOK_CALL_ORIGINAL; }
void post_nop(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}
void hook(const char* type, const char* method, REFPreHookFn pre, REFPostHookFn post, const char* what) {
    auto* m = API::get()->tdb()->find_method(type, method);
    if (m == nullptr) { LOGW("%s reload: %s.%s not found (%s)", TAG, type, method, what); return; }
    m->add_hook(pre, post, false);
}

void clear_bit(char* bb, uint64_t kind) {
    auto& down = *(uint64_t*)(bb + OFF_DOWN);
    auto& on = *(uint64_t*)(bb + OFF_ON);
    auto& up = *(uint64_t*)(bb + OFF_UP);
    down &= ~kind;
    on &= ~kind;
    up &= ~kind;
}
} // namespace

void show_ammo_counter(float seconds) { g_hud_until = now_s() + seconds; }
Commit::Commit() { ++g_commit; }
Commit::~Commit() { --g_commit; }

void install() {
    if (!cfg::RELOAD_ON) { LOGI("%s reload: OFF (cfg::RELOAD_ON): the game reloads as usual", TAG); return; }
    const char* EQ = "app.ropeway.survivor.Equipment";
    hook(EQ, "requestReload", pre_skip_reload, post_nop, "reload request");
    hook(EQ, "executeReload(app.ropeway.EquipmentDefine.WeaponType, System.Int32)", pre_skip_reload, post_nop, "reload by weapon");
    hook(EQ, "executeReload(app.ropeway.EquipmentDefine.EquipCategory, System.Int32)", pre_skip_reload, post_nop, "reload by category");
    hook("app.ropeway.implement.Gun", "executeReload", pre_skip_reload, post_nop, "gun reload");
    hook("app.ropeway.survivor.Inventory", "reloadMainSlot", pre_skip_reload, post_nop, "rounds into the gun");
    hook("app.ropeway.gamemastering.InventoryManager", "getMainWeaponRemainingBullet", pre_nop, post_hud_rounds, "HUD count with the magazine out");
    hook("app.ropeway.gui.RemainingBulletBehavior", "get_RequestDraw", pre_hud_request, post_hud_request, "ammo counter after a reload");
    hook("app.ropeway.survivor.player.PlayerActionOrderer", "doSurvivorActionOrdererUpdate", pre_nop, post_orderer_update, "no reload action");
    LOGI("%s reload: the game's own reload is kept off for the guns reloaded by hand", TAG);
}

void frame() {
    if (!cfg::RELOAD_ON) return;
    const bool ours = guns::managed_now(), session = guns::session_active();
    hud_tick();
    if (ours) inhibit_reload(true);
    else if (g_inhibited) inhibit_reload(false);   // a gun of the game's own (or no headset): its reload is back
    if (menu_body::is_menu_open() || (!ours && !session)) return;
    auto* bb = call_ptr(API::get()->get_managed_singleton("app.ropeway.InputSystem"), "get_ButtonBits");
    if (!is_managed(bb)) return;
    if (ours) clear_bit((char*)bb, KIND_RELOAD);
    if (session) clear_bit((char*)bb, KIND_SUPPORT_HOLD);   // the left grip takes a magazine, not the knife or a grenade
}

} // namespace vn::guns::game_reload
