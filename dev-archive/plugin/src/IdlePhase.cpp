// Visceral native core -- idle phase keeper (2026-09-24, home PC, live-debug pass)
//
// The problem: at every RG press (and release) the hold state swaps layer 0's motion for the hold bank's
// slot, and the engine restarts the clip from frame 0. With the ordinary idle spliced into that slot
// (item 22) the clip is the SAME, so the only visible effect is the body sliding from the idle's current
// breathing/sway pose to its frame-0 pose: 1-7 cm at the pelvis, felt in VR as a nudge and a camera step
// (measured 2026-09-24, modding-notes/2026-09-24-the-aim-hunch-is-a-lookat-profile.md, runs 14-19).
// Every managed knob for "start the next motion here" is ignored by the FSM's own transitions
// (changeMotion, ContinueFromPrevEnd, set_Frame, NextStartFrame/ResetStartFrame/NextStartToFrame -- all
// tried, all dead). So this goes to the engine step itself.
//
// What was found with x64dbg (hardware write trap on the layer-0 clip's frame at the press):
//   the frame is zeroed by a "reset clip state" routine (re2.exe static 0x142479100), called from the
//   node start (0x14247d210 <- 0x1424806f0) inside the TreeLayer's "start pending motion" step
//   (0x142488e00, rcx = the native TreeLayer, returns al). Layout on the way to the frame:
//     node   = 0x142481a70(layer + 0x118, [layer+0x10] & 1)      (the layer's current motion node)
//     bank   = *(u32*)(node + 0xa8), id = *(u32*)(node + 0xb0)    (motion bank / slot number)
//     wrap   = *(void**)(node + 0x148)                            (the node's motion player)
//     child  = wrap->vtable[0x178/8](wrap, 0)                     (the clip player)
//     state  = *(void**)(child + 0x38); frame = *(float*)(state + 0x30)
//   TreeLayer.get_Frame / set_Frame walk exactly this chain (0x142484b00 / 0x142487cf0), so the
//   fields are the engine's own, not a guess.
//
// The lever: detour 0x142488e00. Before the original: remember (bank, id, frame) of the player's layer 0.
// After it: if the old and the new motion are both ordinary-idle slots, write the old frame back into the
// clip state. The pose then continues instead of restarting. Nothing else is touched; other layers and
// other characters pass straight through. Which step runs last: the reset is inside the original, our
// write is after it and before the tree evaluates the pose for this frame -- upstream of the blend.
//
// Proof of effect is in the log: `[visceral_phase]` counts starts seen on layer 0 and restores made, with
// the frame it restored; the press probe (Lua) shows the hips within millimetres.

#include "IdlePhase.h"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "MinHook.h"

