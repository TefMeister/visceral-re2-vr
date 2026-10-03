// Visceral — RE2 VR — native core (REFramework plugin, API 1.15)
//
// v0.1 (2026-09-04): the reqs-1-and-3 RECON PROBE, written native from line one
// under the "reach for the deep end" rule. It reads, it does not drive. What it
// answers, all from the game's own objects rather than anything we invent:
//
//   1. Where the off-hand belongs on the equipped weapon. Every RE2 weapon
//      carries an AID JOINT (Implement.get_AidJoint, typed Narrow / Wide /
//      ExtraNarrow via Equipment.getAidJointType) — the engine's own name for
//      "where the support hand goes". The probe logs the joint, its world
//      position, the game's own left-arm IK matrix for it
//      (Implement.getIKLeftArmMatrix) and the aid target matrix.
//   2. Where the player's hands actually are (l/r hand joints on the player
//      skeleton) and where the tracked controllers are (ferried in by a Lua
//      shim, because plugin API 1.15 has no VR calls) — and the distances
//      between all of those, which is req 1's proximity signal.
//   3. The IK controller the game uses for the arms (IkController: which
//      kinds are enabled, the per-arm status list) — the target for a
//      smooth dock/undock that rides the engine's IK instead of fighting it.
//   4. Which motion layer carries locomotion (the /gr speed-lever caveat):
//      every layer's highest-weight motion name, speed and blend rate,
//      logged on change.
//
// Log tag: [visceral]. Hotkeys (numpad only, per the standing rule; game window
// must be foreground): NUM7 = full dump now, NUM8 = toggle 10 Hz trace,
// NUM9 = motion-layer table now.
//
// Lua bridge: visceral_native_bridge.lua creates a System.Single[64], writes the
// sentinel 12345 into slot 63 and hands the array over ONCE by calling a
// "mailbox" method this plugin hooks: app.ropeway.RagdollControlZoneManager.
// set_AccessMutex(System.Object) — static, one object parameter, a real compiled
// game function (System.GC.KeepAlive was tried first on 2026-09-04: it is an
// internal call whose body REFramework cannot resolve in this build, the hook
// failed, and invoking it from Lua crashed the game inside the native invoker).
// The pre-hook SKIPS the original only when the argument is our sentinel array,
// so the game's own calls to that setter pass through untouched. After that the shim writes
// poses into the array every frame and the plugin reads them here. Slot 62 is
// the plugin's acknowledgement (1.0). No Lua C API, no ABI assumptions beyond
// the managed-array element offset, which the sentinel itself verifies.

// v0.4 (2026-09-05): THE DOCK, built on the lever v0.3 found. The game's wrist solver reads
// Implement.getIKLeftArmMatrix() once per frame (which reads get_AidTargetWorldMatrix(), which is
// the aid joint _101's world matrix) and puts l_arm_wrist exactly where the returned matrix says,
// snapping. So the dock is a post-hook on the OUTER getter that returns a BLENDED matrix:
//   * docked   = the bridge's left grip held (S_LGRIP > 0.5 with controllers active) OR the NUM6
//                flat stand-in (a synthetic target orbiting _101 at 10 cm, yawed 45 deg);
//   * weight w slews 0 -> 1 over DOCK_BLEND_S (0.2 s) on dock and back on release, eased;
//   * translation = lerp(natural, target, w); rotation = slerp(natural, target, w) (NUM3 toggles
//     the rotation write, so a bad rotation can be ruled out in VR without a rebuild);
//   * HOLD is latched natively (setForce(64,true)) while docked and dropped on release, reconciled
//     with the NUM4 manual latch (spec v2.3 reqs 1-2; req 3 is the same blend run backwards).
// The controller pose reaches world space two ways, NUM1 cycles them: mode 0 re-bases the
// controller relative to the HMD onto the game camera's world matrix (right whichever space the
// bridge's poses are in, as long as HMD and controller share it); mode 1 uses the bridge pose as
// world directly. Neither has run in a headset yet. The trace logs, per sample: |Lwrist - target|,
// |Lwrist - natural|, the wrist's rotation error against both (does the rotation follow?), and
// |Rwrist - muzzle| (the right hand must not move on dock/undock).

#include "visceral.h"
#include "port/port.h"
#include "IdlePhase.h"   // v0.18: native idle-phase keeper (own file; Plugin.cpp only wires it)

