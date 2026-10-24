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
  Build: `git worktree add D:
ef-76b 76298bd` (no space in the path), apply `build-local-no-csharp.patch` (+ drop the
  CSharp lines from CMakeLists.txt), apply this patch, `D:
ef-76build-76b.bat` (target RE2) ->
  `build2/bin/RE2/dinput8.dll`. Note: `modding-notes/2026-10-09-grenade-flip-cause-and-main-menu-scene.md`.

- `2026-10-10-re2-left-grip-dock-any-state.patch` — against `76298bd`, CONTAINS the 10-09 no-dock patch (same file,
  cumulative diff; apply this one alone). The grip socket (left wrist relative to the right, from the aim animation) is
  read each frame from the weapon's own aid joint (`Implement.get_AidJoint()`, the game's support-hand anchor the stock hold pins the left wrist
  onto; b119's learned-from-animation version was wrong, our clips hang the hand at the hip); the left hand docks in any state, only while the left grip is held (15 cm at the press, sticky until
  release), the gun turning forestock-into-hand at the press; aiming uses the learned socket too, so the kick no longer
  swings the gun (replaces the 10-02 freeze). Reloads follow the animation. `[Visceral]` lines in the REFramework log.

- `2026-10-10-re2-sub-weapon-buttons-from-plugin.patch` — against `76298bd`, CONTAINS the 10-10 left-grip-dock patch
  (cumulative diff of `src/mods/`; apply this one alone). `VR.cpp openvr_input_to_re2_re3`: HOLD, SUPPORT_HOLD and ATTACK
  are taken from `visceral_native.dll`'s exported `visceral_vr_buttons()` when the plugin is loaded (the knife/grenade
  on the right grip at a back spot, the right grip never aims, the throw waits for a swing; `src/subweapon.cpp`).
  Without the plugin the stock mapping stays. Build: `D:\ref-76b\build-76b.bat` -> `build2\bin\RE2\dinput8.dll`.
  Why here and not in the plugin: writes into the button record at UpdateBehavior pre never reached these three
  (b134, worn 2026-10-10).

- `2026-10-13-re2-hold-hands-still.patch` — against `76298bd`, CONTAINS the 10-10 sub-weapon-buttons patch (cumulative
  diff of `src/mods/`; apply this one alone). `VR.cpp update_hmd_state`, right after the runtime's pose update: while
  `visceral_native.dll`'s exported `visceral_hands_frozen()` returns 1, both controllers keep the pose they had when it
  switched on (OpenVR `render_poses` and OpenXR `hands[].location`), their speeds are zeroed, and the motion-controls
  inactivity timer is kept awake (still controllers would otherwise hand the arms back to the animation after 30 s).
  The headset keeps tracking. For the hand tuning sliders (b169): tick "Hold my hands still" in the "Visceral: hand
  tuning" menu, hold the controllers where the hands should be, 10 s later a strong buzz, put them down. Log:
  `[Visceral] hands HELD STILL` / `follow the controllers again`. Build: as the 10-10 patches (`D:\ref-76b\build-76b.bat`).

- `2026-10-14-re2-one-grip-per-gun-no-steer-on-the-slide.patch` — against `76298bd`, CONTAINS the 10-13 hold-hands-still
  patch (cumulative diff of `src/mods/`; apply this one alone). The plugin's dock socket (`visceral_dock_socket`) is read
  as version 2: `owned` = 1 (a pump gun) means the VR layer never docks the left hand on the gun's aid joint, so there
  is ONE grip spot, the plugin's (Tefa 2026-10-14: the shotgun grip "ending up in 2 places"); `steer` = 0 (a pistol slide
  being racked) means the VR layer skips its turn-the-gun-to-meet-the-left-hand step, so the right hand alone aims
  while the left works the slide. Built on the home PC: `D:\ref-76b\build-76b.bat` (the tree was first reset to the
  10-13 state with `git checkout -- src/mods` + that patch). Note: `modding-notes/2026-10-14-b176-one-grip-per-gun-and-right-hand-aims-on-the-slide.md`.

- `2026-10-19-re2-inventory-close-no-cutscene-path.patch` — against `76298bd`, cumulative (everything installed so far
  in `src/mods/FirstPerson.cpp` / `.hpp` / `VR.cpp`; apply this one alone). New: `update_camera_transform` treats the
  inventory's camera (`CameraControlType::INVENTORY`), still busy for a frame or two after the inventory closes, as the
  player's camera once the menu is closed. Before, those frames took the cutscene path: the last rendered view (head
  turn included) became the controller rotation and the head turn was added again on the first frame back, the
  world jumping sideways by the head's turn (Tefa 2026-10-17: "teleports to the left for a frame or 2"). Opus,
  `[inferred-static 2026-10-19]` from the source; built `dinput8.dll` 78d65384, installed, unworn. Previous build kept:
  `D:/Visceral build versions/dinput8-b3ea3abd-before-inventory-close-fix.dll`.

- `2026-10-19b-re2-switch-start-keeps-the-head-turn.patch` — against `76298bd`, cumulative (contains the 10-19
  inventory-close patch; apply this one alone). New: a switch, ladder or cupboard push starts with a frame or two of
  `SwitchingCamera` on the player camera; those frames are not "in control", so `update_camera_transform` measured the
  headset's turn against the stored pre-cutscene headset rotation (stale) and the view snapped to that old head
  direction (Tefa: "grabs the camera and points it at the centered hmd direction for just a frame or 2"). Now only a
  return FROM a cutscene (`m_has_cutscene_rotation`) uses that compensation; a switch from normal play keeps the real
  headset turn. Opus, `[inferred-static 2026-10-19]`; dinput8 b55e4770, installed, unworn.


- `2026-10-24-re2-menu-text-not-squished.patch` — against `76298bd`, cumulative (contains the 10-19b switch-start
  patch; also touches `src/REFramework.cpp`, so apply it to `src/mods` AND `src/REFramework.cpp`). New: on OpenXR
  the REFramework menu is never put in the headset; it is drawn onto the eye-size back buffer (2688x2880), which the
  window shows shrunk into its 16:9 client area, so the menu came out flattened (Tefa 2026-10-11: "squished").
  `run_imgui_frame` now sets `io.DisplayFramebufferScale` = back buffer size / window client size while the headset
  is on (OpenXR only), so the menu covers the whole back buffer and the window's shrink undoes it; the mouse is
  already in window pixels and lines up unchanged. VR options tick box "Menu Text Not Squished (window)", on by
  default (config `VR_VisceralMenuFitWindow`); off brings back the old drawing, for REFramework's on-screen markers
  (object boxes, Lua `draw.*`), which are placed in back buffer pixels and land off target while it is on. Log:
  `[Visceral] menu drawn at scale X x Y (window WxH, back buffer WxH)`. Opus, `[compile-verified 2026-10-24]`;
  dinput8 293aef6e, installed, unrun. Previous: `D:/Visceral build versions/dinput8-b55e4770-before-menu-fit.dll`.
