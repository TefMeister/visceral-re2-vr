// spread.h -- bullet spread by stance and hands (Tefa 2026-10-10 02:00, idea in mod-ideas).
//
// RE2's accuracy is the reticle fit, Equipment._ReticleFitPoint, 0 (widest) to 100 (dead on, IsReticleFit) for the
// Matilda (ReticleParam Range 0-100, Add 150/s, Move -1000, Shoot -5) [verified-live 2026-10-10, b124 probe]. Measured:
// aiming still it climbs 0 -> 100 in ~0.65 s; aiming and walking it is 0; firing with RT alone (running, or standing
// docked with two hands) it is 0 [verified-live 2026-10-10, n=16 shots].
// Tefa's tiers (2026-10-10 02:30), as a share of the gun's best accuracy: running 25% (any hands); walking with a long gun 50%, with a handgun 75%; standing
// still with a long gun in one hand 75%; standing still with a handgun in one hand 100%; standing still with two hands
// 100%. Values in settings.h.
// Written into the fit point every frame and again right before each shot (Equipment.requestFire pre), so the shot
// uses it. "Two hands" = the left grip held with a gun in hand and the controllers within cfg::SPREAD_DOCK_HANDS_M
// (the framework docks only while the left grip is held, so this follows the dock). Logs the tier at every shot.
#pragma once

namespace vn::spread {
void install();   // the requestFire hook (once, from the game thread)
void frame();     // once per frame (UpdateBehavior pre)
} // namespace vn::spread
