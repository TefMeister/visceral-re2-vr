// port_settings.h -- every number of the RELOADED port, named, in one place (2026-10-03, step 2).
// Design: modding-notes/2026-10-03-reloaded-native-architecture.md section 3 and 6 ("every number lives here or in
// his JSON"). Values copied from Andyalpa's RELOADED Lua where they exist there; the source line is named beside each.
#pragma once

namespace visceral::port {

// ---- where his data lives (shipped with Visceral, credited; Tefa confirmed shipping it 2026-09-27)
// Looked for in this order, under <game>/reframework/data/. The second is where RELOADED itself installs it, so a
// player who already has RELOADED's files gets sound without copying anything.
constexpr const char* RLD_JSON_PATHS[] = {"visceral\\reloaded\\re2_vr_reload.json", "re2_vr\\re2_vr_reload.json"};
constexpr const char* RLD_SFX_ROOTS[] = {"visceral\\reloaded\\custom_sfx", "custom_sfx"};

// ---- the sound player (ext_5)
constexpr float SFX_DEBOUNCE_SEC = 0.10f;        // ext_5 DEBOUNCE_S: the same kind twice inside this = one sound
constexpr float SFX_KIND_VOLUME_MIN = 0.0f;      // ext_5 KIND_VOLUME_MIN
constexpr float SFX_KIND_VOLUME_MAX = 2.0f;      // ext_5 KIND_VOLUME_MAX
constexpr float SFX_KIND_VOLUME_DEFAULT = 1.0f;  // ext_5 KIND_VOLUME_DEFAULT
constexpr int   SFX_VOICES = 16;                 // sounds that can overlap; the oldest finished one is reused
constexpr int   SFX_SELFTEST_PRESSES = 3;        // the first N right-B presses play dry_fire for the current weapon,
                                                 // so one run proves the audio pipeline with no empty gun needed
constexpr const char* SFX_SELFTEST_FALLBACK_WP = "wp0000";   // used by the self-test when no weapon is drawn

// ---- the dry-fire hook (reload.lua 2807-2937)
constexpr int   DRYFIRE_LOG_FIRST = 12;          // the first N requestFire calls are logged with the round count,
                                                 // so the run shows whether the game calls it at all on an empty gun
constexpr const char* DRYFIRE_HOOK_TYPES[] = {"app.ropeway.implement.Gun", "app.ropeway.survivor.Equipment"};
constexpr const char* DRYFIRE_HOOK_METHOD = "requestFire";

} // namespace visceral::port
