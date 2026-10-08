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
//
// THE CAMERA IN MENUS -- read from REFramework's own source on 2026-10-08 (src/mods/VR.cpp update_camera,
// update_camera_origin, apply_hmd_transform, restore_camera; src/mods/FirstPerson.cpp update_camera_transform,
// is_first_person_allowed) [inferred-static 2026-10-08]:
//   * While FirstPerson is "used" (not paused, player camera) IT writes the camera every frame: body view x raw
//     headset turn, and recenters the VR layer's rotation offset to the headset's yaw. The VR layer then leaves the
//     camera alone (it only remembers it).
//   * In the inventory and pause menus (GUI state PAUSE / INVENTORY) FirstPerson steps aside, and the VR layer takes
//     over: at BeginRendering it takes the camera AS IT FINDS IT as the base, multiplies (rotation offset x raw
//     headset turn) on top, adds the headset's offset from the standing origin, renders, and puts the base back at
//     EndRendering.
// So a held camera that already carries the headset turn gets it twice. With the offset = the yaw at the last
// recenter, what doubles is the head's pitch and roll: the slight left-down tilt on opening that b095/b096 could not
// hold away, and the swing after closing is the same doubling during the game's camera switch back. The fix: while
// FirstPerson is not driving, write the camera STRIPPED of what the VR layer will add back:
//   rot = pin x inv(H0) x inv(R_off)          H0 = the raw headset turn FirstPerson folded in at the pin frame
//   pos = pin_pos - rot x (R_off x (P - origin))   R_off, P, origin read fresh from the bridge each frame
// Then the VR layer renders pin x inv(H0) x H: the pre-menu view, turning with the head by exactly what it has
// turned since. While FirstPerson drives, the camera is its own and the old hold (full pin) is kept as before.
struct Cam {
    Vec3 last{}; Quat last_rot{0, 0, 0, 1}; Quat last_hmd{0, 0, 0, 1}; bool has_last = false;   // the camera at the end of the last FirstPerson frame
    Vec3 pin{}; Quat pin_rot{0, 0, 0, 1}; Quat pin_hmd{0, 0, 0, 1}; bool pinned = false;        // where it is held while a menu is open
    int logs = 0; float max_moved = 0.0f, max_turned = 0.0f; bool where_logged = false;
    int release_frames = 0;                      // frames held on after the menu closed
    int stripped = 0, plain = 0;                 // writes of each kind during this hold (for the log)
    int settled = 0;                             // frames in a row FirstPerson's camera sat at the held view after the close
    float max_swing = 0.0f;                      // the biggest turn FirstPerson's camera made away from the held view after the close
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

int gui_state() {
    auto* gm = API::get()->get_managed_singleton("app.ropeway.gui.GUIMaster");
    auto* p = field_ptr<int32_t>(gm, "<State_>k__BackingField");
    return p != nullptr ? *p : -1;
}

bool fp_driving() { return bridge::view(bridge::S_FP_USED) > 0.5f; }

// the view to write: while FirstPerson drives, the pre-menu base with THIS frame's headset turn (pin x inv(H0) x H), so
// the head keeps tracking; while the VR layer drives, the same stripped of what it will add itself
void target(bool fp, Quat& rot, Vec3& pos, bool& stripped) {
    rot = g_cam.pin_rot;
    pos = g_cam.pin;
    stripped = false;
    const Quat h = bridge::view_quat(bridge::S_HMD_Q);
    if (fp && bridge::has(h.x)) { rot = qnorm(qmul(qmul(g_cam.pin_rot, qinv(g_cam.pin_hmd)), h)); return; }
    if (!fp) {
        const Quat roff = bridge::view_quat(bridge::S_ROT_OFF);
        const Vec3 origin = bridge::view_vec3(bridge::S_ORIGIN);
        const Vec3 hp = bridge::view_vec3(bridge::S_HMD_POS);
        if (bridge::has(roff.x) && bridge::has(origin.x) && bridge::has(hp.x)) {
            rot = qnorm(qmul(qmul(g_cam.pin_rot, qinv(g_cam.pin_hmd)), qinv(roff)));
            pos = g_cam.pin - rotate(rot, rotate(roff, hp - origin));
            stripped = true;
        }
    }
}

void write_pin(MO* tf, bool fp) {
    Quat rot; Vec3 pos; bool stripped;
    target(fp, rot, pos, stripped);
    set_pos(tf, pos);
    set_rot(tf, rot);
    if (stripped) ++g_cam.stripped; else ++g_cam.plain;
}
} // namespace

void early_hide() {
    // the frame order in this game is UpdateBehavior -> LateUpdateBehavior -> PrepareRendering -> LockScene: the menu
    // first reads open at LateUpdateBehavior POST, so the body is hidden right there, before the frame is prepared
    // (the probe, b094: hiding at LockScene was after PrepareRendering, and that frame showed the inside of the head)
    if (bridge::live() && !g_is_hidden && menu_open()) hide();
}

