# 2026-11-19: the pump shotgun's left hand goes UNDER the handle (Developer PC, Fable, Tefa away; RE2 b217)

Tefa, 2026-11-19: RE2 and RE3 "both also need the shotgun hand rotated 180 degrees, as at the moment left hand on the
shotgun docks above the pump handle, not under it".

## What was wrong
The clean rebuild (b195) gave the pump the same left-hand hold as a pistol's slide: `rack.cpp socket()` used
`SLIDE_DOCK_YPR` / `SLIDE_DOCK_OFF` for both. Those are Tefa's b164 numbers for a hand laid OVER a slide, so on the
fore-end the hand lay over the top `[inferred-static, then measured: the wrist read 2.6 cm ABOVE the joint by those numbers]`.
Village is right already: its pump hand takes the game's own two-handed hold (under the handle), so nothing to change there.

## What is built (shared gun code: RE3 gets it through `tools/sync_shared.py`)
- `settings_guns.h`: `PUMP_DOCK_YPR {173, 18, 0}` (the slide hold with its roll turned 180 -> 0: the palm faces up) and
  `PUMP_DOCK_OFF {0.078, -0.026, 0.035}` (the fore-end joint's axes: 7.8 cm left, 2.6 cm under, 3.5 cm forward).
- `rack.cpp`: a pump SHOTGUN (a pump with shells of its own, `shell_gun()`) uses them; a slide and the Spark Shot keep the
  slide hold. A tuning file, `reframework/data/visceral_pump_dock.txt` ("yaw pitch roll x y z", degrees and metres), replaces
  the numbers while it exists, re-read twice a second (delete it = the built-in numbers). Not installed; for tuning with Tefa.

## Simulator `[verified-live 2026-11-19, simulator, n=2 launches]`
- Claire's save, the W-870 given by script, right hand holding it level (RG held), left controller at the fore-end, LG held.
- Read by `tools/sim/q_pump_hold.lua` in the gun's axes: wrist 0.078 left, 0.026 UNDER, palm facing up 0.96, fingers pointing
  across under the handle to the right; the palm's middle 5 cm from the joint. Pictures from the side and from above agree:
  the hand grips the handle from below, fingers curled up the far side.
- One full pump by hand (LG + LT, 8 cm back and home): `rack: WP1000 pump done`. The gesture reads the controllers, so the
  hold change does not touch it.
- Final build (numbers built in, no tuning file), second launch: docked, wrist 0.082 left, 0.020 under, 0.025 forward.

## Not established
- How it looks and feels in the headset (Tefa): `[ID RE2-001]`. Leon's hands, the other pump guns' upgrades (long barrel,
  stock) were not looked at; they share the joint and the numbers.
- Whether the forearm twists badly in the new hold (an old fault of 2026-10-14 on the pump: the elbow guard through the hand).
- The loader docks the drawn hand only within 15 cm of the hold, the plugin's grab reaches further: board row.

## Tools left behind (`dev-archive/native/tools/sim/`)
`pump_hold.py` (hold the gun level and the left hand on the pump, kept alive), `q_pump_hold.lua` (where the hand sits, in
the gun's axes), `q_give_w870.lua`, `sim_window.py` (the simulator's preview back to a useful size without clicking it).
