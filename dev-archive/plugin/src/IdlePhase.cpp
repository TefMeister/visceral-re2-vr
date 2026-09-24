// Visceral native core -- idle phase keeper (2026-09-24, home PC; live-debug pass in /lm, request-block rewrite in /pd)
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
//     node   = 0x142481a70(layer + 0x118, idx)                    idx 0 = newest node (the PENDING one while
//                                                                 flag bit 0 is set), idx 1 = the one before it
//     bank   = *(u32*)(node + 0xa8), id = *(u32*)(node + 0xb0)    (motion bank / slot number)
//     wrap   = *(void**)(node + 0x148)                            (the node's motion player)
//     child  = wrap->vtable[0x178/8](wrap, 0)                     (the clip player)
//     state  = *(void**)(child + 0x38); frame = *(float*)(state + 0x30)
//     len    = wrap->vtable[0xd0/8](wrap)                         (float: the clip's length in frames)
//   TreeLayer.get_Frame / set_Frame walk exactly this chain (0x142484b00 / 0x142487cf0), so the
//   fields are the engine's own, not a guess.
//
// THE LEVER (found static 2026-09-24 /pd, disassembly of 0x142488e00 + 0x142488940): the layer carries a
// "start options" block at layer+0x2a0..0x2c0, cleared by 0x1424736b0(layer+0x270) before a request is
// written and again at the END of the start step after it is consumed. Inside the step, right after the
// node start, 0x142488940(layer, wrap) reads it:
//     mode = *(u8*)(layer + 0x2bc):  0 = start at frame 0 (what the FSM's transitions get)
//                                    1 = start at *(float*)(layer + 0x2b4)            (changeMotion(frame) sets this)
//                                    2 = start at *(float*)(layer + 0x2b4) * len      (a FRACTION of the clip)
//                                    3 = keyed on the previous clip; 4/5 = "continue from the previous node"
//   and then does the whole seek itself: wrap vt+0x98 (set frame), vt+0x90 (advance 0), vt+0xf0, the node's
//   frame cache (0x142480f80), the layer's delta (0x142485c40) -- every cached field follows, in the engine's
//   own order, AFTER the reset and BEFORE the tree evaluates the pose. Two early-outs skip the block entirely:
//   layer flag bit 7 ([layer+0x10] & 0x80) and ([layer+0x164] == 3 or 4 with [layer+0x158] != 0); both are
//   logged so a "did not take" can be read from the log.
//
// So the detour does what changeMotion does, one step later than it can: when the step is about to start
// an idle slot in place of an idle slot on the player's layer 0, it writes mode 2 with fraction =
// old_frame / old_len before calling the original. Mode 2 (a fraction) rather than mode 1 (a frame) because
// the target clip's length is not known before the start binds it, and a frame past the end of a 20-frame
// raise excerpt crashed the game on 2026-09-24 (0x142474734); a fraction can never be out of range, and for
// same-length clips it is the old frame exactly. Layer 3 (upper-body raise): on a raise-slot start it asks
// for fraction (idle_len - 2) / idle_len so the raise state ends within a frame or two whatever the clip's
// length (full idle in v4 lists, 20-frame excerpt in v5).
//
// Which step runs last: the FSM queues the transition (pending bit) before this step; the step consumes the
// request block inside 0x142488940 after the reset; our write lands between the two -- upstream of the seek,
// upstream of the blend. Proof of effect is in the log: every real switch prints the requested fraction, the
// frame/length before, and the frame/length that reads back after the step.
//
// Switch: the hook only ACTS when the file  reframework/plugins/visceral_idle_phase.on  exists beside the
// DLL (checked at start and once a second, so it can be flipped live). Without it the hook reads and logs
// only. No numpad key: all fifteen are taken.

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
constexpr uintptr_t kGetNode           = 0x142481a70ull;   // void* __fastcall (void* layer_plus_0x118, int idx)
constexpr uintptr_t kOffNodeArea   = 0x118;
constexpr uintptr_t kOffLayerFlags = 0x10;    // bit 0 = a motion is pending; bit 7 = skips the start options
constexpr uintptr_t kOffLayerKind  = 0x164;   // u32; 3 or 4 together with a non-null +0x158 skips the start options
constexpr uintptr_t kOffLayerLink  = 0x158;
constexpr uintptr_t kOffReqFrame   = 0x2b4;   // float: the requested start frame (mode 1) or fraction (mode 2)
constexpr uintptr_t kOffReqMode    = 0x2bc;   // u8: 0 none, 1 frame, 2 fraction, 3 keyed, 4/5 continue
constexpr uintptr_t kOffNodeBank   = 0xa8;
constexpr uintptr_t kOffNodeId     = 0xb0;
constexpr uintptr_t kOffNodeWrap   = 0x148;
constexpr uintptr_t kVtChildGetter = 0x178;
constexpr uintptr_t kVtClipLength  = 0xd0;
constexpr uintptr_t kOffChildState = 0x38;
constexpr uintptr_t kOffStateFrame = 0x30;
constexpr uint8_t   kModeFraction  = 2;
constexpr float     kMaxFraction   = 0.999f;
constexpr int       kSwitchLogMax  = 200;     // per-switch lines are capped so a long session cannot flood the log

