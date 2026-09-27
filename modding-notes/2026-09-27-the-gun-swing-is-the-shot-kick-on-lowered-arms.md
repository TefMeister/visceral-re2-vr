# The gun swing is the shot kick landing on lowered arms (2026-09-27, home PC, VR)

## How it was found

After a full delete and reinstall of RE2 removed an unexplained swing, the mod went back into a separate
test copy of the game (`D:/RE2 test copy`), one piece at a time, with Tefa firing two-handed pistol shots
after each step. The test copy also moved to OpenXR only (Tefa: OpenXR for every RE game).

| Build | In the copy | Tefa saw |
| --- | --- | --- |
| b012 | the 4 spliced walk lists (as installed since 09-24) | gun thrown right on the first shot |
| b013 | nothing of ours | about 10 shots, gun straight |
| b014 | only `base_hdg_hold` (pl00 + pl10) | thrown right on the first shot |
| b015 | `base_hdg_hold` rebuilt with only the 12 walk slots; gun raise and standing aim vanilla | standing: fine. Walking: thrown right |

All `[reported 2026-09-27, Tefa]`, one run each.

So the cause is our spliced `base_hdg_hold`, and both the standing slots (lowered-gun `OFF_Gazing_Idle`)
and the walk slots (`OFF_GazingWalk`) carry it.

## What the files show `[measured 2026-09-27]`

- The installed file replaced 20 of 30 slots, not 12: the 12 aim walks, the 6 `HG_Hold_Start` gun-raise
  slots (with a cut-down `OFF_Gazing_Idle`) and both `HG_Hold_Idle_Loop` slots.
- The left-arm IK track is **the same** in vanilla and walk motions (`IKBlendRatio` 1.0 at both ends), so
  the 09-24 "IK starves" reading is not about that track's value. The walks add a
  `PlayerGazingSwitchLimitedTrack` (a `Limited` flag pulsed on around frames 29-42 and 62-70).
- The gun attach bones (`r_weapon`, `l_weapon`) hold the **same fixed offset** in every aim and walk motion.
- The shot motions `1100-1102` have **all-zero** weapon offsets while every full pose has (±0.080, -0.029, 0):
  they are additive layers. `[inferred-static]`

## Reading `[hypothesis]`

The shot kick is an additive layer authored over the arms-raised aim pose. On the lowered `OFF_Gazing`
arms the same local rotations push the arm a different way, which throws the gun. It fits every row above:
vanilla standing aim is fine, the lowered-gun standing pose and the lowered-gun walks swing.

## b016: legs from the walk, arms from the aim loop

New tool `dev-archive/tools/re-engine/motlist_graft_arms.py`: in each of the 6 walk loops, the 44
arm/hand/weapon bone tracks (clavicle to fingertips, both weapon bones) are swapped for the vanilla
`HG_Interpolation` loop's, moved whole with their offsets relocated (9812 track offsets checked in bounds).
Legs, hips, spine and head stay from the relaxed walk. It is the first time we edit inside a motion rather
than move one whole, so whether the game accepts it is part of the test.

Bone names were recovered by hashing guesses (murmur3 of the UTF-16 name, seed 0xFFFFFFFF; `root` =
`0xaba7de3c`); 61 of the 64 main-skeleton bones resolve with names like `r_arm_wrist`, `l_hand_index_0`.
