// weapons.cpp -- see weapons.h. Names from RELOADED 1.0.1's data (re2_vr_reload.json / re2_vr_recoil.json);
// holster sorting is Tefa's (2026-10-05). The knife, grenades, EMF visualizer and the extra minigun numbers come
// from the RE Modding forum's ID thread (residentevilmodding.boards.net/thread/9864): its hex weapon list is the
// game's WeaponType enum value (01 handgun = WP0000, 0B shotgun = WP1000, 2E knife = WP4500 ...), which matches
// every weapon we could cross-check [inferred-static 2026-10-05]; the live log confirms each one when held.
#include "weapons.h"

#include <map>

namespace vn::weapons {

namespace {
struct Info { const char* name; Holster holster; };
const std::map<int, Info>& table() {
    static const std::map<int, Info> t = {
        {0,    {"Matilda", Holster::HANDGUN}},
        {100,  {"M19", Holster::HANDGUN}},
        {200,  {"JMB Hp3", Holster::HANDGUN}},
        {300,  {"Quickdraw Army", Holster::HANDGUN}},
        {400,  {"Glock 17", Holster::HANDGUN}},
        {600,  {"MUP", Holster::HANDGUN}},
        {700,  {"Broom Hc", Holster::HANDGUN}},
        {800,  {"SLS 60", Holster::HANDGUN}},
        {3000, {"Lightning Hawk", Holster::HANDGUN}},
        {3200, {"Chief Irons Revolver", Holster::HANDGUN}},
        {7000, {"Samurai Edge", Holster::HANDGUN}},
        {7010, {"Samurai Edge (Chris)", Holster::HANDGUN}},
        {7020, {"Samurai Edge (Jill)", Holster::HANDGUN}},
        {7030, {"Samurai Edge (Albert)", Holster::HANDGUN}},
        {1000, {"W-870", Holster::LONG}},
        {1100, {"Remington 870", Holster::LONG}},
        {1200, {"M3 Shotgun", Holster::LONG}},
        {1300, {"GM 79", Holster::LONG}},
        {1500, {"Lightning Hawk (shotgun)", Holster::LONG}},
        {2000, {"MQ 11", Holster::LONG}},
        {2200, {"LE 5", Holster::LONG}},
        {4100, {"GM 79", Holster::LONG}},
        {4200, {"Chemical Flamethrower", Holster::SPECIAL}},
        {4300, {"Spark Shot", Holster::SPECIAL}},
        {4400, {"ATM-4", Holster::SPECIAL}},
        {4600, {"Anti-Tank Rocket", Holster::SPECIAL}},
        {4700, {"Minigun", Holster::SPECIAL}},
        {8400, {"ATM-4 Unlimited", Holster::SPECIAL}},
        {8700, {"Minigun Unlimited", Holster::SPECIAL}},
        {4520, {"Minigun", Holster::SPECIAL}},
        {4900, {"Minigun", Holster::SPECIAL}},
        {3300, {"EMF Visualizer (Ada)", Holster::SPECIAL}},     // Tefa: a tool, left shoulder with the special weapons
        {4000, {"EMF Visualizer (Ada)", Holster::SPECIAL}},
        {4500, {"Combat Knife", Holster::SUB}},
        {4510, {"Survival Knife (unbreakable)", Holster::SUB}},
        {6200, {"Hand Grenade", Holster::SUB}},
        {6300, {"Flash Grenade", Holster::SUB}},
    };
    return t;
}

// WeaponType enum value -> WP number, read from the game's own enum names ("WP0000" = 1, BareHand = 0, ...)
std::map<int, int>& enum_to_wp() {
    static std::map<int, int> m;
    if (!m.empty()) return m;
    auto* td = API::get()->tdb()->find_type("app.ropeway.EquipmentDefine.WeaponType");
    if (td == nullptr) return m;
    for (auto* f : td->get_fields()) {
        if (f == nullptr || !f->is_static()) continue;
        const std::string n = f->get_name();
        if (n.size() == 6 && n[0] == 'W' && n[1] == 'P') m[f->get_data<int>(nullptr)] = std::stoi(n.substr(2));
    }
    LOGI("%s weapon types read from the game: %d", TAG, (int)m.size());
    return m;
}
} // namespace

int current_id() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    auto* go = call_ptr(pm, "get_CurrentPlayer");
    auto* eq = component(go, "app.ropeway.survivor.Equipment");
    if (eq == nullptr) return -1;
    const int v = call_direct<int>(eq, "get_EquipType", -1);
    auto& m = enum_to_wp();
    auto it = m.find(v);
    return it == m.end() ? -1 : it->second;
}

const char* name(int wp) {
    auto it = table().find(wp);
    return it == table().end() ? "unknown" : it->second.name;
}

Holster holster_for(int wp) {
    auto it = table().find(wp);
    return it == table().end() ? Holster::NONE : it->second.holster;
}

const char* holster_name(Holster h) {
    switch (h) {
    case Holster::HANDGUN: return "right hip (handguns)";
    case Holster::LONG: return "right shoulder (long guns)";
    case Holster::SPECIAL: return "left shoulder (special weapons)";
    case Holster::SUB: return "left hip (sub weapon)";
    default: return "not sorted yet";
    }
}

} // namespace vn::weapons
