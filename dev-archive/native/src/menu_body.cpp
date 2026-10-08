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
// Position AND turn are held: the probe (b094, 2026-10-06) showed the position move while the menu is open, and the
// TURN swing 10-20 degrees for two or three frames right after it closes (the flicker on closing).
struct Cam {
    Vec3 last{}; Quat last_rot{0, 0, 0, 1}; bool has_last = false;   // the camera at the end of the last no-menu frame
    Vec3 pin{}; Quat pin_rot{0, 0, 0, 1}; bool pinned = false;         // where it is held while a menu is open
    int logs = 0; float max_moved = 0.0f, max_turned = 0.0f; bool where_logged = false;
    int release_frames = 0;                      // frames held on after the menu closed
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

void set_rot(MO* tf, const Quat& q) {
    auto* m = tf ? find_method_deep(tf->get_type_definition(), "set_Rotation") : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)tf, (void*)&q);
}

float turn_between(const Quat& a, const Quat& b) {        // degrees
    const float d = std::fabs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
    return 2.0f * std::acos(std::fmin(1.0f, d)) * 57.29578f;
}
} // namespace

void early_hide() {
    // the frame order in this game is UpdateBehavior -> LateUpdateBehavior -> PrepareRendering -> LockScene: the menu
    // first reads open at LateUpdateBehavior POST, so the body is hidden right there, before the frame is prepared
    // (the probe, b094: hiding at LockScene was after PrepareRendering, and that frame showed the inside of the head)
    if (bridge::live() && !g_is_hidden && menu_open()) hide();
}

void camera_point(bool last_point) {
    if (!bridge::live()) { g_cam.has_last = false; return; }
    auto* tf = camera_transform();
    if (tf == nullptr) {
        if (!g_cam.where_logged) { g_cam.where_logged = true; LOGW("%s menu: camera transform not found, the menu camera hold is off", TAG); }
        return;
    }
    if (!last_point && !g_is_hidden && menu_open()) hide();
    Vec3 p;
    Quat q;
    if (!call_vec3(tf, "get_Position", p) || !call_quat(tf, "get_Rotation", q)) return;
    if (!g_is_hidden) {
        // after the menu closes the game's camera takes a few frames to settle (the turn swings, then comes back):
        // keep holding until it is back near the held spot and turn, or for MENU_CAM_RELEASE_FRAMES
        if (g_cam.pinned) {
            const float off = dist(p, g_cam.pin), turned = turn_between(q, g_cam.pin_rot);
            if ((off > cfg::MENU_CAM_BACK_M || turned > cfg::MENU_CAM_BACK_DEG) && g_cam.release_frames < cfg::MENU_CAM_RELEASE_FRAMES) {
                if (last_point) ++g_cam.release_frames;
                set_pos(tf, g_cam.pin);
                set_rot(tf, g_cam.pin_rot);
                return;
            }
            LOGI("%s menu: camera hold off after %d frames (moved up to %.3f m, turned up to %.1f deg; now %.3f m, %.1f deg off)",
                 TAG, g_cam.release_frames, g_cam.max_moved, g_cam.max_turned, off, turned);
            g_cam.pinned = false;
        }
        if (last_point) { g_cam.last = p; g_cam.last_rot = q; g_cam.has_last = true; }
        return;
    }
    g_cam.release_frames = 0;
    if (!g_cam.pinned) {
        if (!g_cam.has_last) return;
        g_cam.pin = g_cam.last;
        g_cam.pin_rot = g_cam.last_rot;
        g_cam.pinned = true;
        g_cam.logs = 0;
        g_cam.max_moved = g_cam.max_turned = 0.0f;
        LOGI("%s menu: camera held at %.3f %.3f %.3f", TAG, g_cam.pin.x, g_cam.pin.y, g_cam.pin.z);
    }
    const float moved = dist(p, g_cam.pin), turned = turn_between(q, g_cam.pin_rot);
    if (moved > g_cam.max_moved) g_cam.max_moved = moved;
    if (turned > g_cam.max_turned) g_cam.max_turned = turned;
    if (moved > 0.001f || turned > 0.05f) {
        set_pos(tf, g_cam.pin);
        set_rot(tf, g_cam.pin_rot);
        if (g_cam.logs < 6) {
            ++g_cam.logs;
            LOGI("%s menu: camera had moved %.3f m and turned %.1f deg (%s), put back", TAG, moved, turned,
                 last_point ? "PrepareRendering" : "LockScene");
        }
    }
}

bool is_menu_open() { return menu_open(); }
bool probe_menu_open() { return menu_open(); }
bool probe_body_hidden() { return g_is_hidden; }
void* probe_camera_tf() { return camera_transform(); }

void frame() {
    const bool want = bridge::live() && menu_open();
    if (want && !g_is_hidden) hide();
    else if (want) reassert();
    else if (g_is_hidden) restore();
}

} // namespace vn::menu_body
