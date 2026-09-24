// Visceral native core -- idle phase keeper (2026-09-24). See IdlePhase.cpp.
#pragma once
#include "reframework/API.h"

// Install the native hook (MinHook) on the engine's "start the pending motion on this layer" step. Safe to
// call once from reframework_plugin_initialize; logs through the given REFramework functions.
void idle_phase_init(const REFrameworkPluginInitializeParam* param);
// Tell the hook which native TreeLayer is the player's layer 0 (the managed object pointer IS the native one).
// nullptr when there is no player.
void idle_phase_set_layer0(void* layer0);
// Once a second: one log line with the counters (calls seen / layer-0 starts / phase restored / last frames).
void idle_phase_tick_log();
// Master switch (NUM* in Plugin.cpp).
void idle_phase_set_enabled(bool on);
bool idle_phase_enabled();
