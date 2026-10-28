// shadowhead.cpp -- a shadow-only stand-in for the hidden head (2026-10-28, Fable; Tefa: "have the neck present as it
// is in VR by default and also have the head joint removed, but a shadow visible").
//
// The two known ways to hide the head each lose something (dossier 7): scaling the "head" joint to zero (REFramework's
// Hide Joint Mesh, what we run) keeps the neck but takes the head out of EVERY pass, shadow included; hiding the face
// mesh by its draw flags keeps the shadow but takes the neck with it (the neck tube is inside the face mesh's one
// part, pl1050_Face_Mat, y 1.40..1.71 in the file; it has no part of its own to switch [measured 2026-10-28]).
// So: keep the joint route, and add the shadow back from a SECOND copy of the game's own face mesh, drawn in the
// shadow pass only (DrawDefault off, DrawShadowCast on), on its own GameObject with no joints, placed each frame so its
// bind-pose head sits on the live head joint. No new geometry, no Blender.
//
// Open questions this answers live: (1) does a copy of the game's own face mesh, with the game's own pl1050.mdf2,
// get its materials (the old plugin's spawned meshes never did with OUR mdf: dossier 7k, "bound by name" hypothesis)?
// (2) does a skinned mesh on a joint-less GameObject draw at all, and where (bind pose)? (3) does it cast a shadow?
//
// Modes (NUM8 cycles, NUM9 re-reads the mode file; reframework/data/visceral_shadowhead.txt holds the start mode):
//   0 off   1 VISIBLE copy (to see it)   2 SHADOW ONLY (the goal)   3 VISIBLE copy held 1 m in front, 0.3 m down (debug)
// Worn 2026-10-28 (Tefa): the shadow head shows, but "the head is carved out from the lower neck and is just too high up".
// reframework/data/visceral_shadowhead_offset.txt ("<down cm> <forward cm>") moves the copies, re-read whenever the file
// changes, so it can be tuned live (10 cm down looked right-ish the same night).
// b190, the same evening: the strand in the left eye was the hair mesh (hidden live: "yes it's gone!"); the hair casts
// no shadow of its own with the head joint hidden (shadow the same with it on or off, Tefa). Tefa: "is there any way to
// hide hair but keep the shadow like you managed to do with the head", and the hair must stay in third person. So the
// hair gets the head's treatment: a second shadow-only copy (the hair's own mesh and material holders), the real hair
// hidden from view during play, and in cutscenes (camera not the PLAYER camera) the real hair comes back and both
// copies stop casting, so the shadow is not doubled. Leon, Claire and Ada alike: both copies take the holders of
// whatever face and hair the costume changer has now.
// b191: Claire's first run (Tefa: "her head shadow was not there") showed two things in the log: in a cutscene the
// game hid her hair again every frame and we showed it again every frame (now: shown ONCE when a cutscene starts, then
// left to the game), and nothing to say where her copy went. The mode file is now re-read whenever it changes too, so
// mode 3 (the copy visible, held out in front) can be switched on from the desk while she plays.
// [compile-verified] at most until it has run.
#include "shadowhead.h"
#include "common.h"
#include "joints.h"
#include <windows.h>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>

using namespace vn;
using namespace vn::joints;

