// rainhold.cpp -- see rainhold.h.
#include "rainhold.h"
#include "common.h"
#include "settings.h"

#include <atomic>
#include <cstdint>

namespace vn::rainhold {

namespace {
constexpr int LOG_FIRST = 6;          // calls logged before the hook goes quiet
constexpr int NEST_MAX = 8;           // nested requestEffectInternal calls tolerated per thread

std::atomic<int> g_calls{0}, g_held{0}, g_nohold{0};
thread_local void* t_ctx[NEST_MAX];   // the context held by each nesting level (nullptr = not held)
thread_local int t_depth = 0;

int32_t* counter(void* ctx) { return (int32_t*)((char*)ctx + cfg::RAINHOLD_CTX_ALLOC_DEPTH); }

int pre(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) {
    void* ctx = argc > 0 ? argv[0] : nullptr;
    void* mine = API::get()->get_vm_context();
    const int n = g_calls.fetch_add(1);
    void* held = nullptr;
    if (ctx != nullptr && ctx == mine) {
        const int32_t before = *counter(ctx);
        *counter(ctx) = before + 1;
        held = ctx;
        g_held.fetch_add(1);
        if (n < LOG_FIRST) LOGI("%s rainhold: requestEffectInternal: counter %d -> %d (held)", TAG, before, before + 1);
    } else {
        g_nohold.fetch_add(1);
        if (n < LOG_FIRST) LOGI("%s rainhold: requestEffectInternal: argv[0]=%p is not this thread's context %p -- no hold", TAG, ctx, mine);
    }
    if (t_depth < NEST_MAX) t_ctx[t_depth] = held;
    t_depth++;
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post(void**, REFrameworkTypeDefinitionHandle, unsigned long long) {
    if (t_depth <= 0) return;
    t_depth--;
    if (t_depth < NEST_MAX && t_ctx[t_depth] != nullptr) {
        *counter(t_ctx[t_depth]) -= 1;
        t_ctx[t_depth] = nullptr;
    }
}
} // namespace

void install() {
    auto* m = API::get()->tdb()->find_method("via.effect.script.ObjectEffectManager", "requestEffectInternal");
    if (m == nullptr) { LOGE("%s rainhold: ObjectEffectManager.requestEffectInternal not found -- no hold", TAG); return; }
    m->add_hook(pre, post, false);
    LOGI("%s rainhold: hook in: ObjectEffectManager.requestEffectInternal (allocation counter at ctx+0x%x held for the call)", TAG, cfg::RAINHOLD_CTX_ALLOC_DEPTH);
}

} // namespace vn::rainhold
