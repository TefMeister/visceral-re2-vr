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

} // namespace visceral
