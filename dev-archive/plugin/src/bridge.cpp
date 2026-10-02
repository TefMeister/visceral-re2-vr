// bridge.cpp -- the Lua bridge contract: array layout sentinel, slot reads, mailbox hook.
// Split out of Plugin.cpp 2026-09-27, move only (SPLIT-MAP-2026-09-24.md).
#include "visceral.h"

namespace visceral {


// Find the shim's sentinel inside a candidate float array and derive the layout.
// Returns true if a sentinel was found at a plausible element offset.
bool measure_array_layout(API::ManagedObject* a) {
    const auto* bytes = (const uint8_t*)a;
    const uint32_t obj_size = API::get()->sdk()->managed_object->get_size(*a);
    LOGI("%s array layout probe: get_size=%u dwords@0x10..0x2c = %08x %08x %08x %08x %08x %08x %08x %08x", TAG, obj_size,
         *(const uint32_t*)(bytes + 0x10), *(const uint32_t*)(bytes + 0x14), *(const uint32_t*)(bytes + 0x18), *(const uint32_t*)(bytes + 0x1c),
         *(const uint32_t*)(bytes + 0x20), *(const uint32_t*)(bytes + 0x24), *(const uint32_t*)(bytes + 0x28), *(const uint32_t*)(bytes + 0x2c));
    for (uint32_t off = 0x10; off + 4 <= 0x200; off += 4) {
        if (*(const float*)(bytes + off) == 12345.0f) {
            if (off < 0x10 + 63 * 4) continue;               // can't be slot 63 of anything
            const uint32_t elem = off - 63 * 4;
            // the count (64) should sit in one of the dwords between the header and the elements
            uint32_t cnt = 0; bool found = false;
            for (uint32_t c = 0x10; c + 4 <= elem; c += 4) {
                if (*(const uint32_t*)(bytes + c) == 64) { cnt = c; found = true; break; }
            }
            g_arr_elem_off = elem;
            if (found) g_arr_count_off = cnt;
            g_arr_measured = true;
            LOGI("%s ARRAY LAYOUT MEASURED: elements @+0x%x, count %s@+0x%x (sentinel found at +0x%x, get_size=%u)", TAG,
                 g_arr_elem_off, found ? "" : "NOT FOUND, keeping default ", g_arr_count_off, off, obj_size);
            return true;
        }
    }
    LOGW("%s array layout probe: no sentinel 12345 within the first 0x200 bytes", TAG);
    return false;
}

bool bridge_live() { return g.bridge != nullptr && arr_f32(g.bridge)[S_SENTINEL] == 12345.0f; }

Vec3 bridge_vec3(int slot) { const float* f = arr_f32(g.bridge); return Vec3{f[slot], f[slot + 1], f[slot + 2]}; }


// ---------------------------------------------------------------------------
// Lua bridge: hook System.GC.KeepAlive, catch the handed-over float array.
// ---------------------------------------------------------------------------

int pre_mailbox(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    // Identify our array on EVERY call: a second hand-over must also be swallowed,
    // or the game's setter would be run with a float array as its argument.
    bool ours = false;
    for (int i = 0; i < argc && i < 8; ++i) {
        auto* o = (API::ManagedObject*)argv[i];
        if (!is_managed(o)) continue;
        if (tname(o) != "System.Single[]") continue;
        if (!g_arr_measured && !measure_array_layout(o)) continue;
        const auto n = arr_count(o);
        float* f = arr_f32(o);
        if (n != 64 || f[S_SENTINEL] != 12345.0f) { LOGW("%s mailbox saw a System.Single[%u] slot63=%.1f — not ours", TAG, n, f[S_SENTINEL]); continue; }
        ours = true;
        if (g.bridge == nullptr) {
            o->add_ref();
            g.bridge = o;
            f[S_ACK] = 1.0f;
            LOGI("%s VR BRIDGE ATTACHED (argv[%d] of %d, array %p) — Lua shim poses are live", TAG, i, argc, (void*)o);
        }
        break;
    }
    return ours ? REFRAMEWORK_HOOK_SKIP_ORIGINAL : REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void install_bridge_hook() {
    auto& api = API::get();
    auto* m = api->tdb()->find_method("app.ropeway.RagdollControlZoneManager", "set_AccessMutex");
    if (m == nullptr) { LOGE("%s mailbox method RagdollControlZoneManager.set_AccessMutex not found — VR bridge unavailable", TAG); return; }
    const auto id = m->add_hook(pre_mailbox, nullptr, false);
    LOGI("%s bridge mailbox hook installed on RagdollControlZoneManager.set_AccessMutex (id=%u, fn=%p) — check the HookManager lines above for 'Failed to hook'", TAG, id, m->get_function_raw());
}


// ---------------------------------------------------------------------------
// Bridge v2 (2026-10-03): typed reads, the rumble out-channel, the late tick and its timing probe,
// and the entry-order probe. Design: modding-notes/2026-10-03-reloaded-native-architecture.md section 1-2.
// ---------------------------------------------------------------------------

float bridge_f32(int slot) { return bridge_live() ? arr_f32(g.bridge)[slot] : 0.0f; }
Quat bridge_quat(int slot) { const float* f = arr_f32(g.bridge); return Quat{f[slot], f[slot + 1], f[slot + 2], f[slot + 3]}; }
bool bridge_down(int slot) { return bridge_f32(slot) > 0.5f; }

void bridge_rumble(int side, float amp, float sec) {
    if (!bridge_live()) return;
    float* f = arr_f32(g.bridge);
    const int a = side == 0 ? S_RUMBLE_L_AMP : S_RUMBLE_R_AMP;
    // a stronger or longer request already waiting wins; the shim clears both slots when it fires
    if (amp > f[a]) f[a] = amp;
    if (sec > f[a + 1]) f[a + 1] = sec;
}

namespace {
struct LateProbe {
    uint32_t frames{}, differ{}, missed_seq{};
    float last_seq{-1.0f};
    double last_report{};
    bool ver_logged{false};
    uint32_t grip_presses{};
    bool rgrip_prev{false}, lgrip_prev{false};
} g_late;
}

void bridge_late_tick() {
    if (!g_api_ok.load() || !bridge_live()) return;
    float* f = arr_f32(g.bridge);
    f[S_LATE_SEEN] = 1.0f;
    if (!g_late.ver_logged) {
        g_late.ver_logged = true;
        if (f[S_BRIDGE_VER] != BRIDGE_VERSION_WANTED)
            LOGW("%s BRIDGE VERSION MISMATCH: shim says %.0f, plugin wants %.0f -- v2 slots (standing origin, rumble, buttons) are dead until the shim is updated", TAG, f[S_BRIDGE_VER], BRIDGE_VERSION_WANTED);
        else
            LOGI("%s bridge v2 handshake: shim %.0f, late tick live", TAG, f[S_BRIDGE_VER]);
    }
    // the timing probe: did the left hand move between the UpdateHID write and this one, inside one frame?
    if (f[S_WRITE_PT] == 2.0f) {
        g_late.frames++;
        const bool diff = f[S_LPOS] != f[S_LPOS_HID] || f[S_LPOS + 1] != f[S_LPOS_HID + 1] || f[S_LPOS + 2] != f[S_LPOS_HID + 2];
        if (diff) g_late.differ++;
    }
    if (g_late.last_seq >= 0.0f && f[S_WRITE_SEQ] - g_late.last_seq != 2.0f) g_late.missed_seq++;   // two shim writes per frame expected
    g_late.last_seq = f[S_WRITE_SEQ];
    const double t = now_s();
    if (t - g_late.last_report >= BRIDGE_PROBE_REPORT_S) {
        g_late.last_report = t;
        if (f[S_HMD_ACTIVE] > 0.5f)
            LOGI("%s bridge v2 probe: frames=%u differ=%u (UpdateHID vs LateUpdateBehavior left hand) seq_gaps=%u stand_ok=%.0f ctl=%.0f  L grip/trig/A/B %.0f%.0f%.0f%.0f  R %.0f%.0f%.0f%.0f  rstick %.2f %.2f",
                 TAG, g_late.frames, g_late.differ, g_late.missed_seq, f[S_STAND_OK], f[S_USING_CTL],
                 f[S_LGRIP], f[S_LTRIG], f[S_LA], f[S_LB], f[S_RGRIP], f[S_RTRIG], f[S_RA], f[S_RB], f[S_RSTICK], f[S_RSTICK + 1]);
        else
            LOGI("%s bridge v2 probe: no HMD (frames=%u seq_gaps=%u) -- the differ count needs the headset and controllers awake", TAG, g_late.frames, g_late.missed_seq);
    }
    // the rumble proof: the first N grip presses after launch buzz that hand, so one run shows the out-channel works
    const bool rg = f[S_RGRIP] > 0.5f, lg = f[S_LGRIP] > 0.5f;
    if (g_late.grip_presses < (uint32_t)BRIDGE_RUMBLE_PROOF_PRESSES) {
        if (rg && !g_late.rgrip_prev) { g_late.grip_presses++; bridge_rumble(1, BRIDGE_RUMBLE_PROOF_AMP, BRIDGE_RUMBLE_PROOF_SEC); LOGI("%s rumble proof #%u -> right", TAG, g_late.grip_presses); }
        if (lg && !g_late.lgrip_prev) { g_late.grip_presses++; bridge_rumble(0, BRIDGE_RUMBLE_PROOF_AMP, BRIDGE_RUMBLE_PROOF_SEC); LOGI("%s rumble proof #%u -> left", TAG, g_late.grip_presses); }
    }
    g_late.rgrip_prev = rg; g_late.lgrip_prev = lg;
}

// ---- entry-order probe: which via.Application entries run, in what order, inside one frame.
// The frame counter steps at LockScene pre (on_frame), so one "frame" here runs LockScene -> LockScene.
namespace {
constexpr const char* ENTRY_NAMES[] = {"UpdateHID", "UpdateScene", "UpdateBehavior", "UpdateMotion", "UpdateJointExpression",
                                       "LateUpdateBehavior", "LockScene", "PrepareRendering", "WaitRendering", "BeginRendering", "EndRendering"};
constexpr int ENTRY_N = sizeof(ENTRY_NAMES) / sizeof(ENTRY_NAMES[0]);
constexpr int ORDER_MAX = 64;
struct OrderRec { int frame; int idx; bool post; };
OrderRec g_order[ORDER_MAX]; int g_order_n = 0; bool g_order_done = false;
void note_entry(int idx, bool post) {
    if (g_order_done || !g_api_ok.load()) return;
    const int fr = (int)g.frame;
    if (fr < BRIDGE_PROBE_ORDER_FRAME - 1) return;
    if (fr == BRIDGE_PROBE_ORDER_FRAME - 1) { if (g_order_n < ORDER_MAX) g_order[g_order_n++] = OrderRec{fr, idx, post}; return; }
    // first entry of the following frame: print once
    g_order_done = true;
    std::string line;
    for (int i = 0; i < g_order_n; ++i) { line += ENTRY_NAMES[g_order[i].idx]; line += g_order[i].post ? "+ " : "- "; }
    LOGI("%s ENTRY ORDER (frame %d, '-' = pre, '+' = post, from LockScene pre): %s", TAG, BRIDGE_PROBE_ORDER_FRAME - 1, line.c_str());
}
template <int I> void pre_cb() { note_entry(I, false); }
template <int I> void post_cb() { note_entry(I, true); }
template <int I> void reg_entry(const REFrameworkPluginFunctions* fns, bool* ok) {
    if constexpr (I < ENTRY_N) {
        ok[I] = fns->on_pre_application_entry(ENTRY_NAMES[I], &pre_cb<I>) && fns->on_post_application_entry(ENTRY_NAMES[I], &post_cb<I>);
        reg_entry<I + 1>(fns, ok);
    }
}
}

void install_entry_order_probe() {
    if (g_param == nullptr || g_param->functions == nullptr) return;
    bool ok[ENTRY_N] = {};
    reg_entry<0>(g_param->functions, ok);
    std::string missing;
    for (int i = 0; i < ENTRY_N; ++i) if (!ok[i]) { missing += ENTRY_NAMES[i]; missing += ' '; }
    LOGI("%s entry-order probe armed for frame %d%s%s", TAG, BRIDGE_PROBE_ORDER_FRAME - 1, missing.empty() ? "" : "; entries REFramework refused: ", missing.c_str());
}

} // namespace visceral
