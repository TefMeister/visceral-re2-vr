# 2026-10-11 — the flashlight like RE3's: first light (b148–b152)

Tefa's design (settled 2026-10-11): a flashlight like RE3's. LT with the left hand at the left shoulder switches it;
only the light shows, shining from the left upper chest where the head looks; the torch is never taken out during
play, only seen in cutscenes. REFramework's ManualFlashlight is on by default (b149), so the game does not light the
torch by itself in dark rooms.

| build | what changed | worn |
| --- | --- | --- |
| b148 | switches the GAME's flashlight (Arcade Controls' set_ManuallyLight + PrecedeBits.Accept(64)); FlashLight.applyTransform post-hook hides the torch mesh and places the IES beam | switch works, but Leon's hand posed as if holding the torch, and no beam |
| b150 | the game's state is left alone; the IES light is enabled directly; one report line per switch | no beam; log: Direction (0,0,-1), beam 0.27 m from the camera |
| b151 | beam turn = camera turn (the light shines along local -Z, b148/b150 had it backwards) | no beam; intensity 300000, enabled |
| b152 | the light's GameObject had DrawSelf 0: forced on while switched on | **"i saw the actual flashlight come on and turn with my head!"** — comes and goes, mostly while moving; odd colour and shape |

Next (parked by Tefa for another day):
1. Enforce DrawSelf / Enabled / position every frame, not only inside applyTransform (it ran only some frames).
2. Copy the light's colour / cone / IES profile / shadow settings from the game's own lit state (read them with the
   game's flashlight on, F key) `[hypothesis]`.

Source: `dev-archive/native/src/flashlight.cpp`; builds in D:\Visceral build versions\flashlight\.