namespace visceral {

const REFrameworkPluginInitializeParam* g_param = nullptr;

std::atomic<bool> g_api_ok{false};

State g;   // the one global state (struct in visceral.h)


// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

bool game_is_foreground() {
    HWND h = GetForegroundWindow();
    if (h == nullptr) return false;
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId();
}

void poll_hotkeys() {
    static bool prev[15] = {};
    const int vks[15] = {VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD1, VK_NUMPAD3, VK_NUMPAD2, VK_NUMPAD0,
                         VK_DECIMAL, VK_ADD, VK_SUBTRACT, VK_MULTIPLY, VK_DIVIDE};   // v0.8: NUM. head hider mode, NUM+ rescan; v0.10: NUM- NUM* NUM/ bracelets
    if (!game_is_foreground()) return;
    for (int i = 0; i < 15; ++i) {
        const bool down = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
        if (down && !prev[i]) {
            if (i == 0) { g.want_dump = true; LOGI("%s NUM7: dump requested", TAG); }
            if (i == 1) { g.trace = !g.trace; LOGI("%s NUM8: trace %s", TAG, g.trace ? "ON (10 Hz)" : "OFF (1 Hz)"); }
            if (i == 2) { g.want_layers = true; LOGI("%s NUM9: layer table requested", TAG); }
            // HOLD itself is reconciled once per frame from force_hold || dock.docked (see update_dock)
            if (i == 3) { g.force_hold = !g.force_hold; LOGI("%s NUM4: force HOLD %s", TAG, g.force_hold ? "ON" : "OFF"); }
            // 8 frames (~130 ms): a real trigger press, long enough for the aim FSM to see it on a frame where HOLD is up.
            if (i == 4) { g.attack_pulse = 8; LOGI("%s NUM5: ATTACK pulse (8 frames)", TAG); set_force(KIND_ATTACK, true); }
            if (i == 5) { g.dock.synthetic = !g.dock.synthetic; if (g.dock.synthetic) g.dock.orbit_t0 = now_s();
                          LOGI("%s NUM6: synthetic dock (flat stand-in for LG held) %s — target orbits _101 at 10 cm, yawed 45 deg", TAG, g.dock.synthetic ? "ON" : "OFF"); }
            if (i == 8) { g.dock.use_reach_clamp = !g.dock.use_reach_clamp;
                          LOGI("%s NUM2: reach clamp %s (off = the v0.5 target, unclamped)", TAG, g.dock.use_reach_clamp ? "ON" : "OFF"); }
            if (i == 6) { g.dock.space_mode = (g.dock.space_mode + 1) % 3;
                          static const char* names[] = {"controller HMD-relative, re-based on the game camera", "bridge pose used as world directly", "as mode 0 with the camera rows transposed"};
                          LOGI("%s NUM1: VR space mode -> %d (%s)", TAG, g.dock.space_mode, names[g.dock.space_mode]); }
            if (i == 7) { g.dock.write_rot = !g.dock.write_rot; LOGI("%s NUM3: rotation write %s", TAG, g.dock.write_rot ? "ON" : "OFF (translation only)"); }
            if (i == 9) { g.plug.enabled = !g.plug.enabled; LOGI("%s NUM0: neck plug %s", TAG, g.plug.enabled ? "ON" : "OFF"); }
            if (i == 10) { g.head.mode = (g.head.mode + 1) % 3;
                           static const char* names[] = {"OFF (originals restored)", "ON with reveals (cutscene / grab / not first person / camera off the head)", "ON, FORCED (no reveals)"};
                           LOGI("%s NUM.: head hider -> %d (%s)", TAG, g.head.mode, names[g.head.mode]); }
            if (i == 11) { g.head.want_rescan = true; LOGI("%s NUM+: head hider rescan requested", TAG); }
            if (i == 12) { g.bracelets.k_idx = (g.bracelets.k_idx + 1) % 4;
                           LOGI("%s NUM-: bracelet twist blend k -> %.1f (0 = rigid to the radius joint; 1 = the wrist's full twist about the forearm)", TAG, BRACELET_K[g.bracelets.k_idx]); }
            if (i == 13) { g.bracelets.enabled = !g.bracelets.enabled; LOGI("%s NUM*: bracelets %s", TAG, g.bracelets.enabled ? "ON" : "OFF"); }
            if (i == 14) { g.bracelets.conv = (g.bracelets.conv + 1) % 4;
                           LOGI("%s NUM/: bracelet twist convention -> %d (order=%s sign=%s); only matters when k > 0", TAG, g.bracelets.conv, (g.bracelets.conv & 1) ? "qa*qr" : "qr*qa", (g.bracelets.conv & 2) ? "-" : "+"); }
        }
        prev[i] = down;
    }
}

void rebind_player(API::ManagedObject* pm, API::ManagedObject* go) {
    g.player_go = go;
    g.cond = inv_ptr(pm, "get_CurrentPlayerCondition");
    g.equipment = inv_ptr(g.cond, "get_Equipment");
    g.ik = inv_ptr(g.cond, "get_IkController");
    g.transform = inv_ptr(go, "get_Transform");
    g.motion = get_component(go, "via.motion.Motion");
    g.l_hand = nullptr; g.r_hand = nullptr; g.aid_joint = nullptr;
    // v0.6: the dock survives a rebind on purpose (its settings and latches must), but a measured arm length must
    // NOT — a different playable character is a different skeleton, and a kept reach would clamp the new arm to
    // the old one's length. Cleared here, where the joints it was measured from are also dropped.
    g.l_humerus = nullptr; g.l_radius = nullptr;
    g.neck0 = nullptr; g.plug.tried = false;   // v0.7: re-resolve the neck on the new skeleton; the plug object itself is kept if still alive
    g.r_radius = nullptr; g.l_wrist = nullptr; g.r_wrist = nullptr; g.bracelets.l.tried = false; g.bracelets.r.tried = false;   // v0.10
    // v0.17: RESTORE BEFORE WIPING. `g.head` holds the only record of which meshes we altered and what their
    // flags were; clearing it first threw that away and left the previous level's meshes with our flags on them.
    head_restore_all("rebind");
    // v0.17: AND CLEAR THE CATCH LIST. It is global and was never cleared, so every load after the first
    // inherited the PREVIOUS level's mesh pointers — dead objects that route E would then try to read and
    // write draw flags on. That is the difference Tefa measured 2026-09-12: on the FIRST load after launching
    // the game the head keeps its shadow and the bracelets are absent; on EVERY load after that the shadow is
    // gone and the bracelets appear, and reloading the very same save reproduces it. The save is not the
    // variable — whether a level has been loaded before in this process is `[verified-live 2026-09-12, n=1 wearer]`.
    g_catch.reset_for_new_player();
    // v0.8: a new player is a new set of meshes — keep the mode, drop everything else (the old components may be dead)
    g.head = State::Head{.mode = g.head.mode};
    g.dock.reach = 0.f; g.dock.reach_raw = 0.f; g.dock.reach_valid = false; g.dock.clamp_on = false;
    g.weapon = nullptr; g.weapon_transform = nullptr;
    g.layer_last.clear();
    LOGI("%s PLAYER BOUND: go=%p (%s) cond=%p (%s) equipment=%p ik=%p transform=%p motion=%p", TAG,
         (void*)go, sysstr(inv_ptr(go, "get_Name")).c_str(), (void*)g.cond, tname(g.cond).c_str(), (void*)g.equipment, (void*)g.ik, (void*)g.transform, (void*)g.motion);
    // v0.8: who is being played — the GameObject is named pl1000 for Claire too (2026-09-06 run); the body's material names say
    LOGI("%s   body mesh materials: %s", TAG, mesh_material_names(get_component(go, "via.render.Mesh")).c_str());
    if (g.transform != nullptr) {
        dump_joints(g.transform, "player skeleton (hand/arm/weapon names only)", false, &g.l_hand, &g.r_hand);
        LOGI("%s   l_hand=%s r_hand=%s", TAG, g.l_hand != nullptr ? joint_name(g.l_hand).c_str() : "NOT FOUND", g.r_hand != nullptr ? joint_name(g.r_hand).c_str() : "NOT FOUND");
    }
    dump_ik();
}

void on_frame() {
    if (!g_api_ok.load()) return;
    auto& api = API::get();
    g.frame++;
    poll_hotkeys();

    auto* pm = api->get_managed_singleton("app.ropeway.PlayerManager");
    if (pm == nullptr) return;
    auto* go = inv_ptr(pm, "get_CurrentPlayer");
    if (go == nullptr) {
        if (g.player_go != nullptr) {
            LOGI("%s player gone (scene change?) — unbinding", TAG);
            // keep what outlives the player: the bridge, the counters, the latches (a forced HOLD must stay reconcilable), the dock settings
            g = State{.bridge = g.bridge, .frame = g.frame, .trace = g.trace, .force_hold = g.force_hold, .hold_sent = g.hold_sent, .dock = g.dock, .plug = g.plug,
                      .head = State::Head{.mode = g.head.mode}};
            g.dock.natural_valid = false; g.dock.target_valid = false;
        }
        return;
    }
    if (go != g.player_go) rebind_player(pm, go);

    auto* w = inv_ptr(g.equipment, "get_EquipWeapon");
    if (w != g.weapon) {
        g.weapon = w;
        g.aid_joint = nullptr; g.w_narrow = nullptr; g.w_wide = nullptr;
        g.weapon_transform = nullptr;
        if (w != nullptr) {
            auto* wgo = inv_ptr(w, "get_GameObject");
            g.weapon_transform = inv_ptr(wgo, "get_Transform");
            LOGI("%s WEAPON CHANGED -> %s (go=%s)", TAG, tname(w).c_str(), sysstr(inv_ptr(wgo, "get_Name")).c_str());
        } else {
            LOGI("%s WEAPON CHANGED -> none", TAG);
        }
        dump_weapon();
    }
    if (g.attack_pulse > 0 && --g.attack_pulse == 0) set_force(KIND_ATTACK, false);
    update_camera2();   // v0.11: every frame, unconditionally — the whole point of the diagnostic
    {
        static double last_t = 0.0;
        const double tn = now_s();
        const double dt = last_t > 0.0 ? std::clamp(tn - last_t, 0.0, 0.1) : 0.0;
        last_t = tn;
        update_dock(tn, dt);
    }
    plug_update();   // v0.7
    port::dryfire_frame();   // 2026-10-03: the port's sound self-test (first right-B presses)
    bracelets_update();   // v0.10
    head_update();   // v0.8
    {
        static bool last_hold = false;
        const bool hold = inv_bool(g.cond, "get_IsHold");
        if (hold != last_hold) { last_hold = hold; LOGI("%s IsHold -> %d (force_hold=%d)", TAG, (int)hold, (int)g.force_hold); if (hold) { dump_weapon(); dump_ik(); } }
    }
    if (g.want_dump) { g.want_dump = false; dump_weapon(); dump_ik(); dump_layers(true); dump_motion(); }
    if (g.want_layers) { g.want_layers = false; dump_layers(true); }

    dump_layers(false);   // logs only on change
    // v0.18: hand the native idle-phase keeper the player's layer 0 (managed pointer == native TreeLayer) and its 1 Hz line
    idle_phase_set_layer0(g.motion != nullptr ? inv_ptr(g.motion, "getLayer", {(void*)(uintptr_t)0}) : nullptr);
    idle_phase_set_layer3(g.motion != nullptr ? inv_ptr(g.motion, "getLayer", {(void*)(uintptr_t)3}) : nullptr);
    if (!g.layer_last.empty()) idle_phase_set_idle_len(g.layer_last[0].find("_OLF_") != std::string::npos ? 1000u : 3354u);
    if (g.frame % 60 == 0) idle_phase_tick_log();

    const double t = now_s();
    const double period = g.trace ? 0.1 : 1.0;
    if (t - g.last_summary_t >= period) { g.last_summary_t = t; summary_line(); }
    if (g.trace && t - g.last_layer_table_t >= 1.0) { g.last_layer_table_t = t; dump_layers(true); }
}

void on_initialized() {
    install_bridge_hook();
    install_shift_hooks();
    install_mesh_catch_hooks();   // v0.15
    port::dryfire_install();      // 2026-10-03: RELOADED port step 2 (sound player + dry-fire)
}

} // namespace visceral