using StartFn   = bool(__fastcall*)(void* layer);
using GetNodeFn = void*(__fastcall*)(void* node_area, int idx);
using ChildFn   = void*(__fastcall*)(void* wrap, int idx);
using LenFn     = float(__fastcall*)(void* wrap);

StartFn   g_orig_start = nullptr;
GetNodeFn g_get_node = nullptr;
std::atomic<void*> g_layer0{nullptr};
std::atomic<void*> g_layer3{nullptr};
std::atomic<uint32_t> g_idle_len{3354};   // frames of the ordinary idle now in the hold bank (OFF 3354, OLF 1000), set by Plugin.cpp
std::atomic<bool> g_enabled{false};       // follows the switch file; reads + logs stay on either way
wchar_t g_switch_path[MAX_PATH] = {};

// counters for the 1 Hz line
std::atomic<uint32_t> g_calls{0}, g_pending{0}, g_switches{0}, g_requested{0}, g_layer3_ended{0};
int g_switch_lines = 0;
float g_last_old = -1.f, g_last_new = -1.f;
uint32_t g_last_from_bank = 0, g_last_from_id = 0, g_last_to_bank = 0, g_last_to_id = 0;

void logi(const char* fmt, ...) {
    if (g_param == nullptr || g_param->functions == nullptr || g_param->functions->log_info == nullptr) return;
    char buf[640];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    g_param->functions->log_info("%s %s", TAG, buf);
}

// the ordinary-idle slots: bank 1 slot 160 (relaxed Gazing_Idle), bank 2 slot 160 (hold idle, spliced to the
// ordinary idle) and the six raise slots (140/141/143/150/151/153, spliced to the idle or an excerpt of it)
bool is_raise_slot(uint32_t bank, uint32_t id) {
    return bank == 2 && (id == 140 || id == 141 || id == 143 || id == 150 || id == 151 || id == 153);
}
bool is_idle_slot(uint32_t bank, uint32_t id) {
    if (bank == 1) return id == 160;
    if (bank == 2) return id == 160 || is_raise_slot(bank, id);
    return false;
}

template <typename T> T& at(void* base, uintptr_t off) { return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + off); }

struct Snap { void* node{}; uint32_t bank{}; uint32_t id{}; float frame{-1.f}; float len{-1.f}; };

