# 2026-10-28 — the game's automatic brightness kept off (home PC, `/lm`, Opus, flat)

Tefa: *"how easy or difficult is it to remove the adaptive lighting from re2?"*, then *"please see if you can just turn it
off and i'll check in the headset"*.

## Found live (RE Engine MCP, flat, RPD hall) `[verified-live 2026-10-28, n=1]`
- The main camera (`via.SceneManager` → `get_MainView` → `get_PrimaryCamera`, "Main Camera") carries a
  `via.render.ToneMapping`: `getAutoExposure` = Enable (0), `get_UseAutoExposure` true, EV 4.2, adapting toward bright
  slowly (`BrightAdaptationRate` 0.01) and toward dark fast (`DarkAdaptationRate` 0.7), between EV −8 and +16.
- The game's own `app.ropeway.posteffect.ToneMapController` (also on the main camera) blends its layers into
  `CurrentParam` (`AutoExposure = Enable (0)`) and copies it onto the component in its `lateUpdate`: a one-off
  `setAutoExposure` through the live link read back Enable at once.
- The enum: Enable = 0, **Disable = 2** (read back `UseAutoExposure` false), FixedEnable = 1 by elimination
  `[inferred]`.

## The switch
`dev-archive/native/src/exposure.cpp` (new): at `BeginRendering` pre (after the game's late update, before drawing) it
writes `setAutoExposure(2)` on the main camera's ToneMapping every frame. With it off, each place shows at the
brightness the game's lighting authored for it (its EV). NUM7 toggles it; the tuning menu (REFramework menu,
"Visceral: hand tuning") has a tick box **"Let the game adjust the brightness by itself"** to compare in the headset
(bridge slot 538, `S_AUTO_EXPOSURE_ON`; `visceral_bridge.lua` N = 539). On by default.

## Measured (build b180, flat) `[verified-live 2026-10-28, n=2 turns each way]`
Mean picture brightness sampled once a second for 8 s right after a sharp view turn:

| automatic brightness | turn 1 | turn 2 |
| --- | --- | --- |
| OFF (ours) | 25.4 → 25.6 (+0.2) | 42.2 → 41.9 (−0.3) |
| ON (the game's own) | 39.0 → 37.7 (−1.3) | 66.0 → 61.0 (−5.0, mostly in the first second) |

So with it off the picture stays put after a turn; with the game's own it drifts. The two sets of pictures ended at
different spots, so they say nothing about which is brighter overall.

## Not established
- How it looks and feels in the headset (Tefa). Some dark places may now stay darker than the game's own drift made
  them: if so, a brightness offset slider is the easy next step.
- Cutscenes and other cameras: the write follows whatever the primary camera is; not watched in a cutscene.
