# REFramework patches for Visceral (RE2)

- `2026-10-02-re2-grip-socket-freeze.patch` — against praydog's `pd-upscaler` branch at `a24c3459` (the build the
  test copy runs). `src/mods/FirstPerson.cpp` re-reads the two-handed grip socket (left wrist relative to the right,
  from the animation) every frame; the shot's kick moves it and the "pistol fix" rotates the gun after it. The patch
  keeps the socket taken at the grip while the grip is held. Also carries `build-local-no-csharp.patch` (cmake.toml
  without the C# language) so the tree builds locally. Same cause and fix as the Village rifle
  (`re-village-scope-vr/dev-archive/reframework-patch/grip-no-throw.patch`).

Build: `git worktree add <dir> a24c3459`, apply the patch, `git submodule update --init --recursive`,
`cmake .. -G "Visual Studio 17 2022" -A x64 -DDEVELOPER_MODE=ON` in `build/`, then
`cmake --build . --config Release --target RE2`; the result is `build/bin/RE2/dinput8.dll`.

- `2026-10-09-re2-no-dock-on-sub-weapon.patch` — against praydog's `pd-upscaler` branch at `76298bd` (the installed
  DLSS-loader build, `dinput8.dll` 06f626da30fb). `FirstPerson.cpp update_player_arm_ik` docked the left hand on a
  knife or grenade as if it were a gun, and the VR layer drops SUPPORT_HOLD while docked: the grenade went in and out
  in a loop. The patch skips the dock while the equipped weapon is an `implement.Melee` or a grenade-throwing Gun.
  Build: `git worktree add D:ef-76b 76298bd` (no space in the path), apply `build-local-no-csharp.patch` (+ drop the
  CSharp lines from CMakeLists.txt), apply this patch, `D:ef-76build-76b.bat` (target RE2) ->
  `build2/bin/RE2/dinput8.dll`. Note: `modding-notes/2026-10-09-grenade-flip-cause-and-main-menu-scene.md`.
