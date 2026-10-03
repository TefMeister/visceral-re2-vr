// rld_data.cpp -- RELOADED's re2_vr_reload.json, read as DATA (2026-10-03, port step 2).
// Only the weapon_sfx block for now; later steps add poses and per-weapon reload numbers from the same file.
// JSON reader: nlohmann/json 3.11.3 (MIT, third_party/nlohmann/LICENSE.MIT).
#include "../visceral.h"
#include "port.h"
#include "port_settings.h"

#include <fstream>

#include "../../third_party/nlohmann/json.hpp"

namespace visceral::port {

namespace {
SfxConfig g_cfg;

std::string data_dir() {
    char buf[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    return p.substr(0, p.find_last_of("\\/") + 1) + "reframework\\data\\";
}

float clamp_kind(const nlohmann::json& v) {
    if (!v.is_number()) return SFX_KIND_VOLUME_DEFAULT;
    const float f = v.get<float>();
    return f < SFX_KIND_VOLUME_MIN ? SFX_KIND_VOLUME_MIN : f > SFX_KIND_VOLUME_MAX ? SFX_KIND_VOLUME_MAX : f;
}

void read_volumes(const nlohmann::json& obj, std::map<std::string, float>& out) {
    auto it = obj.find("volume_by_kind");
    if (it == obj.end() || !it->is_object()) return;
    for (auto& [k, v] : it->items()) out[k] = clamp_kind(v);
}
} // namespace

const SfxConfig& rld_sfx_config() { return g_cfg; }

bool rld_data_load() {
    g_cfg = SfxConfig{};
    const std::string dir = data_dir();
    for (const char* rel : RLD_JSON_PATHS) {
        std::ifstream f(dir + rel);
        if (!f) continue;
        nlohmann::json j = nlohmann::json::parse(f, nullptr, false);   // no exceptions: a bad file is a log line
        if (j.is_discarded()) { LOGE("%s rld data: %s%s is not valid JSON -- skipped", TAG, dir.c_str(), rel); continue; }
        auto ws = j.find("weapon_sfx");
        if (ws == j.end() || !ws->is_object()) { LOGW("%s rld data: %s has no weapon_sfx block", TAG, rel); continue; }
        g_cfg.enabled = ws->value("enabled", false);
        g_cfg.master = ws->value("master_volume", 1.0f);
        if (g_cfg.master < 0.0f) g_cfg.master = 0.0f; else if (g_cfg.master > 1.0f) g_cfg.master = 1.0f;   // ext_5 M.play
        g_cfg.default_fallback_wp = ws->value("default_fallback_wp", std::string{});
        read_volumes(*ws, g_cfg.volume);
        auto by = ws->find("by_wp");
        if (by != ws->end() && by->is_object()) {
            for (auto& [wp, e] : by->items()) {
                if (!e.is_object()) continue;
                WeaponSfx w;
                w.folder = e.value("sfx_folder", std::string{});
                w.fallback_wp = e.value("fallback_wp", std::string{});
                read_volumes(e, w.volume);
                for (auto& [k, v] : e.items())
                    if (v.is_string() && k != "sfx_folder" && k != "fallback_wp") w.file[k] = v.get<std::string>();
                g_cfg.by_wp[wp] = std::move(w);
            }
        }
        g_cfg.loaded = true;
        g_cfg.source = rel;
        LOGI("%s rld data: weapon_sfx from reframework\\data\\%s -- enabled=%d master=%.2f, %zu weapon(s)", TAG, rel,
             (int)g_cfg.enabled, g_cfg.master, g_cfg.by_wp.size());
        return true;
    }
    LOGW("%s rld data: no re2_vr_reload.json under reframework\\data\\ (looked for visceral\\reloaded\\ and re2_vr\\) -- no port sounds", TAG);
    return false;
}

} // namespace visceral::port