// idx 0 = the newest node in the layer's ring (the pending one while flag bit 0 is set), 1 = the one before.
// with_clip: also read the clip's frame and length (only safe on a node whose clip is bound)
Snap snapshot(void* layer, int idx, bool with_clip) {
    Snap s{};
    if (layer == nullptr || g_get_node == nullptr) return s;
    __try {
        void* node = g_get_node(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(layer) + kOffNodeArea), idx);
        if (node == nullptr) return s;
        s.node = node;
        s.bank = at<uint32_t>(node, kOffNodeBank);
        s.id   = at<uint32_t>(node, kOffNodeId);
        if (!with_clip) return s;
        void* wrap = at<void*>(node, kOffNodeWrap);
        if (wrap == nullptr) return s;
        void** vt = *reinterpret_cast<void***>(wrap);
        if (vt == nullptr) return s;
        auto len_getter = reinterpret_cast<LenFn>(vt[kVtClipLength / sizeof(void*)]);
        if (len_getter != nullptr) s.len = len_getter(wrap);
        auto child_getter = reinterpret_cast<ChildFn>(vt[kVtChildGetter / sizeof(void*)]);
        if (child_getter == nullptr) return s;
        void* child = child_getter(wrap, 0);
        if (child == nullptr) return s;
        void* state = at<void*>(child, kOffChildState);
        if (state == nullptr) return s;
        s.frame = at<float>(state, kOffStateFrame);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s = Snap{};
    }
    return s;
}

struct Req { uint8_t prev_mode{}; float prev_value{}; bool flag7{}; uint32_t kind{}; bool linked{}; bool written{}; };

// write the start request the engine's own changeMotion writes, one step later than it can
Req request_fraction(void* layer, float fraction) {
    Req r{};
    __try {
        r.prev_mode  = at<uint8_t>(layer, kOffReqMode);
        r.prev_value = at<float>(layer, kOffReqFrame);
        r.flag7      = (at<uint32_t>(layer, kOffLayerFlags) & 0x80u) != 0;
        r.kind       = at<uint32_t>(layer, kOffLayerKind);
        r.linked     = at<void*>(layer, kOffLayerLink) != nullptr;
        at<float>(layer, kOffReqFrame) = fraction;
        at<uint8_t>(layer, kOffReqMode) = kModeFraction;
        r.written = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r.written = false;
    }
    return r;
}

