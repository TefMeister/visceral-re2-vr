// Visceral — RE2 VR — native core (REFramework plugin, API 1.15)
//
// v0.1 (2026-09-04): the reqs-1-and-3 RECON PROBE, written native from line one
// under the "reach for the deep end" rule. It reads, it does not drive. What it
// answers, all from the game's own objects rather than anything we invent:
//
//   1. Where the off-hand belongs on the equipped weapon. Every RE2 weapon
//      carries an AID JOINT (Implement.get_AidJoint, typed Narrow / Wide /
//      ExtraNarrow via Equipment.getAidJointType) — the engine's own name for
//      "where the support hand goes". The probe logs the joint, its world
//      position, the game's own left-arm IK matrix for it
//      (Implement.getIKLeftArmMatrix) and the aid target matrix.
//   2. Where the player's hands actually are (l/r hand joints on the player
//      skeleton) and where the tracked controllers are (ferried in by a Lua
//      shim, because plugin API 1.15 has no VR calls) — and the distances
//      between all of those, which is req 1's proximity signal.
//   3. The IK controller the game uses for the arms (IkController: which
//      kinds are enabled, the per-arm status list) — the target for a
//      smooth dock/undock that rides the engine's IK instead of fighting it.
//   4. Which motion layer carries locomotion (the /gr speed-lever caveat):
//      every layer's highest-weight motion name, speed and blend rate,
//      logged on change.
//
// Log tag: [visceral]. Hotkeys (numpad only, per the standing rule; game window
// must be foreground): NUM7 = full dump now, NUM8 = toggle 10 Hz trace,
// NUM9 = motion-layer table now.
//
// Lua bridge: visceral_native_bridge.lua creates a System.Single[64], writes the
// sentinel 12345 into slot 63 and hands the array over ONCE by calling a
// "mailbox" method this plugin hooks: app.ropeway.RagdollControlZoneManager.
// set_AccessMutex(System.Object) — static, one object parameter, a real compiled
// game function (System.GC.KeepAlive was tried first on 2026-09-04: it is an
// internal call whose body REFramework cannot resolve in this build, the hook
// failed, and invoking it from Lua crashed the game inside the native invoker).
// The pre-hook SKIPS the original only when the argument is our sentinel array,
// so the game's own calls to that setter pass through untouched. After that the shim writes
// poses into the array every frame and the plugin reads them here. Slot 62 is
// the plugin's acknowledgement (1.0). No Lua C API, no ABI assumptions beyond
// the managed-array element offset, which the sentinel itself verifies.

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "reframework/API.h"
#include "reframework/API.hpp"

using reframework::API;

