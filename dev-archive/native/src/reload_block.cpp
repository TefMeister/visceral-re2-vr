// reload_block.cpp -- see reload_block.h.
#include "reload_block.h"
#include "bridge.h"
#include "common.h"
#include "menu_body.h"
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

float now_s() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
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
    if (reload::mag_out()) {
        const int n = g_fire_blocked.fetch_add(1) + 1;
        if (n <= 10 || n % 100 == 0) LOGI("%s block: shot stopped, the magazine is out (#%d)", TAG, n);
        dry_click();
        return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
    }
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_nop(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {}

// an empty but seated gun: the game still asks to fire (port step 2, 2026-10-03), so the click is played after the
// game has had its go (getBulletNumber read in the post, when the shot, if any, has already taken its round)
int g_rounds_at_pre = -1;
int pre_fire_count(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    g_rounds_at_pre = -1;
    if (!cfg::RELOAD_ON || !reload::managed_now() || reload::mag_out() || argc < 2) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    auto* eq = is_managed(argv[1]) ? (MO*)argv[1] : nullptr;
    g_rounds_at_pre = call_direct<int>(field_obj(eq, "<EquipWeapon>k__BackingField"), "getBulletNumber", -1);
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_fire_count(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (g_rounds_at_pre == 0) dry_click();
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
}

void frame() {
    if (!cfg::RELOAD_ON || menu_body::is_menu_open()) return;
    const bool ours = reload::managed_now(), session = reload::session_active();
    if (!ours && !session) return;
    auto* bb = call_ptr(API::get()->get_managed_singleton("app.ropeway.InputSystem"), "get_ButtonBits");
    if (!is_managed(bb)) return;
    if (ours) clear_bit((char*)bb, KIND_RELOAD);
    if (session) clear_bit((char*)bb, KIND_SUPPORT_HOLD);   // the left grip takes a magazine, not the knife or grenade
}

} // namespace vn::reload_block
