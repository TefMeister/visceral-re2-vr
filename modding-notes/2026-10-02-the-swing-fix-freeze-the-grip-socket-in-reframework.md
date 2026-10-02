# The swing fix: freeze the two-handed grip socket in REFramework's RE2 code (2026-10-02 night, home PC, `/pd`)

**The game was not launched; nothing here has been run.** Static reading of praydog's source plus a build.

## Where RE2's two-handed aim really lives

Not in `re8_vr.lua` / `RE8VR.cpp` — that script returns on anything but RE7/RE8 (`if not is_re7 and not is_re8 then
return end`, line 6) and the evening's patch to it never loaded `[verified-live 2026-10-02]`. RE2's VR hands are in
**`src/mods/FirstPerson.cpp`** (the FirstPerson mod does the motion controls for RE2/RE3), `update_joints` region, read at
`a24c3459` (the build in the test copy) `[measured 2026-10-02, source]`:

- Every frame it asks the animation for the left wrist relative to the right wrist (`via.motion.Motion.getWorldPosition /
  getWorldRotation` of `l_arm_wrist` and `r_arm_wrist`) → `original_left_pos_relative` / `original_left_rot_relative`:
  **the grip socket**.
- The controllers give `rh_pos/rh_rotation` and `lh_pos/lh_rotation`; `lh_grip_position = rh_pos + rh_rotation *
  original_left_pos_relative`.
- While aiming, not reloading, and (the left hand within 10 cm of the socket, or already gripping with the left grip held):
  the **"pistol fix"** rotates the right hand (= the gun) by the delta between the right-hand→socket line and the
  right-hand→real-left-hand line, then pins the drawn left hand to the socket.

So when the shot's kick moves the animated left wrist ~7 cm off the right for ~0.35 s (measured, 2026-10-02 trace), the
socket moves with it, and the "pistol fix" turns the gun to make the moved socket line up with the real left hand: the
swing. One-handed there is no pistol fix, hence no swing. Exactly the Village rifle jerk (RE8VR has a copy of this code;
its fix was the socket freeze, worn 2026-09-21).

## The patch (`dev-archive/reframework-patch/2026-10-02-re2-grip-socket-freeze.patch`, 24 lines)

After the socket is read: if the grip was held last frame, use the socket captured when it was taken; otherwise keep
capturing. It thaws whenever the grip is released (not aiming, reloading, left grip let go), so reload animations and the
raise still move the hand. Steering by the real hands is untouched — only animation-driven socket motion is removed.
`m_freeze_grip_socket` (default true) in `FirstPerson.hpp` switches it.

Built from a worktree of praydog's `pd-upscaler` at `a24c3459` (the same commit as the smooth September build, NOT the
March `76298bd` tree that shook) with `build-local-no-csharp.patch`, VS 2022 x64 Release, target `RE2`.
Build result and deployment: see the end of this note. (The evening's kick-strip list was installed without a snapshot and reverted; **b045 is this build**.)

## What is NOT established

- That the freeze is the whole fix: the trace showed the left WRIST joint leaving the gun, which this code explains
  (the drawn left hand is pinned to the moved socket), but whether anything else kicks the gun off the right controller
  is a headset question `[hypothesis until worn]`.
- Whether a frozen socket feels wrong anywhere else (weapon changes while gripping, the knife, the reload exit). The
  thaw on release should cover those; one evening's play will say.

## Test, VR USER

Test copy with the patched `dinput8.dll` (b045): walk, shoot two-handed, stop — the shot should kick (the right hand
and gun move ~4 cm and come back) with no swing; the left hand stays on the gun. Then a reload and a weapon change while
gripping, to see the thaw. Revert = the previous `dinput8.dll` from `D:/RE2 REFramework builds/deploy-backups/`.

## Built and installed `[compile-verified 2026-10-02 23:30]`

`dinput8.dll` 23,279,104 bytes, sha256 `fab88f715462…`, from the worktree `D:/RE2 REFramework builds/tools/REFramework-a24c3459`
(junction `D:/ref-a24`, build dir `build2`), VS 2022 x64 Release, target `REFramework` (the all-games build; the per-game
`RE2` target of the March tree no longer exists), **0 errors, no warnings in `FirstPerson.cpp`**. Installed in the test copy as
b045 with the stock a24c3459 DLL (`e327a9c2f6b0…`) kept in `D:/RE2 REFramework builds/deploy-backups/`; a copy of the build in
`D:/RE2 REFramework builds/dinput8_pd-upscaler_a24c3459_grip-socket-freeze_2026-10-02.dll`.

Build traps, for next time (all three cost a rebuild): the tree needs `build-local-no-csharp.patch`; DirectXTK's
`CompileShaders.cmd` fails under CMake 4.4 (`-E env … CompileShaders.cmd` → "no such file or directory") and also with a
space in the path, so compile the shaders by hand first (`shaders-only.bat` in the worktree: `CompileShadersOutput` =
`<build>/_deps/directxtk-build/Shaders/Compiled`, plain for DXTK and `dxil` for DXTK12, called by FULL path) and build from
the junction; MSBuild needs the VS environment (`vcvars64.bat`).

## Worn `[verified-live 2026-10-02 ~23:45, Tefa, n=1 round]`

Tefa, in the headset with b045: *"you've done it!"* — the swing is gone. Ten days of bisects (b001–b045) close on a 24-line
patch to REFramework. What ships: the patched `dinput8.dll` (a modified file, allowed in releases since 2026-09-27) with the
patch file beside it so anyone can rebuild; worth offering upstream to praydog as a PR (`FirstPerson.cpp`, freeze the grip
socket while gripping — the Village `RE8VR.cpp` has the same bug).
