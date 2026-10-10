// sfx.h -- the reload sound player: miniaudio (public domain, third_party/miniaudio) inside our plugin, playing
// Andyalpa's RELOADED sound pack from reframework/data/custom_sfx/<folder>/<kind>.ogg|.wav (used with his permission;
// credit: Andyalpa). Same idea as RELOADED's ext_5 + REAudio.dll, rewritten; first built 2026-10-03 for the old plugin,
// rebuilt here 2026-10-10. Every play and every failure is logged ("sfx: ...").
#pragma once

namespace vn::sfx {
// kind: "mag_drop", "mag_floor", "mag_grab", "mag_insert", "dry_fire", "slide_rack_pull", "slide_rack_release" ...
// folder: the weapon's sound folder; volume: 1.0 = as recorded. Repeats of one kind within cfg::SFX_DEBOUNCE_SEC drop.
bool play(const char* kind, const char* folder, float volume);
} // namespace vn::sfx