namespace {

constexpr const char* FACE_MESH = "SectionRoot/Character/Player/pl1000/pl1050/pl1050.mesh";
constexpr const char* FACE_MDF  = "SectionRoot/Character/Player/pl1000/pl1050/pl1050.mdf2";
constexpr const char* MODE_FILE = "reframework/data/visceral_shadowhead.txt";
constexpr const char* OFFSET_FILE = "reframework/data/visceral_shadowhead_offset.txt";
constexpr const char* CHANGER = "app.ropeway.survivor.SurvivorCostumeChanger";
constexpr float OFFSET_CHECK_S = 0.5f;   // how often the offset file's date is looked at
constexpr bool HIDE_HAIR = true;         // the player's real hair out of the normal view during play
constexpr int CAMERA_PLAYER = 0;         // CameraSystem.BusyCameraType: the player camera (FirstPerson drives)
// The face mesh's "head" bone in its bind pose (pl1050.mesh skeleton, read 2026-10-28): position (0, 1.564, 0.018),
// rotation a 5.6 degree pitch about x, ignored here (a shadow does not show it). The hair mesh is built on the same
// skeleton (its joints: root, neck_1, head, hair..), so the same bind head is taken for it [hypothesis].
constexpr Vec3 BIND_HEAD{0.0f, 1.5640067f, 0.0178933f};
constexpr float REPORT_S = 5.0f;
constexpr Vec3 DEBUG_OFFSET{0.0f, -0.3f, 1.0f};   // mode 3: along the head's own axes

// One shadow copy: its own GameObject with a via.render.Mesh, given the holders of a mesh the game has loaded.
struct Copy {
    const char* what{};          // "head" / "hair", for the log
    const char* getter{};        // the costume changer's getter of the real mesh
    const wchar_t* go_name{};
    MO* go{};
    MO* tf{};
    MO* mesh{};
    MO* source{};                // the real mesh whose holders this copy carries (a costume change swaps it)
    bool created{false}, tried{false}, parented{false};
    float created_t{0.0f};
    int mat_reports{0};
};

struct State {
    Copy head{"head", "get_Face", L"visceral_shadowhead"};
    Copy hair{"hair", "get_Hair", L"visceral_shadowhair"};
    int mode{2}, mode_sent{-1};   // 0 off, 1 visible copy, 2 shadow only, 3 visible copy held out in front (debug)
    bool cutscene{false}, cutscene_sent{false};
    int hair_shown{-1};           // what we last did to the real hair: -1 nothing yet, 0 hidden, 1 shown
    float last_report_t{0.0f};
    bool key8{false}, key9{false};
    MO* player{};
} g;

// The tuning, outside State so recreated copies keep it.
struct Offset {
    float down_m{0.0f}, forward_m{0.0f};   // world down; forward along the body's facing, flat
    std::filesystem::file_time_type stamp{};
    bool seen{false};
    float last_check_t{-1.0f};
} off;

struct ModeFile {   // the mode file is watched like the offset file, so the mode can be changed from outside
    std::filesystem::file_time_type stamp{};
    bool seen{false};
    float last_check_t{-1.0f};
} mode_file;

void reset_keep_mode() { const int m = g.mode; g = State{}; g.mode = m; }

int read_mode_file() {
    std::ifstream f(MODE_FILE);
    int m = -1;
    if (f && (f >> m) && m >= 0 && m <= 3) return m;
    return -1;
}

void watch_mode_file(float t) {
    if (t - mode_file.last_check_t < OFFSET_CHECK_S) return;
    mode_file.last_check_t = t;
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(MODE_FILE, ec);
    if (ec || (mode_file.seen && stamp == mode_file.stamp)) return;
    const bool first_look = !mode_file.seen;
    mode_file.stamp = stamp; mode_file.seen = true;
    if (first_look) return;   // the start mode is read once at the first frame
    const int m = read_mode_file();
    if (m >= 0 && m != g.mode) { g.mode = m; LOGI("%s shadowhead: mode %d from the changed file", TAG, m); }
}

void read_offset_file(float t) {
    if (t - off.last_check_t < OFFSET_CHECK_S) return;
    off.last_check_t = t;
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(OFFSET_FILE, ec);
    if (ec || (off.seen && stamp == off.stamp)) return;
    off.stamp = stamp; off.seen = true;
    std::ifstream f(OFFSET_FILE);
    float down_cm = 0.0f, fwd_cm = 0.0f;
    if (!(f >> down_cm)) return;
    if (!(f >> fwd_cm)) fwd_cm = 0.0f;
    off.down_m = down_cm / 100.0f; off.forward_m = fwd_cm / 100.0f;
    LOGI("%s shadowhead: offset from the file: %.1f cm down, %.1f cm forward", TAG, down_cm, fwd_cm);
}

MO* resource_holder(const char* res_type, const char* path, const char* holder_type) {
    auto& api = API::get();
    auto* res = api->resource_manager()->create_resource(res_type, path);
    if (res == nullptr) { LOGW("%s shadowhead: create_resource(%s, %s) returned null", TAG, res_type, path); return nullptr; }
    // The old plugin's proven route: a holder allocated without its constructor, the resource written into its slot.
    auto* ht = api->tdb()->find_type(holder_type);
    auto* h = ht != nullptr ? ht->create_instance(1) : nullptr;
    if (h == nullptr) { LOGW("%s shadowhead: create_instance(%s) failed", TAG, holder_type); return nullptr; }
    h->add_ref();
    res->add_ref();
    *(void**)((uintptr_t)h + 0x10) = (void*)res;
    return h;
}

// The player's real face or hair mesh, from the game's costume changer (get_Face / get_Hair: live 2026-10-28 the
// player's transform children are only weapons, so a child walk finds nothing).
MO* real_mesh(MO* player, const char* getter) { return call_ptr(component(player, CHANGER), getter); }

int material_num(MO* mesh) { return mesh ? call_direct<int>(mesh, "get_MaterialNum", -1) : -1; }
void set_bool(MO* o, const char* name, bool v) {
    auto* m = o ? find_method_deep(o->get_type_definition(), name) : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)o, v);
}

