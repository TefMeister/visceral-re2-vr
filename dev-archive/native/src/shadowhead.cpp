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
// reframework/data/visceral_shadowhead_offset.txt ("<down cm> <forward cm>") moves the copy, re-read whenever the file
// changes, so it can be tuned live. The same evening the strand in the left eye was Leon's hair mesh: HIDE_HAIR keeps it
// out of view, its shadow is not affected (the hair casts none with the head joint hidden; seen in the headset, on vs off).
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
constexpr float OFFSET_CHECK_S = 0.5f;   // how often the offset file's date is looked at
constexpr bool HIDE_HAIR = true;         // the player's hair out of the normal view (shadow casting left as it is)
// The face mesh's "head" bone in its bind pose (pl1050.mesh skeleton, read 2026-10-28): position (0, 1.564, 0.018),
// rotation a 5.6 degree pitch about x, ignored here (a shadow does not show it).
constexpr Vec3 BIND_HEAD{0.0f, 1.5640067f, 0.0178933f};
constexpr float REPORT_S = 5.0f;
constexpr Vec3 DEBUG_OFFSET{0.0f, -0.3f, 1.0f};   // mode 3: along the head's own axes

struct State {
    MO* go{};
    MO* tf{};
    MO* mesh{};
    bool created{false}, tried{false};
    int mode{2}, mode_sent{-1};   // 0 off, 1 visible copy, 2 shadow only, 3 visible copy held out in front (debug)
    bool parented{false};
    bool handed{false};   // route B done: the face's own mesh + material holders are on the copy
    float created_t{0.0f}, last_report_t{0.0f};
    int mat_reports{0};
    bool key8{false}, key9{false};
    MO* player{};
} g;

// The tuning, outside State so a recreated copy keeps it.
struct Offset {
    float down_m{0.0f}, forward_m{0.0f};   // world down; forward along the body's facing, flat
    std::filesystem::file_time_type stamp{};
    bool seen{false};
    float last_check_t{-1.0f};
} off;

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

// The player's own face mesh component, from the game's costume changer (get_Face: the same setter the old plugin
// hooked; live 2026-10-28 the player's transform children are only weapons, so a child walk finds nothing).
MO* find_face_mesh(MO* player) {
    auto* changer = component(player, "app.ropeway.survivor.SurvivorCostumeChanger");
    auto* face = call_ptr(changer, "get_Face");
    static bool said = false;
    if (face != nullptr) LOGI("%s shadowhead: the player's face mesh is %p (costume changer %p), materials=%d", TAG, (void*)face, (void*)changer, call_direct<int>(face, "get_MaterialNum", -1));
    else if (!said) { said = true; LOGW("%s shadowhead: no face mesh from the costume changer (%p) yet, waiting", TAG, (void*)changer); }
    return face;
}

int material_num(MO* mesh) { return mesh ? call_direct<int>(mesh, "get_MaterialNum", -1) : -1; }
void set_bool(MO* o, const char* name, bool v) {
    auto* m = o ? find_method_deep(o->get_type_definition(), name) : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)o, v);
}

// The real hair: hidden from the normal view, every frame (a costume change brings it back otherwise).
void hide_hair(MO* player) {
    if (!HIDE_HAIR) return;
    auto* hair = call_ptr(component(player, "app.ropeway.survivor.SurvivorCostumeChanger"), "get_Hair");
    if (hair == nullptr || !call_direct<bool>(hair, "get_DrawDefault", false)) return;
    set_bool(hair, "set_DrawDefault", false);
    LOGI("%s shadowhead: the player's hair %p hidden from view (shadow casting left on: %d)", TAG, (void*)hair,
         (int)call_direct<bool>(hair, "get_DrawShadowCast", false));
}

int read_mode_file() {
    std::ifstream f(MODE_FILE);
    int m = -1;
    if (f && (f >> m) && m >= 0 && m <= 3) return m;
    return -1;
}