namespace {

const char* TAG = "[visceral_phase]";
const REFrameworkPluginInitializeParam* g_param = nullptr;

// re2.exe static VAs (image base 0x140000000), pinned build 76298bd
constexpr uintptr_t kStaticBase        = 0x140000000ull;
constexpr uintptr_t kLayerStartPending = 0x142488e00ull;   // bool __fastcall (TreeLayer* layer)
constexpr uintptr_t kGetNode           = 0x142481a70ull;   // void* __fastcall (void* layer_plus_0x118, int flag)
constexpr uintptr_t kLayerSetFrame     = 0x142487cf0ull;   // void __fastcall (TreeLayer* layer, float frame): the engine's own TreeLayer.set_Frame -- seeks the clip player AND re-derives its cached state (vt+0x98 then vt+0x90). Poking the frame field directly left stale caches and crashed at 0x142474734 (runs 3-5, 2026-09-24).
constexpr uintptr_t kOffNodeArea  = 0x118;
constexpr uintptr_t kOffLayerFlags = 0x10;
constexpr uintptr_t kOffNodeBank  = 0xa8;
constexpr uintptr_t kOffNodeId    = 0xb0;
constexpr uintptr_t kOffNodeWrap  = 0x148;
constexpr uintptr_t kVtChildGetter = 0x178;
constexpr uintptr_t kOffChildState = 0x38;
constexpr uintptr_t kOffStateFrame = 0x30;
constexpr uintptr_t kOffStatePrev  = 0x38;   // the reset routine zeroes +0x30, +0x38, +0x40, +0x48 together; +0x38 reads as the previous frame

using StartFn   = bool(__fastcall*)(void* layer);
using GetNodeFn = void*(__fastcall*)(void* node_area, int flag);
using ChildFn   = void*(__fastcall*)(void* wrap, int idx);
using SetFrameFn = void(__fastcall*)(void* layer, float frame);

StartFn   g_orig_start = nullptr;
GetNodeFn g_get_node = nullptr;
SetFrameFn g_set_frame = nullptr;
std::atomic<void*> g_layer0{nullptr};
std::atomic<void*> g_layer3{nullptr};
std::atomic<uint32_t> g_idle_len{3354};   // frames of the ordinary idle now in the hold bank (OFF 3354, OLF 1000), set by Plugin.cpp
std::atomic<bool> g_enabled{false};   // OFF by default until the seek is proven to take (2026-09-24 17:25: set_Frame reads back 0.0 right after); reads + logs stay on

// counters for the 1 Hz line
std::atomic<uint32_t> g_calls{0}, g_layer0_starts{0}, g_switches{0}, g_restored{0}, g_layer3_ended{0};
float g_last_old = -1.f, g_last_new = -1.f;
uint32_t g_last_from_bank = 0, g_last_from_id = 0, g_last_to_bank = 0, g_last_to_id = 0;

void logi(const char* fmt, ...) {
    if (g_param == nullptr || g_param->functions == nullptr || g_param->functions->log_info == nullptr) return;
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    g_param->functions->log_info("%s %s", TAG, buf);
}

// the ordinary-idle slots: bank 1 slot 160 (relaxed Gazing_Idle), bank 2 slot 160 (hold idle, spliced to the
// ordinary idle) and the six raise slots (140/141/143/150/151/153, spliced to idle excerpts)
bool is_raise_slot(uint32_t bank, uint32_t id) {
    return bank == 2 && (id == 140 || id == 141 || id == 143 || id == 150 || id == 151 || id == 153);
}
bool is_idle_slot(uint32_t bank, uint32_t id) {
    if (bank == 1) return id == 160;
    if (bank == 2) return id == 160 || is_raise_slot(bank, id);
    return false;
}

struct Snap { void* node{}; uint32_t bank{}; uint32_t id{}; float* frame{}; };

Snap snapshot(void* layer) {
    Snap s{};
    if (layer == nullptr || g_get_node == nullptr) return s;
    __try {
        const int flag = *(reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(layer) + kOffLayerFlags)) & 1;
        void* node = g_get_node(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(layer) + kOffNodeArea), flag);
        if (node == nullptr) return s;
        s.node = node;
        s.bank = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(node) + kOffNodeBank);
        s.id   = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(node) + kOffNodeId);
        void* wrap = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + kOffNodeWrap);
        if (wrap == nullptr) return s;
        void** vt = *reinterpret_cast<void***>(wrap);
        if (vt == nullptr) return s;
        auto child_getter = reinterpret_cast<ChildFn>(vt[kVtChildGetter / sizeof(void*)]);
        if (child_getter == nullptr) return s;
        void* child = child_getter(wrap, 0);
        if (child == nullptr) return s;
        void* state = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(child) + kOffChildState);
        if (state == nullptr) return s;
        s.frame = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(state) + kOffStateFrame);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s = Snap{};
    }
    return s;
}

bool __fastcall detour_start(void* layer) {
    g_calls.fetch_add(1);
    void* l0 = g_layer0.load();
    void* l3 = g_layer3.load();
    if (layer == l3 && l3 != nullptr) {
        // layer 3 (upper-body action): the raise state ends on MOTION END of this layer's clip. With the FULL
        // ordinary idle in the raise slots (v4 base list / v2 light list) it would run 56 s and freeze the torso
        // (run 6, 2026-09-24); so the moment layer 3 starts a raise slot, put its clip at its last frame. This
        // write lands AFTER the engine's own reset (it happens inside the original), which is why the same
        // write from Lua at LateUpdateBehavior never stuck (runs 17/18).
        const Snap before = snapshot(layer);
        const bool r = g_orig_start(layer);
        if (!g_enabled.load()) return r;
        const Snap after = snapshot(layer);
        if (after.node != nullptr && after.frame != nullptr && is_raise_slot(after.bank, after.id)
            && (before.bank != after.bank || before.id != after.id)) {
            // put the clip at its last frame -- and make the PREVIOUS frame the same, so the engine does not walk
            // from 0 to the end (a jump of 488 frames on layer 0 and one of 3353 here both crashed the game at
            // 0x142474734, a per-frame-cache walk; a jump of 142 survived -- 2026-09-24 17:13 / 17:18)
            // seek through the engine's own set_Frame so every cached field follows (a raw field write crashed)
            const float last = static_cast<float>(g_idle_len.load()) - 2.f;
            if (g_set_frame != nullptr) g_set_frame(layer, last);
            g_layer3_ended.fetch_add(1);
            logi("layer3 raise slot %u started -> set_Frame(%.0f) so the raise state ends now (reads %.1f)", after.id, last, *after.frame);
        }
        return r;
    }
    if (layer != l0 || l0 == nullptr) return g_orig_start(layer);
    g_layer0_starts.fetch_add(1);
    const Snap before = snapshot(layer);
    const float old_frame = before.frame != nullptr ? *before.frame : -1.f;
    const bool r = g_orig_start(layer);
    if (!g_enabled.load()) return r;
    const Snap after = snapshot(layer);
    // this engine step runs EVERY frame for the layer (first deploy logged it 60x/s); only a real switch matters
    const bool switched = before.node != nullptr && after.node != nullptr && (before.bank != after.bank || before.id != after.id);
    if (!switched) return r;
    g_switches.fetch_add(1);
    g_last_from_bank = before.bank; g_last_from_id = before.id; g_last_to_bank = after.bank; g_last_to_id = after.id;
    g_last_old = old_frame; g_last_new = after.frame != nullptr ? *after.frame : -1.f;
    if (before.frame != nullptr && after.frame != nullptr && old_frame >= 0.f
        && is_idle_slot(before.bank, before.id) && is_idle_slot(after.bank, after.id)) {
        // never write a frame at or beyond the clip's length: frame 488 into a 20-frame excerpt read past its
        // keyframe tables and crashed the game (17:13:22, RIP 0x142474734). The raise slots now hold the full
        // idle, and the length comes from Plugin.cpp (OFF 3354 / OLF 1000); wrap as the loop would.
        const float len = static_cast<float>(g_idle_len.load());
        float target = old_frame;
        if (len > 1.f && target >= len - 1.f) target = std::fmod(target, len);
        if (g_set_frame != nullptr) g_set_frame(layer, target);   // the engine's own seek, caches included
        g_restored.fetch_add(1);
        logi("layer0 idle -> idle: bank %u slot %u (frame %.1f) -> bank %u slot %u: reset to %.1f, restored %.1f",
             before.bank, before.id, old_frame, after.bank, after.id, g_last_new, target);
    }
    return r;
}

}  // namespace

