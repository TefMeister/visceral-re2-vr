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

// Hamilton product a*b (apply b, then a), both {x,y,z,w}
inline Quat qmul(Quat a, Quat b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Quat qnorm(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 0.0f ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : Quat{0, 0, 0, 1};
}
inline Quat qinv(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }   // unit quaternions only
inline Quat yaw_quat(float rad) { return {0.0f, std::sin(rad * 0.5f), 0.0f, std::cos(rad * 0.5f)}; }
inline float wrap_pi(float a) { return std::atan2(std::sin(a), std::cos(a)); }
inline float yaw_of(Vec3 f) { return std::atan2(f.x, f.z); }    // the game's yaw convention (Arcade Controls)

bool is_managed(void* p);
std::string type_name(MO* o);
API::Method* find_method_deep(API::TypeDefinition* td, std::string_view name);
API::Field* find_field_deep(API::TypeDefinition* td, std::string_view name);
MO* field_obj(MO* o, std::string_view field);      // a reference-type field, or nullptr
template <typename T>
T* field_ptr(MO* o, std::string_view field) {      // a value field in place, or nullptr
    if (o == nullptr) return nullptr;
    auto* f = find_field_deep(o->get_type_definition(), field);
    return f != nullptr ? (T*)f->get_data_raw(o, false) : nullptr;
}
std::string read_string(void* s);                   // a System.String to UTF-8 ("" if not a string)

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
