# Status flip: the equipped-weapon-surface lead is 👀 reviewed

**From:** modding session, 2026-08-31 (dev PC)
**For:** `/gr visceral-re2-vr` — please flip the `INDEX.md` rows for
`topics/2026-08-29-weapon-equipped-state-surface.md` and
`topics/2026-08-29-caf-custom-animation-framework.md` to 👀 **reviewed**.

## What was done with it

Drained from `engine-research/inbox/` into `ENGINE-DOSSIER.md` as a new **§8b**, kept clearly
separate from the verified §8 material and headed *"from public sources, NOT yet verified live"*.
Everything in it stays `[reported]`: the `Equipment` → `<EquipWeapon>k__BackingField` read, the
`vfx_muzzle1` muzzle joint, `SurvivorCondition.get_IsReload`, the `FireBulletType` enum, and the
CAF technique (`DynamicMotionBank` + pausing `MotionFsm2`, and `set_Position` **plus**
`CharacterController:warp()`).

**Reviewed, not incorporated:** it has changed our documentation and our plans, but nothing has
been tested against our own build, and testing needs the game running — which only the user
starts. When each item is checked, expect a further drop moving it to ✅ or ❌ individually.

## The part that earned its own dossier entry

Your `FireBulletType` (`Camera` vs `AlongMuzzle`) find is the **second** dormant enum selector in
this engine, after `TargetBankType`. That pattern is now written into **§11 as a standing habit**:
*before building a mechanism, spend ten minutes looking for the enum.* The 2026-08-29/30 animation
work burned a full day on bank poisoning, motlist swapping and bone correction before the answer
turned out to be a state flag — so this generalisation is the most valuable thing in the drop, and
it is credited to the research lane.

Directly relevant to the queued manual-reload work (`get_IsReload`, and CAF if a bespoke clip is
ever needed).
