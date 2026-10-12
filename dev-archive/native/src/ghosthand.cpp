// ghosthand.cpp -- see ghosthand.h.
#include "ghosthand.h"
#include "bridge.h"
#include "common.h"
#include "joints.h"
#include "menu_body.h"
#include "rack.h"
#include "settings.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#include <windows.h>

namespace vn::ghosthand {

using namespace joints;

namespace {
FILE* g_f = nullptr;
std::string g_buf;
std::chrono::steady_clock::time_point g_t0, g_last_flush;
bool g_need_sent = false;

long long now_us() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - g_t0).count(); }

void flush() {
    if (g_f != nullptr && !g_buf.empty()) { std::fwrite(g_buf.data(), 1, g_buf.size(), g_f); std::fflush(g_f); g_buf.clear(); }
    g_last_flush = std::chrono::steady_clock::now();
}

void put(const char* fmt, ...) {
    if (g_f == nullptr) return;
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    g_buf += line;
    if (g_buf.size() > 64 * 1024) flush();
}

void vr_on() {
    if (g_f != nullptr) return;
    const char* base = std::getenv("LOCALAPPDATA");
    std::string dir = std::string(base ? base : ".") + "\\BeG0nE";
    CreateDirectoryA(dir.c_str(), nullptr);
    dir += "\\ghost-hand";
    CreateDirectoryA(dir.c_str(), nullptr);
    char stamp[32];
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    const std::string path = dir + "\\visceral-re2-vr_" + stamp + ".ghl";
    if (fopen_s(&g_f, path.c_str(), "wb") != 0) { g_f = nullptr; LOGW("%s ghost hand: could not open %s", TAG, path.c_str()); return; }
    g_t0 = std::chrono::steady_clock::now();
    g_need_sent = false;
    put("#ghosthand 1 game=visceral-re2-vr runtime=openxr\n");
    flush();
    LOGI("%s ghost hand: recording to %s", TAG, path.c_str());
}

void vr_off() {
    if (g_f == nullptr) return;
    put("#end\n");
    flush();
    std::fclose(g_f);
    g_f = nullptr;
    LOGI("%s ghost hand: recording closed", TAG);
}

// game world -> real room, through the camera (see ghosthand.h)
struct ToRoom {
    bool ok = false;
    Vec3 cam{}, hmd{};
    Quat turn{0, 0, 0, 1};
    Vec3 operator()(Vec3 w) const { return hmd + rotate(turn, w - cam); }
};

ToRoom to_room() {
    ToRoom r;
    auto* cam_tf = static_cast<MO*>(menu_body::probe_camera_tf());
    Quat cq;
    if (cam_tf == nullptr || !get_v3(XFORM, cam_tf, "get_Position", r.cam) || !get_q(XFORM, cam_tf, "get_Rotation", cq)) return r;
    const Quat hq = bridge::hmd_rot();
    r.hmd = bridge::hmd_pos();
    if (!bridge::has(r.hmd.x) || !bridge::has(hq.x)) return r;
    r.turn = qnorm(qmul(qnorm(hq), qinv(qnorm(cq))));
    r.ok = true;
    return r;
}

void real(const char* side, Vec3 p) {
    const bool tracked = bridge::has(p.x) && !(p.x == 0.0f && p.y == 0.0f && p.z == 0.0f);
    put("%lld R %s %d %.4f %.4f %.4f\n", now_us(), side, tracked ? 1 : 0, tracked ? p.x : 0.0f, tracked ? p.y : 0.0f, tracked ? p.z : 0.0f);
}
} // namespace

void event(const char* gesture, const char* what) { put("%lld E %s %s\n", now_us(), gesture, what); }
void value(const char* name, float v) { put("%lld V %s %.5f\n", now_us(), name, v); }

void render_point() {
    if (!cfg::GHOST_HAND_ON) return;
    if (!bridge::live()) { vr_off(); return; }
    vr_on();
    if (g_f == nullptr) return;
    if (!g_need_sent) { value("rack_need", cfg::RACK_PULL_M); g_need_sent = true; }

    real("L", bridge::hand_pos(bridge::LEFT));
    real("R", bridge::hand_pos(bridge::RIGHT));

    const ToRoom room = to_room();
    if (room.ok) {
        auto* tf = call_ptr(call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"), "get_Transform");
        Vec3 p;
        if (auto* lw = joint_by_name(tf, L"l_arm_wrist"); get_v3(JOINT, lw, "get_Position", p)) { p = room(p); put("%lld G L %.4f %.4f %.4f\n", now_us(), p.x, p.y, p.z); }
        if (auto* rw = joint_by_name(tf, L"r_arm_wrist"); get_v3(JOINT, rw, "get_Position", p)) { p = room(p); put("%lld G R %.4f %.4f %.4f\n", now_us(), p.x, p.y, p.z); }
        if (auto* sj = static_cast<MO*>(rack::slide_joint()); get_v3(JOINT, sj, "get_Position", p)) { p = room(p); put("%lld P slide %.4f %.4f %.4f\n", now_us(), p.x, p.y, p.z); }
    }
    value("rack", rack::travel());
    value("rack_pull_m", rack::pull_m());
    value("wp", static_cast<float>(rack::wp_now()));
    value("hand_on", rack::active() ? 1.0f : 0.0f);
    put("%lld B LG %d\n", now_us(), bridge::held(bridge::S_LGRIP) ? 1 : 0);
    put("%lld B LT %d\n", now_us(), bridge::held(bridge::S_LTRIG) ? 1 : 0);
    put("%lld F\n", now_us());
    if (std::chrono::steady_clock::now() - g_last_flush > std::chrono::seconds(1)) flush();
}

void shutdown() { vr_off(); }

bool to_room_point(Vec3 world, Vec3& room) { const ToRoom r = to_room(); if (!r.ok) return false; room = r(world); return true; }
bool to_room_dir(Vec3 world, Vec3& room) { const ToRoom r = to_room(); if (!r.ok) return false; room = rotate(r.turn, world); return true; }

} // namespace vn::ghosthand
