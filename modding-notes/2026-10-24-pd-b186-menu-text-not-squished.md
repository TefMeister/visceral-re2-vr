# 2026-10-24 · /pd: b186, the REFramework menu text no longer squished on the window

/pd, home PC, Opus. **The game was not launched; nothing here has been run.**

## What was wrong `[inferred-static 2026-10-24, ref-76b source]`

- On OpenXR the REFramework menu never reaches the headset: `D3D12Component::on_frame` copies the back buffer into
  the eye images inside `m_mods->on_present()`, and `on_frame_d3d12` draws the menu onto the back buffer only AFTER
  that. The separate `RTV::IMGUI` target is read only by the OpenVR overlay. So Tefa reads the menu on the game
  window (through Virtual Desktop's desktop view), not in the eye picture.
- In VR the back buffer is the eye size: the 2026-10-19 log shows `ResizeBuffers ... width 2688 height 2880`
  `[measured 2026-10-24, one log]`. The window shows it shrunk into its 16:9 client area (x 0.71, y 0.38 for a
  1920x1080 window), i.e. flattened about 1.9x.
- ImGui lays the menu out for the window's client size (`DisplaySize`) and the DX12 renderer draws it 1:1
  (viewport = `DisplaySize * FramebufferScale`, scale 1) into the back buffer's top-left. The window's shrink then
  flattened it. That is the squish.
- ⚠️ This corrects §2 of `2026-10-24-pd-build-saved-menu-text-emv-poses-bullets.md` (same day), which said the menu
  is drawn into the eye picture and stretched; the renderer does not stretch it, and the eye picture never has it.

## The fix (b186, REFramework patch `2026-10-24-re2-menu-text-not-squished.patch`) `[compile-verified 2026-10-24]`

- `run_imgui_frame`: while the headset is on (OpenXR only), `io.DisplayFramebufferScale = back buffer / client`.
  The renderer then draws the menu over the WHOLE back buffer (viewport and scissor both honour the scale), and the
  window's shrink is the exact inverse: menu point (x, y) -> back buffer (x*sx, y*sy) -> window (x, y). The mouse is
  already in window pixels, so no mouse change (this morning's design needed one; this one does not).
- ImGui 1.92 also uses the scale's X as the font's rasteriser density: text is rasterised 1.4x finer, then shrunk.
- Tick box in REFramework's VR options: **"Menu Text Not Squished (window)"**, on by default.
- Known cost: REFramework's on-screen markers (Object Explorer boxes, chain colliders, Lua `draw.*`, EMV gizmos)
  are placed in back buffer pixels and will land off target while it is on. Untick to use them.
- Log line: `[Visceral] menu drawn at scale 1.400 x 2.667 (window 1920x1080, back buffer 2688x2880)` (numbers for
  that window).

## What is NOT established

- That the window's picture is the back buffer stretched to the client area (DXGI's default STRETCH scaling) is
  `[inferred-static]`. If the menu now looks stretched the other way, or cut off, the swap chain does not stretch,
  and the fix is wrong in its premise, not in a number. Untick the box to compare at once.
- Why the 1008x1080 window of 2026-10-11 gave "a thin strip" is still unexplained.

## To check (any headset launch, nothing else needed)

Open the REFramework menu with the headset on and look at the window: the text should look like it does flat, and
the mouse should hit what it points at. Untick the box: the old squish should come back.
