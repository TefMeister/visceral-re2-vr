// common.h -- logging, small maths and the managed-call helpers every Visceral native file uses.
// New code, 2026-10-05 (Tefa: only new code from here on). Lessons from the old plugin kept as comments.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "reframework/API.hpp"

namespace vn {

using API = reframework::API;
using MO = reframework::API::ManagedObject;

extern const REFrameworkPluginInitializeParam* g_param;
constexpr const char* TAG = "[visceral-native]";

#define VN_LOG(fn, ...) do { if (vn::g_param && vn::g_param->functions && vn::g_param->functions->fn) vn::g_param->functions->fn(__VA_ARGS__); } while (0)
#define LOGI(...) VN_LOG(log_info, __VA_ARGS__)
#define LOGW(...) VN_LOG(log_warn, __VA_ARGS__)
#define LOGE(...) VN_LOG(log_error, __VA_ARGS__)

struct Vec3 { float x, y, z; };
struct Quat { float x, y, z, w; };

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dist(Vec3 a, Vec3 b) { Vec3 d = a - b; return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z); }
// rotate v by unit quaternion q
inline Vec3 rotate(Quat q, Vec3 v) {
    Vec3 u{q.x, q.y, q.z};
    auto cross = [](Vec3 a, Vec3 b) { return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; };
    Vec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

bool is_managed(void* p);
std::string type_name(MO* o);
API::Method* find_method_deep(API::TypeDefinition* td, std::string_view name);

// Pointer / value-type results go through invoke. Floats and bools DO NOT: the invoke path returned 0 for
// every float in this build (old plugin, measured 2026-09-04), so scalars use the direct call.
MO* call_ptr(MO* o, std::string_view method, std::vector<void*> args = {});
bool call_vec3(MO* o, std::string_view method, Vec3& out);
bool call_quat(MO* o, std::string_view method, Quat& out);
template <typename T, typename... Args>
T call_direct(MO* o, std::string_view method, T fallback, Args... args) {
    if (o == nullptr) return fallback;
    auto* m = find_method_deep(o->get_type_definition(), method);
    if (m == nullptr) return fallback;
    return m->call<T>(API::get()->get_vm_context(), (void*)o, args...);
}
MO* component(MO* game_object, const char* type);

} // namespace vn
