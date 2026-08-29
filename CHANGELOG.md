# Changelog

All notable changes to Visceral — RE2 VR.

## 0.1.0 — 2026-08-30 — first release: body & posture

The first slice of Visceral: fixes to how your character's body behaves in VR
while a weapon is in your hands. Everything here is body/posture only — weapon
handling, reloads and holsters come in later versions.

**What it adds to the vanilla RE2 VR experience:**

- **Torso straightening.** Removes the twisted, hunched weapon-hold posture the
  characters (especially Claire and Ada) adopt with a gun out, so the upper body
  faces forward naturally. The correction adapts to the animation, so normal
  breathing/gait motion is preserved rather than frozen.
- **Feet on the ground while aiming.** In VR the braced aim pose makes the body
  float ~10 cm off the floor (the body hangs from your headset height, and the
  braced legs leave the feet hanging). Visceral lowers the body so the feet plant
  on the floor and the knees bend naturally — the pose sits down where it belongs.
  Your hands, the gun, and where bullets land are untouched; only the body is
  adjusted. Tunable in the menu (default: 0.175 m).
- **Faster aim-walk (collision-safe).** While aiming, movement is sped up so you're
  no longer stuck in the vanilla aim-crawl — done by amplifying the game's *own*
  movement, so walls and doors still stop you (no clipping) and it stays analog to
  your stick. Aiming only; normal movement is untouched. Tunable multiplier
  (default 1.3×); off-by-default experimental options for applying it to all
  movement and speed-driven run animation are left in the panel for tinkering.
- **Cutscene & grab safety.** All body adjustments automatically switch off during
  cutscenes, scripted camera events, and the enemy-grab third-person camera, so
  those shots are never distorted.

**Notes**

- Tested on both Leon and Claire.
- Each feature has an in-game panel (REFramework menu) with a toggle and sliders,
  usable with the VR controller pointer.
- No game files are modified or included; nothing is written to disk during play.
- Reversible: disabling a feature (or removing the script) restores vanilla.