using namespace visceral;


extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    g_param = param;
    const auto* fns = param->functions;
    try {
        API::initialize(param);
        g_api_ok.store(true);
        idle_phase_init(param);   // v0.18: native hook, needs only the module base
    } catch (...) {
        fns->log_error("%s C++ SDK wrapper init failed — probe disabled", TAG);
        return true;
    }
    fns->log_info("%s native core v0.8 loaded — HEAD HIDER (per-pass draw flags, shadow kept; NUM. off/on/forced, NUM+ rescan; needs HideJointMesh OFF) + PLUG read-back, on top of v0.7 — NECK PLUG (own mesh pinned to neck_0, NUM0 toggles) on top of v0.6 — THE DOCK (final-space blend mapped into the getIKLeftArmMatrix hook, HOLD latched while docked, M measured every frame, target clamped to the measured arm length). NUM7 dump / NUM8 trace / NUM9 layers / NUM4 force-HOLD / NUM5 ATTACK pulse / NUM6 synthetic dock (flat) / NUM1 VR space mode / NUM3 rotation write / NUM2 reach clamp", TAG);
    // Hooks need the TDB up; register them from the game thread on the first frame.
    static std::atomic<bool> hooked{false};
    fns->on_pre_application_entry("LockScene", []() {
        if (!hooked.exchange(true)) on_initialized();
        on_frame();
    });
    // bridge v2 (2026-10-03): the late tick is where RELOADED read the hand; the port's hand-driven features
    // will hang off this point, and for now it measures whether the UpdateHID write and this one ever differ.
    fns->on_post_application_entry("LateUpdateBehavior", []() { bridge_late_tick(); });
    install_entry_order_probe();
    return true;
}
