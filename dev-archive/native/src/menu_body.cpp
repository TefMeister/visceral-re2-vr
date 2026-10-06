// menu_body.cpp -- see menu_body.h.
#include "menu_body.h"
#include "bridge.h"
#include "common.h"
#include "settings.h"

#include <vector>

namespace vn::menu_body {

namespace {
const char* const FLAGS[] = {"DrawDefault", "DrawShadowCast", "DrawRaytracing"};
constexpr int NFLAGS = 3;

struct Hidden { MO* mesh; int orig[NFLAGS]; };   // orig: -1 = flag not readable, 0/1 = its value before
std::vector<Hidden> g_hidden;
bool g_is_hidden = false;

bool get_flag(MO* mesh, int f, bool& out) {
    auto* m = find_method_deep(mesh->get_type_definition(), std::string("get_") + FLAGS[f]);
    if (m == nullptr) return false;
    out = m->call<bool>(API::get()->get_vm_context(), (void*)mesh);
    return true;
}

void set_flag(MO* mesh, int f, bool v) {
    auto* m = find_method_deep(mesh->get_type_definition(), std::string("set_") + FLAGS[f]);
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)mesh, v);
}

bool menu_open() {
    auto* gm = API::get()->get_managed_singleton("app.ropeway.gui.GUIMaster");
    if (gm == nullptr) return false;
    for (const char* q : {"get_IsOpenInventory", "get_IsOpenMap", "get_IsOpenPause", "get_IsOpenPauseForEvent"})
        if (call_direct<bool>(gm, q, false)) return true;
    return false;
}

// every via.render.Mesh under a transform, children first-to-last (get_Child, then get_Next), like Arcade Controls
void walk(MO* tf, int depth, std::vector<MO*>& out) {
    if (tf == nullptr || depth > cfg::MENU_WALK_DEPTH) return;
    if (auto* mesh = component(call_ptr(tf, "get_GameObject"), "via.render.Mesh")) {
        bool dup = false;
        for (auto* m : out) dup = dup || m == mesh;
        if (!dup) out.push_back(mesh);
    }
    int guard = 0;
    for (MO* c = call_ptr(tf, "get_Child"); c != nullptr && guard < cfg::MENU_WALK_CHILDREN; c = call_ptr(c, "get_Next"), ++guard)
        walk(c, depth + 1, out);
}

std::vector<MO*> player_meshes() {
    std::vector<MO*> out;
    auto* player = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    if (player == nullptr) return out;
    walk(call_ptr(player, "get_Transform"), 0, out);
    // the weapon in hand may not hang under the player: walk it too (duplicates skipped)
    auto* weapon = field_obj(component(player, "app.ropeway.survivor.Equipment"), "<EquipWeapon>k__BackingField");
    if (weapon != nullptr) walk(call_ptr(call_ptr(weapon, "get_GameObject"), "get_Transform"), 0, out);
    return out;
}

void hide() {
    g_hidden.clear();
    for (auto* mesh : player_meshes()) {
        Hidden h{mesh, {-1, -1, -1}};
        for (int f = 0; f < NFLAGS; ++f) {
            bool v;
            if (get_flag(mesh, f, v)) { h.orig[f] = v ? 1 : 0; set_flag(mesh, f, false); }
        }
        if (h.orig[0] < 0) continue;   // DrawDefault is the one that matters; without it, leave the mesh alone
        mesh->add_ref();
        g_hidden.push_back(h);
    }
    g_is_hidden = true;
    LOGI("%s menu: opened -- player hidden (%zu meshes)", TAG, g_hidden.size());
}

void reassert() {
    for (auto& h : g_hidden)
        for (int f = 0; f < NFLAGS; ++f)
            if (h.orig[f] >= 0) set_flag(h.mesh, f, false);
}

void restore() {
    for (auto& h : g_hidden) {
        for (int f = 0; f < NFLAGS; ++f)
            if (h.orig[f] >= 0) set_flag(h.mesh, f, h.orig[f] == 1);
        h.mesh->release();
    }
    if (g_is_hidden) LOGI("%s menu: closed -- %zu meshes put back", TAG, g_hidden.size());
    g_hidden.clear();
    g_is_hidden = false;
}
} // namespace

void frame() {
    const bool want = bridge::live() && menu_open();
    if (want && !g_is_hidden) hide();
    else if (want) reassert();
    else if (g_is_hidden) restore();
}

} // namespace vn::menu_body
