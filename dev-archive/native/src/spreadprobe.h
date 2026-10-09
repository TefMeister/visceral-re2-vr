// spreadprobe.h -- PROBE (b124, 2026-10-10, idea: bullet spread by stance and hands, Tefa 02:00).
// RE2's accuracy is the "reticle fit": Equipment._ReticleFitPoint grows while the aim is held still and shrinks when
// moving or shooting, by the gun's ReticleParam (_AddPoint, _KeepPoint, _MovePoint, _ShootPoint, _WatchPoint,
// _PointRange, MAX_POINT) [inferred-static 2026-10-10, type database 2026-09-25]. Before writing tiers into it we need
// the real numbers: the fit right after the aim starts standing still (Tefa's "ok" one-handed value), walking while
// aiming (the "running" value), the maximum (two hands, still = no spread), and whether it is used at all when firing
// with RT alone (no aim state). This logs, on change, the fit point and IsReticleFit with IsHold / IsWalk / IsJog /
// LG held, the gun's ReticleParam once per weapon, and the fit at every RT press.
// Lines: "spread" in re2_framework_log.txt, at most cfg::SPREADPROBE_MAX_LINES per launch. Remove after the tiers land.
#pragma once

namespace vn::spreadprobe {
void frame();   // once per frame (UpdateBehavior pre)
} // namespace vn::spreadprobe
