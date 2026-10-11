# 2026-10-11 — hand poses on reload, the slide grip, shells (b153–b159, /ms with Tefa)

A manual session (Tefa in the headset, Claude building between turns). Started as "a first hand pose on the gun when
reloading with EMV" and turned into a port of the Arcade Controls era finger poses.

## What was tried

1. **EMV Engine Poser (alphaZomega).** Installed into autorun with a one-click "open the player" helper
   (`dev-archive/emv-poses/visceral_emv_open_leon.lua`). Works flat; in VR the extra window smears (REFramework's VR
   overlay does not clear outside its own window) `[verified-live 2026-10-11, n=1]`. Found the 17 left-hand finger
   joints (`l_hand_index_0..2`, `middle_0..2`, `ring_0..3`, `little_0..3`, `thumb_0..2`). Taken out again, kept in
   `D:\claude video game stuff\taken-out (local)\`.
2. **Tefa: "can't you get all the hand animations from the Arcade Controls mod?"** Yes: the Arcade Controls era
   `re2_vr_reload.json` (staging/arcade-controls-re2-vr) holds RELOADED's `left_hand_pose` data: `mag_hold` and
   `slide_rack` per weapon and per character (leon/claire/hunk), local Euler angles in radians, applied by his ext_5
   with `set_LocalEulerAngle` at LateUpdate and PrepareRendering, blended over 0.12 s. Credit: Andyalpa.
   `tools/gen_handpose_data.py` turns it into `src/handpose_data.h` (73 poses).

## Builds

| build | change | Tefa |
| --- | --- | --- |
| b153 | `handpose.cpp`: magazine-hold fingers while a magazine or shell is in the left hand | **"looks exactly right"** `[verified-live 2026-10-11, n=1]`; log proof: written = read back |
| b154 | slide_rack pose while `rack::active()` | no change: in button mode the hand never "docks", so it never ran |
| b155 | LG+LT holds the slide back, LT release = forward; grip pose when LG is held near the slide; shell ox 0.09; shell insert ≤ 0.10 m; empty gun's `requestFire` skipped | slide fingers no change; empty still a gunshot; shell better; shell upright through a finger |
| b156 | slide grip uses the MAG pose (the saved slide poses ≈ the relaxed hand: index_1 -0.69 = idle); shell sliders in the REFramework menu; click muted test | fingers change on the slide; shell set by Tefa; **gunshot still with our click muted and requestFire skipped** |
| b157 | the plugin's dock socket published while gripping (the hand moves onto the slide); Tefa's shell numbers; shell insert ≤ 0.07 m | hand on the slide but turned wrong; shell 2–3 cm too far |
| b158 | "hand on the slide" sliders; shell insert ≤ 0.045 m; FirstPerson_Enabled back to true | Matilda works well, hand approximately right; hand stays on the gun with LG held; magnum slide stayed open after the rack |
| b159 | finger sliders; LT release = hand off the slide; slide held closed after a rack until the next shot | hand comes off then docks onto the HANDLE (Tefa likes it); magnum slide closes |

Tefa's numbers (built in): shell `{0.108, -0.042, 0.032, yaw 13, pitch 86, roll 0}`; hand on the slide
`{0.072, 0.052, -0.078, yaw 180, pitch 18, roll 180}` (Matilda).

## Established

- The finger write works and holds (`[verified-live 2026-10-11, n=3]` reload rounds, proof lines).
- The empty-gun "gunshot" is NOT our click and NOT `Equipment.requestFire` `[verified-live 2026-10-11, n=1]`: both
  were removed in b156 and Tefa still heard it. Its source is upstream (the motion FSM's shoot action / input).
- The slide-dock socket in the VR layer (b143 patch) works: the hand goes onto the slide `[verified-live 2026-10-11]`.
- The VR menu text squish: in VR REFramework draws at the eye size (2688×2880) and stretches it onto the window. Giving
  the window that shape (NormalWindowResolution 1008×1080) made it worse (a thin strip) `[disproved 2026-10-11, n=1]`;
  reverted.

## Open (Tefa's list at the end)

1. **VR layer (Fable, Wednesday):** the dock must slide in smoothly, not teleport; a magazine/shell in the left hand
   must suppress BOTH docks (handle and slide); the slide socket must not re-aim the gun (the dock's two-hand aim turns
   the right hand to fit the socket, so a slide socket with an offset throws the gun).
2. **Per-gun hand and finger settings** (the Matilda grip does not fit the magnum): the sliders should save per gun.
3. **Empty-gun gunshot:** find the real source (Fable).
