// visceral.h -- shared declarations for visceral_core (split out of Plugin.cpp 2026-09-27, move only;
// see SPLIT-MAP-2026-09-24.md). Everything lives in namespace visceral.
#pragma once

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "reframework/API.h"
#include "reframework/API.hpp"

using reframework::API;

namespace visceral {

extern const REFrameworkPluginInitializeParam* g_param;
// ---- plain globals defined in the job files (before the types: SelfCall writes g_self_call)
extern std::atomic<bool> g_api_ok;
extern uint32_t g_arr_count_off;
extern uint32_t g_arr_elem_off;
extern bool g_arr_measured;
extern std::atomic<bool> g_head_hide_hair;
extern std::atomic<uint32_t> g_calls_aid;
extern std::atomic<uint32_t> g_calls_ikl;
extern bool g_self_call;
extern API::ManagedObject** g_grab_named;
extern std::string g_grab_name;

#define LOGI(...) do { if (g_param != nullptr && g_param->functions != nullptr && g_param->functions->log_info  != nullptr) g_param->functions->log_info(__VA_ARGS__);  } while (0)
#define LOGW(...) do { if (g_param != nullptr && g_param->functions != nullptr && g_param->functions->log_warn  != nullptr) g_param->functions->log_warn(__VA_ARGS__);  } while (0)
#define LOGE(...) do { if (g_param != nullptr && g_param->functions != nullptr && g_param->functions->log_error != nullptr) g_param->functions->log_error(__VA_ARGS__); } while (0)

constexpr const char* TAG = "[visceral]";
// v0.10: the forearm bracelets' loose assets (natives/stm/ + path) and the twist-blend steps NUM- cycles through
constexpr const char* BRACELET_MDF_PATH = "visceral/visceral_bracelets.mdf2";
constexpr const char* BRACELET_MESH_PATH[2] = {"visceral/visceral_bracelet_l_radiuslocal.mesh", "visceral/visceral_bracelet_r_radiuslocal.mesh"};
constexpr float BRACELET_K[4] = {0.f, 0.5f, 0.8f, 1.f};
struct Vec3 { float x{}, y{}, z{}; };
struct Quat { float x{}, y{}, z{}, w{}; };
struct Mat4 { float m[16]{}; };  // RE Engine mat4: row-major, translation in m[12..14]

// ---- rotation helpers. A 3x3 "rows" block is the top-left of an RE Engine row-major mat4: row i
// is basis vector i of the frame, in world coordinates. Quaternions here are only ever produced
// from and consumed by these helpers (rows -> quat -> rows round-trips exactly), so the blend does
// not depend on the engine's quaternion conventions. The one place a FOREIGN quaternion enters is
// the VR bridge (controller/HMD rotation) — see world_from_bridge(); that path is unverified.
struct Rows { float r[9]{1, 0, 0, 0, 1, 0, 0, 0, 1}; };
struct Inv {
    reframework::InvokeRet r{};
    bool ok{false};
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

struct State {
    API::ManagedObject* player_go{};
    API::ManagedObject* cond{};
    API::ManagedObject* equipment{};
    API::ManagedObject* ik{};
    API::ManagedObject* transform{};
    API::ManagedObject* motion{};
    API::ManagedObject* weapon{};
    API::ManagedObject* weapon_transform{};
    API::ManagedObject* l_hand{};
    API::ManagedObject* r_hand{};
    // v0.6: the left arm chain above the wrist, for the reach clamp. RE2's pl1000 skeleton names them
    // l_arm_clavicle / l_arm_humerus / l_arm_radius / l_arm_wrist (verified-live 2026-09-04); the humerus is
    // the shoulder end the arm pivots about and radius is the elbow, so |hu-ra| + |ra-wr| is the arm's length.
    API::ManagedObject* l_humerus{};
    API::ManagedObject* l_radius{};
    API::ManagedObject* aid_joint{};
    API::ManagedObject* w_narrow{};     // weapon joint the NARROW hash resolves to (_101 on wp8700)
    API::ManagedObject* w_wide{};       // weapon joint the WIDE hash resolves to (_100 on wp8700)
    API::ManagedObject* neck0{};        // v0.7: the player's neck_0 joint — what the neck plug is pinned to
    // v0.10: the bracelets ride the radius joints; the wrist joints supply the forearm axis and the twist to blend
    API::ManagedObject* r_radius{};
    API::ManagedObject* l_wrist{};
    API::ManagedObject* r_wrist{};
    API::ManagedObject* bridge{};      // System.Single[64] from the Lua shim
    uint64_t frame{};
    bool trace{false};
    bool want_dump{false};
    bool want_layers{false};
    bool force_hold{false};          // NUM4: InputSystem.setForce(HOLD, true) — the latch proven in Lua on 2026-08-27, now native
    int attack_pulse{0};             // NUM5: setForce(ATTACK, true) for one frame, then false
    bool hold_sent{false};           // what setForce(HOLD) was last told; reconciled from force_hold || dock.docked
    // v0.4: THE DOCK. v0.3 proved the wrist goes wherever getIKLeftArmMatrix() returns (a +10 cm shift moved
    // l_arm_wrist 10 cm, both getters additive, ~345 calls/s each, and the solver snaps). v0.4 returns a
    // blended matrix from that hook instead of a shifted one.
    struct Dock {
        bool synthetic{false};       // NUM6: flat stand-in for "LG held" — target orbits _101 at 10 cm, yawed 45 deg
        bool lg_held{false};         // bridge S_LGRIP > 0.5 while controllers are active
        bool docked{false};          // lg_held || synthetic, evaluated once per frame
        float w{0.f};                // raw blend weight, slews at 1 / DOCK_BLEND_S per second
        float w_eased{0.f};          // smoothstep(w) — what the hook uses
        int space_mode{0};           // NUM1: 0 = controller re-based HMD-relative onto the game camera; 1 = bridge pose as world
        bool write_rot{true};        // NUM3: also blend the rotation rows (off = translation only)
        bool use_reach_clamp{true};  // NUM2: apply the v0.6 reach clamp (off = the v0.5 behaviour exactly)
        Mat4 natural{};              // the last un-hooked getIKLeftArmMatrix value the GAME's call saw (pre-edit copy)
        bool natural_valid{false};
        // v0.4.1 instrumentation (run 10 showed the wrist 10 cm from the joint but ~0.3 m from the computed
        // target): the plugin's OWN summary call of the getter runs through the same hook, at a different point
        // in the frame. Its value is kept apart (natural_self) so it never becomes the blend origin, and the INNER
        // getter's un-hooked value (get_AidTargetWorldMatrix, captured on the game's call) is kept to compare.
        Mat4 natural_self{}; bool natural_self_valid{false};
        Vec3 inner_t{}; bool inner_valid{false};
        Vec3 target_t{};             // where the wrist should go — FINAL world space (what the trace compares the wrist to)
        Rows target_r{};             // and which way it should face, final world rows
        bool target_valid{false};
        // v0.5: the getter-space form the hook writes. Run 11 (2026-09-05) fitted, to 2 mm over six samples:
        //   wrist_final_rows = returned_rows * M   and   (wrist_final - aid_final) = (returned_t - natural_t) * M
        // with M one constant rotation (~48 deg here) = natural_rows^T * aid_final_rows. So the dock blends in
        // FINAL space and maps the result back: returned_t = natural_t + (blended - aid_final) * M^T,
        // returned_rows = blended_rows * M^T. At zero offset that is exactly the natural value.
        Vec3 d_get{};                // translation offset to add in getter space
        Rows T_rows{};               // rotation rows to write in getter space
        Rows M{}; bool M_valid{false};
        // v0.6 reach clamp. A target further from the shoulder than the arm is long leaves the engine's solver
        // straining at full extension and the trace reporting a distance that can never reach zero, which reads
        // as a broken mapping rather than an out-of-range target. reach is measured from the skeleton, not
        // assumed, so it is this character's arm and not a constant.
        float reach{0.f};            // usable radius = reach_raw * DOCK_REACH_FRAC
        float reach_raw{0.f};        // running max of |humerus-radius| + |radius-wrist| over accepted frames
        bool reach_valid{false};
        Vec3 shoulder_t{};           // humerus world position this frame (the clamp centre)
        float clamped_m{0.f};        // how far the raw target was pulled in this frame; 0 = it was in reach
        bool clamp_on{false};        // edge state, so the clamp engaging is logged once and not every frame
        double orbit_t0{};           // NUM6 orbit phase origin
        uint32_t no_value_frames{};  // docked, but the getter returned no value (minigun at one read) — counted, logged 1/s
        Vec3 cam_t{}; Rows cam_r{}; bool cam_valid{false};   // game camera world pose (VR re-basing + logging)
        // v0.11 (2026-09-12): the camera DIAGNOSTIC, added because `cam=` printed the same triple for seven
        // minutes across two level loads. update_camera() is only ever called from head_update() behind
        // `if (!cam_valid)` and from update_dock()'s real-controller branch, and it sets cam_valid = true on its
        // first success — so after frame 1 it never runs again and cam_t is frozen at whatever camera the very
        // first frame saw. These fields are filled by update_camera2(), which runs UNCONDITIONALLY every frame
        // and touches nothing the dock or the head hider consume; cam_* above is left exactly as it was so one
        // flat run can print the frozen value and the live ones side by side.
        Vec3 cam2_t{}; Rows cam2_r{}; bool cam2_valid{false};   // camera GameObject -> Transform -> joint 0 (REFramework's own route)
        const char* cam2_src{"none"};                           // which fallback produced cam2 this frame
        Vec3 camf_t{}; bool camf_valid{false};                  // the OLD path (via.Camera.get_WorldMatrix), re-read fresh this frame
        uintptr_t cam_addr{0};                                  // the primary-camera object address; constant across a level load => stale handle
    } dock;
    // v0.7: THE NECK PLUG (roadmap v2 H1). See plug_create() for the why.
    struct Plug {
        API::ManagedObject* go{};
        API::ManagedObject* transform{};
        API::ManagedObject* mesh{};
        bool tried{false};           // create attempted (success or not) — one attempt per binding, so a missing file logs once
        bool created{false};
        bool lost{false};            // the GameObject stopped being a managed object (scene wipe) — logged once, then recreated
        bool enabled{true};          // NUM0 toggles; drives set_DrawDefault
        bool enabled_sent{true};
        Vec3 last_pos{};
        bool readback_pending{false};   // v0.8: read get_DrawDefault back one frame after writing it
        int  mat_tries{0};              // v0.17: set_Material succeeds but get_MaterialNum stays 0 — re-apply until it takes
        double last_mat_t{-1e9};
    } plug;
    // v0.10: FOREARM BRACELETS (Tefa's idea, 2026-09-06). See bracelets_update() for the why.
    struct Bracelet {
        API::ManagedObject* go{};
        API::ManagedObject* transform{};
        API::ManagedObject* mesh{};
        bool tried{false};           // v0.17: "has given up", not "has attempted once" — see attempts below
        int  attempts{0};            // v0.17: creation is retried; one shot per bind was losing the first load
        double last_try_t{-1e9};
        bool created{false};
        bool lost{false};
        bool enabled_sent{true};
        Vec3 last_pos{};
        float theta{0.f};        // wrist twist about the forearm axis, relative to the radius joint (radians)
        uint64_t created_frame{};   // v0.11: get_MaterialNum read 0 at creation on 2026-09-06 (so did the plug's); re-read once, later
        int  mat_tries{0};          // v0.17: as the plug — re-apply the material until the mesh actually reports one
        double last_mat_t{-1e9};
        bool relogged{false};
    };
    struct Bracelets {
        Bracelet l, r;
        bool enabled{true};      // NUM* toggles both
        int k_idx{0};            // NUM- cycles the twist blend: 0 = rigid to the radius joint (safe baseline)
        int conv{0};             // NUM/ cycles the quaternion convention used to APPLY the blend (order x sign, 4 combos)
    } bracelets;
    // v0.8: THE HEAD HIDER (roadmap v1 #4 / v2 phase H). Hides every head-ish via.render.Mesh under the player by
    // its per-pass draw flags — DrawDefault off, DrawShadowCast on — so the head is gone from the camera but still
    // casts its shadow. REFramework's own HideJointMesh scales the head joint to zero and takes the shadow with it,
    // so it must be OFF for this to do anything (the head hider does not touch that setting). See head_update().
    struct Head {
        struct Entry {
            API::ManagedObject* mesh{};
            std::string label;           // GameObject name (+ " #n" for a second mesh on the same object)
            std::string mats;            // material names, for the log and for the match
            bool hide{false};            // decided at scan time by name / material patterns
            bool has_rt{false};          // set_DrawRaytracing exists on this build
            bool orig_default{true}, orig_shadow{true}, orig_rt{true};
            bool applied{false};         // our flags are currently written on it
        };
        int mode{1};                     // NUM.: 0 = off (originals restored), 1 = on with reveals, 2 = on, forced (no reveals — A/B isolation)
        std::vector<Entry> meshes;
        bool scanned{false};
        bool want_rescan{false};         // NUM+ : throw the list away and walk again (logs the full mesh table)
        double last_scan_t{};
        bool revealed{false};            // this frame the head is deliberately shown (cutscene / grab / camera off the head)
        std::string reveal_why;
        double last_far_t{-1e9};         // last time a reveal trigger was true — 0.3 s tail so a threshold jitter cannot strobe the head
        API::ManagedObject* head_joint{};
        API::ManagedObject* jack{};      // app.ropeway.JackDominator on the player (grab detector REFramework's FirstPerson.cpp reads too)
        API::ManagedObject* anchor{};    // v0.14: the node route C proved every player mesh hangs under — route D's cheap start
        std::string anchor_name;
        bool gathered{false};            // v0.15: SurvivorCostumeChanger.gatherPartsMesh() already asked for, this bind
        float head_cam_d{-1.f};          // |camera - head joint| this frame, metres; -1 = unavailable
        int hidden_n{0};
        bool stale_logged{false};
    } head;
    std::vector<std::string> layer_last;   // last highest-weight motion name per layer
    uint64_t layer_lines_this_second{};
    uint64_t layer_second{};
    double last_summary_t{};
    double last_layer_table_t{};
};
constexpr int MESH_CATCH_MAX = 32;
struct MeshCatcher {
    // `owner` is argv[0] — the instance the hooked setter was called on. The hooks are GLOBAL: one flat run in
    // the RPD caught Claire (pl1000/pl1050/pl1070), Sherry (pl5700/pl5750) and an NPC (pl7800/pl7850/pl7870)
    // through the same three setters `[verified-live 2026-09-12, n=1]`. Without the owner there is no way to
    // tell them apart, and hiding an NPC's face is a far worse bug than not hiding the player's.
    struct Slot { API::ManagedObject* mesh{}; API::ManagedObject* owner{}; const char* how{}; uint32_t hits{}; };
    Slot s[MESH_CATCH_MAX]{};
    int n{0};
    std::atomic<uint32_t> calls{0};
    std::string install;                 // which hooks went on and which did not — route E's "why" line
    int installed{0}, attempted{0};
    // v0.17: bumped on every player re-bind. Everything derived from this list — the announce index, the
    // take index — is keyed to it, so a new life cannot inherit the previous one's bookkeeping.
    std::atomic<uint32_t> gen{0};
    void reset_for_new_player() {
        for (int i = 0; i < n; ++i) s[i] = Slot{};
        n = 0;
        ++gen;
    }
};
struct SelfCall { SelfCall() { g_self_call = true; } ~SelfCall() { g_self_call = false; } };

// Bridge slot map (must match visceral_native_bridge.lua)
enum Slot : int {
    S_FRAME = 0, S_HMD_ACTIVE = 1, S_USING_CTL = 2,
    S_LPOS = 3,  S_LROT = 6,        // 3 + 4
    S_RPOS = 10, S_RROT = 13,
    S_HPOS = 17, S_HROT = 20,
    S_LSTICK = 24,                  // x, y
    S_LGRIP = 26, S_LTRIG = 27, S_RGRIP = 28, S_RTRIG = 29,
    S_CINE = 30,                    // v0.8: 1.0 while visceral_cinematic_gate.lua says the player is not in first-person control
    S_FP = 31,                      // v0.8: 1.0 while REFramework's FirstPerson mod reports will_be_used() (Lua-only API)
    S_ACK = 62, S_SENTINEL = 63,
};
constexpr const char* PLUG_MESH_PATH = "visceral/visceral_neckplug_neck0local.mesh";   // natives/stm/ + this + .2109108288, via REFramework's loose-file loader
constexpr const char* PLUG_MDF_PATH  = "sectionroot/character/player/pl1000/pl1000/pl1000.mdf2";   // v0.9: CLAIRE is pl1000 (pl3000 is Sherry, found 2026-09-06 13:15); her skin material is pl1000_Body_Mat
struct alignas(16) V4 { float x{}, y{}, z{}, w{}; };   // via.vec3 / via.Quaternion as the engine lays them out (16 bytes)
constexpr float HEAD_REVEAL_DIST_M = 0.35f;
constexpr double HEAD_REVEAL_TAIL_S = 0.3;
constexpr double HEAD_RESCAN_MIN_S = 2.0;

// ---------------------------------------------------------------------------
// v0.13: ROUTE B — ask the component that OWNS the player's meshes instead of walking for them.
//
// Why walk A (above) cannot reach the head: it starts at the player GameObject's via.Transform and descends
// get_Child/get_Next. On 2026-09-09 that returned 25 transforms and 5 meshes, and three of the five were objects
// WE parented there ourselves (bracelets, neck plug) — the only two of the game's own were `Transceiver` and
// `FlashLight`, i.e. accessories. The player's body/face/hair meshes are NOT transform-children of the player.
// RE8VR.cpp's fix_player_shadow() says why in another RE game: the part-mesh GameObjects are separate, and
// REFramework has to COPY the head joint from the player transform onto them (copy_joint(head_hash,
// m_player->transform, mesh_gameobject->transform)) precisely because they are not parented. [inferred-static 2026-09-12]
//
// What the dump says RE2 has (il2cpp_dump.json, read by name + signature 2026-09-12) [inferred-static]:
//   app.ropeway.survivor.SurvivorCondition.get_CostumeChanger() -> app.ropeway.survivor.SurvivorCostumeChanger
//   ...and app.ropeway.survivor.player.PlayerCondition — what PlayerManager.get_CurrentPlayerCondition returns,
//   already held in g.cond — derives from SurvivorCondition, so find_method_deep reaches the getter.
//   SurvivorCostumeChanger then names every part mesh outright:
//     get_Face / get_Hair / get_Body / get_Other / get_SheathKnife  -> via.render.Mesh
//     get_FaceObject / get_HairObject / get_BodyObject / get_OtherObject / get_SheathKnifeObject /
//     get_AccessoryObject                                           -> via.GameObject
//   plus SurvivorCondition.get_Mesh() -> via.render.Mesh, an independent third shot at the body.
// Same shape as the zombie work's Em0000SimpleMontageBase.get_FaceMesh(): the component that owns the mesh hands
// it over, no hierarchy search at all.
//
// The *Object getters are WALKED, not merely read, because getComponent returns only the FIRST via.render.Mesh on
// a GameObject and eyelashes / eyes / tearline are usually extra mesh components or child objects (dossier §7).
//
// THIS PASS IS DIAGNOSTIC ONLY. Route B does not feed h.meshes and changes nothing that gets hidden — it logs its
// counts and names beside route A's so one flat run decides which route is right.
// ---------------------------------------------------------------------------
struct HeadProbeB {
    int objects{0};                      // GameObjects reached through the changer
    int transforms{0};                   // transforms walked under those objects
    std::string route;                   // how the changer was reached, or why it was not
    std::vector<API::ManagedObject*> seen_meshes;
    std::vector<std::pair<std::string, std::string>> meshes;   // label, material names
};
struct HeadProbeC {
    uint32_t scene_total{0};                 // every via.render.Mesh in the scene
    std::vector<API::ManagedObject*> hit_mesh;
    std::vector<std::string> hit_name, hit_mat;
    API::ManagedObject* anchor{};            // deepest transform every hit shares
    std::string anchor_name{"none"};
    std::string route{"not run"};
};

// app.ropeway.InputDefine.Kind values learned by the Lua probes (2026-08-27): HOLD=64, ATTACK=256.
constexpr int KIND_HOLD = 64;
constexpr int KIND_ATTACK = 256;
// via.motion.IkKind as the IkController indexes it (isEnabled(k) read 2026-09-04): LEG 0, SPINE 1, LOOKAT 2, ARM 3, ARMFIT 4, HAND 5.
constexpr int KIND_ARM = 3;

// ---------------------------------------------------------------------------
// v0.4: the dock. Inputs -> blend weight -> HOLD reconcile -> target for the hook.
// ---------------------------------------------------------------------------

constexpr float DOCK_BLEND_S = 0.2f;     // spec v2.3 req 1: smooth, not snap. The solver snaps; this is the ramp.
constexpr float DOCK_ORBIT_PERIOD_S = 4.0f;
constexpr float DOCK_ORBIT_RADIUS = 0.10f;
constexpr float DOCK_ORBIT_YAW_DEG = 45.f;
// v0.6: clamp to just inside the measured arm length. A target at exactly 1.0 puts the elbow at full lock,
// which is both an ugly pose and the point where a two-bone solver is least stable.
constexpr float DOCK_REACH_FRAC = 0.98f;

// ---- functions defined in the job files
float dist(const Vec3& a, const Vec3& b);
Vec3 vsub(const Vec3& a, const Vec3& b);
Vec3 vadd(const Vec3& a, const Vec3& b);
Vec3 vscale(const Vec3& a, float s);
Vec3 vlerp(const Vec3& a, const Vec3& b, float t);
float vlen(const Vec3& a);
Rows rows_of(const Mat4& m);
void rows_into(Mat4& m, const Rows& o);
Vec3 row(const Rows& o, int i);
float rows_angle_deg(const Rows& a, const Rows& b);
Rows rows_yaw(const Rows& o, float deg);
Quat quat_of_rows(const Rows& o);
Rows rows_of_quat(const Quat& q);
Quat quat_norm(Quat q);
Quat quat_slerp(Quat a, Quat b, float t);
Rows rows_slerp(const Rows& a, const Rows& b, float t);
Rows rows_mul(const Rows& a, const Rows& b);
Rows rows_T(const Rows& a);
Vec3 vec_mul_rows(const Vec3& v, const Rows& R);
Quat quat_mul(const Quat& a, const Quat& b);
Quat quat_conj(const Quat& q);
Vec3 quat_rotate(const Quat& q, const Vec3& v);
API::Method* find_method_deep(API::TypeDefinition* td, std::string_view name);
API::Field* find_field_deep(API::TypeDefinition* td, std::string_view name);
bool is_managed(void* p);
std::string tname(API::ManagedObject* o);
Inv inv(API::ManagedObject* o, std::string_view name, std::vector<void*> args = {});
API::ManagedObject* inv_ptr(API::ManagedObject* o, std::string_view name, std::vector<void*> args = {});
bool inv_bool(API::ManagedObject* o, std::string_view name);
bool inv_bool_i(API::ManagedObject* o, std::string_view name, int arg);
[[maybe_unused]] void* float_arg(float f);
uint32_t inv_u32(API::ManagedObject* o, std::string_view name);
float inv_f32(API::ManagedObject* o, std::string_view name);
bool inv_vec3(API::ManagedObject* o, std::string_view name, Vec3& v);
bool inv_mat4(API::ManagedObject* o, std::string_view name, Mat4& m);
bool inv_nullable_mat4(API::ManagedObject* o, std::string_view name, bool& has, Mat4& m);
std::string sysstr(API::ManagedObject* s);
uint32_t arr_count(API::ManagedObject* a);
API::ManagedObject* arr_ptr_at(API::ManagedObject* a, uint32_t i);
float* arr_f32(API::ManagedObject* a);
bool measure_array_layout(API::ManagedObject* a);
API::ManagedObject* get_component(API::ManagedObject* go, const char* type);
std::string joint_name(API::ManagedObject* joint);
std::string lower(std::string s);
void catch_mesh_args(int argc, void** argv, const char* how);
double now_s();
bool bridge_live();
Vec3 bridge_vec3(int slot);
void dump_joints(API::ManagedObject* transform, const char* who, bool all, API::ManagedObject** l_hand, API::ManagedObject** r_hand);
void log_nullable_mat(const char* label, bool ok, bool has, const Mat4& m);
void dump_weapon();
void dump_type_surface(API::ManagedObject* o, const char* label,
                       const std::vector<const char*>& words = {"target", "weight", "enable", "joint"});
void dump_ik();
void dump_layers(bool force_table);
void dump_motion();
void summary_line();
std::string mesh_material_names(API::ManagedObject* mesh);
bool remat_if_empty(API::ManagedObject* mesh, const char* mdf_path, const char* what,
                    int& tries, double& last_t, bool manual = true);
API::ManagedObject* create_resource_holder(const char* res_type, const char* path, const char* holder_type, bool manual);
void plug_create();
void plug_update();
void bracelet_create(State::Bracelet& b, int side);
void bracelets_update();
bool head_pattern(const std::string& lname);
void head_restore_all(const char* why);
void head_walk(API::ManagedObject* tf, int depth, int& seen, bool verbose);
void head_b_add_mesh(API::ManagedObject* mesh, const std::string& how, HeadProbeB& b);
void head_b_add_go(API::ManagedObject* go, const std::string& how, HeadProbeB& b);
void head_b_walk(API::ManagedObject* tf, int depth, const std::string& how, HeadProbeB& b);
void head_probe_b(HeadProbeB& b);
API::ManagedObject* current_scene();
API::ManagedObject* scene_find_components(API::ManagedObject* scene, const char* type);
std::string mesh_first_material(API::ManagedObject* mesh);
bool player_like(const std::string& s);
std::string component_types(API::ManagedObject* go, int cap = 32);
std::vector<API::ManagedObject*> ancestor_chain(API::ManagedObject* tf);
std::string tf_go_name(API::ManagedObject* tf);
void head_probe_c(HeadProbeC& c, bool verbose);
void head_probe_owners();
bool mesh_belongs_to_player(API::ManagedObject* mesh);
int head_take_caught(bool verbose);
void head_scan(bool verbose);
void head_update();
bool game_is_foreground();
void set_force(int kind, bool on);
void poll_hotkeys();
void update_camera();
void update_camera2();
void update_dock(double t, double dt);
void rebind_player(API::ManagedObject* pm, API::ManagedObject* go);
void on_frame();
int pre_mailbox(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long);
int pre_passthrough(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long);
void post_aid_target(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long);
void post_ik_left_arm(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long);
void install_shift_hooks();
void install_mesh_catch_hooks();
void install_bridge_hook();
void on_initialized();

// ---- the global state objects (after their types)
extern State g;
extern MeshCatcher g_catch;

// ---- templates (bodies must be visible to every caller)
// Scalar getters go through the DIRECT function-pointer route (vmctx, this, args...):
// the reflection invoke path returned 0 for every float on 2026-09-04 (XMM0 results are
// not copied into InvokeRet by this build), and the direct route is what Lua's own
// float returns rely on. Pointers and value types keep using invoke.
template <typename T, typename... Args>
T call_direct(API::ManagedObject* o, std::string_view name, T fallback, Args... args) {
    if (o == nullptr) return fallback;
    auto* m = find_method_deep(o->get_type_definition(), name);
    if (m == nullptr) return fallback;
    return m->call<T>(API::get()->get_vm_context(), (void*)o, args...);
}
// Direct-route call with no return: the real function pointer, real C ABI, so a float
// argument lands where the callee reads it. Returns false only if the method is missing.
template <typename... Args>
bool call_void_direct(API::ManagedObject* o, std::string_view name, Args... args) {
    if (o == nullptr) return false;
    auto* m = find_method_deep(o->get_type_definition(), name);
    if (m == nullptr) return false;
    m->call<void>(API::get()->get_vm_context(), (void*)o, args...);
    return true;
}
template <typename T> T field_at(API::ManagedObject* o, uint32_t off) { return *(const T*)((const char*)o + off); }

} // namespace visceral
