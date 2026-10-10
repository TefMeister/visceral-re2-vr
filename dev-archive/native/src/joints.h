// joints.h -- small helpers shared by the reload family (reload.cpp, rack.cpp, pump_native.cpp): a clock, easing,
// and calls on joints / transforms by tdb method (works for native objects too; the x64 ABI passes value types by
// pointer). Split out of reload.cpp on 2026-10-10 for bundle 2.
#pragma once
#include "common.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace vn::joints {

inline float now_s() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
}
inline float ease(float t) { return t <= 0 ? 0 : t >= 1 ? 1 : t * t * (3.0f - 2.0f * t); }
inline Vec3 lerp(Vec3 a, Vec3 b, float u) { return a + (b - a) * u; }

// ---- managed helpers: joints and transforms by tdb method (works for native objects too) ---------------------------
inline API::Method* method(const char* type, const char* name) {
    static std::map<std::string, API::Method*> cache;
    const std::string key = std::string(type) + "." + name;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    auto* m = API::get()->tdb()->find_method(type, name);
    if (m == nullptr) LOGW("%s reload: %s not found", TAG, key.c_str());
    return cache[key] = m;
}
inline bool get_v3(const char* type, MO* o, const char* name, Vec3& out) {
    auto* m = o ? method(type, name) : nullptr;
    if (m == nullptr) return false;
    auto r = m->invoke(o, std::vector<void*>{});
    if (r.exception_thrown) return false;
    std::memcpy(&out, r.bytes.data(), sizeof out);
    return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
}
inline bool get_q(const char* type, MO* o, const char* name, Quat& out) {
    auto* m = o ? method(type, name) : nullptr;
    if (m == nullptr) return false;
    auto r = m->invoke(o, std::vector<void*>{});
    if (r.exception_thrown) return false;
    std::memcpy(&out, r.bytes.data(), sizeof out);
    return std::isfinite(out.w);
}
inline void set_any(const char* type, MO* o, const char* name, const void* value) {   // value types go by pointer (x64 ABI)
    auto* m = o ? method(type, name) : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)o, (void*)value);
}
constexpr const char* JOINT = "via.Joint";
constexpr const char* XFORM = "via.Transform";

inline MO* joint_by_name(MO* tf, const wchar_t* name) {
    static std::map<std::wstring, MO*> strings;   // one managed string per name, kept alive
    auto it = strings.find(name);
    if (it == strings.end()) {
        auto* s = API::get()->create_managed_string(name);
        if (s != nullptr) s->add_ref();
        it = strings.emplace(name, s).first;
    }
    if (tf == nullptr || it->second == nullptr) return nullptr;
    return call_ptr(tf, "getJointByName", {(void*)it->second});
}

inline std::wstring widen(const char* s) { return std::wstring(s, s + std::strlen(s)); }

} // namespace vn::joints