void camera_point(bool last_point) {
    if (!bridge::live()) { g_cam.has_last = false; g_cam.pinned = false; return; }
    auto* tf = camera_transform();
    if (tf == nullptr) {
        if (!g_cam.where_logged) { g_cam.where_logged = true; LOGW("%s menu: camera transform not found, the menu camera hold is off", TAG); }
        return;
    }
    if (!last_point && !g_is_hidden && menu_open()) hide();
    Vec3 p;
    Quat q;
    if (!call_vec3(tf, "get_Position", p) || !call_quat(tf, "get_Rotation", q)) return;
    const bool fp = fp_driving();
    if (!g_is_hidden) {
        if (g_cam.pinned) {
            // after the menu closes: FirstPerson's first frames take the MENU camera controller's rotation as their base
            // (its on_update_camera_controller recorded it while the menu was open), so its camera sits right for one
            // frame and then swings 10-20 deg for a frame or two before coming back (the probe with b100: f1041 fine,
            // f1042 turned 20 deg). b092-b100 released the hold the moment the camera first looked right: one frame too
            // early, and that was the flicker on closing. Now the hold goes on until FirstPerson's camera has sat at the
            // held view MENU_CAM_SETTLE_FRAMES frames in a row (judged at LockScene PRE, after its write), or for
            // MENU_CAM_RELEASE_FRAMES at most. The written view keeps tracking the head (see target()).
            Quat tgt_rot; Vec3 tgt_pos; bool stripped;
            target(fp, tgt_rot, tgt_pos, stripped);
            const float off = dist(p, tgt_pos), turned = turn_between(q, tgt_rot);
            const bool near = fp && off <= cfg::MENU_CAM_BACK_M && turned <= cfg::MENU_CAM_BACK_DEG;
            if (!last_point) {
                g_cam.settled = near ? g_cam.settled + 1 : 0;
                ++g_cam.release_frames;
                if (fp && turned > g_cam.max_swing) g_cam.max_swing = turned;
            }
            if (g_cam.settled < cfg::MENU_CAM_SETTLE_FRAMES && g_cam.release_frames < cfg::MENU_CAM_RELEASE_FRAMES) {
                write_pin(tf, fp);
                return;
            }
            LOGI("%s menu: camera hold off after %d frames (FirstPerson's camera swung up to %.1f deg from the held view after the close, settled %d frames; menu moved it up to %.3f m / %.1f deg; %d stripped + %d plain writes; FirstPerson driving %d, gui %d)",
                 TAG, g_cam.release_frames, g_cam.max_swing, g_cam.settled, g_cam.max_moved, g_cam.max_turned, g_cam.stripped, g_cam.plain, (int)fp, gui_state());
            g_cam.pinned = false;
        }
        // the spot to hold is FirstPerson's own camera, with the headset turn it folded in this frame
        const Quat h = bridge::view_quat(bridge::S_HMD_Q);
        if (fp && bridge::has(h.x)) { g_cam.last = p; g_cam.last_rot = q; g_cam.last_hmd = h; g_cam.has_last = true; }
        return;
    }
    g_cam.release_frames = 0;
    if (!g_cam.pinned) {
        if (!g_cam.has_last) return;
        g_cam.pin = g_cam.last;
        g_cam.pin_rot = g_cam.last_rot;
        g_cam.pin_hmd = g_cam.last_hmd;
        g_cam.pinned = true;
        g_cam.logs = 0;
        g_cam.max_moved = g_cam.max_turned = 0.0f;
        g_cam.stripped = g_cam.plain = 0;
        g_cam.settled = 0;
        g_cam.max_swing = 0.0f;
        LOGI("%s menu: camera held at %.3f %.3f %.3f (FirstPerson driving %d, gui %d)", TAG, g_cam.pin.x, g_cam.pin.y, g_cam.pin.z, (int)fp, gui_state());
    }
    const float moved = dist(p, g_cam.pin), turned = turn_between(q, g_cam.pin_rot);
    if (moved > g_cam.max_moved) g_cam.max_moved = moved;
    if (turned > g_cam.max_turned) g_cam.max_turned = turned;
    write_pin(tf, fp);
    if (g_cam.logs < 6) {
        ++g_cam.logs;
        LOGI("%s menu: camera had moved %.3f m and turned %.1f deg (%s), written %s", TAG, moved, turned,
             last_point ? "PrepareRendering" : "LockScene", fp ? "as the full pin (FirstPerson driving)" : "stripped of the headset turn");
    }
}

bool is_menu_open() { return menu_open(); }
bool probe_menu_open() { return menu_open(); }
bool probe_body_hidden() { return g_is_hidden; }
int probe_gui_state() { return gui_state(); }
void* probe_camera_tf() { return camera_transform(); }

void frame() {
    const bool want = bridge::live() && menu_open();
    if (want && !g_is_hidden) hide();
    else if (want) reassert();
    else if (g_is_hidden) restore();
}

} // namespace vn::menu_body