void idle_phase_init(const REFrameworkPluginInitializeParam* param) {
    g_param = param;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (base == 0) { logi("no module base; hook not installed"); return; }
    const auto target = base + (kLayerStartPending - kStaticBase);
    g_get_node = reinterpret_cast<GetNodeFn>(base + (kGetNode - kStaticBase));
    g_set_frame = reinterpret_cast<SetFrameFn>(base + (kLayerSetFrame - kStaticBase));
    // sanity: the target must start with `push rdi; sub rsp, 0x50` -- encoded 40 57 48 83 EC 50 (REX-prefixed
    // push; the first deploy checked for 57 48 83 EC 50 and refused its own target, 2026-09-24 17:04)
    const auto* b = reinterpret_cast<const uint8_t*>(target);
    if (!(b[0] == 0x40 && b[1] == 0x57 && b[2] == 0x48 && b[3] == 0x83 && b[4] == 0xEC && b[5] == 0x50)) {
        logi("target 0x%llx does not look like TreeLayer start-pending (bytes %02x %02x %02x %02x %02x %02x) -- different build? hook NOT installed",
             (unsigned long long)target, b[0], b[1], b[2], b[3], b[4], b[5]);
        return;
    }
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) { logi("MinHook init failed"); return; }
    if (MH_CreateHook(reinterpret_cast<LPVOID>(target), reinterpret_cast<LPVOID>(&detour_start), reinterpret_cast<LPVOID*>(&g_orig_start)) != MH_OK) {
        logi("MH_CreateHook failed"); return;
    }
    if (MH_EnableHook(reinterpret_cast<LPVOID>(target)) != MH_OK) { logi("MH_EnableHook failed"); return; }
    logi("hook installed on TreeLayer start-pending @%p (static 0x%llx); get_node @%p", (void*)target, (unsigned long long)kLayerStartPending, (void*)g_get_node);
}

void idle_phase_set_layer0(void* layer0) { g_layer0.store(layer0); }
void idle_phase_set_layer3(void* layer3) { g_layer3.store(layer3); }
void idle_phase_set_idle_len(uint32_t frames) { g_idle_len.store(frames); }
void idle_phase_set_enabled(bool on) { g_enabled.store(on); logi("enabled=%d", on ? 1 : 0); }
bool idle_phase_enabled() { return g_enabled.load(); }

void idle_phase_tick_log() {
    logi("steps seen %u | layer0 steps %u | layer0 switches %u | restored %u | layer3 raises ended %u | last switch: bank %u slot %u -> bank %u slot %u, frame %.1f -> %.1f | idle len %u | layer0=%p layer3=%p enabled=%d",
         g_calls.exchange(0), g_layer0_starts.exchange(0), g_switches.exchange(0), g_restored.exchange(0), g_layer3_ended.exchange(0),
         g_last_from_bank, g_last_from_id, g_last_to_bank, g_last_to_id, g_last_old, g_last_new, g_idle_len.load(), g_layer0.load(), g_layer3.load(), g_enabled.load() ? 1 : 0);
}
