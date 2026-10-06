// bridge.cpp -- see bridge.h. The array's element offset is not assumed: it is found by the sentinel.
#include "bridge.h"
#include "common.h"

namespace vn::bridge {

namespace {
MO* g_arr = nullptr;
uint32_t g_elem_off = 0;          // measured from the sentinel
float g_now[S_COUNT]{}, g_prev[S_COUNT]{};

float* elems() { return (float*)((char*)g_arr + g_elem_off); }

bool measure(MO* a) {
    const auto* b = (const uint8_t*)a;
    for (uint32_t off = 0x10 + S_SENTINEL * 4; off <= 0x200; off += 4) {
        if (*(const float*)(b + off) == SENTINEL) {
            g_elem_off = off - S_SENTINEL * 4;
            LOGI("%s bridge array: elements at +0x%x (found by the sentinel)", TAG, g_elem_off);
            return true;
        }
    }
    return false;
}

int pre_mailbox(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    for (int i = 0; i < argc && i < 6; ++i) {
        auto* o = (MO*)argv[i];
        if (!is_managed(o) || type_name(o) != "System.Single[]") continue;
        if (g_arr == o) return REFRAMEWORK_HOOK_SKIP_ORIGINAL;      // a repeat hand-over: still swallow it
        if (!measure(o)) continue;
        o->add_ref();
        g_arr = o;
        elems()[S_ACK] = 1.0f;
        LOGI("%s BRIDGE ATTACHED: controller buttons are live", TAG);
        return REFRAMEWORK_HOOK_SKIP_ORIGINAL;                       // never run the game's setter with our array
    }
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
} // namespace

void install() {
    auto* m = API::get()->tdb()->find_method("app.ropeway.RagdollControlZoneManager", "set_AccessMutex");
    if (m == nullptr) { LOGE("%s mailbox method not found: no controller buttons", TAG); return; }
    m->add_hook(pre_mailbox, nullptr, false);
    LOGI("%s bridge mailbox hook in", TAG);
}

bool live() { return g_arr != nullptr && elems()[S_SENTINEL] == SENTINEL && elems()[S_HMD] > 0.5f; }

void frame_begin() {
    for (int i = 0; i < S_COUNT; ++i) g_prev[i] = g_now[i];
    if (live()) for (int i = 0; i < S_COUNT; ++i) g_now[i] = elems()[i];
    else for (float& v : g_now) v = 0.0f;
}

bool held(Slot s) { return g_now[s] > 0.5f; }
bool pressed(Slot s) { return g_now[s] > 0.5f && g_prev[s] <= 0.5f; }

void rumble(Hand h, float amplitude, float seconds) {
    if (!live()) return;
    const int s = h == LEFT ? S_RUMBLE_L_AMP : S_RUMBLE_R_AMP;
    elems()[s] = amplitude;
    elems()[s + 1] = seconds;
}

Vec3 hmd_pos() { return {g_now[S_HMD_POS], g_now[S_HMD_POS + 1], g_now[S_HMD_POS + 2]}; }
Quat hmd_rot() { return {g_now[S_HMD_ROT], g_now[S_HMD_ROT + 1], g_now[S_HMD_ROT + 2], g_now[S_HMD_ROT + 3]}; }
float view(Slot s) { return live() ? elems()[s] : NO_VALUE; }
Vec3 hand_pos(Hand h) { const int s = h == LEFT ? S_LPOS : S_RPOS; return {g_now[s], g_now[s + 1], g_now[s + 2]}; }

} // namespace vn::bridge
