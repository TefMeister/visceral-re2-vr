// spreadprobe.cpp -- see spreadprobe.h.
#include "spreadprobe.h"
#include "bridge.h"
#include "common.h"
#include "settings.h"
#include "weapons.h"

#include <cmath>

namespace vn::spreadprobe {

namespace {
int g_lines = 0;
int g_frame = 0;
int g_last_wp = -99;
float g_last_fit = -1.0f;
int g_last_state = -1;

bool flag(MO* cond, const char* getter) { return call_direct<bool>(cond, getter, false); }

void log_param(MO* eq, int wp) {
    auto* rp = call_ptr(eq, "get_ReticleParam");
    if (rp == nullptr) { LOGI("%s spread: WP%04d has no ReticleParam", TAG, wp); return; }
    auto f = [&](const char* n) { auto* p = field_ptr<float>(rp, n); return p ? *p : NAN; };
    auto* range = field_ptr<float>(rp, "_PointRange");   // via.Range {s, r}
    LOGI("%s spread: WP%04d ReticleParam Add %.3f Keep %.3f Move %.3f Shoot %.3f Watch %.3f Range (%.3f %.3f) MAX %.3f",
         TAG, wp, f("_AddPoint"), f("_KeepPoint"), f("_MovePoint"), f("_ShootPoint"), f("_WatchPoint"),
         range ? range[0] : NAN, range ? range[1] : NAN, f("MAX_POINT"));
}
} // namespace

void frame() {
    ++g_frame;
    if (g_lines >= cfg::SPREADPROBE_MAX_LINES || !bridge::live()) return;
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    auto* go = call_ptr(pm, "get_CurrentPlayer");
    auto* eq = component(go, "app.ropeway.survivor.Equipment");
    auto* cond = component(go, "app.ropeway.survivor.SurvivorCondition");
    if (eq == nullptr || cond == nullptr) return;
    const int wp = weapons::current_id();
    if (wp < 0 || wp == 4500 || wp == 4510 || wp == 6200 || wp == 6300) { g_last_wp = wp; return; }   // guns only
    if (wp != g_last_wp) { g_last_wp = wp; log_param(eq, wp); ++g_lines; }

    auto* fitp = field_ptr<float>(eq, "_ReticleFitPoint");
    auto* isfit = field_ptr<bool>(eq, "_IsReticleFit");
    const float fit = fitp ? *fitp : NAN;
    const int state = (flag(cond, "get_IsHold") ? 1 : 0) | (flag(cond, "get_IsWalk") ? 2 : 0) | (flag(cond, "get_IsJog") ? 4 : 0) |
                      (bridge::held(bridge::S_LGRIP) ? 8 : 0) | ((isfit && *isfit) ? 16 : 0);
    const bool shot = bridge::pressed(bridge::S_RTRIG);
    const bool fit_moved = std::fabs(fit - g_last_fit) > cfg::SPREADPROBE_FIT_STEP;
    if (!shot && !fit_moved && state == g_last_state) return;
    g_last_fit = fit;
    g_last_state = state;
    ++g_lines;
    LOGI("%s spread f%d%s: fit %.3f IsReticleFit %d | IsHold %d IsWalk %d IsJog %d LG %d",
         TAG, g_frame, shot ? " RT PRESSED" : "", fit, (state >> 4) & 1, state & 1, (state >> 1) & 1, (state >> 2) & 1, (state >> 3) & 1);
}

} // namespace vn::spreadprobe