bool pending(void* layer) {
    __try { return (at<uint32_t>(layer, kOffLayerFlags) & 1u) != 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int current_index(void* layer) {
    __try { return static_cast<int>(at<uint32_t>(layer, kOffLayerFlags) & 1u); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

bool __fastcall detour_start(void* layer) {
    g_calls.fetch_add(1);
    void* l0 = g_layer0.load();
    void* l3 = g_layer3.load();
    const bool ours = layer != nullptr && (layer == l0 || layer == l3);
    // the step runs every frame for every layer of every character; only a pending switch on the player's
    // layer 0 or 3 is our business (the original returns at once when bit 0 is clear)
    if (!ours || !pending(layer)) return g_orig_start(layer);
    g_pending.fetch_add(1);
    const Snap cur  = snapshot(layer, 1, true);    // the node playing now (its clip is bound)
    const Snap next = snapshot(layer, 0, false);   // the node about to start (clip not bound yet: names only)
    const bool enabled = g_enabled.load();
    Req req{};
    float fraction = -1.f;
    const bool layer3 = layer == l3;
    if (enabled && next.node != nullptr) {
        if (layer3) {
            // the raise state ends on MOTION END of layer 3's clip; ask for the last frame or two, whatever the
            // clip's length (full idle in v4 lists, 20-frame excerpt in v5)
            if (is_raise_slot(next.bank, next.id)) {
                const float len = static_cast<float>(g_idle_len.load());
                fraction = len > 2.f ? (len - 2.f) / len : kMaxFraction;
                if (fraction > kMaxFraction) fraction = kMaxFraction;
                req = request_fraction(layer, fraction);
                if (req.written) g_layer3_ended.fetch_add(1);
            }
        } else if (cur.node != nullptr && cur.frame >= 0.f && cur.len > 1.f
                   && is_idle_slot(cur.bank, cur.id) && is_idle_slot(next.bank, next.id)) {
            fraction = cur.frame / cur.len;
            if (fraction < 0.f) fraction = 0.f;
            if (fraction > kMaxFraction) fraction = kMaxFraction;
            req = request_fraction(layer, fraction);
            if (req.written) g_requested.fetch_add(1);
        }
    }
    const bool r = g_orig_start(layer);
    const Snap after = snapshot(layer, current_index(layer), true);
    const bool switched = cur.node != nullptr && after.node != nullptr && (cur.bank != after.bank || cur.id != after.id);
    if (!switched) return r;
    g_switches.fetch_add(1);
    if (!layer3) {
        g_last_from_bank = cur.bank; g_last_from_id = cur.id; g_last_to_bank = after.bank; g_last_to_id = after.id;
        g_last_old = cur.frame; g_last_new = after.frame;
    }
    if (g_switch_lines < kSwitchLogMax) {
        ++g_switch_lines;
        if (req.written) {
            logi("layer%d switch bank %u slot %u (frame %.1f of %.0f) -> bank %u slot %u: asked fraction %.4f, reads frame %.1f of %.0f | prev mode %u value %.3f, flag7 %d, kind %u linked %d",
                 layer3 ? 3 : 0, cur.bank, cur.id, cur.frame, cur.len, after.bank, after.id, fraction, after.frame, after.len,
                 req.prev_mode, req.prev_value, req.flag7 ? 1 : 0, req.kind, req.linked ? 1 : 0);
        } else {
            logi("layer%d switch bank %u slot %u (frame %.1f of %.0f) -> bank %u slot %u: reads frame %.1f of %.0f (no request%s)",
                 layer3 ? 3 : 0, cur.bank, cur.id, cur.frame, cur.len, after.bank, after.id, after.frame, after.len,
                 enabled ? "" : ", switch file absent");
        }
    }
    return r;
}

bool switch_file_present() {
    if (g_switch_path[0] == 0) return false;
    return GetFileAttributesW(g_switch_path) != INVALID_FILE_ATTRIBUTES;
}

}  // namespace

void idle_phase_init(const REFrameworkPluginInitializeParam* param) {
    g_param = param;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (base == 0) { logi("no module base; hook not installed"); return; }
    // the switch file lives beside the DLL: <game>\reframework\plugins\visceral_idle_phase.on
    wchar_t exe[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exe, MAX_PATH) > 0) {
        wchar_t* slash = wcsrchr(exe, L'\\');
        if (slash != nullptr) { *slash = 0; _snwprintf_s(g_switch_path, MAX_PATH, _TRUNCATE, L"%s\\reframework\\plugins\\visceral_idle_phase.on", exe); }
    }
    g_enabled.store(switch_file_present());
    const auto target = base + (kLayerStartPending - kStaticBase);
    g_get_node = reinterpret_cast<GetNodeFn>(base + (kGetNode - kStaticBase));
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
    logi("hook installed on TreeLayer start-pending @%p (static 0x%llx); get_node @%p; switch file %s -> %s",
         (void*)target, (unsigned long long)kLayerStartPending, (void*)g_get_node, g_switch_path[0] ? "set" : "unknown", g_enabled.load() ? "ON (requests written)" : "OFF (reads and logs only)");
}

void idle_phase_set_layer0(void* layer0) { g_layer0.store(layer0); }
void idle_phase_set_layer3(void* layer3) { g_layer3.store(layer3); }
void idle_phase_set_idle_len(uint32_t frames) { g_idle_len.store(frames); }
void idle_phase_set_enabled(bool on) { g_enabled.store(on); logi("enabled=%d", on ? 1 : 0); }
bool idle_phase_enabled() { return g_enabled.load(); }

void idle_phase_tick_log() {
    const bool file = switch_file_present();
    if (file != g_enabled.load()) { g_enabled.store(file); logi("switch file %s -> %s", file ? "appeared" : "removed", file ? "ON" : "OFF"); }
    logi("steps seen %u | pending on ours %u | switches %u | requests written %u | layer3 raises shortened %u | last layer0 switch: bank %u slot %u -> bank %u slot %u, frame %.1f -> %.1f | idle len %u | layer0=%p layer3=%p enabled=%d",
         g_calls.exchange(0), g_pending.exchange(0), g_switches.exchange(0), g_requested.exchange(0), g_layer3_ended.exchange(0),
         g_last_from_bank, g_last_from_id, g_last_to_bank, g_last_to_id, g_last_old, g_last_new, g_idle_len.load(), g_layer0.load(), g_layer3.load(), g_enabled.load() ? 1 : 0);
}
