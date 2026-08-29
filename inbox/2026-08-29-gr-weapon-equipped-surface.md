# /gr drop: public complement to dossier §8/§11 — the equipped-weapon surface

From a research session on "the weapon-equipped state and changing game behavior in it."
Full write-ups: `visceral-re2-vr-external-research/topics/2026-08-29-weapon-equipped-state-surface.md`
and `...-caf-custom-animation-framework.md`. Everything `[reported]` (read from public source,
mainly REFramework `FirstPerson.cpp` and the RE2R Custom Animation Framework repo), not yet
verified live.

Suggested dossier additions once a modding session verifies them:

1. **§4/§8 addition — the equipped weapon is one component read:** player's
   `app.ropeway.survivor.Equipment` component → `<EquipWeapon>k__BackingField`; weapon kind via
   type check against `implement.Gun` / `implement.Melee`; gun muzzle = weapon joint
   `vfx_muzzle1`. Also `SurvivorCondition.get_IsReload` as the reload-state flag alongside our
   known `get_IsHold`.
2. **New §8 fact — dormant enum selectors:** RE2 ships both fire-origin paths;
   `app.ropeway.weapon.shell.ShellDefine.FireBulletType` (`Camera` vs `AlongMuzzle`) is flipped
   by REFramework to fire from the muzzle in VR. Same "narrow selector the engine already
   honors" shape as TargetBankType.
3. **§8 addition — playing a custom animation without the FSM stomping it:** the public CAF
   project registers runtime `via.motion.DynamicMotionBank`s for its own motlists and pauses
   AND disables `via.motion.MotionFsm2` for the clip's duration; manual root motion needs
   `transform:set_Position` **plus `CharacterController:warp()`** or physics snaps it back.

No dossier dead end is directly answered; these extend §8's animation/selector picture and give
§11 a new "check for a dormant enum before building a mechanism" habit.