namespace {

const REFrameworkPluginInitializeParam* g_param = nullptr;
std::atomic<bool> g_api_ok{false};

#define LOGI(...) do { if (g_param != nullptr && g_param->functions != nullptr && g_param->functions->log_info  != nullptr) g_param->functions->log_info(__VA_ARGS__);  } while (0)
#define LOGW(...) do { if (g_param != nullptr && g_param->functions != nullptr && g_param->functions->log_warn  != nullptr) g_param->functions->log_warn(__VA_ARGS__);  } while (0)
#define LOGE(...) do { if (g_param != nullptr && g_param->functions != nullptr && g_param->functions->log_error != nullptr) g_param->functions->log_error(__VA_ARGS__); } while (0)

constexpr const char* TAG = "[visceral]";

struct Vec3 { float x{}, y{}, z{}; };
struct Quat { float x{}, y{}, z{}, w{}; };
struct Mat4 { float m[16]{}; };  // RE Engine mat4: row-major, translation in m[12..14]

float dist(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ---------------------------------------------------------------------------
// Reflection helpers. find_method on a runtime type does not reliably resolve
// inherited methods through the C API, so walk the parent chain.
// ---------------------------------------------------------------------------

API::Method* find_method_deep(API::TypeDefinition* td, std::string_view name) {
    for (int i = 0; td != nullptr && i < 12; ++i) {
        auto* m = td->find_method(name);
        if (m != nullptr) return m;
        td = td->get_parent_type();
    }
    return nullptr;
}

API::Field* find_field_deep(API::TypeDefinition* td, std::string_view name) {
    for (int i = 0; td != nullptr && i < 12; ++i) {
        auto* f = td->find_field(name);
        if (f != nullptr) return f;
        td = td->get_parent_type();
    }
    return nullptr;
}

bool is_managed(void* p) {
    if (p == nullptr || !g_api_ok.load()) return false;
    return API::get()->sdk()->managed_object->is_managed_object(p);
}

std::string tname(API::ManagedObject* o) {
    if (o == nullptr) return "null";
    auto* td = o->get_type_definition();
    return td != nullptr ? td->get_full_name() : "?";
}

struct Inv {
    reframework::InvokeRet r{};
    bool ok{false};
};

Inv inv(API::ManagedObject* o, std::string_view name, std::vector<void*> args = {}) {
    Inv out{};
    if (o == nullptr) return out;
    auto* m = find_method_deep(o->get_type_definition(), name);
    if (m == nullptr) return out;
    out.r = m->invoke(o, args);
    out.ok = !out.r.exception_thrown;
    return out;
}

API::ManagedObject* inv_ptr(API::ManagedObject* o, std::string_view name, std::vector<void*> args = {}) {
    auto x = inv(o, name, std::move(args));
    return x.ok ? (API::ManagedObject*)x.r.ptr : nullptr;
}
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
bool inv_bool(API::ManagedObject* o, std::string_view name) { return call_direct<bool>(o, name, false); }
bool inv_bool_i(API::ManagedObject* o, std::string_view name, int arg) { return call_direct<bool>(o, name, false, arg); }
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
// The invoke path marshals every argument in an 8-byte slot ("each arg is always 8 bytes" — API.h);
// a float goes in as its bit pattern in the low 32 bits.
[[maybe_unused]] void* float_arg(float f) { uint64_t v = 0; memcpy(&v, &f, sizeof f); return (void*)(uintptr_t)v; }
uint32_t inv_u32(API::ManagedObject* o, std::string_view name) { return call_direct<uint32_t>(o, name, 0xFFFFFFFFu); }
float inv_f32(API::ManagedObject* o, std::string_view name) { return call_direct<float>(o, name, NAN); }
bool inv_vec3(API::ManagedObject* o, std::string_view name, Vec3& v) {
    auto x = inv(o, name);
    if (!x.ok) return false;
    memcpy(&v, x.r.bytes.data(), sizeof(Vec3));
    return true;
}
bool inv_mat4(API::ManagedObject* o, std::string_view name, Mat4& m) {
    auto x = inv(o, name);
    if (!x.ok) return false;
    memcpy(&m, x.r.bytes.data(), sizeof(Mat4));
    return true;
}
// System.Nullable`1<via.mat4>: _HasValue at +0, _Value at +0x10 (dump: 0x10 / 0x20 minus the 0x10 box header).
bool inv_nullable_mat4(API::ManagedObject* o, std::string_view name, bool& has, Mat4& m) {
    auto x = inv(o, name);
    if (!x.ok) return false;
    has = x.r.bytes[0] != 0;
    memcpy(&m, x.r.bytes.data() + 0x10, sizeof(Mat4));
    return true;
}

// System.String: int32 length at +0x10, UTF-16 data at +0x14.
std::string sysstr(API::ManagedObject* s) {
    if (s == nullptr) return "";
    const auto len = *(const int32_t*)((const char*)s + 0x10);
    if (len <= 0 || len > 512) return "";
    const auto* w = (const wchar_t*)((const char*)s + 0x14);
    std::string out; out.reserve((size_t)len);
    for (int32_t i = 0; i < len; ++i) out.push_back(w[i] < 0x80 ? (char)w[i] : '?');
    return out;
}

// Managed arrays. REFramework's REArrayBase puts the count at +0x18 and the
// elements at +0x20 on current engines; on this RE2 build the count read as 1
// through those offsets (2026-09-04 run 2), so the layout is MEASURED from the
// Lua shim's sentinel array at hand-over (see pre_mailbox) and the measured
// offsets are used everywhere. Until measured, the defaults apply and every
// reader self-checks.
uint32_t g_arr_count_off = 0x18;
uint32_t g_arr_elem_off  = 0x20;
bool     g_arr_measured  = false;
uint32_t arr_count(API::ManagedObject* a) { return a != nullptr ? *(const uint32_t*)((const char*)a + g_arr_count_off) : 0; }
API::ManagedObject* arr_ptr_at(API::ManagedObject* a, uint32_t i) { return ((API::ManagedObject**)((char*)a + g_arr_elem_off))[i]; }
float* arr_f32(API::ManagedObject* a) { return (float*)((char*)a + g_arr_elem_off); }

// Find the shim's sentinel inside a candidate float array and derive the layout.
// Returns true if a sentinel was found at a plausible element offset.
bool measure_array_layout(API::ManagedObject* a) {
    const auto* bytes = (const uint8_t*)a;
    const uint32_t obj_size = API::get()->sdk()->managed_object->get_size(*a);
    LOGI("%s array layout probe: get_size=%u dwords@0x10..0x2c = %08x %08x %08x %08x %08x %08x %08x %08x", TAG, obj_size,
         *(const uint32_t*)(bytes + 0x10), *(const uint32_t*)(bytes + 0x14), *(const uint32_t*)(bytes + 0x18), *(const uint32_t*)(bytes + 0x1c),
         *(const uint32_t*)(bytes + 0x20), *(const uint32_t*)(bytes + 0x24), *(const uint32_t*)(bytes + 0x28), *(const uint32_t*)(bytes + 0x2c));
    for (uint32_t off = 0x10; off + 4 <= 0x200; off += 4) {
        if (*(const float*)(bytes + off) == 12345.0f) {
            if (off < 0x10 + 63 * 4) continue;               // can't be slot 63 of anything
            const uint32_t elem = off - 63 * 4;
            // the count (64) should sit in one of the dwords between the header and the elements
            uint32_t cnt = 0; bool found = false;
            for (uint32_t c = 0x10; c + 4 <= elem; c += 4) {
                if (*(const uint32_t*)(bytes + c) == 64) { cnt = c; found = true; break; }
            }
            g_arr_elem_off = elem;
            if (found) g_arr_count_off = cnt;
            g_arr_measured = true;
            LOGI("%s ARRAY LAYOUT MEASURED: elements @+0x%x, count %s@+0x%x (sentinel found at +0x%x, get_size=%u)", TAG,
                 g_arr_elem_off, found ? "" : "NOT FOUND, keeping default ", g_arr_count_off, off, obj_size);
            return true;
        }
    }
    LOGW("%s array layout probe: no sentinel 12345 within the first 0x200 bytes", TAG);
    return false;
}

template <typename T> T field_at(API::ManagedObject* o, uint32_t off) { return *(const T*)((const char*)o + off); }

API::ManagedObject* get_component(API::ManagedObject* go, const char* type) {
    if (go == nullptr) return nullptr;
    auto* t = API::get()->typeof(type);
    if (t == nullptr) { LOGW("%s typeof(%s) failed", TAG, type); return nullptr; }
    // The plain name has dozens of generic overloads (fn=0 in the dump); only the
    // System.Type overload is a real function, so never fall back to the plain name.
    auto* m = find_method_deep(go->get_type_definition(), "getComponent(System.Type)");
    if (m == nullptr) { LOGW("%s getComponent(System.Type) not found on %s", TAG, tname(go).c_str()); return nullptr; }
    auto r = m->invoke(go, {t});
    return r.exception_thrown ? nullptr : (API::ManagedObject*)r.ptr;
}

std::string joint_name(API::ManagedObject* joint) { return sysstr(inv_ptr(joint, "get_Name")); }

std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

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
    API::ManagedObject* aid_joint{};
    API::ManagedObject* w_narrow{};     // weapon joint the NARROW hash resolves to (_101 on wp8700)
    API::ManagedObject* w_wide{};       // weapon joint the WIDE hash resolves to (_100 on wp8700)
    API::ManagedObject* bridge{};      // System.Single[64] from the Lua shim
    uint64_t frame{};
    bool trace{false};
    bool want_dump{false};
    bool want_layers{false};
    bool force_hold{false};          // NUM4: InputSystem.setForce(HOLD, true) — the latch proven in Lua on 2026-08-27, now native
    int attack_pulse{0};             // NUM5: setForce(ATTACK, true) for one frame, then false
    // NUM6 (v0.3, 2026-09-05): the AID-TARGET OVERRIDE. The ARM kind is dead (v0.2: setEnable(ARM) never
    // sticks by either call route, setArmTarget throws on both indices, IkTwoArm/IkHand are null) and
    // the reload test proved _101 is an anchor the wrist is solved onto. So: post-hook the two managed
    // getters that serve the aid target and shift what they return 10 cm up. Bit 1 = get_AidTargetWorldMatrix,
    // bit 2 = getIKLeftArmMatrix. Cycle 0 -> 1 -> 2 -> 3 -> 0. If the wrist follows, that is the dock lever.
    int shift_mode{0};
    std::vector<std::string> layer_last;   // last highest-weight motion name per layer
    uint64_t layer_lines_this_second{};
    uint64_t layer_second{};
    double last_summary_t{};
    double last_layer_table_t{};
} g;

// v0.3 hook call counters (the hooks themselves are defined with the bridge hook below).
std::atomic<uint32_t> g_calls_aid{0}, g_calls_ikl{0};

double now_s() {
    static LARGE_INTEGER f{}; static bool init = false;
    if (!init) { QueryPerformanceFrequency(&f); init = true; }
    LARGE_INTEGER c{}; QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

// Bridge slot map (must match visceral_native_bridge.lua)
enum Slot : int {
    S_FRAME = 0, S_HMD_ACTIVE = 1, S_USING_CTL = 2,
    S_LPOS = 3,  S_LROT = 6,        // 3 + 4
    S_RPOS = 10, S_RROT = 13,
    S_HPOS = 17, S_HROT = 20,
    S_LSTICK = 24,                  // x, y
    S_LGRIP = 26, S_LTRIG = 27, S_RGRIP = 28, S_RTRIG = 29,
    S_ACK = 62, S_SENTINEL = 63,
};

bool bridge_live() { return g.bridge != nullptr && arr_f32(g.bridge)[S_SENTINEL] == 12345.0f; }
Vec3 bridge_vec3(int slot) { const float* f = arr_f32(g.bridge); return Vec3{f[slot], f[slot + 1], f[slot + 2]}; }

// ---------------------------------------------------------------------------
// Dumps
// ---------------------------------------------------------------------------

API::ManagedObject** g_grab_named = nullptr;   // dump_joints side channel: capture the joint with this name
std::string g_grab_name;
void dump_joints(API::ManagedObject* transform, const char* who, bool all, API::ManagedObject** l_hand, API::ManagedObject** r_hand) {
    auto* joints = inv_ptr(transform, "get_Joints");
    if (joints == nullptr) { LOGW("%s %s: get_Joints failed", TAG, who); return; }
    if (!g_arr_measured) LOGW("%s %s: array layout not yet measured (no bridge hand-over seen) — reading with default offsets, expect a self-check warning if they are wrong", TAG, who);
    const auto n = arr_count(joints);
    LOGI("%s %s: %u joints (array type %s)", TAG, who, n, tname(joints).c_str());
    if (n > 2048) { LOGW("%s %s: joint count implausible, array layout assumption wrong?", TAG, who); return; }
    for (uint32_t i = 0; i < n; ++i) {
        auto* j = arr_ptr_at(joints, i);
        if (!is_managed(j)) { LOGW("%s %s: joint[%u] is not a managed object — array layout assumption wrong", TAG, who, i); return; }
        const auto name = joint_name(j);
        const auto ln = lower(name);
        const bool interesting = all || ln.find("hand") != std::string::npos || ln.find("wrist") != std::string::npos ||
                                 ln.find("arm") != std::string::npos || ln.find("weapon") != std::string::npos ||
                                 ln.find("wp") != std::string::npos || ln.find("grip") != std::string::npos ||
                                 ln.find("hold") != std::string::npos || ln.find("aid") != std::string::npos;
        if (interesting) {
            Vec3 p{}; inv_vec3(j, "get_Position", p);
            LOGI("%s   joint[%u] %-28s pos=(%.3f %.3f %.3f)", TAG, i, name.c_str(), p.x, p.y, p.z);
        }
        // RE2 pl1000 skeleton (verified-live 2026-09-04): no "l_hand"; the palm points are l_weapon / r_weapon, wrists l_arm_wrist / r_arm_wrist
        if (l_hand != nullptr && *l_hand == nullptr && (ln == "l_weapon" || ln == "l_hand" || ln == "l_arm_wrist")) *l_hand = j;
        if (r_hand != nullptr && *r_hand == nullptr && (ln == "r_weapon" || ln == "r_hand" || ln == "r_arm_wrist")) *r_hand = j;
        if (g_grab_named != nullptr && *g_grab_named == nullptr && ln == g_grab_name) *g_grab_named = j;
    }
}

void log_nullable_mat(const char* label, bool ok, bool has, const Mat4& m) {
    if (!ok) { LOGI("%s   %s: call failed", TAG, label); return; }
    if (!has) { LOGI("%s   %s: (no value)", TAG, label); return; }
    LOGI("%s   %s: t=(%.3f %.3f %.3f) r0=(%.2f %.2f %.2f) r2=(%.2f %.2f %.2f)", TAG, label,
         m.m[12], m.m[13], m.m[14], m.m[0], m.m[1], m.m[2], m.m[8], m.m[9], m.m[10]);
}

void dump_weapon() {
    auto* w = g.weapon;
    if (w == nullptr) { LOGI("%s weapon: none equipped", TAG); return; }
    LOGI("%s ---- WEAPON DUMP: %s ----", TAG, tname(w).c_str());
    LOGI("%s   WeaponType=%u  MuzzleJointName=%s  AidJointType=%u (0 None,1 ExtraNarrow,2 Narrow,3 Wide)", TAG,
         inv_u32(w, "get_WeaponType"), sysstr(inv_ptr(w, "get_MuzzleJointName")).c_str(), inv_u32(g.equipment, "getAidJointType"));
    LOGI("%s   IsEquiped=%d  EnabledHoldMainWeapon=%d  ParentIkController=%p (cond IkController=%p)", TAG,
         (int)inv_bool(w, "get_IsEquiped"), (int)inv_bool(g.equipment, "get_EnabledHoldMainWeapon"), (void*)inv_ptr(w, "get_ParentIkController"), (void*)g.ik);

    // Aid joint — the engine's own "support hand goes here"
    g.aid_joint = inv_ptr(w, "get_AidJoint");
    if (g.aid_joint != nullptr) {
        Vec3 p{}; inv_vec3(g.aid_joint, "get_Position", p);
        auto* owner = inv_ptr(g.aid_joint, "get_Owner");
        LOGI("%s   AidJoint=%s pos=(%.3f %.3f %.3f) owner_transform=%p (weapon transform=%p, player transform=%p)", TAG,
             joint_name(g.aid_joint).c_str(), p.x, p.y, p.z, (void*)owner, (void*)g.weapon_transform, (void*)g.transform);
    } else {
        LOGI("%s   AidJoint=null", TAG);
    }
    const auto narrow = inv_u32(w, "get_LEFT_ARM_JOINT_NARROW");
    const auto wide = inv_u32(w, "get_LEFT_ARM_JOINT_WIDE");
    auto resolve = [&](API::ManagedObject* tr, uint32_t hash) -> std::string {
        if (tr == nullptr) return "(no transform)";
        auto* j = inv_ptr(tr, "getJointByHash", {(void*)(uintptr_t)hash});
        return j != nullptr ? joint_name(j) : "(not on this skeleton)";
    };
    LOGI("%s   LEFT_ARM_JOINT_NARROW=0x%08x -> weapon:%s player:%s", TAG, narrow, resolve(g.weapon_transform, narrow).c_str(), resolve(g.transform, narrow).c_str());
    LOGI("%s   LEFT_ARM_JOINT_WIDE  =0x%08x -> weapon:%s player:%s", TAG, wide, resolve(g.weapon_transform, wide).c_str(), resolve(g.transform, wide).c_str());

    // Attach joint (where the weapon hangs on the player) and aim joint
    if (auto* ci = inv_ptr(w, "get_AttachJoint"); ci != nullptr) {
        auto* j = field_at<API::ManagedObject*>(ci, 0x10);
        const auto op = field_at<Vec3>(ci, 0x20);
        LOGI("%s   AttachJoint=%s offset=(%.3f %.3f %.3f)", TAG, is_managed(j) ? joint_name(j).c_str() : "null", op.x, op.y, op.z);
    }
    if (auto* aj = inv_ptr(w, "get_AimJoint"); aj != nullptr) {
        Vec3 p{}; inv_vec3(aj, "get_Position", p);
        LOGI("%s   AimJoint (%s) pos=(%.3f %.3f %.3f)", TAG, tname(aj).c_str(), p.x, p.y, p.z);
    }
    Mat4 m{}; bool has = false;
    if (inv_mat4(w, "get_AimJointWorldMatrix", m)) LOGI("%s   AimJointWorldMatrix t=(%.3f %.3f %.3f)", TAG, m.m[12], m.m[13], m.m[14]);
    if (auto* mj = inv_ptr(w, "get_MuzzleJoint"); mj != nullptr) {
        Vec3 p{}; inv_vec3(mj, "get_Position", p);
        LOGI("%s   MuzzleJoint (%s) pos=(%.3f %.3f %.3f)", TAG, tname(mj).c_str(), p.x, p.y, p.z);
    }
    if (inv_mat4(w, "get_MuzzleJointWorldMatrix", m)) LOGI("%s   MuzzleJointWorldMatrix t=(%.3f %.3f %.3f)", TAG, m.m[12], m.m[13], m.m[14]);
    log_nullable_mat("IKLeftArmMatrix", inv_nullable_mat4(w, "getIKLeftArmMatrix", has, m), has, m);
    log_nullable_mat("AidTargetWorldMatrix", inv_nullable_mat4(w, "get_AidTargetWorldMatrix", has, m), has, m);

    g.w_narrow = g.weapon_transform != nullptr ? inv_ptr(g.weapon_transform, "getJointByHash", {(void*)(uintptr_t)narrow}) : nullptr;
    g.w_wide   = g.weapon_transform != nullptr ? inv_ptr(g.weapon_transform, "getJointByHash", {(void*)(uintptr_t)wide}) : nullptr;
    if (g.weapon_transform != nullptr) dump_joints(g.weapon_transform, "weapon skeleton", true, nullptr, nullptr);
    LOGI("%s ---- end weapon dump ----", TAG);
}

// Every method and field of an object's type (parent chain included) whose name contains one of the
// hunt words. For the two unread IK objects — what places the support hand may be in here.
void dump_type_surface(API::ManagedObject* o, const char* label) {
    if (o == nullptr) { LOGI("%s   %s: null", TAG, label); return; }
    static const char* words[] = {"target", "weight", "enable", "joint"};
    LOGI("%s   %s: %s", TAG, label, tname(o).c_str());
    int lines = 0;
    for (auto* td = o->get_type_definition(); td != nullptr && lines < 120; td = td->get_parent_type()) {
        const auto tn = td->get_full_name();
        if (tn == "System.Object" || tn == "via.Component" || tn == "via.Base") break;
        for (auto* m : td->get_methods()) {
            const auto ln = lower(m->get_name());
            bool hit = false; for (auto* w : words) hit = hit || ln.find(w) != std::string::npos;
            if (!hit) continue;
            auto* rt = m->get_return_type();
            std::string ps;
            for (const auto& p : m->get_params()) {
                auto* pt = (API::TypeDefinition*)p.t;
                ps += (ps.empty() ? "" : ", ") + std::string(pt != nullptr ? pt->get_full_name() : "?") + " " + (p.name != nullptr ? p.name : "");
            }
            LOGI("%s     %s :: %s %s(%s)", TAG, tn.c_str(), rt != nullptr ? rt->get_full_name().c_str() : "void", m->get_name(), ps.c_str());
            if (++lines >= 120) break;
        }
        for (auto* f : td->get_fields()) {
            const auto ln = lower(f->get_name());
            bool hit = false; for (auto* w : words) hit = hit || ln.find(w) != std::string::npos;
            if (!hit) continue;
            auto* ft = f->get_type();
            LOGI("%s     %s :: field %s %s @+0x%x%s", TAG, tn.c_str(), ft != nullptr ? ft->get_full_name().c_str() : "?", f->get_name(), f->get_offset_from_base(), f->is_static() ? " (static)" : "");
            if (++lines >= 120) break;
        }
    }
    if (lines >= 120) LOGW("%s     (surface dump capped at 120 lines)", TAG);
}

void dump_ik() {
    auto* ik = g.ik;
    if (ik == nullptr) { LOGI("%s ik: none", TAG); return; }
    LOGI("%s ---- IK DUMP: %s ----", TAG, tname(ik).c_str());
    // The two unread candidates for what places the support hand (board, 2026-09-04 22:30).
    dump_type_surface(inv_ptr(ik, "getIkTwoArm"), "getIkTwoArm()");
    dump_type_surface(inv_ptr(ik, "getIkHand"), "getIkHand()");
    LOGI("%s   UseIkArm=%d UseIkWrist=%d UseIkArmFitAsWrist=%d WristKind=%u WristSolveMode=%u", TAG,
         (int)inv_bool(ik, "get_UseIkArm"), (int)inv_bool(ik, "get_UseIkWrist"), (int)inv_bool(ik, "get_UseIkArmFitAsWrist"),
         inv_u32(ik, "get_WristKind"), inv_u32(ik, "get_WristSolveMode"));
    static const char* kinds[] = {"LEG", "SPINE", "LOOKAT", "ARM", "ARMFIT", "HAND"};
    for (int k = 0; k < 6; ++k) {
        LOGI("%s   isEnabled(%s)=%d", TAG, kinds[k], (int)inv_bool_i(ik, "isEnabled", k));
    }
    auto* arms = inv_ptr(ik, "get_ArmStatusList");
    const auto n = arr_count(arms);
    auto* ctl = inv_ptr(ik, "get_ControlStatus");
    LOGI("%s   ArmStatusList: %u entries; ControlStatus: %u entries", TAG, arms != nullptr ? n : 0, ctl != nullptr ? arr_count(ctl) : 0);
    for (uint32_t i = 0; arms != nullptr && i < n && i < 8; ++i) {
        auto* st = arr_ptr_at(arms, i);
        if (!is_managed(st)) { LOGW("%s   arm[%u] not managed", TAG, i); break; }
        const auto has = field_at<uint8_t>(st, 0x20);
        const auto ap = field_at<Vec3>(st, 0x30);
        LOGI("%s   arm[%u] Index=%d AdjustMode=%d(0 NONE,1 CANCEL,2 FIT) ActivateTime=%.3f ResetTime=%.3f AdjustedPoint=%s(%.3f %.3f %.3f)", TAG, i,
             field_at<int32_t>(st, 0x10), field_at<int32_t>(st, 0x1c), field_at<float>(st, 0x14), field_at<float>(st, 0x18),
             has ? "" : "none ", ap.x, ap.y, ap.z);
        if (auto* m = find_method_deep(st->get_type_definition(), "get_TargetPosition"); m != nullptr) {
            auto r = m->invoke(st, std::vector<void*>{});
            const float* f = (const float*)r.bytes.data();
            LOGI("%s          get_TargetPosition -> %s raw=(%.3f %.3f %.3f %.3f) exc=%d", TAG,
                 m->get_return_type() != nullptr ? m->get_return_type()->get_full_name().c_str() : "?", f[0], f[1], f[2], f[3], (int)r.exception_thrown);
        }
        if (auto* m = find_method_deep(st->get_type_definition(), "get_DampingPosition"); m != nullptr) {
            auto r = m->invoke(st, std::vector<void*>{});
            const float* f = (const float*)r.bytes.data();
            LOGI("%s          get_DampingPosition -> %s raw=(%.3f %.3f %.3f %.3f) exc=%d", TAG,
                 m->get_return_type() != nullptr ? m->get_return_type()->get_full_name().c_str() : "?", f[0], f[1], f[2], f[3], (int)r.exception_thrown);
        }
    }
    LOGI("%s ---- end IK dump ----", TAG);
}

void dump_layers(bool force_table) {
    auto* mo = g.motion;
    if (mo == nullptr) return;
    const auto n = inv_u32(mo, "getLayerCount");
    if (n == 0xFFFFFFFFu || n > 64) { if (force_table) LOGW("%s motion: getLayerCount=%u", TAG, n); return; }
    if (g.layer_last.size() != n) g.layer_last.assign(n, "");
    const auto sec = (uint64_t)now_s();
    if (sec != g.layer_second) { g.layer_second = sec; g.layer_lines_this_second = 0; }
    if (force_table) LOGI("%s ---- MOTION LAYERS (%u) TargetBankType=%u ----", TAG, n, inv_u32(mo, "get_TargetBankType"));
    for (uint32_t i = 0; i < n; ++i) {
        auto* layer = inv_ptr(mo, "getLayer", {(void*)(uintptr_t)i});
        if (layer == nullptr) continue;
        auto* node = inv_ptr(layer, "get_HighestWeightMotionNode");
        const std::string name = node != nullptr ? sysstr(inv_ptr(node, "get_MotionName")) : "-";
        const bool changed = name != g.layer_last[i];
        if (changed || force_table) {
            if (!force_table && g.layer_lines_this_second++ > 24) { g.layer_last[i] = name; continue; }
            LOGI("%s   layer[%u] %s motion=%-36s speed=%.3f blend=%.2f bank=%u id=%u frame=%.0f/%.0f", TAG, i, changed ? "CHG" : "   ",
                 name.c_str(), inv_f32(layer, "get_Speed"), inv_f32(layer, "get_BlendRate"),
                 inv_u32(layer, "get_MotionBankID"), inv_u32(layer, "get_MotionID"), inv_f32(layer, "get_Frame"), inv_f32(layer, "get_EndFrame"));
            g.layer_last[i] = name;
        }
    }
}

void summary_line() {
    Vec3 lh{}, rh{}, aid{};
    const bool have_lh = g.l_hand != nullptr && inv_vec3(g.l_hand, "get_Position", lh);
    const bool have_rh = g.r_hand != nullptr && inv_vec3(g.r_hand, "get_Position", rh);
    if (g.aid_joint == nullptr && g.weapon != nullptr) {
        g.aid_joint = inv_ptr(g.weapon, "get_AidJoint");
        if (g.aid_joint != nullptr) LOGI("%s AidJoint appeared: %s", TAG, joint_name(g.aid_joint).c_str());
    }
    const bool have_aid = g.aid_joint != nullptr && inv_vec3(g.aid_joint, "get_Position", aid);
    Vec3 wn{}, ww{};
    const bool have_wn = g.w_narrow != nullptr && inv_vec3(g.w_narrow, "get_Position", wn);
    const bool have_ww = g.w_wide != nullptr && inv_vec3(g.w_wide, "get_Position", ww);
    const bool hold = inv_bool(g.cond, "get_IsHold");
    const auto aidtype = g.equipment != nullptr ? inv_u32(g.equipment, "getAidJointType") : 0;

    Mat4 ikm{}; bool ik_has = false;
    const bool ik_ok = g.weapon != nullptr && inv_nullable_mat4(g.weapon, "getIKLeftArmMatrix", ik_has, ikm);

    char buf[640];
    int o = snprintf(buf, sizeof buf, "%s f=%llu hold=%d aidT=%u", TAG, (unsigned long long)g.frame, (int)hold, aidtype);
    if (have_lh) o += snprintf(buf + o, sizeof buf - o, " Lhand=(%.2f %.2f %.2f)", lh.x, lh.y, lh.z);
    if (have_rh) o += snprintf(buf + o, sizeof buf - o, " Rhand=(%.2f %.2f %.2f)", rh.x, rh.y, rh.z);
    if (have_aid) o += snprintf(buf + o, sizeof buf - o, " aid=(%.2f %.2f %.2f)", aid.x, aid.y, aid.z);
    if (have_lh && have_aid) o += snprintf(buf + o, sizeof buf - o, " |Lhand-aid|=%.3f", dist(lh, aid));
    if (have_lh && have_wn) o += snprintf(buf + o, sizeof buf - o, " |Lhand-narrow|=%.3f", dist(lh, wn));
    if (have_lh && have_ww) o += snprintf(buf + o, sizeof buf - o, " |Lhand-wide|=%.3f", dist(lh, ww));
    if (have_rh && have_lh) o += snprintf(buf + o, sizeof buf - o, " |Lhand-Rhand|=%.3f", dist(lh, rh));
    if (ik_ok) {
        if (ik_has) o += snprintf(buf + o, sizeof buf - o, " ikL=(%.2f %.2f %.2f)", ikm.m[12], ikm.m[13], ikm.m[14]);
        else o += snprintf(buf + o, sizeof buf - o, " ikL=none");
    }
    o += snprintf(buf + o, sizeof buf - o, " shift=%d hooks(aid=%u ikL=%u)", g.shift_mode, g_calls_aid.exchange(0), g_calls_ikl.exchange(0));
    if (bridge_live()) {
        const float* f = arr_f32(g.bridge);
        o += snprintf(buf + o, sizeof buf - o, " | vr f=%.0f hmd=%.0f ctl=%.0f", f[S_FRAME], f[S_HMD_ACTIVE], f[S_USING_CTL]);
        if (f[S_USING_CTL] != 0.0f) {
            const Vec3 lc = bridge_vec3(S_LPOS), rc = bridge_vec3(S_RPOS);
            o += snprintf(buf + o, sizeof buf - o, " Lctl=(%.2f %.2f %.2f) Rctl=(%.2f %.2f %.2f)", lc.x, lc.y, lc.z, rc.x, rc.y, rc.z);
            if (have_aid) o += snprintf(buf + o, sizeof buf - o, " |Lctl-aid|=%.3f", dist(lc, aid));
            if (have_lh) o += snprintf(buf + o, sizeof buf - o, " |Lctl-Lhand|=%.3f", dist(lc, lh));
            if (have_rh) o += snprintf(buf + o, sizeof buf - o, " |Rctl-Rhand|=%.3f", dist(rc, rh));
        }
        o += snprintf(buf + o, sizeof buf - o, " stick=(%.2f %.2f)", f[S_LSTICK], f[S_LSTICK + 1]);
    } else {
        o += snprintf(buf + o, sizeof buf - o, " | vr bridge: not attached");
    }
    LOGI("%s", buf);
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

bool game_is_foreground() {
    HWND h = GetForegroundWindow();
    if (h == nullptr) return false;
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId();
}

// app.ropeway.InputDefine.Kind values learned by the Lua probes (2026-08-27): HOLD=64, ATTACK=256.
constexpr int KIND_HOLD = 64;
constexpr int KIND_ATTACK = 256;
// via.motion.IkKind as the IkController indexes it (isEnabled(k) read 2026-09-04): LEG 0, SPINE 1, LOOKAT 2, ARM 3, ARMFIT 4, HAND 5.
constexpr int KIND_ARM = 3;
void set_force(int kind, bool on) {
    auto* is = API::get()->get_managed_singleton("app.ropeway.InputSystem");
    if (is == nullptr) { LOGW("%s InputSystem singleton missing", TAG); return; }
    auto r = inv(is, "setForce", {(void*)(uintptr_t)kind, (void*)(uintptr_t)(on ? 1 : 0)});
    LOGI("%s setForce(kind=%d, %s) %s", TAG, kind, on ? "true" : "false", r.ok ? "ok" : "THREW/NOT FOUND");
}

void poll_hotkeys() {
    static bool prev[5] = {false, false, false, false, false};
    const int vks[6] = {VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6};
    static bool prev6 = false;
    if (!game_is_foreground()) return;
    {
        const bool down = (GetAsyncKeyState(vks[5]) & 0x8000) != 0;
        if (down && !prev6) {
            g.shift_mode = (g.shift_mode + 1) % 4;
            LOGI("%s NUM6: aid-target shift mode -> %d (bit1 AidTargetWorldMatrix, bit2 IKLeftArmMatrix; +0.10 m up)", TAG, g.shift_mode);
        }
        prev6 = down;
    }
    for (int i = 0; i < 5; ++i) {
        const bool down = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
        if (down && !prev[i]) {
            if (i == 0) { g.want_dump = true; LOGI("%s NUM7: dump requested", TAG); }
            if (i == 1) { g.trace = !g.trace; LOGI("%s NUM8: trace %s", TAG, g.trace ? "ON (10 Hz)" : "OFF (1 Hz)"); }
            if (i == 2) { g.want_layers = true; LOGI("%s NUM9: layer table requested", TAG); }
            if (i == 3) { g.force_hold = !g.force_hold; LOGI("%s NUM4: force HOLD %s", TAG, g.force_hold ? "ON" : "OFF"); set_force(KIND_HOLD, g.force_hold); }
            // 8 frames (~130 ms): a real trigger press, long enough for the aim FSM to see it on a frame where HOLD is up.
            if (i == 4) { g.attack_pulse = 8; LOGI("%s NUM5: ATTACK pulse (8 frames)", TAG); set_force(KIND_ATTACK, true); }
        }
        prev[i] = down;
    }
}

void rebind_player(API::ManagedObject* pm, API::ManagedObject* go) {
    g.player_go = go;
    g.cond = inv_ptr(pm, "get_CurrentPlayerCondition");
    g.equipment = inv_ptr(g.cond, "get_Equipment");
    g.ik = inv_ptr(g.cond, "get_IkController");
    g.transform = inv_ptr(go, "get_Transform");
    g.motion = get_component(go, "via.motion.Motion");
    g.l_hand = nullptr; g.r_hand = nullptr; g.aid_joint = nullptr;
    g.weapon = nullptr; g.weapon_transform = nullptr;
    g.layer_last.clear();
    LOGI("%s PLAYER BOUND: go=%p (%s) cond=%p (%s) equipment=%p ik=%p transform=%p motion=%p", TAG,
         (void*)go, sysstr(inv_ptr(go, "get_Name")).c_str(), (void*)g.cond, tname(g.cond).c_str(), (void*)g.equipment, (void*)g.ik, (void*)g.transform, (void*)g.motion);
    if (g.transform != nullptr) {
        dump_joints(g.transform, "player skeleton (hand/arm/weapon names only)", false, &g.l_hand, &g.r_hand);
        LOGI("%s   l_hand=%s r_hand=%s", TAG, g.l_hand != nullptr ? joint_name(g.l_hand).c_str() : "NOT FOUND", g.r_hand != nullptr ? joint_name(g.r_hand).c_str() : "NOT FOUND");
    }
    dump_ik();
}

void on_frame() {
    if (!g_api_ok.load()) return;
    auto& api = API::get();
    g.frame++;
    poll_hotkeys();

    auto* pm = api->get_managed_singleton("app.ropeway.PlayerManager");
    if (pm == nullptr) return;
    auto* go = inv_ptr(pm, "get_CurrentPlayer");
    if (go == nullptr) {
        if (g.player_go != nullptr) { LOGI("%s player gone (scene change?) — unbinding", TAG); g = State{.bridge = g.bridge, .frame = g.frame, .trace = g.trace}; }
        return;
    }
    if (go != g.player_go) rebind_player(pm, go);

    auto* w = inv_ptr(g.equipment, "get_EquipWeapon");
    if (w != g.weapon) {
        g.weapon = w;
        g.aid_joint = nullptr; g.w_narrow = nullptr; g.w_wide = nullptr;
        g.weapon_transform = nullptr;
        if (w != nullptr) {
            auto* wgo = inv_ptr(w, "get_GameObject");
            g.weapon_transform = inv_ptr(wgo, "get_Transform");
            LOGI("%s WEAPON CHANGED -> %s (go=%s)", TAG, tname(w).c_str(), sysstr(inv_ptr(wgo, "get_Name")).c_str());
        } else {
            LOGI("%s WEAPON CHANGED -> none", TAG);
        }
        dump_weapon();
    }
    if (g.attack_pulse > 0 && --g.attack_pulse == 0) set_force(KIND_ATTACK, false);
    {
        static bool last_hold = false;
        const bool hold = inv_bool(g.cond, "get_IsHold");
        if (hold != last_hold) { last_hold = hold; LOGI("%s IsHold -> %d (force_hold=%d)", TAG, (int)hold, (int)g.force_hold); if (hold) { dump_weapon(); dump_ik(); } }
    }
    if (g.want_dump) { g.want_dump = false; dump_weapon(); dump_ik(); dump_layers(true); }
    if (g.want_layers) { g.want_layers = false; dump_layers(true); }

    dump_layers(false);   // logs only on change

    const double t = now_s();
    const double period = g.trace ? 0.1 : 1.0;
    if (t - g.last_summary_t >= period) { g.last_summary_t = t; summary_line(); }
    if (g.trace && t - g.last_layer_table_t >= 1.0) { g.last_layer_table_t = t; dump_layers(true); }
}

// ---------------------------------------------------------------------------
// Lua bridge: hook System.GC.KeepAlive, catch the handed-over float array.
// ---------------------------------------------------------------------------

int pre_mailbox(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    // Identify our array on EVERY call: a second hand-over must also be swallowed,
    // or the game's setter would be run with a float array as its argument.
    bool ours = false;
    for (int i = 0; i < argc && i < 8; ++i) {
        auto* o = (API::ManagedObject*)argv[i];
        if (!is_managed(o)) continue;
        if (tname(o) != "System.Single[]") continue;
        if (!g_arr_measured && !measure_array_layout(o)) continue;
        const auto n = arr_count(o);
        float* f = arr_f32(o);
        if (n != 64 || f[S_SENTINEL] != 12345.0f) { LOGW("%s mailbox saw a System.Single[%u] slot63=%.1f — not ours", TAG, n, f[S_SENTINEL]); continue; }
        ours = true;
        if (g.bridge == nullptr) {
            o->add_ref();
            g.bridge = o;
            f[S_ACK] = 1.0f;
            LOGI("%s VR BRIDGE ATTACHED (argv[%d] of %d, array %p) — Lua shim poses are live", TAG, i, argc, (void*)o);
        }
        break;
    }
    return ours ? REFRAMEWORK_HOOK_SKIP_ORIGINAL : REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

// ---------------------------------------------------------------------------
// v0.3: the aid-target override. Both getters return System.Nullable`1<via.mat4> through a hidden
// return-buffer pointer, so at return RAX holds that pointer: *ret_val -> { u8 HasValue @0, mat4 @+0x10 }.
// The post-hook edits the buffer before the caller reads it. Call counts tell whether the GAME reads
// these getters at all (our own summary reads them ~1/s, the game would be ~60/s).
// ---------------------------------------------------------------------------

int pre_passthrough(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) { return REFRAMEWORK_HOOK_CALL_ORIGINAL; }

void shift_nullable_mat4(void** ret_val, int bit) {
    if ((g.shift_mode & bit) == 0 || ret_val == nullptr) return;
    auto* nb = (uint8_t*)*ret_val;
    if (nb == nullptr || nb[0] == 0) return;
    float* m = (float*)(nb + 0x10);
    m[13] += 0.10f;
}
void post_aid_target(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) { g_calls_aid++; shift_nullable_mat4(ret_val, 1); }
void post_ik_left_arm(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) { g_calls_ikl++; shift_nullable_mat4(ret_val, 2); }

void install_shift_hooks() {
    auto& api = API::get();
    auto* a = api->tdb()->find_method("app.ropeway.implement.Implement", "get_AidTargetWorldMatrix");
    auto* b = api->tdb()->find_method("app.ropeway.implement.Implement", "getIKLeftArmMatrix");
    if (a != nullptr) { const auto id = a->add_hook(pre_passthrough, post_aid_target, false); LOGI("%s hook get_AidTargetWorldMatrix id=%u fn=%p", TAG, id, a->get_function_raw()); }
    else LOGE("%s Implement.get_AidTargetWorldMatrix not found — no aid-target hook", TAG);
    if (b != nullptr) { const auto id = b->add_hook(pre_passthrough, post_ik_left_arm, false); LOGI("%s hook getIKLeftArmMatrix id=%u fn=%p", TAG, id, b->get_function_raw()); }
    else LOGE("%s Implement.getIKLeftArmMatrix not found — no IK-left-arm hook", TAG);
}

void install_bridge_hook() {
    auto& api = API::get();
    auto* m = api->tdb()->find_method("app.ropeway.RagdollControlZoneManager", "set_AccessMutex");
    if (m == nullptr) { LOGE("%s mailbox method RagdollControlZoneManager.set_AccessMutex not found — VR bridge unavailable", TAG); return; }
    const auto id = m->add_hook(pre_mailbox, nullptr, false);
    LOGI("%s bridge mailbox hook installed on RagdollControlZoneManager.set_AccessMutex (id=%u, fn=%p) — check the HookManager lines above for 'Failed to hook'", TAG, id, m->get_function_raw());
}

void on_initialized() {
    install_bridge_hook();
    install_shift_hooks();
}

} // namespace

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    g_param = param;
    const auto* fns = param->functions;
    try {
        API::initialize(param);
        g_api_ok.store(true);
    } catch (...) {
        fns->log_error("%s C++ SDK wrapper init failed — probe disabled", TAG);
        return true;
    }
    fns->log_info("%s native core v0.3 loaded — reqs-1-and-3 recon probe. NUM7 dump / NUM8 trace / NUM9 layers / NUM4 force-HOLD toggle / NUM5 ATTACK pulse / NUM6 aid-target shift (hooked getters, +10 cm)", TAG);
    // Hooks need the TDB up; register them from the game thread on the first frame.
    static std::atomic<bool> hooked{false};
    fns->on_pre_application_entry("LockScene", []() {
        if (!hooked.exchange(true)) on_initialized();
        on_frame();
    });
    return true;
}