void apply_mode() {
    if (g.mesh == nullptr) return;
    const bool draw = g.mode == 1 || g.mode == 3, shadow = g.mode != 0;
    set_bool(g.mesh, "set_DrawDefault", draw);
    set_bool(g.mesh, "set_DrawShadowCast", shadow);
    g.mode_sent = g.mode;
    LOGI("%s shadowhead: mode %d (%s): DrawDefault=%d DrawShadowCast=%d materials=%d", TAG, g.mode,
         g.mode == 0 ? "off" : g.mode == 1 ? "VISIBLE copy" : g.mode == 2 ? "SHADOW ONLY" : "VISIBLE, held out in front",
         (int)call_direct<bool>(g.mesh, "get_DrawDefault", false), (int)call_direct<bool>(g.mesh, "get_DrawShadowCast", false),
         material_num(g.mesh));
}

bool create(MO* player) {
    g.tried = true;
    auto& api = API::get();
    auto* go_t = api->tdb()->find_type("via.GameObject");
    auto* create = go_t != nullptr ? go_t->find_method("create(System.String)") : nullptr;
    if (create == nullptr) { LOGE("%s shadowhead: via.GameObject.create(System.String) not found", TAG); return false; }
    auto* name = api->create_managed_string(L"visceral_shadowhead");
    auto r = create->invoke(nullptr, {name});
    if (r.exception_thrown || r.ptr == nullptr) { LOGE("%s shadowhead: GameObject.create failed", TAG); return false; }
    g.go = (MO*)r.ptr;
    g.go->add_ref();
    g.tf = call_ptr(g.go, "get_Transform");
    auto* mesh_t = api->typeof("via.render.Mesh");
    auto* add = find_method_deep(g.go->get_type_definition(), "createComponent(System.Type)");
    if (mesh_t == nullptr || add == nullptr) { LOGE("%s shadowhead: createComponent / typeof(via.render.Mesh) missing", TAG); return false; }
    auto cr = add->invoke(g.go, {mesh_t});
    if (cr.exception_thrown || cr.ptr == nullptr) { LOGE("%s shadowhead: createComponent(via.render.Mesh) failed", TAG); return false; }
    g.mesh = (MO*)cr.ptr;
    g.mesh->add_ref();
    auto* mesh_h = resource_holder("via.render.MeshResource", FACE_MESH, "via.render.MeshResourceHolder");
    auto* mdf_h = resource_holder("via.render.MeshMaterialResource", FACE_MDF, "via.render.MeshMaterialResourceHolder");
    if (mesh_h == nullptr) return false;
    auto* set_mesh = find_method_deep(g.mesh->get_type_definition(), "setMesh");
    if (set_mesh == nullptr) { LOGE("%s shadowhead: via.render.Mesh.setMesh not found", TAG); return false; }
    if (set_mesh->invoke(g.mesh, {mesh_h}).exception_thrown) { LOGE("%s shadowhead: setMesh threw", TAG); return false; }
    auto* set_mat = find_method_deep(g.mesh->get_type_definition(), "set_Material");
    // Two material routes, logged apart: the game's own mdf through a fresh holder (route A), and, if the player's
    // face mesh is found, ITS material holder (route B: already loaded and bound to this very mesh).
    if (mdf_h != nullptr && set_mat != nullptr) {
        if (set_mat->invoke(g.mesh, {mdf_h}).exception_thrown) LOGW("%s shadowhead: set_Material(fresh holder) threw", TAG);
        LOGI("%s shadowhead: route A (fresh holder of %s): materials=%d", TAG, FACE_MDF, material_num(g.mesh));
    }
    auto* ptf = call_ptr(player, "get_Transform");
    if (ptf != nullptr) {
        auto* set_parent = find_method_deep(g.tf->get_type_definition(), "set_Parent");
        g.parented = set_parent != nullptr && !set_parent->invoke(g.tf, {ptf}).exception_thrown;
    }
    g.created = true;
    g.created_t = now_s();
    g.mat_reports = 0;
    LOGI("%s shadowhead: CREATED go=%p mesh=%p (the game's own %s + %s), parented to the player: %d, materials now %d",
         TAG, (void*)g.go, (void*)g.mesh, FACE_MESH, FACE_MDF, (int)g.parented, material_num(g.mesh));
    g.mode_sent = -1;
    return true;
}