// A cutscene (or any camera that is not the player's): REFramework shows the real head then, so the copies stand down.
bool in_cutscene() {
    const int t = call_direct<int>(API::get()->get_managed_singleton("app.ropeway.camera.CameraSystem"), "get_BusyCameraType", -1);
    return t >= 0 && t != CAMERA_PLAYER;
}

// The real hair: hidden from view during play, held hidden every frame (a costume change brings a fresh hair mesh
// that draws); shown ONCE as a cutscene starts and then left to the game, which hides it itself in some cutscenes.
void real_hair(MO* player) {
    if (!HIDE_HAIR) return;
    auto* hair = real_mesh(player, "get_Hair");
    if (hair == nullptr) return;
    if (g.cutscene) {
        if (g.hair_shown == 1) return;
        set_bool(hair, "set_DrawDefault", true);
        g.hair_shown = 1;
        LOGI("%s shadowhead: the player's hair %p shown (a cutscene starts; the game has it from here)", TAG, (void*)hair);
        return;
    }
    if (!call_direct<bool>(hair, "get_DrawDefault", true)) { g.hair_shown = 0; return; }
    set_bool(hair, "set_DrawDefault", false);
    if (g.hair_shown != 0) LOGI("%s shadowhead: the player's hair %p hidden from view (play)", TAG, (void*)hair);
    g.hair_shown = 0;
}

void apply_mode(Copy& c, int mode) {
    if (c.mesh == nullptr) return;
    const bool draw = mode == 1 || mode == 3, shadow = mode != 0;
    set_bool(c.mesh, "set_DrawDefault", draw);
    set_bool(c.mesh, "set_DrawShadowCast", shadow);
    LOGI("%s shadowhead: %s copy mode %d (%s): DrawDefault=%d DrawShadowCast=%d materials=%d", TAG, c.what, mode,
         mode == 0 ? "off" : mode == 1 ? "VISIBLE copy" : mode == 2 ? "SHADOW ONLY" : "VISIBLE, held out in front",
         (int)call_direct<bool>(c.mesh, "get_DrawDefault", false), (int)call_direct<bool>(c.mesh, "get_DrawShadowCast", false),
         material_num(c.mesh));
}

bool create(Copy& c, MO* player, bool with_file_route) {
    c.tried = true;
    auto& api = API::get();
    auto* go_t = api->tdb()->find_type("via.GameObject");
    auto* create = go_t != nullptr ? go_t->find_method("create(System.String)") : nullptr;
    if (create == nullptr) { LOGE("%s shadowhead: via.GameObject.create(System.String) not found", TAG); return false; }
    auto* name = api->create_managed_string(c.go_name);
    auto r = create->invoke(nullptr, {name});
    if (r.exception_thrown || r.ptr == nullptr) { LOGE("%s shadowhead: GameObject.create (%s) failed", TAG, c.what); return false; }
    c.go = (MO*)r.ptr;
    c.go->add_ref();
    c.tf = call_ptr(c.go, "get_Transform");
    auto* mesh_t = api->typeof("via.render.Mesh");
    auto* add = find_method_deep(c.go->get_type_definition(), "createComponent(System.Type)");
    if (mesh_t == nullptr || add == nullptr) { LOGE("%s shadowhead: createComponent / typeof(via.render.Mesh) missing", TAG); return false; }
    auto cr = add->invoke(c.go, {mesh_t});
    if (cr.exception_thrown || cr.ptr == nullptr) { LOGE("%s shadowhead: createComponent(via.render.Mesh) failed (%s)", TAG, c.what); return false; }
    c.mesh = (MO*)cr.ptr;
    c.mesh->add_ref();
    if (with_file_route) {   // route A, the head only: Leon's face from its file, until the game's own holders arrive
        auto* mesh_h = resource_holder("via.render.MeshResource", FACE_MESH, "via.render.MeshResourceHolder");
        auto* mdf_h = resource_holder("via.render.MeshMaterialResource", FACE_MDF, "via.render.MeshMaterialResourceHolder");
        auto* set_mesh = find_method_deep(c.mesh->get_type_definition(), "setMesh");
        auto* set_mat = find_method_deep(c.mesh->get_type_definition(), "set_Material");
        if (mesh_h != nullptr && set_mesh != nullptr && set_mesh->invoke(c.mesh, {mesh_h}).exception_thrown) LOGE("%s shadowhead: setMesh threw", TAG);
        if (mdf_h != nullptr && set_mat != nullptr) {
            if (set_mat->invoke(c.mesh, {mdf_h}).exception_thrown) LOGW("%s shadowhead: set_Material(fresh holder) threw", TAG);
            LOGI("%s shadowhead: route A (fresh holder of %s): materials=%d", TAG, FACE_MDF, material_num(c.mesh));
        }
    }
    auto* ptf = call_ptr(player, "get_Transform");
    if (ptf != nullptr) {
        auto* set_parent = find_method_deep(c.tf->get_type_definition(), "set_Parent");
        c.parented = set_parent != nullptr && !set_parent->invoke(c.tf, {ptf}).exception_thrown;
    }
    c.created = true;
    c.created_t = now_s();
    c.mat_reports = 0;
    LOGI("%s shadowhead: CREATED the %s copy go=%p mesh=%p, parented to the player: %d, materials now %d",
         TAG, c.what, (void*)c.go, (void*)c.mesh, (int)c.parented, material_num(c.mesh));
    return true;
}

