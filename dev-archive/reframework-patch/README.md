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
