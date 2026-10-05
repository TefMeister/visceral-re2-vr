// common.cpp -- see common.h
#include "common.h"

#include <cstring>

namespace vn {

const REFrameworkPluginInitializeParam* g_param = nullptr;

bool is_managed(void* p) {
    return p != nullptr && API::get()->sdk()->managed_object->is_managed_object(p);
}

std::string type_name(MO* o) {
    if (o == nullptr) return "null";
    auto* td = o->get_type_definition();
    return td != nullptr ? td->get_full_name() : "?";
}

API::Method* find_method_deep(API::TypeDefinition* td, std::string_view name) {
    for (int i = 0; td != nullptr && i < 12; ++i) {
        if (auto* m = td->find_method(name)) return m;
        td = td->get_parent_type();
    }
    return nullptr;
}

static bool invoke(MO* o, std::string_view method, std::vector<void*> args, reframework::InvokeRet& r) {
    if (o == nullptr) return false;
    auto* m = find_method_deep(o->get_type_definition(), method);
    if (m == nullptr) return false;
    r = m->invoke(o, args);
    return !r.exception_thrown;
}

MO* call_ptr(MO* o, std::string_view method, std::vector<void*> args) {
    reframework::InvokeRet r{};
    return invoke(o, method, std::move(args), r) ? (MO*)r.ptr : nullptr;
}

bool call_vec3(MO* o, std::string_view method, Vec3& out) {
    reframework::InvokeRet r{};
    if (!invoke(o, method, {}, r)) return false;
    std::memcpy(&out, r.bytes.data(), sizeof out);
    return true;
}

bool call_quat(MO* o, std::string_view method, Quat& out) {
    reframework::InvokeRet r{};
    if (!invoke(o, method, {}, r)) return false;
    std::memcpy(&out, r.bytes.data(), sizeof out);
    return true;
}

MO* component(MO* go, const char* type) {
    if (go == nullptr) return nullptr;
    auto* rt = API::get()->typeof(type);
    if (rt == nullptr) return nullptr;
    // only the System.Type overload is a real function (the plain name has generic stubs; old plugin, 2026-09-04)
    return call_ptr(go, "getComponent(System.Type)", {rt});
}

} // namespace vn
