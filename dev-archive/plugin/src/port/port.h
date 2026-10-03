// port.h -- the RELOADED port's shared declarations (2026-10-03). One file per feature under src/port/;
// Plugin.cpp only calls the port_* entry points below and holds no feature logic.
#pragma once

#include <map>
#include <string>

namespace visceral::port {

// ---- rld_data.cpp: his JSON, read as data
struct WeaponSfx {
    std::string folder;                          // sfx_folder (empty = the weapon id itself)
    std::string fallback_wp;                     // another weapon id to borrow sounds from
    std::map<std::string, std::string> file;     // kind -> file name ("" = none)
    std::map<std::string, float> volume;         // kind -> multiplier
};
struct SfxConfig {
    bool loaded{false};
    bool enabled{false};
    float master{1.0f};
    std::string default_fallback_wp;
    std::map<std::string, float> volume;         // global kind -> multiplier
    std::map<std::string, WeaponSfx> by_wp;
    std::string source;                          // the file it came from, for the log
};
const SfxConfig& rld_sfx_config();
bool rld_data_load();                            // safe to call again; logs what it found

// ---- rld_sfx.cpp: miniaudio, one engine for the plugin's life
bool sfx_init();                                 // lazy; true when the device is open
bool sfx_play(const char* kind, const std::string& wp);   // false + one log line saying why when it cannot
std::string sfx_root();                          // the custom_sfx folder in use ("" if none)

// ---- rld_dryfire.cpp: the click on an empty trigger
void dryfire_install();                          // hooks requestFire (Gun + Equipment); call once the TDB is up
void dryfire_frame();                            // per frame: the self-test presses

// ---- shared: the drawn weapon's id ("wp0000") or "" -- the name RELOADED keys everything by
std::string current_wp();

} // namespace visceral::port