// Route B: the real mesh's own mesh + material holders (already loaded and bound) onto the copy; again after a
// costume change, when the costume changer hands out a different mesh.
void hand_over(Copy& c, MO* player) {
    auto* src = real_mesh(player, c.getter);
    if (src == nullptr || src == c.source) return;
    auto* mesh_holder = call_ptr(src, "getMesh");
    auto* mat_holder = call_ptr(src, "get_Material");
    auto* set_mesh = find_method_deep(c.mesh->get_type_definition(), "setMesh");
    auto* set_mat = find_method_deep(c.mesh->get_type_definition(), "set_Material");
    if (mesh_holder == nullptr || set_mesh == nullptr) return;
    if (set_mesh->invoke(c.mesh, {mesh_holder}).exception_thrown) LOGW("%s shadowhead: setMesh(%s's holder) threw", TAG, c.what);
    if (mat_holder != nullptr && set_mat != nullptr && set_mat->invoke(c.mesh, {mat_holder}).exception_thrown) LOGW("%s shadowhead: set_Material(%s's holder) threw", TAG, c.what);
    c.source = src;
    c.created_t = now_s(); c.mat_reports = 0;   // report readiness again from here
    g.mode_sent = -1;
    LOGI("%s shadowhead: route B: the real %s %p handed its holders over (mesh %p, material %p): materials=%d", TAG, c.what,
         (void*)src, (void*)mesh_holder, (void*)mat_holder, material_num(c.mesh));
}

void place(Copy& c, MO* player) {
    if (c.tf == nullptr) return;
    auto* ptf = call_ptr(player, "get_Transform");
    auto* head = joint_by_name(ptf, L"head");
    Vec3 hp{}; Quat hq{};
    if (head == nullptr || !get_v3(JOINT, head, "get_Position", hp) || !get_q(JOINT, head, "get_Rotation", hq)) return;
    // T = live head frame x inverse(bind head frame), with the bind rotation taken as identity: the copy's bind-pose
    // head then sits on the live head joint, turning with it.
    const Vec3 dbg = g.mode == 3 ? DEBUG_OFFSET : Vec3{0, 0, 0};
    Vec3 pos = hp - rotate(hq, BIND_HEAD) + rotate(hq, dbg);
    pos.y -= off.down_m;
    if (off.forward_m != 0.0f) {   // along the body's facing, kept flat
        Quat pq{};
        if (get_q(joints::XFORM, ptf, "get_Rotation", pq)) {
            Vec3 f = rotate(pq, Vec3{0, 0, 1});
            const float len = std::sqrt(f.x * f.x + f.z * f.z);
            if (len > 1e-4f) { pos.x += f.x / len * off.forward_m; pos.z += f.z / len * off.forward_m; }
        }
    }
    struct V4 { float x, y, z, w; };
    const V4 p{pos.x, pos.y, pos.z, 0.0f}, q{hq.x, hq.y, hq.z, hq.w};
    set_any(joints::XFORM, c.tf, "set_Position", &p);
    set_any(joints::XFORM, c.tf, "set_Rotation", &q);
}

