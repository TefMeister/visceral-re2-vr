// reflect.cpp -- engine reflection helpers, pure maths, managed-array layout.
// Split out of Plugin.cpp 2026-09-27, move only (SPLIT-MAP-2026-09-24.md).
#include "visceral.h"

namespace visceral {

float dist(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vec3 vsub(const Vec3& a, const Vec3& b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }

Vec3 vadd(const Vec3& a, const Vec3& b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }

Vec3 vscale(const Vec3& a, float s) { return Vec3{a.x * s, a.y * s, a.z * s}; }

Vec3 vlerp(const Vec3& a, const Vec3& b, float t) { return Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }

float vlen(const Vec3& a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

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

Inv inv(API::ManagedObject* o, std::string_view name, std::vector<void*> args) {
    Inv out{};
    if (o == nullptr) return out;
    auto* m = find_method_deep(o->get_type_definition(), name);
    if (m == nullptr) return out;
    out.r = m->invoke(o, args);
    out.ok = !out.r.exception_thrown;
    return out;
}

API::ManagedObject* inv_ptr(API::ManagedObject* o, std::string_view name, std::vector<void*> args) {
    auto x = inv(o, name, std::move(args));
    return x.ok ? (API::ManagedObject*)x.r.ptr : nullptr;
}

bool inv_bool(API::ManagedObject* o, std::string_view name) { return call_direct<bool>(o, name, false); }

bool inv_bool_i(API::ManagedObject* o, std::string_view name, int arg) { return call_direct<bool>(o, name, false, arg); }

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

double now_s() {
    static LARGE_INTEGER f{}; static bool init = false;
    if (!init) { QueryPerformanceFrequency(&f); init = true; }
    LARGE_INTEGER c{}; QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

} // namespace visceral
