# 2026-10-24 · /pd: the build saved, why the menu text is squished, EMV poses into our hands, bullets in the magazine

/pd, home PC, Opus. **The game was not launched; nothing here has been run.**

## 1. The game folder belongs to a build again

Session start said the RE2 folder matched no saved build (26 files off b174). Checked file by file: 22 of them are
praydog's RE Engine MCP connector (REFramework.NET and its WebAPI, 30 MB, put in by Lanes setup on 2026-10-23 21:40),
and the other 4 are ours: b183 to b185 were installed on 2026-10-19 and never snapped (dinput8 `b55e4770` = board
b185; `visceral_native.dll`, `visceral_bridge.lua`, `visceral_tuning.lua` of 10:17 = b183/b184) `[measured 2026-10-24]`.
- The connector is a test tool, not the mod: its 22 file names are now `skip =` lines in the builds `PROJECT.conf`.
- Saved as **`Menu body/v0.2.0-b175 - board b185 ...`** (201 files, pushed to the builds repo). `builds.py which` now
  names it. Nothing was changed in the game folder.

## 2. Why the REFramework menu text is squished in VR `[inferred-static 2026-10-24, ref-76b source]`

- `ImGui_ImplWin32_NewFrame` sets ImGui's `DisplaySize` from the game WINDOW's client size every frame
  (`dependencies/imgui/backends/imgui_impl_win32.cpp` ~399), so the menu is laid out for a 16:9 window.
- With OpenXR (Virtual Desktop) REFramework draws that menu into the BACK BUFFER (`REFramework.cpp` ~1100), which in
  VR is the eye image, 2688x2880 (aspect 0.93). The draw data is stretched to the whole target, so a 1280x720 layout
  lands 2.1x wider and 4x taller: letters about half as wide as they should be. That is the squish.
- The separate `RTV::IMGUI` target (same size) is only read by the OpenVR overlay (`OverlayComponent.cpp`), not on
  OpenXR.
- Why the 1008x1080 window of 2026-10-11 gave a thin strip instead of a fix is not explained by this
  `[hypothesis: the game resized its swap chain to the window]`.

The fix, in our REFramework patch series (not built): in `REFramework::run_imgui_frame`, right after
`ImGui_ImplWin32_NewFrame()`, while the headset is live, set `io.DisplaySize` to the back buffer's aspect at the
window's height (`h_win * rt_w / rt_h`, `h_win`), and scale the mouse X by the same factor before `ImGui::NewFrame`
(mouse positions arrive in window pixels). Without the mouse scaling the sliders would answer at the wrong place,
which is why it was not built blind. Low priority (Tefa).

## 3. EMV Engine poses go straight into our hand poses

- EMV's Poser (flat only, 2026-10-11) saves `reframework/data/EMV_Engine/Poses/<object>.json`:
  `{ "<slot>": { "<joint>": { "_LocalEulerAngle": "vec:x y z" } } }` (`EMV Engine/init.lua` ~9335) `[inferred-static 2026-10-24]`.
  That is the joint property `handpose.cpp` already writes (LocalEulerAngle, radians), so no maths is needed.
- New `dev-archive/native/tools/emv_pose_to_handpose.py`: slots named `mag wp0000 leon` / `slide wp0000 leon` (the
  game's weapon numbers, as in `handpose_data.h`) replace or add rows in `src/handpose_data.h`; anything else is
  skipped with the reason. Dry run by default, `--write` to change the file, then rebuild.
  Checked against the real 73-row file: same values round-trip unchanged, one changed joint lands in the right row and
  nothing else moves, a new gun is appended, bad names and missing joints are skipped, a short row is refused
  `[verified-numerically 2026-10-24, 7 checks]`.
- No EMV pose is saved on this PC yet (`data/EMV_Engine/` holds only its caches).
- Next (Tefa at the keyboard, flat): open the player with our "pose Leon's hands (EMV)" button, pose the left hand on
  the Matilda's magazine, Save Pose as `mag wp0000 leon`, then run the tool.

## 4. Bullets in the magazines: where they would come from `[inferred-static 2026-10-24]`

Pulled with `pak_pull.py` (local only, `extracted (game data - never commit)/bullets-2026-10-24/`):
- `wp0000/wp0000_vp70.mesh` (the Matilda, 432 KB): joints `root`, `_00` `_01` (slide) `_03` `_04` (magazine,
  `reload_data.h`), `_100` `_101`, `vfx_muzzle1-3`; materials `wp0000_VP70_Mat`, `wp0000_PowerUp_Mat` (the long-magazine
  upgrade lives in the same mesh) and `wp0100_VP70Custom_Mat`. No bullet material: the gun's own magazine is empty.
- `sm71_901_vp70longmagazine.mesh` (the long-magazine PICKUP): uses `sm70_100_HandGun_Bullet01A__AST_A_Mat`, so the
  game already has a Matilda magazine model WITH rounds showing. Its rounds are the cheapest source.
- `wp0001/wp0001_handguncartridge.mesh` (15 KB): a single handgun round, material `wp0001_HandgunCartridge_Mat`.
- Tools on this PC: RE Mesh Editor and RE Asset Library are installed in Blender; the project has exported its own
  `.mesh.2109108288` files before (bracelets, neck plug), so the round trip works.
- The job, together in Blender (the rule): import `wp0000_vp70.mesh` and the long-magazine pickup, copy its top rounds
  onto the magazine part, weight them to `_04`, export as a loose `natives/stm/sectionroot/character/weapon/wp0000/
  wp0000_vp70.mesh.2109108288`. It shows flat too (the magazine is rarely seen there). Then the other guns the same way.
