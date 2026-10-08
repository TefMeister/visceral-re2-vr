// fire.h -- RT fires without holding RG: no aim stance, no latch (Tefa's detour step 2, 2026-10-05).
//
// Found flat on 2026-10-07 (Fable, modding-notes/2026-10-07-fire-without-the-aim-state-three-switches.md): with the
// gun lowered, three of the game's own switches make one press of the ATTACK input a real shot (bullets 7 -> 6, the
// real shoot clip) [verified-live 2026-10-07, n=1 shot]:
//   1. SurvivorActionOrderer.setForcePrecede(true, ATTACK=4) from the pull until the round has gone (b098; b097 held
//      it while RT was down, and the shoot clip kept replaying with no round [verified-live 2026-10-08, Tefa])
//   2. SurvivorUserVariablesUpdater.set_Fire(true) once at the press (the FSM's own shot trigger)
//   3. Equipment.enableAttack(WeaponType) answered YES while our shot is in flight -- but only when the gun is not
//      empty, so an empty gun still clicks (the game's answer is checkHold && !checkEmpty; we drop only checkHold)
// Only while the headset is live, RG is NOT held, a gun (not the knife or a grenade) is in hand, no menu is open and
// the character is not already aiming. Automatics (settings.h FIRE_AUTOMATIC_WP) keep firing while RT is held (b099). RG + RT (the aimed shot) is never touched.
#pragma once

namespace vn::fire {
void install();   // the enableAttack answer + the observe-only doorbells (once, from the game thread)
void frame();     // once per frame, UpdateBehavior pre (where the flat probe applied the switches)
} // namespace vn::fire
