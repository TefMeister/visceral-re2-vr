# 2026-11-16: RE2 cutscenes on a flat screen in the headset, the plan (Developer PC, Opus, static only)

Tefa: *"can you see if you can start the process of getting re2 cutscenes to show in 2d, like you somehow worked your
magic on sh2?"* (chosen idea `ideas/chosen/re2-visceral--fix-the-broken-cutscenes-maybe-by-showing-them-on-a-flat-screen.md`,
the Claire-after-the-crash cutscene named as broken). Nothing run; read from the sources `[inferred-static 2026-11-16]`.

## Why SH2 was quick and RE2 is not
- SH2 runs on UEVR, which HAS a flat-screen mode (`VR_2DScreenMode`, plus `UI_FollowView`, `UI_Size`, `UI_Distance`);
  `sh2vr` only switches it on while a cutscene plays (`silent-hill-2-remake-vr/dev-archive/native/src/cutscreen.cpp`,
  written from a thread of its own: setting it from the game thread killed the game).
- REFramework has NO such mode: not in the March build we patch (`D:/ref-76b`, 76298bd), not in praydog's master today
  (its VR options: DecoupledPitch, AlternateFrameRendering, 2DUIScale / 2DUIDistance (the game's UI drawn INTO the eyes),
  WorldSpaceUIScale, AllowEngineOverlays, AsyncRendering ..., read 2026-11-16 from `src/mods/VR.hpp` on GitHub).
- So it has to be built, in our patched REFramework (`dev-archive/reframework-patch/`, the cumulative patch series),
  and the same patch can serve RE3 (`D:/ref-76b-re3`) and Village (its Wednesday Fable row "2D cutscenes").

## What it takes (three parts)
1. **Know a cutscene is on.** Already there: `FirstPerson.cpp` switches itself off in cutscenes ("Show In Cutscenes",
   `m_has_cutscene_rotation`, the not-the-player-camera path ~line 1683), and our `visceral_cinematic_gate.lua` gates the
   body scripts on the same thing. Pick the one that is true for real cutscenes only (not ladders, doors, the
   inventory: the 2026-10-19 patch "inventory close: no cutscene path" shows those can look alike).
2. **The game's own camera, one picture.** In a cutscene VR.cpp still lays the headset's turn and the two eyes'
   offsets onto the game camera. For the screen: stop the head's turn and position, the same picture for both eyes, and
   ideally the game's own flat field of view (the eye's wide, lopsided headset view would look stretched on a screen).
3. **Show that picture on a screen, not around the head.** `vr/runtimes/OpenXR.cpp` `end_frame()` sends ONE projection
   layer (both eyes). In a cutscene send instead one `XrCompositionLayerQuad` with the left eye's swapchain as its
   picture, placed in the stage space in front of where the head looked when the cutscene began (not following the
   head, as SH2's `UI_FollowView` off), a few metres away, 16:9 cut from the square eye image, black around it.
   OpenVR (SteamVR) users would need the same in the OpenVR path (an overlay); Tefa's Virtual Desktop runs OpenXR.

## Order of work
1. (Opus, no game) read VR.cpp's camera path in a cutscene and OpenXR.cpp's swapchain sizes; write the patch with a
   switch (`[Visceral] cutscene screen` in the REFramework menu), off by default for the first build.
2. (Fable) the picture's field of view and how the eye's square image maps onto a 16:9 quad; the camera hand-back at
   the cutscene's end (FirstPerson's existing "no jump after a cutscene" code must still run).
3. (flat, then simulator) the Claire cutscene from a save before it; the simulator's two-eye pictures show the quad.
4. (Tefa, headset) comfort and size; then RE3 and Village get the same patch.

## Step 1 done (same day, 13:30): built, installed, switched on, NOT run `[compile-verified 2026-11-16]`
Patch `dev-archive/reframework-patch/2026-11-16-re2-cutscene-screen.patch` (README there says what each part does).
Builds b211 (switch off) and b212 (switch on in `re2_fw_config.txt`, the installed state). The trigger is the game's
own EVENT camera type, so ladders, doors, grabs and the inventory should stay in VR `[inferred-static]`; the log lines
`[Visceral] cutscene screen: the game's camera type A -> B` say which types the first test really meets.
What the first wear tells us, from the REFramework log:
- `cutscene screen ON` / `OFF` at the cutscene's start and end, and nothing in between (no flicker);
- `camera FOV x (the game's own | the fallback slider's)`: whether the game writes its own field of view in cutscenes;
- `cutscene screen placed ...`: the quad's place.
What to watch for in the headset: a screen at all (if black: the quad's picture or the swapchain chosen); squashed or
stretched people (the field of view / aspect, step 2); half-rate picture (only the left eye's image, AFR); the return
to VR at the end (FirstPerson's "no jump after a cutscene").
