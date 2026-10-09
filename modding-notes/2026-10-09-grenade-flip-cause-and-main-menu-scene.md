# 2026-10-09 night (home PC, Fable): the grenade flip at its cause, and the main-menu scene that was only missing

## The grenade flip (board 6d)

What Tefa saw: holding LG with a grenade or knife, looking at it from a certain distance, the sub weapon goes away and
comes back in a loop (video VirtualDesktop.Android-20261009-162726-0.mp4, 30:52-30:58).

Measured with the b113 probe `[verified-live 2026-10-09, n=10 cycles]`: with LG held, the game's SUPPORT_HOLD button bit
reads 0 for exactly ONE frame every 1.5-2.5 s, IsHold drops in that frame, and the bit is Down again the frame after.

Why, read from REFramework's source (`VR.cpp openvr_input_to_re2_re3`, `FirstPerson.cpp update_player_arm_ik`)
`[inferred-static 2026-10-09]`:
- the VR layer sends SUPPORT_HOLD = left grip AND NOT FirstPerson's `was_gripping_weapon` (the two-handed dock);
- the dock switches on whenever the game is in its hold state (it is, with a grenade readied), not reloading, and the
  real left hand is within 0.10 m of where the playing clip puts the left wrist relative to the right wrist;
- so with the sub weapon out the dock can flick on: SUPPORT_HOLD drops, the grenade is put away, the hold ends, the
  dock lets go, SUPPORT_HOLD returns, the grenade comes out again.

What did NOT fix it (all worn by Tefa the same evening, `[verified-live 2026-10-09]`):
- b109: `InputSystem.setForce(SUPPORT_HOLD, true)` while LG is held first -- the weapon no longer SWAPPED in the log, but
  the grenade still vanished (the one-frame gap in the bit stayed);
- b114: the bit written back at UpdateBehavior pre -- one drop in 52 s instead of every 2 s, but not zero (the game
  reads the bit before our write, so the fill is a frame late);
- b115: + the game's forbid-aim answered no -- forbid never fired; the grenade still dropped once; and AFTER a drop
  THE MENUS STOPPED WORKING (Tefa closed the game with the window X). b116 switched the forcing off and the menus worked
  again, so the forcing (as b078/b080 before it) is what kills them. Tefa: "instead of a guard, we need to fix the cause".

The cause fix: `dev-archive/reframework-patch/2026-10-09-re2-no-dock-on-sub-weapon.patch` against praydog's
pd-upscaler branch at 76298bd (the installed DLSS-loader build, `dinput8.dll` 06f626da30fb). In
`update_player_arm_ik`, the dock is skipped while the equipped weapon is an `implement.Melee` (knives) or a Gun whose
ShellGenerator is a `ThrowGrenadeGenerator` (grenades) -- the same classification FirstPerson already uses for the
muzzle. Guns keep the two-handed grip unchanged; a sub weapon is held in the left hand by the game itself, so the left
hand follows its controller. Built from a clean worktree `D:\ref-76b` (`build-76b.bat`, target RE2) `[compile-verified
2026-10-09]`; installed as b118 with the stock DLSS files untouched.

Not re-applied: the 2026-10-02 grip-socket freeze patch (the swing). It was in the test copy only; the Steam game has
run the stock 76298bd build since 2026-10-04 and the swing has not been reported since. One change per build.

Still installed: `src/subprobe.cpp` (b113's probe, read-only). Take it out once b118 is worn clean.

## The main-menu scene (new row)

Tefa 22:45: "the police station is not seen in the main menu, it is visible rain now, but not the picture from the last
save file". This was solved on 2026-09-27 (`visceral_title_one_scene.lua`, round 11, Tefa: "the switch is now
seamless"; `modding-notes/2026-09-27-one-title-background-nine-rounds.md`). The 2026-10-04 clean reinstall took only
the rain script back (b058: "No ... other menu extras"), so the scene script was missing, not broken. Reinstalled as
b117 (commit 4b9b559 of the script). The rain script was written to sit beside it.

## 2026-10-10 00:25 -- b119: the left hand docks in any state (Fable)

Tefa: "grabbing a gun with the left hand doesn't work yet ... put left hand on the gun so it docks and doesn't move the
pose, and finally be able to run while holding on to the weapon." Grenade pinned behind it.

Why it never could: FirstPerson's dock needed `IsHold` (the aim state) because its grip socket came from the playing
animation every frame, and only the aim clips put both hands on the gun; the relaxed clips hang the left hand at the
side, so that "socket" is nowhere near the gun. Patch `2026-10-10-re2-left-grip-dock-any-state.patch` (cumulative,
contains the 10-09 change): the animation's socket is remembered per weapon type while aiming, once still for 15 frames
(the kick moves it for ~0.35 s after a shot), saved to `reframework/data/visceral_grip_sockets.txt`, and used in every
state. Dock rule = Tefa's 2026-09-22 rule: only while LG is held, within 0.15 m at the press, sticky until release, the
gun turned so the socket meets the left hand (praydog's "pistol fix"). No input is touched, so the body pose is whatever
the game plays; running keeps the dock (both wrists follow the controllers in every PLAYER-camera state). The learned
socket is also used while aiming, which is the 10-02 swing fix by another route. Reloads follow the animation's own
left-hand position, as before. A gun never aimed has no socket yet (aim it once with RG).
`[compile-verified 2026-10-10]`, unworn. Log lines: `[Visceral] grip sockets loaded`, `grip socket learned`,
`left hand docked`, `left hand let go`.

Also in b119: `visceral_title_rain.lua` logs the reason the launch-time rain request is refused (10-03 note, step 1).
