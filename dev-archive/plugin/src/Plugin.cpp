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

// v0.4 (2026-09-05): THE DOCK, built on the lever v0.3 found. The game's wrist solver reads
// Implement.getIKLeftArmMatrix() once per frame (which reads get_AidTargetWorldMatrix(), which is
// the aid joint _101's world matrix) and puts l_arm_wrist exactly where the returned matrix says,
// snapping. So the dock is a post-hook on the OUTER getter that returns a BLENDED matrix:
//   * docked   = the bridge's left grip held (S_LGRIP > 0.5 with controllers active) OR the NUM6
//                flat stand-in (a synthetic target orbiting _101 at 10 cm, yawed 45 deg);
//   * weight w slews 0 -> 1 over DOCK_BLEND_S (0.2 s) on dock and back on release, eased;
//   * translation = lerp(natural, target, w); rotation = slerp(natural, target, w) (NUM3 toggles
//     the rotation write, so a bad rotation can be ruled out in VR without a rebuild);
//   * HOLD is latched natively (setForce(64,true)) while docked and dropped on release, reconciled
//     with the NUM4 manual latch (spec v2.3 reqs 1-2; req 3 is the same blend run backwards).
// The controller pose reaches world space two ways, NUM1 cycles them: mode 0 re-bases the
// controller relative to the HMD onto the game camera's world matrix (right whichever space the
// bridge's poses are in, as long as HMD and controller share it); mode 1 uses the bridge pose as
// world directly. Neither has run in a headset yet. The trace logs, per sample: |Lwrist - target|,
// |Lwrist - natural|, the wrist's rotation error against both (does the rotation follow?), and
// |Rwrist - muzzle| (the right hand must not move on dock/undock).

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
Vec3 vsub(const Vec3& a, const Vec3& b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 vadd(const Vec3& a, const Vec3& b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 vscale(const Vec3& a, float s) { return Vec3{a.x * s, a.y * s, a.z * s}; }
Vec3 vlerp(const Vec3& a, const Vec3& b, float t) { return Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }
float vlen(const Vec3& a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

// ---- rotation helpers. A 3x3 "rows" block is the top-left of an RE Engine row-major mat4: row i
// is basis vector i of the frame, in world coordinates. Quaternions here are only ever produced
// from and consumed by these helpers (rows -> quat -> rows round-trips exactly), so the blend does
// not depend on the engine's quaternion conventions. The one place a FOREIGN quaternion enters is
// the VR bridge (controller/HMD rotation) — see world_from_bridge(); that path is unverified.
struct Rows { float r[9]{1, 0, 0, 0, 1, 0, 0, 0, 1}; };
Rows rows_of(const Mat4& m) { Rows o; for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) o.r[i * 3 + j] = m.m[i * 4 + j]; return o; }
void rows_into(Mat4& m, const Rows& o) { for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) m.m[i * 4 + j] = o.r[i * 3 + j]; }
Vec3 row(const Rows& o, int i) { return Vec3{o.r[i * 3], o.r[i * 3 + 1], o.r[i * 3 + 2]}; }
// angle in degrees between two frames: acos((tr(A^T B) - 1) / 2), pure matrix, convention-free
float rows_angle_deg(const Rows& a, const Rows& b) {
    float tr = 0.f;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) tr += a.r[i * 3 + j] * b.r[i * 3 + j];   // tr(A^T B) = sum a_ij b_ij
    float c = (tr - 1.f) * 0.5f; c = std::clamp(c, -1.f, 1.f);
    return std::acos(c) * 57.29578f;
}
// rotate every row (a world vector) by a world yaw of `deg` about +Y
Rows rows_yaw(const Rows& o, float deg) {
    const float a = deg * 0.01745329f, c = std::cos(a), s = std::sin(a);
    Rows out;
    for (int i = 0; i < 3; ++i) { const Vec3 v = row(o, i); out.r[i * 3] = c * v.x + s * v.z; out.r[i * 3 + 1] = v.y; out.r[i * 3 + 2] = -s * v.x + c * v.z; }
    return out;
}
Quat quat_of_rows(const Rows& o) {   // standard Shepperd, treating rows as the matrix R with R[i][j] = r[i*3+j]
    const float* r = o.r; Quat q{};
    const float tr = r[0] + r[4] + r[8];
    if (tr > 0.f) { const float s = std::sqrt(tr + 1.f) * 2.f; q.w = 0.25f * s; q.x = (r[7] - r[5]) / s; q.y = (r[2] - r[6]) / s; q.z = (r[3] - r[1]) / s; }
    else if (r[0] > r[4] && r[0] > r[8]) { const float s = std::sqrt(1.f + r[0] - r[4] - r[8]) * 2.f; q.w = (r[7] - r[5]) / s; q.x = 0.25f * s; q.y = (r[1] + r[3]) / s; q.z = (r[2] + r[6]) / s; }
    else if (r[4] > r[8]) { const float s = std::sqrt(1.f + r[4] - r[0] - r[8]) * 2.f; q.w = (r[2] - r[6]) / s; q.x = (r[1] + r[3]) / s; q.y = 0.25f * s; q.z = (r[5] + r[7]) / s; }
    else { const float s = std::sqrt(1.f + r[8] - r[0] - r[4]) * 2.f; q.w = (r[3] - r[1]) / s; q.x = (r[2] + r[6]) / s; q.y = (r[5] + r[7]) / s; q.z = 0.25f * s; }
    return q;
}
Rows rows_of_quat(const Quat& q) {   // inverse of quat_of_rows
    Rows o; const float x = q.x, y = q.y, z = q.z, w = q.w;
    o.r[0] = 1 - 2 * (y * y + z * z); o.r[1] = 2 * (x * y - z * w);     o.r[2] = 2 * (x * z + y * w);
    o.r[3] = 2 * (x * y + z * w);     o.r[4] = 1 - 2 * (x * x + z * z); o.r[5] = 2 * (y * z - x * w);
    o.r[6] = 2 * (x * z - y * w);     o.r[7] = 2 * (y * z + x * w);     o.r[8] = 1 - 2 * (x * x + y * y);
    return o;
}
Quat quat_norm(Quat q) { const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w); if (n > 1e-6f) { q.x /= n; q.y /= n; q.z /= n; q.w /= n; } else q = Quat{0, 0, 0, 1}; return q; }
Quat quat_slerp(Quat a, Quat b, float t) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0.f) { d = -d; b = Quat{-b.x, -b.y, -b.z, -b.w}; }
    if (d > 0.9995f) return quat_norm(Quat{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t});
    const float th = std::acos(d), s = std::sin(th), wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
    return quat_norm(Quat{a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb});
}
Rows rows_slerp(const Rows& a, const Rows& b, float t) { return rows_of_quat(quat_slerp(quat_of_rows(a), quat_of_rows(b), t)); }
// plain 3x3 algebra on Rows, indices r[i*3+j] = row i, column j
Rows rows_mul(const Rows& a, const Rows& b) { Rows o; for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) { float s = 0; for (int k = 0; k < 3; ++k) s += a.r[i * 3 + k] * b.r[k * 3 + j]; o.r[i * 3 + j] = s; } return o; }
Rows rows_T(const Rows& a) { Rows o; for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) o.r[i * 3 + j] = a.r[j * 3 + i]; return o; }
Vec3 vec_mul_rows(const Vec3& v, const Rows& R) {   // row vector times matrix: out_j = sum_i v_i R[i][j]
    return Vec3{v.x * R.r[0] + v.y * R.r[3] + v.z * R.r[6], v.x * R.r[1] + v.y * R.r[4] + v.z * R.r[7], v.x * R.r[2] + v.y * R.r[5] + v.z * R.r[8]};
}
// Hamilton product and conjugate, used only on the VR path
Quat quat_mul(const Quat& a, const Quat& b) {
    return Quat{a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
Quat quat_conj(const Quat& q) { return Quat{-q.x, -q.y, -q.z, q.w}; }
Vec3 quat_rotate(const Quat& q, const Vec3& v) {
    const Quat p{v.x, v.y, v.z, 0.f};
    const Quat r = quat_mul(quat_mul(q, p), quat_conj(q));
    return Vec3{r.x, r.y, r.z};
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
    // v0.6: the left arm chain above the wrist, for the reach clamp. RE2's pl1000 skeleton names them
    // l_arm_clavicle / l_arm_humerus / l_arm_radius / l_arm_wrist (verified-live 2026-09-04); the humerus is
    // the shoulder end the arm pivots about and radius is the elbow, so |hu-ra| + |ra-wr| is the arm's length.
    API::ManagedObject* l_humerus{};
    API::ManagedObject* l_radius{};
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
    } dock;
    std::vector<std::string> layer_last;   // last highest-weight motion name per layer
    uint64_t layer_lines_this_second{};
    uint64_t layer_second{};
    double last_summary_t{};
    double last_layer_table_t{};
} g;

// v0.3 hook call counters (the hooks themselves are defined with the bridge hook below).
std::atomic<uint32_t> g_calls_aid{0}, g_calls_ikl{0};
// v0.4.1: true while the plugin itself is calling the hooked getters (summary / dumps), so the hooks can tell
// the plugin's own reads apart from the game's per-frame read and never take them as the blend origin.
bool g_self_call = false;
struct SelfCall { SelfCall() { g_self_call = true; } ~SelfCall() { g_self_call = false; } };

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
        // v0.6: the reach clamp needs the two joints above the wrist. Bound here rather than by a second walk of
        // the joint array, and only on the player's own skeleton (dump_joints is called for the weapon too).
        if (l_hand != nullptr && g.l_humerus == nullptr && ln == "l_arm_humerus") g.l_humerus = j;
        if (l_hand != nullptr && g.l_radius  == nullptr && ln == "l_arm_radius")  g.l_radius  = j;
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
    { SelfCall sc; log_nullable_mat("IKLeftArmMatrix", inv_nullable_mat4(w, "getIKLeftArmMatrix", has, m), has, m); }
    { SelfCall sc; log_nullable_mat("AidTargetWorldMatrix", inv_nullable_mat4(w, "get_AidTargetWorldMatrix", has, m), has, m); }

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
    bool ik_ok = false;
    { SelfCall sc; ik_ok = g.weapon != nullptr && inv_nullable_mat4(g.weapon, "getIKLeftArmMatrix", ik_has, ikm); }

    char buf[1024];
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
    // v0.4 dock report: where the wrist is against the target and against the natural (un-hooked) value, whether
    // its ROTATION followed (angle vs target and vs natural), and whether the RIGHT hand stayed on the muzzle.
    {
        auto& d = g.dock;
        o += snprintf(buf + o, sizeof buf - o, " | dock=%d(syn=%d lg=%d) w=%.2f rot=%d sp=%d hooks(aid=%u ikL=%u)",
                      (int)d.docked, (int)d.synthetic, (int)d.lg_held, d.w_eased, (int)d.write_rot, d.space_mode, g_calls_aid.exchange(0), g_calls_ikl.exchange(0));
        if (have_lh && d.target_valid) o += snprintf(buf + o, sizeof buf - o, " |Lw-tgt|=%.3f", dist(lh, d.target_t));
        if (have_lh && d.natural_valid) o += snprintf(buf + o, sizeof buf - o, " |Lw-nat|=%.3f", dist(lh, Vec3{d.natural.m[12], d.natural.m[13], d.natural.m[14]}));
        // v0.4.1: the four positions side by side — the game's natural (blend origin), the plugin's own read of the
        // same getter, the inner getter's value on the game's call, and the target — each as a distance from the joint.
        if (have_aid) {
            if (d.natural_valid)      o += snprintf(buf + o, sizeof buf - o, " |natG-aid|=%.3f", dist(Vec3{d.natural.m[12], d.natural.m[13], d.natural.m[14]}, aid));
            if (d.natural_self_valid) o += snprintf(buf + o, sizeof buf - o, " |natS-aid|=%.3f", dist(Vec3{d.natural_self.m[12], d.natural_self.m[13], d.natural_self.m[14]}, aid));
            if (d.inner_valid)        o += snprintf(buf + o, sizeof buf - o, " |inner-aid|=%.3f", dist(d.inner_t, aid));
            if (d.target_valid)       o += snprintf(buf + o, sizeof buf - o, " |tgt-aid|=%.3f", dist(d.target_t, aid));
        }
        if (d.M_valid) { Rows I; o += snprintf(buf + o, sizeof buf - o, " angM=%.0f", rows_angle_deg(d.M, I)); }
        // v0.6: the arm's measured length and whether this frame's target was outside it. clamp=0.000 with a
        // non-zero reach means the target was in range; a reach of 0 means the two arm joints were never bound.
        if (d.reach_valid) o += snprintf(buf + o, sizeof buf - o, " reach=%.3f clamp=%.3f%s", d.reach, d.clamped_m, d.use_reach_clamp ? "" : "(OFF)");
        else               o += snprintf(buf + o, sizeof buf - o, " reach=none");
        if (d.natural_valid && d.natural_self_valid)
            o += snprintf(buf + o, sizeof buf - o, " |natG-natS|=%.3f rotG-S=%.0f", dist(Vec3{d.natural.m[12], d.natural.m[13], d.natural.m[14]}, Vec3{d.natural_self.m[12], d.natural_self.m[13], d.natural_self.m[14]}),
                          rows_angle_deg(rows_of(d.natural), rows_of(d.natural_self)));
        Mat4 wm{};
        if (g.l_hand != nullptr && inv_mat4(g.l_hand, "get_WorldMatrix", wm)) {
            const Rows wr = rows_of(wm);
            if (d.natural_valid) o += snprintf(buf + o, sizeof buf - o, " rotW-N=%.0f", rows_angle_deg(wr, rows_of(d.natural)));
            if (d.target_valid)  o += snprintf(buf + o, sizeof buf - o, " rotW-T=%.0f", rows_angle_deg(wr, d.target_r));
            // once per second while the dock is active: the three frames in full, for the rotation-convention check offline
            static double last_rows_t = 0.0;
            if ((d.docked || d.w > 0.f) && d.natural_valid && d.target_valid && now_s() - last_rows_t >= 1.0) {
                last_rows_t = now_s();
                const Rows nr = rows_of(d.natural);
                LOGI("%s ROWS natG=[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f] t=(%.3f %.3f %.3f)", TAG, nr.r[0], nr.r[1], nr.r[2], nr.r[3], nr.r[4], nr.r[5], nr.r[6], nr.r[7], nr.r[8], d.natural.m[12], d.natural.m[13], d.natural.m[14]);
                LOGI("%s ROWS tgt =[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f] t=(%.3f %.3f %.3f)", TAG, d.target_r.r[0], d.target_r.r[1], d.target_r.r[2], d.target_r.r[3], d.target_r.r[4], d.target_r.r[5], d.target_r.r[6], d.target_r.r[7], d.target_r.r[8], d.target_t.x, d.target_t.y, d.target_t.z);
                LOGI("%s ROWS wrst=[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f] t=(%.3f %.3f %.3f)", TAG, wr.r[0], wr.r[1], wr.r[2], wr.r[3], wr.r[4], wr.r[5], wr.r[6], wr.r[7], wr.r[8], wm.m[12], wm.m[13], wm.m[14]);
                if (have_aid) LOGI("%s ROWS aid=(%.3f %.3f %.3f) Lwrist=(%.3f %.3f %.3f)", TAG, aid.x, aid.y, aid.z, lh.x, lh.y, lh.z);
            }
        }
        Mat4 mz{};
        if (have_rh && g.weapon != nullptr && inv_mat4(g.weapon, "get_MuzzleJointWorldMatrix", mz))
            o += snprintf(buf + o, sizeof buf - o, " |Rw-muz|=%.3f", dist(rh, Vec3{mz.m[12], mz.m[13], mz.m[14]}));
        if (d.no_value_frames != 0) { o += snprintf(buf + o, sizeof buf - o, " NOVALUE=%u", d.no_value_frames); d.no_value_frames = 0; }
        if (d.cam_valid) o += snprintf(buf + o, sizeof buf - o, " cam=(%.2f %.2f %.2f)", d.cam_t.x, d.cam_t.y, d.cam_t.z);
    }
    if (bridge_live()) {
        const float* f = arr_f32(g.bridge);
        o += snprintf(buf + o, sizeof buf - o, " | vr f=%.0f hmd=%.0f ctl=%.0f", f[S_FRAME], f[S_HMD_ACTIVE], f[S_USING_CTL]);
        if (f[S_USING_CTL] != 0.0f) {
            const Vec3 lc = bridge_vec3(S_LPOS), rc = bridge_vec3(S_RPOS);
            o += snprintf(buf + o, sizeof buf - o, " Lctl=(%.2f %.2f %.2f) Rctl=(%.2f %.2f %.2f)", lc.x, lc.y, lc.z, rc.x, rc.y, rc.z);
            if (have_aid) o += snprintf(buf + o, sizeof buf - o, " |Lctl-aid|=%.3f", dist(lc, aid));
            if (have_lh) o += snprintf(buf + o, sizeof buf - o, " |Lctl-Lhand|=%.3f", dist(lc, lh));
            if (have_rh) o += snprintf(buf + o, sizeof buf - o, " |Rctl-Rhand|=%.3f", dist(rc, rh));
            const Vec3 hc = bridge_vec3(S_HPOS);
            o += snprintf(buf + o, sizeof buf - o, " Hctl=(%.2f %.2f %.2f) LG=%.2f RG=%.2f RT=%.2f", hc.x, hc.y, hc.z, f[S_LGRIP], f[S_RGRIP], f[S_RTRIG]);
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
    static bool prev[9] = {};
    const int vks[9] = {VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD1, VK_NUMPAD3, VK_NUMPAD2};
    if (!game_is_foreground()) return;
    for (int i = 0; i < 9; ++i) {
        const bool down = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
        if (down && !prev[i]) {
            if (i == 0) { g.want_dump = true; LOGI("%s NUM7: dump requested", TAG); }
            if (i == 1) { g.trace = !g.trace; LOGI("%s NUM8: trace %s", TAG, g.trace ? "ON (10 Hz)" : "OFF (1 Hz)"); }
            if (i == 2) { g.want_layers = true; LOGI("%s NUM9: layer table requested", TAG); }
            // HOLD itself is reconciled once per frame from force_hold || dock.docked (see update_dock)
            if (i == 3) { g.force_hold = !g.force_hold; LOGI("%s NUM4: force HOLD %s", TAG, g.force_hold ? "ON" : "OFF"); }
            // 8 frames (~130 ms): a real trigger press, long enough for the aim FSM to see it on a frame where HOLD is up.
            if (i == 4) { g.attack_pulse = 8; LOGI("%s NUM5: ATTACK pulse (8 frames)", TAG); set_force(KIND_ATTACK, true); }
            if (i == 5) { g.dock.synthetic = !g.dock.synthetic; if (g.dock.synthetic) g.dock.orbit_t0 = now_s();
                          LOGI("%s NUM6: synthetic dock (flat stand-in for LG held) %s — target orbits _101 at 10 cm, yawed 45 deg", TAG, g.dock.synthetic ? "ON" : "OFF"); }
            if (i == 8) { g.dock.use_reach_clamp = !g.dock.use_reach_clamp;
                          LOGI("%s NUM2: reach clamp %s (off = the v0.5 target, unclamped)", TAG, g.dock.use_reach_clamp ? "ON" : "OFF"); }
            if (i == 6) { g.dock.space_mode = (g.dock.space_mode + 1) % 3;
                          static const char* names[] = {"controller HMD-relative, re-based on the game camera", "bridge pose used as world directly", "as mode 0 with the camera rows transposed"};
                          LOGI("%s NUM1: VR space mode -> %d (%s)", TAG, g.dock.space_mode, names[g.dock.space_mode]); }
            if (i == 7) { g.dock.write_rot = !g.dock.write_rot; LOGI("%s NUM3: rotation write %s", TAG, g.dock.write_rot ? "ON" : "OFF (translation only)"); }
        }
        prev[i] = down;
    }
}

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

// Game camera world pose. Under REFramework's VR the camera follows the HMD, so this is the anchor the
// controller pose is re-based on in space mode 0. Read through the TDB (via.SceneManager is a NATIVE
// singleton; via.SceneView / via.Camera are called through their type-definitions, as praydog's example
// plugin does) so no managed-header assumption is made on the view object.
void update_camera() {
    auto& d = g.dock;
    d.cam_valid = false;
    auto& api = API::get();
    auto* sm = api->get_native_singleton("via.SceneManager");
    if (sm == nullptr) return;
    static API::Method* m_view = nullptr; static API::Method* m_cam = nullptr; static API::Method* m_wm = nullptr; static bool looked = false;
    if (!looked) {
        looked = true;
        if (auto* t = api->tdb()->find_type("via.SceneManager"); t != nullptr) m_view = t->find_method("get_MainView");
        if (auto* t = api->tdb()->find_type("via.SceneView"); t != nullptr) m_cam = t->find_method("get_PrimaryCamera");
        if (auto* t = api->tdb()->find_type("via.Camera"); t != nullptr) m_wm = t->find_method("get_WorldMatrix");
        LOGI("%s camera path: get_MainView=%p get_PrimaryCamera=%p Camera.get_WorldMatrix=%p", TAG, (void*)m_view, (void*)m_cam, (void*)m_wm);
    }
    if (m_view == nullptr || m_cam == nullptr || m_wm == nullptr) return;
    auto* view = m_view->call<API::ManagedObject*>(api->get_vm_context(), sm);
    if (view == nullptr) return;
    auto* cam = m_cam->call<API::ManagedObject*>(api->get_vm_context(), (void*)view);
    if (cam == nullptr) return;
    auto r = m_wm->invoke(cam, std::vector<void*>{});
    if (r.exception_thrown) return;
    Mat4 cm{}; memcpy(&cm, r.bytes.data(), sizeof(Mat4));
    d.cam_t = Vec3{cm.m[12], cm.m[13], cm.m[14]}; d.cam_r = rows_of(cm); d.cam_valid = true;
}

void update_dock(double t, double dt) {
    auto& d = g.dock;
    const bool vr = bridge_live() && arr_f32(g.bridge)[S_USING_CTL] != 0.0f && arr_f32(g.bridge)[S_HMD_ACTIVE] != 0.0f;
    d.lg_held = vr && arr_f32(g.bridge)[S_LGRIP] > 0.5f;
    const bool was = d.docked;
    d.docked = d.lg_held || d.synthetic;
    if (d.docked != was) LOGI("%s DOCK %s (lg=%d synthetic=%d vr=%d) w=%.2f", TAG, d.docked ? "ENGAGED" : "RELEASED", (int)d.lg_held, (int)d.synthetic, (int)vr, d.w);

    const float step = (float)(dt / DOCK_BLEND_S);
    d.w = std::clamp(d.w + (d.docked ? step : -step), 0.f, 1.f);
    d.w_eased = d.w * d.w * (3.f - 2.f * d.w);

    // spec v2.3 req 2: HOLD rides the dock. One setForce per edge, reconciled with the NUM4 manual latch.
    const bool want_hold = g.force_hold || d.docked;
    if (want_hold != g.hold_sent) { g.hold_sent = want_hold; set_force(KIND_HOLD, want_hold); }

    d.target_valid = false;

    // v0.6: measure the arm's own length while it is doing nothing in particular. Bone lengths are rigid, so the
    // running max over plausible frames IS the length; the band rejects a garbage read rather than letting one
    // bad frame set the clamp forever. Reset on rebind, because Claire's arm is not Leon's.
    d.reach_valid = false;
    if (g.l_humerus != nullptr && g.l_radius != nullptr && g.l_hand != nullptr) {
        Vec3 hu{}, ra{}, wr{};
        if (inv_vec3(g.l_humerus, "get_Position", hu) && inv_vec3(g.l_radius, "get_Position", ra) &&
            inv_vec3(g.l_hand, "get_Position", wr)) {
            const float upper = dist(hu, ra), fore = dist(ra, wr);
            if (upper > 0.05f && upper < 1.0f && fore > 0.05f && fore < 1.0f) {
                const float sum = upper + fore;
                if (sum > d.reach_raw) {
                    d.reach_raw = sum; d.reach = sum * DOCK_REACH_FRAC;
                    LOGI("%s reach measured: upper=%.3f fore=%.3f arm=%.3f clamp at %.3f m from l_arm_humerus", TAG, upper, fore, sum, d.reach);
                }
            }
            d.shoulder_t = hu;
            d.reach_valid = d.reach > 0.f;
        }
    }

    // v0.6: M is measured EVERY frame, docked or not. It was only computed while the dock was active, so the
    // summary's angM was whatever the last dock left behind — misleading exactly when a headset run wants to
    // read it before docking. It costs one get_WorldMatrix per frame. A stale M is worse than none, so an
    // unreadable joint clears M_valid rather than leaving the previous frame's mapping to be read as this one's.
    Mat4 am{}; Vec3 a_t{}; Rows A{};
    const bool have_frame = g.aid_joint != nullptr && d.natural_valid && inv_mat4(g.aid_joint, "get_WorldMatrix", am);
    if (have_frame) {
        a_t = Vec3{am.m[12], am.m[13], am.m[14]};
        A = rows_of(am);
        d.M = rows_mul(rows_T(rows_of(d.natural)), A); d.M_valid = true;
    } else {
        d.M_valid = false;
    }

    if (!d.docked && d.w <= 0.f) return;      // idle: the hook leaves the matrix alone
    if (!have_frame) return;                  // no natural value or no aid joint — nothing to blend from
    Vec3 p_f{}; Rows C{};                     // the desired FINAL wrist pose, unblended

    if (vr && !d.synthetic) {
        // Real controller. Both HMD and controller poses come from the same vrmod space, so their DIFFERENCE is
        // space-independent; re-base it on the game camera (which REFramework pins to the HMD) to get world.
        // Space mode 1 trusts the bridge pose as world outright; mode 2 is mode 0 with the camera rows transposed
        // (the one convention ambiguity in the rows<->quaternion path). None of the three has run in a headset.
        update_camera();
        const float* f = arr_f32(g.bridge);
        const Vec3 lp = bridge_vec3(S_LPOS), hp = bridge_vec3(S_HPOS);
        const Quat lq = quat_norm(Quat{f[S_LROT], f[S_LROT + 1], f[S_LROT + 2], f[S_LROT + 3]});
        const Quat hq = quat_norm(Quat{f[S_HROT], f[S_HROT + 1], f[S_HROT + 2], f[S_HROT + 3]});
        if (d.space_mode != 1 && d.cam_valid) {
            Rows cr = d.cam_r;
            if (d.space_mode == 2) { Rows tr; for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) tr.r[i * 3 + j] = cr.r[j * 3 + i]; cr = tr; }
            const Quat fix = quat_mul(quat_of_rows(cr), quat_conj(hq));   // tracking -> world
            p_f = vadd(d.cam_t, quat_rotate(fix, vsub(lp, hp)));
            C = rows_of_quat(quat_mul(fix, lq));
        } else {
            p_f = lp;
            C = rows_of_quat(lq);
        }
    } else {
        // Flat stand-in: a point 10 cm from the FINAL aid joint, orbiting above it once every 4 s, facing 45 deg
        // (world yaw) off the joint's final frame. If the mapping is right the trace shows |Lw-tgt| -> 0 and
        // rotW-T -> 0 while docked.
        const float th = (float)((t - d.orbit_t0) * 2.0 * 3.14159265 / DOCK_ORBIT_PERIOD_S);
        const Vec3 dir{std::cos(th) * 0.7071f, 0.7071f, std::sin(th) * 0.7071f};
        p_f = vadd(a_t, vscale(dir, DOCK_ORBIT_RADIUS));
        C = rows_yaw(A, DOCK_ORBIT_YAW_DEG);
    }
    // v0.6 reach clamp, applied BEFORE the target is published so the trace compares the wrist against a pose the
    // arm can actually hold and |Lw-tgt| still goes to 0.000 when the mapping is right. The rotation is left
    // alone: a wrist orientation is always reachable, it is only the position that runs out of arm.
    d.clamped_m = 0.f;
    float want_L = 0.f;                       // how far the UNCLAMPED target sat from the shoulder
    if (d.reach_valid && d.use_reach_clamp) {
        const Vec3 sd = vsub(p_f, d.shoulder_t);
        want_L = vlen(sd);
        if (want_L > d.reach && want_L > 1e-4f) {
            d.clamped_m = want_L - d.reach;
            p_f = vadd(d.shoulder_t, vscale(sd, d.reach / want_L));
        }
    }
    const bool clamping = d.clamped_m > 0.001f;
    if (clamping != d.clamp_on) {
        d.clamp_on = clamping;
        LOGI("%s REACH CLAMP %s (target %.3f m from l_arm_humerus, arm reaches %.3f, pulled in %.3f)", TAG,
             clamping ? "ON" : "off", want_L, d.reach, d.clamped_m);
    }

    d.target_t = p_f; d.target_r = C; d.target_valid = true;

    // Blend in final space, then map into getter space for the hook.
    const Vec3 p_b = vlerp(a_t, p_f, d.w_eased);
    const Rows R_b = rows_slerp(A, C, d.w_eased);
    const Rows MT = rows_T(d.M);
    d.d_get = vec_mul_rows(vsub(p_b, a_t), MT);
    d.T_rows = rows_mul(R_b, MT);
}

void rebind_player(API::ManagedObject* pm, API::ManagedObject* go) {
    g.player_go = go;
    g.cond = inv_ptr(pm, "get_CurrentPlayerCondition");
    g.equipment = inv_ptr(g.cond, "get_Equipment");
    g.ik = inv_ptr(g.cond, "get_IkController");
    g.transform = inv_ptr(go, "get_Transform");
    g.motion = get_component(go, "via.motion.Motion");
    g.l_hand = nullptr; g.r_hand = nullptr; g.aid_joint = nullptr;
    // v0.6: the dock survives a rebind on purpose (its settings and latches must), but a measured arm length must
    // NOT — a different playable character is a different skeleton, and a kept reach would clamp the new arm to
    // the old one's length. Cleared here, where the joints it was measured from are also dropped.
    g.l_humerus = nullptr; g.l_radius = nullptr;
    g.dock.reach = 0.f; g.dock.reach_raw = 0.f; g.dock.reach_valid = false; g.dock.clamp_on = false;
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
        if (g.player_go != nullptr) {
            LOGI("%s player gone (scene change?) — unbinding", TAG);
            // keep what outlives the player: the bridge, the counters, the latches (a forced HOLD must stay reconcilable), the dock settings
            g = State{.bridge = g.bridge, .frame = g.frame, .trace = g.trace, .force_hold = g.force_hold, .hold_sent = g.hold_sent, .dock = g.dock};
            g.dock.natural_valid = false; g.dock.target_valid = false;
        }
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
        static double last_t = 0.0;
        const double tn = now_s();
        const double dt = last_t > 0.0 ? std::clamp(tn - last_t, 0.0, 0.1) : 0.0;
        last_t = tn;
        update_dock(tn, dt);
    }
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
// v0.3/v0.4: the aid-target hooks. Both getters return System.Nullable`1<via.mat4> through a hidden
// return-buffer pointer, so at return RAX holds that pointer: *ret_val -> { u8 HasValue @0, mat4 @+0x10 }.
// The post-hook edits the buffer before the caller reads it. v0.3 shifted it (+10 cm) and proved the wrist
// follows; v0.4 BLENDS it toward the dock target on the OUTER getter only (getIKLeftArmMatrix calls
// get_AidTargetWorldMatrix inside itself — the two counts were always equal — so editing the outer one
// is what the wrist solver sees, and the inner one stays honest for anything else that reads it).
// ---------------------------------------------------------------------------

int pre_passthrough(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) { return REFRAMEWORK_HOOK_CALL_ORIGINAL; }

void post_aid_target(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    g_calls_aid++;
    if (g_self_call || ret_val == nullptr) return;
    auto* nb = (uint8_t*)*ret_val;
    if (nb == nullptr || nb[0] == 0) return;
    const float* m = (const float*)(nb + 0x10);
    g.dock.inner_t = Vec3{m[12], m[13], m[14]}; g.dock.inner_valid = true;
}

void post_ik_left_arm(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    g_calls_ikl++;
    if (ret_val == nullptr) return;
    auto* nb = (uint8_t*)*ret_val;
    if (nb == nullptr) return;
    auto& d = g.dock;
    if (nb[0] == 0) { if (d.docked) d.no_value_frames++; return; }   // no value: do not invent one (counted, so the minigun case shows)
    float* m = (float*)(nb + 0x10);
    if (g_self_call) { memcpy(d.natural_self.m, m, sizeof(Mat4)); d.natural_self_valid = true; }
    else             { memcpy(d.natural.m, m, sizeof(Mat4)); d.natural_valid = true; }   // the game's un-hooked value: the blend origin
    if (d.w_eased <= 0.f || !d.target_valid || !d.M_valid) return;
    // v0.5: the blend already happened in final space (update_dock); write its getter-space form.
    m[12] += d.d_get.x;
    m[13] += d.d_get.y;
    m[14] += d.d_get.z;
    if (d.write_rot) {
        Mat4 tmp{}; memcpy(tmp.m, m, sizeof(Mat4));
        rows_into(tmp, d.T_rows);
        memcpy(m, tmp.m, sizeof(Mat4));
    }
}

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
    fns->log_info("%s native core v0.6 loaded — THE DOCK (final-space blend mapped into the getIKLeftArmMatrix hook, HOLD latched while docked, M measured every frame, target clamped to the measured arm length). NUM7 dump / NUM8 trace / NUM9 layers / NUM4 force-HOLD / NUM5 ATTACK pulse / NUM6 synthetic dock (flat) / NUM1 VR space mode / NUM3 rotation write / NUM2 reach clamp", TAG);
    // Hooks need the TDB up; register them from the game thread on the first frame.
    static std::atomic<bool> hooked{false};
    fns->on_pre_application_entry("LockScene", []() {
        if (!hooked.exchange(true)) on_initialized();
        on_frame();
    });
    return true;
}
