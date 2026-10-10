// sfx.cpp -- see sfx.h.
#include "sfx.h"
#include "common.h"
#include "settings.h"

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <windows.h>

#include "../third_party/miniaudio/miniaudio.h"

namespace vn::sfx {

namespace {
std::mutex g_mx;
ma_engine g_engine;
bool g_ok = false, g_tried = false;
struct Voice { ma_sound s; bool used = false; };
Voice g_voice[cfg::SFX_VOICES];
std::map<std::string, float> g_last;   // kind -> time of its last play
std::string g_root;

float now_s() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
}

bool is_file(const std::string& p) {
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool init() {   // under g_mx
    if (g_tried) return g_ok;
    g_tried = true;
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);   // re2.exe, whatever the working folder
    std::string p(buf);
    g_root = p.substr(0, p.find_last_of("\\/") + 1) + "reframework\\data\\custom_sfx\\";
    const ma_result r = ma_engine_init(nullptr, &g_engine);
    g_ok = r == MA_SUCCESS;
    LOGI("%s sfx: miniaudio %s engine %s (result %d), sounds in %s", TAG, MA_VERSION_STRING, g_ok ? "OPEN" : "FAILED", (int)r, g_root.c_str());
    return g_ok;
}
} // namespace

bool play(const char* kind, const char* folder, float volume) {
    std::lock_guard<std::mutex> lk(g_mx);
    if (!init()) return false;
    const float t = now_s();
    auto it = g_last.find(kind);
    if (it != g_last.end() && t - it->second < cfg::SFX_DEBOUNCE_SEC) return false;   // a repeat, silently
    g_last[kind] = t;
    std::string path = g_root + folder + "\\" + kind + ".ogg";
    if (!is_file(path)) path = g_root + folder + "\\" + kind + ".wav";
    if (!is_file(path)) { LOGW("%s sfx: %s for folder %s -- no .ogg or .wav there", TAG, kind, folder); return false; }
    Voice* v = nullptr;
    for (auto& x : g_voice) {   // a free voice, or the first finished one
        if (!x.used) { v = &x; break; }
        if (ma_sound_at_end(&x.s)) { ma_sound_uninit(&x.s); x.used = false; v = &x; break; }
    }
    if (v == nullptr) { LOGW("%s sfx: all %d voices busy, %s dropped", TAG, cfg::SFX_VOICES, kind); return false; }
    const ma_result r = ma_sound_init_from_file(&g_engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &v->s);
    if (r != MA_SUCCESS) { LOGW("%s sfx: %s could not be opened (miniaudio %d)", TAG, path.c_str(), (int)r); return false; }
    v->used = true;
    ma_sound_set_volume(&v->s, volume * cfg::SFX_MASTER);
    ma_sound_start(&v->s);
    LOGI("%s sfx: played %s (%s) at %.2f", TAG, kind, folder, volume * cfg::SFX_MASTER);
    return true;
}

} // namespace vn::sfx
