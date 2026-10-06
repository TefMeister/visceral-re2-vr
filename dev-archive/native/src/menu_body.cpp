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
// ---- the camera: kept where it was before the menu opened (Tefa 2026-10-06: "the menus ... make the picture
// jump forward a bit"; the menus move the camera to their own outside spot, Arcade Controls' notes) ----
struct Cam {
    Vec3 last{}; bool has_last = false;          // the camera position at the end of the last frame with no menu
    Vec3 pin{}; bool pinned = false;             // where it is held while a menu is open
    int logs = 0; float max_moved = 0.0f; bool where_logged = false;
} g_cam;

MO* camera_transform() {
    static API::Method* main_view = API::get()->tdb()->find_method("via.SceneManager", "get_MainView");
    static API::Method* primary = API::get()->tdb()->find_method("via.SceneView", "get_PrimaryCamera");
    void* sm = API::get()->get_native_singleton("via.SceneManager");
    if (main_view == nullptr || primary == nullptr || sm == nullptr) return nullptr;
    auto* ctx = API::get()->get_vm_context();
    void* view = main_view->call<void*>(ctx, sm);
    if (view == nullptr) return nullptr;
    auto* cam = (MO*)primary->call<void*>(ctx, view);
    return cam != nullptr ? call_ptr(call_ptr(cam, "get_GameObject"), "get_Transform") : nullptr;
}

void set_pos(MO* tf, const Vec3& p) {
    auto* m = tf ? find_method_deep(tf->get_type_definition(), "set_Position") : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)tf, (void*)&p);
}
} // namespace

void camera_point(bool last_point) {
    if (!bridge::live()) { g_cam.has_last = false; return; }
    auto* tf = camera_transform();
    if (tf == nullptr) {
        if (!g_cam.where_logged) { g_cam.where_logged = true; LOGW("%s menu: camera transform not found, the menu camera hold is off", TAG); }
        return;
    }
    Vec3 p;
    if (!call_vec3(tf, "get_Position", p)) return;
    if (!g_is_hidden) {
        if (last_point) { g_cam.last = p; g_cam.has_last = true; }
        if (g_cam.pinned) {
            LOGI("%s menu: camera hold off (it had been moved up to %.3f m from where it was)", TAG, g_cam.max_moved);
            g_cam.pinned = false;
        }
        return;
    }
    if (!g_cam.pinned) {
        if (!g_cam.has_last) return;
        g_cam.pin = g_cam.last;
        g_cam.pinned = true;
        g_cam.logs = 0;
        g_cam.max_moved = 0.0f;
        LOGI("%s menu: camera held at %.3f %.3f %.3f", TAG, g_cam.pin.x, g_cam.pin.y, g_cam.pin.z);
    }
    const float moved = dist(p, g_cam.pin);
    if (moved > g_cam.max_moved) g_cam.max_moved = moved;
    if (moved > 0.001f) {
        set_pos(tf, g_cam.pin);
        Vec3 back;
        if (g_cam.logs < 6 && call_vec3(tf, "get_Position", back)) {
            ++g_cam.logs;
            LOGI("%s menu: camera had moved %.3f m (%s), put back; now %.3f m off", TAG, moved,
                 last_point ? "PrepareRendering" : "LockScene", dist(back, g_cam.pin));
        }
    }
}

void frame() {
    const bool want = bridge::live() && menu_open();
    if (want && !g_is_hidden) hide();
    else if (want) reassert();
    else if (g_is_hidden) restore();
}

} // namespace vn::menu_body
