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
// sentinel 12345 into slot 63 and hands the array over ONCE by calling
// System.GC.KeepAlive(arr), which this plugin hooks. After that the shim writes
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
bool inv_bool(API::ManagedObject* o, std::string_view name, std::vector<void*> args = {}) {
    auto x = inv(o, name, std::move(args));
    return x.ok && x.r.byte != 0;
}
uint32_t inv_u32(API::ManagedObject* o, std::string_view name, std::vector<void*> args = {}) {
    auto x = inv(o, name, std::move(args));
    return x.ok ? x.r.dword : 0xFFFFFFFFu;
}
float inv_f32(API::ManagedObject* o, std::string_view name) {
    auto x = inv(o, name);
    return x.ok ? x.r.f : NAN;
}
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

// Managed arrays: element count at +0x18, elements at +0x20 (REFramework's REArrayBase).
uint32_t arr_count(API::ManagedObject* a) { return a != nullptr ? *(const uint32_t*)((const char*)a + 0x18) : 0; }
API::ManagedObject* arr_ptr_at(API::ManagedObject* a, uint32_t i) { return ((API::ManagedObject**)((char*)a + 0x20))[i]; }
float* arr_f32(API::ManagedObject* a) { return (float*)((char*)a + 0x20); }

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
    API::ManagedObject* bridge{};      // System.Single[64] from the Lua shim
    uint64_t frame{};
    bool trace{false};
    bool want_dump{false};
    bool want_layers{false};
    std::vector<std::string> layer_last;   // last highest-weight motion name per layer
    uint64_t layer_lines_this_second{};
    uint64_t layer_second{};
    double last_summary_t{};
    double last_layer_table_t{};
} g;

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

void dump_joints(API::ManagedObject* transform, const char* who, bool all, API::ManagedObject** l_hand, API::ManagedObject** r_hand) {
    auto* joints = inv_ptr(transform, "get_Joints");
    if (joints == nullptr) { LOGW("%s %s: get_Joints failed", TAG, who); return; }
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
        if (l_hand != nullptr && *l_hand == nullptr && (ln == "l_hand" || ln == "lefthand" || ln == "l_hand_0")) *l_hand = j;
        if (r_hand != nullptr && *r_hand == nullptr && (ln == "r_hand" || ln == "righthand" || ln == "r_hand_0")) *r_hand = j;
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

    if (g.weapon_transform != nullptr) dump_joints(g.weapon_transform, "weapon skeleton", true, nullptr, nullptr);
    LOGI("%s ---- end weapon dump ----", TAG);
}

void dump_ik() {
    auto* ik = g.ik;
    if (ik == nullptr) { LOGI("%s ik: none", TAG); return; }
    LOGI("%s ---- IK DUMP: %s ----", TAG, tname(ik).c_str());
    LOGI("%s   UseIkArm=%d UseIkWrist=%d UseIkArmFitAsWrist=%d WristKind=%u WristSolveMode=%u", TAG,
         (int)inv_bool(ik, "get_UseIkArm"), (int)inv_bool(ik, "get_UseIkWrist"), (int)inv_bool(ik, "get_UseIkArmFitAsWrist"),
         inv_u32(ik, "get_WristKind"), inv_u32(ik, "get_WristSolveMode"));
    static const char* kinds[] = {"LEG", "SPINE", "LOOKAT", "ARM", "ARMFIT", "HAND"};
    for (int k = 0; k < 6; ++k) {
        LOGI("%s   isEnabled(%s)=%d", TAG, kinds[k], (int)inv_bool(ik, "isEnabled", {(void*)(uintptr_t)k}));
    }
    auto* arms = inv_ptr(ik, "get_ArmStatusList");
    const auto n = arr_count(arms);
    LOGI("%s   ArmStatusList: %u entries", TAG, arms != nullptr ? n : 0);
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
    const bool have_aid = g.aid_joint != nullptr && inv_vec3(g.aid_joint, "get_Position", aid);
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
    if (ik_ok) {
        if (ik_has) o += snprintf(buf + o, sizeof buf - o, " ikL=(%.2f %.2f %.2f)", ikm.m[12], ikm.m[13], ikm.m[14]);
        else o += snprintf(buf + o, sizeof buf - o, " ikL=none");
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

void poll_hotkeys() {
    static bool prev[3] = {false, false, false};
    const int vks[3] = {VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9};
    if (!game_is_foreground()) return;
    for (int i = 0; i < 3; ++i) {
        const bool down = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
        if (down && !prev[i]) {
            if (i == 0) { g.want_dump = true; LOGI("%s NUM7: dump requested", TAG); }
            if (i == 1) { g.trace = !g.trace; LOGI("%s NUM8: trace %s", TAG, g.trace ? "ON (10 Hz)" : "OFF (1 Hz)"); }
            if (i == 2) { g.want_layers = true; LOGI("%s NUM9: layer table requested", TAG); }
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
        g.aid_joint = nullptr;
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

int pre_keepalive(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    if (g.bridge != nullptr) return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    for (int i = 0; i < argc && i < 8; ++i) {
        auto* o = (API::ManagedObject*)argv[i];
        if (!is_managed(o)) continue;
        if (tname(o) != "System.Single[]") continue;
        const auto n = arr_count(o);
        if (n != 64) { LOGW("%s KeepAlive saw a System.Single[%u], expected 64", TAG, n); continue; }
        float* f = arr_f32(o);
        if (f[S_SENTINEL] != 12345.0f) { LOGW("%s KeepAlive float[64] without sentinel (slot63=%.1f) — element offset assumption wrong?", TAG, f[S_SENTINEL]); continue; }
        o->add_ref();
        g.bridge = o;
        f[S_ACK] = 1.0f;
        LOGI("%s VR BRIDGE ATTACHED (argv[%d] of %d, array %p) — Lua shim poses are live", TAG, i, argc, (void*)o);
        break;
    }
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void install_bridge_hook() {
    auto& api = API::get();
    auto* m = api->tdb()->find_method("System.GC", "KeepAlive");
    if (m == nullptr) { LOGE("%s System.GC.KeepAlive not found — VR bridge unavailable", TAG); return; }
    const auto id = m->add_hook(pre_keepalive, nullptr, false);
    LOGI("%s bridge hook installed on System.GC.KeepAlive (id=%u)", TAG, id);
}

void on_initialized() {
    install_bridge_hook();
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
    fns->log_info("%s native core v0.1 loaded — reqs-1-and-3 recon probe. NUM7 dump / NUM8 trace / NUM9 layers", TAG);
    // Hooks need the TDB up; register them from the game thread on the first frame.
    static std::atomic<bool> hooked{false};
    fns->on_pre_application_entry("LockScene", []() {
        if (!hooked.exchange(true)) on_initialized();
        on_frame();
    });
    return true;
}
