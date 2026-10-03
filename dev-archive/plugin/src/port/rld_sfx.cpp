// rld_sfx.cpp -- the port's sound player (2026-10-03, port step 2). Replaces RELOADED's ext_5 sound manager +
// REAudio.dll with miniaudio inside our plugin (the same public-domain library REAudio is built on;
// third_party/miniaudio/, decoded by miniaudio_impl.c). Lookup rules follow ext_5 M.play: the weapon's file for the
// kind, its sfx_folder (or the weapon id), master x global kind volume x weapon kind volume, 0.1 s debounce per kind.
// Not ported yet: spatial playback at the hand (ext_5 `spatial`), the common/ folder fallback list.
#include "../visceral.h"
#include "port.h"
#include "port_settings.h"

#include <mutex>

#include "../../third_party/miniaudio/miniaudio.h"

namespace visceral::port {

namespace {
std::mutex g_mu;
ma_engine g_engine;
bool g_engine_ok = false, g_engine_tried = false;
struct Voice { ma_sound s; bool used{false}; };
Voice g_voice[SFX_VOICES];
std::map<std::string, double> g_last_kind;
std::string g_root;

std::string data_dir() {
    char buf[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    return p.substr(0, p.find_last_of("\\/") + 1) + "reframework\\data\\";
}
bool is_dir(const std::string& p) { const DWORD a = GetFileAttributesA(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
bool is_file(const std::string& p) { const DWORD a = GetFileAttributesA(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }

float kind_vol(const std::map<std::string, float>& m, const char* kind) {
    auto it = m.find(kind);
    return it == m.end() ? SFX_KIND_VOLUME_DEFAULT : it->second;
}

// ext_5 resolve_assignment: the weapon's own file, else its fallback weapon's, else the default fallback weapon's.
const WeaponSfx* resolve(const SfxConfig& c, const std::string& wp, const char* kind, std::string& file) {
    const std::string chain[3] = {wp, c.by_wp.count(wp) ? c.by_wp.at(wp).fallback_wp : std::string{}, c.default_fallback_wp};
    for (const auto& w : chain) {
        if (w.empty()) continue;
        auto it = c.by_wp.find(w);
        if (it == c.by_wp.end()) continue;
        auto f = it->second.file.find(kind);
        if (f != it->second.file.end() && !f->second.empty()) { file = f->second; return &it->second; }
    }
    return nullptr;
}
} // namespace

std::string sfx_root() { return g_root; }

bool sfx_init() {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_engine_tried) return g_engine_ok;
    g_engine_tried = true;
    const std::string dir = data_dir();
    for (const char* rel : RLD_SFX_ROOTS) if (is_dir(dir + rel)) { g_root = dir + rel; break; }
    const ma_result r = ma_engine_init(nullptr, &g_engine);
    g_engine_ok = r == MA_SUCCESS;
    LOGI("%s sfx: miniaudio %s engine %s (result %d), sounds folder %s", TAG, MA_VERSION_STRING, g_engine_ok ? "OPEN" : "FAILED",
         (int)r, g_root.empty() ? "NOT FOUND (reframework\\data\\visceral\\reloaded\\custom_sfx or reframework\\data\\custom_sfx)" : g_root.c_str());
    return g_engine_ok;
}

bool sfx_play(const char* kind, const std::string& wp_in) {
    const SfxConfig& c = rld_sfx_config();
    if (!c.loaded || !c.enabled) { LOGI("%s sfx: %s not played -- %s", TAG, kind, c.loaded ? "weapon_sfx.enabled is false" : "no RELOADED data loaded"); return false; }
    if (!sfx_init() || g_root.empty()) return false;
    const double t = now_s();
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_last_kind.find(kind);
        if (it != g_last_kind.end() && t - it->second < SFX_DEBOUNCE_SEC) return false;   // debounced, silently (ext_5)
        g_last_kind[kind] = t;
    }
    const std::string wp = wp_in.empty() ? c.default_fallback_wp : wp_in;
    std::string file;
    const WeaponSfx* e = resolve(c, wp, kind, file);
    if (e == nullptr) { LOGI("%s sfx: %s for '%s' -- no file assigned", TAG, kind, wp.c_str()); return false; }
    const std::string folder = e->folder.empty() ? wp : e->folder;
    const std::string path = file.rfind("common/", 0) == 0 ? g_root + "\\" + file : g_root + "\\" + folder + "\\" + file;
    if (!is_file(path)) { LOGW("%s sfx: %s for '%s' -> %s is MISSING", TAG, kind, wp.c_str(), path.c_str()); return false; }
    const float vol = c.master * kind_vol(c.volume, kind) * kind_vol(e->volume, kind);

    std::lock_guard<std::mutex> lk(g_mu);
    Voice* v = nullptr;
    for (auto& x : g_voice) {   // a free voice, or the first one that has finished
        if (!x.used) { v = &x; break; }
        if (ma_sound_at_end(&x.s)) { ma_sound_uninit(&x.s); x.used = false; v = &x; break; }
    }
    if (v == nullptr) { LOGW("%s sfx: all %d voices busy -- %s dropped", TAG, SFX_VOICES, kind); return false; }
    const ma_result r = ma_sound_init_from_file(&g_engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &v->s);
    if (r != MA_SUCCESS) { LOGW("%s sfx: %s could not be opened (miniaudio %d)", TAG, path.c_str(), (int)r); return false; }
    v->used = true;
    ma_sound_set_volume(&v->s, vol);
    ma_sound_start(&v->s);
    LOGI("%s sfx: PLAYED %s for %s -> %s\\%s at volume %.2f", TAG, kind, wp.c_str(), folder.c_str(), file.c_str(), vol);
    return true;
}

} // namespace visceral::port
