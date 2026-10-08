# Long guns sit off to the right in the light: the grip socket on relaxed clips (2026-10-08, Fable, home PC, static)

**Build:** `Long gun grip/v0.2.0-b101 - shotgun with the game's own two-handed arms on the relaxed clips`. Installed,
unworn. Game not launched; nothing here has been run.

## Tefa's report (worn b097-b098, 2026-10-08)

*"shots go straight and the gun throws right after that. but then i found out that in the light the gun is sitting off
to the right in my hand (long gun) at the spot where the was throwing to after shooting in the dark. so with the
flashlight out, a long gun is in my hand the way it is supposed to be, holding straight. in the light, a long weapon
sits off to the right and looks off. we need to get the shotgun facing the same way in the light as it does in the
dark."*

## What the files say `[measured 2026-10-08, pak + installed lists]`

- The shotgun has NO flashlight hold clips of its own: `stg_hold_stLIGHT_01` is an empty list (31 slots, 0 entries) and
  `stg_hold_stLIGHT_cpB_01` holds only the SG2 shoot clips. So "straight in the dark" is not a clip of the shotgun's.
- The gun hangs from `setProp_A_00` under the `r_weapon` palm joint under `r_arm_wrist` (dossier: AttachJoint). No
  shotgun clip tracks `setProp_A_00`; every clip tracks `r_weapon`. The relaxed KFF_ clips track `r_weapon` too, as an
  empty hand. FirstPerson's IK drives the wrist; `r_weapon` below it is the clip's.

## The reading `[hypothesis]` -- the two-handed grip, not the clip pose

`modding-notes/2026-10-02-the-swing-fix-freeze-the-grip-socket-in-reframework.md` already found the mechanism:
FirstPerson's "pistol fix" turns the right hand (= the gun) so the ANIMATED left-wrist-relative-to-right-wrist line (the
grip socket, read from the playing clip every frame) lines up with the REAL left hand, whenever aiming with the left
hand on the gun (within 10 cm of the socket, or gripping with the left grip held). On a relaxed clip the socket is two
hanging arms, not a forestock: the gun is turned by the angle between "hanging left arm" and "hand on the forestock" --
off to the right. With the flashlight in the left hand (the dark) there is no two-handed grip, no pistol fix, and the
gun follows the right controller exactly: straight. After a shot in the dark the additive shot kick (authored over
raised arms) throws the lowered arms and the gun settles aside (2026-09-27's finding), which Tefa saw as "throws right".

Handguns escape it because the relaxed hand geometry happens to be close to a pistol grip (both hands near each
other), and the socket freeze idea of 2026-10-02 was never carried into the clean rebuild.

## The fix (b101): the game's own arms on the relaxed clips

`dev-archive/tools/re-engine/motlist_graft_grip.py --bones arms`: every relaxed (KFF_) clip in Leon's `base_stg_hold`
(10 entries: walks, idle, raise) and `base_stg_move` (42 entries: jogs, stairs, pivots) gets the 44 arm/hand/palm joint
tracks of the stock `pl00_0160_SG_Hold_Idle_Loop` (432 frames), whole, by bone hash, the way `motlist_graft_arms.py`
did for b016. Legs, hips, spine and head stay relaxed. In VR the arms are IK'd to the controllers, so the body should
look as before; what changes is the socket the pistol fix reads (a real two-handed shotgun grip) and `r_weapon` (a real
shotgun hand). Verified by re-parsing: 880 + 2024 bone headers point at the source's tracks, names and slots unchanged.

Caveat: the source idle is 432 frames; the 1100-frame idle slot and nothing else is longer, so the arm tracks' last key
has to hold there `[hypothesis]` (a static grip pose, so invisible if it does).

## Test, VR USER

Shotgun in the light, both hands on it, left grip held: does the gun now sit straight between the hands? Then one-handed
(left hand away): still straight? Then the dark with the flashlight: unchanged? Then a shot two-handed: the kick should
come back to the hands. If the body's arms look raised/stiff instead of relaxed, the IK is not covering the clip's arms:
say so.

Roll back: `builds.py restore visceral-re2-vr 100 --yes`. Other long guns (smg, gnl, rkl, etc, mag) get the same once
the shotgun says yes; Claire's (`pl10`) lists untouched.

## Worn (2026-10-08 23:50): no change -- the reading above is wrong for the one-handed case

Tefa: *"shotgun is still in my hands the same way one handed, left hand doesn't go on the gun yet we still have to figure
this part out"* `[verified-live 2026-10-08]`. One-handed there is no pistol fix, and the grafted `r_weapon` changed nothing
either, so the off-right angle is not in the body clips. Next lead `[hypothesis]`: the weapon's OWN animation state (RE2
carries a lowered long gun at an angle and turns it into the shooting grip only in Hold; the no-aim detour never enters
Hold). Probe next: the weapon GameObject's motion layer names and its rotation relative to `r_arm_wrist`, light vs dark,
RG held vs not. The b101 arms stay in unless the body looks wrong.
