// shellclip.cpp -- see shellclip.h. (2026-11-02)
#include "shellclip.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "common.h"
#include "joints.h"
#include "settings.h"

namespace vn::shellclip {

using namespace joints;

namespace {
constexpr int N = 17;
const wchar_t* LEFT_FINGERS[N] = {
    L"l_hand_index_0", L"l_hand_index_1", L"l_hand_index_2", L"l_hand_middle_0", L"l_hand_middle_1", L"l_hand_middle_2",
    L"l_hand_ring_0", L"l_hand_ring_1", L"l_hand_ring_2", L"l_hand_ring_3", L"l_hand_little_0", L"l_hand_little_1",
    L"l_hand_little_2", L"l_hand_little_3", L"l_hand_thumb_0", L"l_hand_thumb_1", L"l_hand_thumb_2",
};
constexpr float BLEND_S = 0.06f;   // in from the hand's pose, and back out, over this long

struct Clip { std::vector<float> t; std::vector<std::array<Quat, N>> q; bool ok = false; };
std::map<int, Clip> g_clips;
const Clip* g_clip = nullptr;
float g_t0 = 0.0f;
bool g_have = false;
MO* g_tf = nullptr;
MO* g_joint[N] = {};
Quat g_found[N] = {};

std::filesystem::path data_dir() {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return std::filesystem::path(exe).parent_path() / "reframework" / "data" / "visceral" / "clips";
}
std::string game_prefix() {   // the game's own exe name (re3.exe = RE3)
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return std::filesystem::path(exe).filename().wstring() == L"re3.exe" ? "RE3" : "RE2";
}

const Clip* load(int wp) {
    auto it = g_clips.find(wp);
    if (it != g_clips.end()) return it->second.ok ? &it->second : nullptr;
    Clip& c = g_clips[wp];
    char name[32];
    std::snprintf(name, sizeof(name), "wp%04d_round_in.csv", wp);
    std::ifstream f(data_dir() / name);
    if (!f) { LOGI("%s shellclip: no clip for WP%04d (clips/%s): the fingers stay as they are", TAG, wp, name); return nullptr; }
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string cell;
        std::vector<float> v;
        while (std::getline(ss, cell, ',')) v.push_back(std::strtof(cell.c_str(), nullptr));
        if (v.size() != 1 + N * 4) continue;
        c.t.push_back(v[0]);
        std::array<Quat, N> q{};
        for (int i = 0; i < N; ++i) q[i] = qnorm({v[1 + i * 4], v[2 + i * 4], v[3 + i * 4], v[4 + i * 4]});
        c.q.push_back(q);
    }
    c.ok = c.t.size() >= 2;
    LOGI("%s shellclip: WP%04d clip %s: %d frames, %.2f s", TAG, wp, c.ok ? "loaded" : "UNUSABLE", (int)c.t.size(), c.ok ? c.t.back() : 0.0f);
    return c.ok ? &c : nullptr;
}

Quat nlerp(Quat a, Quat b, float u) {
    if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0.0f) b = {-b.x, -b.y, -b.z, -b.w};
    return qnorm({a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u, a.w + (b.w - a.w) * u});
}
} // namespace

void play(int wp) {
    const Clip* c = load(wp);
    if (c == nullptr) return;
    g_clip = c;
    g_t0 = now_s();
    g_have = false;
    LOGI("%s shellclip: WP%04d shell in: the game's finger movement plays (%.2f s)", TAG, wp, c->t.back());
}

bool playing() { return g_clip != nullptr; }

void frame() {
    static float last = -1.0f;
    const float t = now_s();
    if (t - last < 0.25f) return;
    last = t;
    const auto p = data_dir() / "play.txt";
    std::ifstream f(p);
    std::string id;
    if (f && std::getline(f, id) && id.size() > 2) {
        f.close();
        std::ofstream(p, std::ios::trunc).close();
        LOGI("%s shellclip: test play asked for %s", TAG, id.c_str());
        play(std::atoi(id.c_str() + 2));
    }
}

void render_point() {
    if (g_clip == nullptr) return;
    auto* go = call_ptr(API::get()->get_managed_singleton(game_prefix() == "RE3" ? "offline.PlayerManager" : "app.ropeway.PlayerManager"), "get_CurrentPlayer");
    auto* tf = go != nullptr ? call_ptr(go, "get_Transform") : nullptr;
    if (tf == nullptr) { g_clip = nullptr; return; }
    if (tf != g_tf) { g_tf = tf; for (auto& j : g_joint) j = nullptr; }
    for (int i = 0; i < N; ++i) if (g_joint[i] == nullptr) g_joint[i] = joint_by_name(g_tf, LEFT_FINGERS[i]);
    const float t = now_s() - g_t0, len = g_clip->t.back();
    if (!g_have) {
        for (int i = 0; i < N; ++i) {
            Quat q{0, 0, 0, 1};
            g_found[i] = g_joint[i] != nullptr && get_q(JOINT, g_joint[i], "get_LocalRotation", q) ? qnorm(q) : q;
        }
        g_have = true;
    }
    if (t >= len + BLEND_S) { g_clip = nullptr; LOGI("%s shellclip: finished", TAG); return; }
    size_t k = 0;
    const float tc = std::fmin(t, len);
    while (k + 1 < g_clip->t.size() && g_clip->t[k + 1] <= tc) ++k;
    const size_t k1 = std::min(k + 1, g_clip->t.size() - 1);
    const float span = g_clip->t[k1] - g_clip->t[k], u = span > 0.0f ? (tc - g_clip->t[k]) / span : 0.0f;
    const float in = std::fmin(1.0f, t / BLEND_S), out = t > len ? std::fmax(0.0f, 1.0f - (t - len) / BLEND_S) : 1.0f;
    for (int i = 0; i < N; ++i) {
        if (g_joint[i] == nullptr) continue;
        const Quat pose = nlerp(g_clip->q[k][i], g_clip->q[k1][i], u);
        const Quat q = nlerp(g_found[i], pose, ease(std::fmin(in, out)));
        set_any(JOINT, g_joint[i], "set_LocalRotation", &q);
    }
}

} // namespace vn::shellclip