void report_ready(Copy& c, float t) {
    if (c.mesh == nullptr || c.mat_reports >= 4 || t - c.created_t < 0.5f * (c.mat_reports + 1)) return;
    ++c.mat_reports;
    LOGI("%s shadowhead: %s copy %.1f s after creation: materials=%d DrawDefault=%d DrawShadowCast=%d MeshReady=%d MaterialReady=%d ReadyToDraw=%d HasJoint=%d Skinning=%d Viewable=%d",
         TAG, c.what, t - c.created_t, material_num(c.mesh), (int)call_direct<bool>(c.mesh, "get_DrawDefault", false),
         (int)call_direct<bool>(c.mesh, "get_DrawShadowCast", false), (int)call_direct<bool>(c.mesh, "get_MeshReady", false),
         (int)call_direct<bool>(c.mesh, "get_MaterialReady", false), (int)call_direct<bool>(c.mesh, "get_ReadyToDraw", false),
         (int)call_direct<bool>(c.mesh, "get_HasJoint", false), (int)call_direct<bool>(c.mesh, "get_SkinningMesh", false),
         (int)call_direct<bool>(c.mesh, "get_Viewable", false));
}

void keys() {
    const bool k8 = (GetAsyncKeyState(VK_NUMPAD8) & 0x8000) != 0, k9 = (GetAsyncKeyState(VK_NUMPAD9) & 0x8000) != 0;
    if (k8 && !g.key8) { g.mode = (g.mode + 1) % 4; LOGI("%s shadowhead: NUM8 -> mode %d", TAG, g.mode); }
    if (k9 && !g.key9) { const int m = read_mode_file(); if (m >= 0) { g.mode = m; LOGI("%s shadowhead: NUM9 -> mode %d from the file", TAG, m); } }
    g.key8 = k8; g.key9 = k9;
}

}  // namespace

namespace vn::shadowhead {

void frame() {
    static bool first = true;
    if (first) { first = false; const int m = read_mode_file(); if (m >= 0) g.mode = m; LOGI("%s shadowhead: start mode %d (file %s)", TAG, g.mode, MODE_FILE); }
    keys();
    read_offset_file(now_s());
    watch_mode_file(now_s());
    auto* player = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    if (player == nullptr) { if (g.head.created) { LOGI("%s shadowhead: player gone, the copies are dropped", TAG); } reset_keep_mode(); return; }
    for (Copy* c : {&g.head, &g.hair})
        if (c->created && (!is_managed(c->go) || !is_managed(c->mesh))) { LOGW("%s shadowhead: the %s copy is no longer a managed object, recreating", TAG, c->what); reset_keep_mode(); break; }
    if (player != g.player) { if (g.head.created) { LOGI("%s shadowhead: a new player object, recreating", TAG); reset_keep_mode(); } g.player = player; }
    if (!g.head.created) { if (g.head.tried) return; if (!create(g.head, player, true)) return; }
    if (!g.hair.created && !g.hair.tried && real_mesh(player, "get_Hair") != nullptr) create(g.hair, player, false);
    for (Copy* c : {&g.head, &g.hair}) if (c->created) hand_over(*c, player);
    g.cutscene = in_cutscene();
    real_hair(player);
    for (Copy* c : {&g.head, &g.hair}) if (c->created) place(*c, player);
    if (g.mode_sent != g.mode || g.cutscene_sent != g.cutscene) {
        const int m = g.cutscene ? 0 : g.mode;   // in cutscenes the real head and hair cast their own shadow
        for (Copy* c : {&g.head, &g.hair}) apply_mode(*c, m);
        g.mode_sent = g.mode; g.cutscene_sent = g.cutscene;
    }
    const float t = now_s();
    for (Copy* c : {&g.head, &g.hair}) report_ready(*c, t);
    if (t - g.last_report_t >= REPORT_S) {
        g.last_report_t = t;
        Vec3 p{}, hp{}; get_v3(joints::XFORM, g.head.tf, "get_Position", p);
        auto* head = joint_by_name(call_ptr(player, "get_Transform"), L"head");
        if (head != nullptr) get_v3(JOINT, head, "get_Position", hp);
        LOGI("%s shadowhead: mode %d%s, head copy at (%.2f %.2f %.2f), live head joint (%.2f %.2f %.2f), materials head=%d hair=%d, ShadowCast head=%d hair=%d",
             TAG, g.mode, g.cutscene ? " (cutscene: copies off)" : "", p.x, p.y, p.z, hp.x, hp.y, hp.z,
             material_num(g.head.mesh), material_num(g.hair.mesh),
             (int)call_direct<bool>(g.head.mesh, "get_DrawShadowCast", false), (int)call_direct<bool>(g.hair.mesh, "get_DrawShadowCast", false));
    }
}

}  // namespace vn::shadowhead