void place(MO* player) {
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
    set_any(joints::XFORM, g.tf, "set_Position", &p);
    set_any(joints::XFORM, g.tf, "set_Rotation", &q);
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
    auto* player = call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer");
    if (player == nullptr) { if (g.created) { LOGI("%s shadowhead: player gone, the copy is dropped", TAG); } g = State{.mode = g.mode}; return; }
    if (g.created && (!is_managed(g.go) || !is_managed(g.mesh))) { LOGW("%s shadowhead: the copy is no longer a managed object, recreating", TAG); g = State{.mode = g.mode}; }
    if (player != g.player) { g.player = player; if (g.created) { LOGI("%s shadowhead: a new player object, recreating", TAG); g = State{.mode = g.mode, .player = player}; } }
    if (!g.created) { if (g.tried) return; if (!create(player)) return; }
    if (!g.handed) {   // route B, deferred: the game assigns the face mesh after our creation moment
        if (auto* face = find_face_mesh(player); face != nullptr) {
            auto* mesh_holder = call_ptr(face, "getMesh");
            auto* mat_holder = call_ptr(face, "get_Material");
            auto* set_mesh = find_method_deep(g.mesh->get_type_definition(), "setMesh");
            auto* set_mat = find_method_deep(g.mesh->get_type_definition(), "set_Material");
            if (mesh_holder != nullptr && set_mesh != nullptr) {
                if (set_mesh->invoke(g.mesh, {mesh_holder}).exception_thrown) LOGW("%s shadowhead: setMesh(face's holder) threw", TAG);
                if (mat_holder != nullptr && set_mat != nullptr && set_mat->invoke(g.mesh, {mat_holder}).exception_thrown) LOGW("%s shadowhead: set_Material(face's holder) threw", TAG);
                g.handed = true;
                g.created_t = now_s(); g.mat_reports = 0;   // report readiness again from here
                g.mode_sent = -1;
                LOGI("%s shadowhead: route B: the face's own holders handed over (mesh %p, material %p): materials=%d", TAG, (void*)mesh_holder, (void*)mat_holder, material_num(g.mesh));
            }
        }
    }
    hide_hair(player);
    place(player);
    if (g.mode_sent != g.mode) apply_mode();
    const float t = now_s();
    if (g.mat_reports < 4 && t - g.created_t >= 0.5f * (g.mat_reports + 1)) {
        ++g.mat_reports;
        LOGI("%s shadowhead: %.1f s after creation: materials=%d DrawDefault=%d DrawShadowCast=%d MeshReady=%d MaterialReady=%d ReadyToDraw=%d HasJoint=%d Skinning=%d Viewable=%d",
             TAG, t - g.created_t, material_num(g.mesh), (int)call_direct<bool>(g.mesh, "get_DrawDefault", false),
             (int)call_direct<bool>(g.mesh, "get_DrawShadowCast", false), (int)call_direct<bool>(g.mesh, "get_MeshReady", false),
             (int)call_direct<bool>(g.mesh, "get_MaterialReady", false), (int)call_direct<bool>(g.mesh, "get_ReadyToDraw", false),
             (int)call_direct<bool>(g.mesh, "get_HasJoint", false), (int)call_direct<bool>(g.mesh, "get_SkinningMesh", false),
             (int)call_direct<bool>(g.mesh, "get_Viewable", false));
    }
    if (t - g.last_report_t >= REPORT_S) {
        g.last_report_t = t;
        Vec3 p{}; get_v3(joints::XFORM, g.tf, "get_Position", p);
        LOGI("%s shadowhead: mode %d, copy at (%.2f %.2f %.2f), materials=%d", TAG, g.mode, p.x, p.y, p.z, material_num(g.mesh));
    }
}

}  // namespace vn::shadowhead
