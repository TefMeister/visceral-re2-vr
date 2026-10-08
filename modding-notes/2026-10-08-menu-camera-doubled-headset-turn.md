# The menu tilt and the closing flicker: the VR layer adds the headset turn twice (2026-10-08, Fable, home PC)

**Build:** `Menu body/v0.2.0-b100 - menu camera stripped of the headset turn for the VR layer`. Installed, unworn.
Game not launched; nothing here has been run. Read from REFramework's source (praydog/REFramework master,
`src/mods/VR.cpp` and `src/mods/FirstPerson.cpp`, fetched 2026-10-08) `[inferred-static 2026-10-08]`.

## What Tefa saw

b095 and b096 held the camera (position and turn) at the pre-menu spot from LateUpdateBehavior POST on, and Tefa still
saw (a) the background tilt slightly left and down the moment a menu opens and (b) a flicker when it closes
`[verified-live 2026-10-08, n=1 each]`. The log confirms the hold itself worked: the game's menu camera sat 1.1-1.4 m
off and was put back every frame.

## How REFramework's VR layer owns the camera in RE2

Two regimes, decided by `FirstPerson::will_be_used()` = enabled and `is_first_person_allowed()` and a player transform.
`is_first_person_allowed()` is false while the GUI state is PAUSE or INVENTORY (`GUIMaster.State_`), or while the
camera type is not PLAYER (cutscenes, and probably jacks) [`FirstPerson.cpp` 1917].

| regime | who writes the camera | what the VR layer does at BeginRendering |
| --- | --- | --- |
| A: FirstPerson used (normal play) | FirstPerson, every frame: `final = body_view * raw_headset_turn`, written to the transform AND joint 0 [`FirstPerson.cpp` 1690-1760]; it also calls `vr->recenter_view()` every frame, so the VR rotation offset = inverse of the headset's yaw | `update_camera()` only remembers the camera; no apply, no restore [`VR.cpp` 1486-1505] |
| B: FirstPerson steps aside (menus, cutscenes) | the game's own camera controller (the menu's outside spot) | `update_camera_origin()`: base = the camera AS FOUND, then `apply_hmd_transform`: rotation = base * (rotation_offset * raw_headset), position = base_pos + base_rot * (rotation_offset * (headset_pos - standing_origin)); `restore_camera()` puts the base back at EndRendering [`VR.cpp` 1531-1612, 1740] |

Mod order: VR and FirstPerson run before PluginLoader (`Mods.cpp`), so our hooks at LockScene PRE run after theirs in
the same entry, and our write is the last thing before BeginRendering reads the camera.

## Why the tilt and the flicker

Our hold wrote the pre-menu camera = `body_view * H0` (FirstPerson's output, headset turn H0 already inside). In regime
B the VR layer multiplied `(yaw(H0)^-1 * H)` on top. With the head still (H = H0) the rendered turn is
`body_view * H0 * pitch-and-roll(H0)`: the yaw is fine, **the head's pitch and roll are applied twice**. Looking slightly
down when opening the menu = the background tilts down, with a left lean from the roll. The swing on closing is the same
doubling during the game's 2-3 frame camera switch back (camera type not PLAYER = regime B), on top of the game's own
turn that the hold already cancelled.

## The fix (b100, `src/menu_body.cpp`)

While a menu is open (or the switch back is still running) **and FirstPerson is not driving** (bridge slot `S_FP_USED` =
`firstpersonmod:will_be_used()`), the held camera is written stripped of exactly what the VR layer will add back:

```
rot = pin_rot * inv(H0) * inv(R_off)
pos = pin_pos - rot * (R_off * (P - origin))
```

H0 = the raw headset quaternion at the pin frame (new bridge slot `S_HMD_Q`, `vrmod:get_transform(0):to_quat()`, the same
OpenXR `view_space_location` FirstPerson reads [`VR.cpp` 4007-4060]); `R_off` = `vrmod:get_rotation_offset()`,
`origin` = `vrmod:get_standing_origin()`, `P` = headset position, all read fresh each frame (bridge array grown 40 -> 52).
The VR layer then renders `pin_rot * inv(H0) * H`: the pre-menu view, turning with the head by exactly what it has turned
since the menu opened, and the position pinned (as in play, where room-scale is off). While FirstPerson drives, the camera
is its own and the old rule stays (hold the full pin only until it is back near the spot).

The pin is now taken only from frames where FirstPerson drove (so H0 is known), at both LockScene PRE and
PrepareRendering POST.

## What proves it, in `re2_framework_log.txt`

- `menu: camera held at ... (FirstPerson driving 0, gui N)` on open: driving must be 0 for the inventory and pause; **N
  for the map tells whether the map counts as INVENTORY** (if the map opens with driving 1, the map is not regime B
  and the old hold applies there).
- `menu: camera had moved ... written stripped of the headset turn` on the first frames.
- `menu: camera hold off after K frames (...; S stripped + P plain writes; FirstPerson driving 1, gui N)` on close.
- The probe (still installed) now prints `gui=N fp=0/1` per frame step around each open/close.

## What is NOT established

- That the pitch/roll doubling is the whole of the tilt: `[hypothesis]` until worn. If a tilt remains with
  `stripped` writes in the log, the residual is in FirstPerson's own first regime-A frame after close, or in
  `m_last_headset_rotation_pre_cutscene` bookkeeping.
- Whether the map menu is regime B (log tells).

## Spin-offs seen while reading

- **Ladder / cupboard start flick (board row 1):** jacks probably switch the camera type away from PLAYER for a frame or
  more, which is regime B with FirstPerson's camera (headset turn inside) as the base: the same doubling, which would
  explain a flick that differs every time (it is the head's turn relative to the body at that moment) `[hypothesis]`.
  Same cure: write the base stripped for those frames, instead of the 400-line view servo.
- **Bridge bug:** `poses()` in `visceral_bridge.lua` reads `vr:get_rotation(0)`, which returns a Matrix4x4f in Lua; its
  `.w` is nil, so slot `S_HMD_ROT` has very likely been the identity since 2026-10-05 and the holster set never turned
  with the head `[inferred-static 2026-10-08]`. Not changed in b100 (one change per build); the fix is
  `vr:get_transform(0):to_quat()`, the path `S_HMD_Q` now uses.
