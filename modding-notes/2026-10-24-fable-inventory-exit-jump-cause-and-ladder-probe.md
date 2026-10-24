# 2026-10-24 · /pd, Fable: the inventory exit jump decoded from the 10-19 log, cured in b187; the ladder flick gets a probe and a switch (b188)

/pd, home PC, Fable. **The game was not launched; nothing here has been run.** Data: `re2_framework_log.txt` of the
2026-10-19 20:41–21:14 headset run (b184/b185 installed at 16:26 that day, so this run wore them; the board had them as
unworn). 40 menu events dumped by `menu_probe`, 25 jack starts logged by `ladder.cpp`.

## 1. The inventory exit jump: what the 10-19 log says `[measured 2026-10-24, 22 inventory closes]`

For every inventory close (gui 1, `endInventoryCamera (pick-up up 0)` + `switchCamera to type 0`), the camera's yaw at
the close frame F:

| | LockScene POST (our hold, "stripped") | BeginRendering PRE, same frame | from F+1 on |
|---|---|---|---|
| what it is | the pre-menu view | FirstPerson's first write back in control | FirstPerson, settled |
| off the held view by | 0 | **the headset's yaw** (first six closes: the raw yaw; later ones: the head's turn since the menu opened; 20 of 22 within 2 deg) | **the head's turn during the menu** (20 of 22 within 2 deg) |

So: the body direction never changes (the settled view is the pre-menu view turned by exactly what the head turned
while the menu was open, which is right); only the close frame shows the head's yaw twice. The pick-up closes
(`pick-up up 1`) and the gui-3 closes show nothing.

### Cause, read in FirstPerson.cpp `[inferred-static 2026-10-24]`

- In control, FirstPerson builds the view as (the game's player camera controller rotation, the "body direction", stick
  driven, no headset) x (the raw headset rotation), and `on_update_camera_controller` re-reads that body direction
  every frame from `controller->worldRotation` after the game's own controller update.
- When the inventory closes the game switches back to the player camera (`switchCamera to type 0`) and seeds the
  returning controller's rotation from the camera as it stood, through its switch smoothing (`SyncCameraRotation`, a
  `DampingQuat`). That camera already carries a headset turn (the one the VR layer, or our hold, put there). FirstPerson
  took it as the body direction and multiplied the live headset rotation on top: the head's yaw twice, for the frame or
  two the smoothing lasts. The game's own yaw/pitch fields are untouched, so the next frame it is right again.
- This is why b180 (writing the view ourselves at render) could not work: FirstPerson rewrites the camera later from the
  render thread, from the same wrong body direction. And why b181 (no camera switch) made it worse rather than better is
  still not explained `[hypothesis: with the player controller active during the menu, our stripped hold went straight
  into it]`.
- Two flavours of the first-frame error (raw headset yaw in the first six closes, head turn since the open in the rest)
  are both "the headset rotation the VR layer had been adding itself during the menu"; what set its rotation offset
  differently in the first two minutes of the run is not known and does not change the cure.

### The cure: b187, REFramework patch `2026-10-24b-re2-menu-close-hold.patch` `[compile-verified 2026-10-24]`

`FirstPerson::note_pause_state` sees every menu open and close exactly once (both places that refreshed the pause flag go
through it). Open: remember the body direction (`m_last_controller_rotation` + the controller's pitch/yaw). Close: for
`MENU_CLOSE_HOLD_FRAMES` (4) frames the remembered direction rules: `on_update_camera_controller` does not read the
controller, `on_update_camera_controller2` writes the remembered rotation and pitch/yaw back into the controller (the
existing "ignore the next player angles" path), and `update_camera_transform` builds the view from it; a camera still
switching keeps the hold alive. The stick cannot have turned the body during a menu, so the remembered direction is
exact. Proof line, once per close:
`[Visceral] menu closed: the game offered a body direction N deg off the one from before the menu; the old one rules for 4 frames`
N should be about the head's yaw (what the jump was); 0 means the game offered the right direction and the hold did
nothing (the pause menu should read like that).

What would show this reading is wrong: the line prints N near the head's yaw AND Tefa still sees the jump. Then the
wrong value reaches the view by a third path (the render-thread rewrite reading the controller directly), and the next
step is a probe line in `update_camera_transform` itself.

## 2. The ladder flick at the start of a climb: what the 10-19 log says

- Our ladder hold (`ladder.cpp`, the Arcade Controls port) DID engage at every jack: `HOLD captured ... branch=measured`,
  then `snap N: view was X deg off the body, turned in one go` 14–55 ms after `jacked -> 1`, X = −12.8, 4.6, −80.3,
  17.2, −4.6, −11.2 `[measured 2026-10-24, 6 jacks read]`. The board row saying "b174's hold never engaged" was about
  menu_body's jack hold (which only acts when FirstPerson steps aside, and at a ladder it does not); it is corrected.
- That snap turns the rendered view so the head looks straight at what the body faces, i.e. it takes the head's own turn
  out of the view in one frame, which is one reading of Tefa's words ("grabs the camera and points it at the centered hmd
  direction"). But the hold then keeps it there, and Tefa says "for just a frame or 2", so the b185 switching-frame cause
  (the stale pre-cutscene headset rotation) is the other candidate, and whether b185 removed it is unknown: no report
  from the 10-19 run.
- The frames of a jack start were never dumped: `menu_probe` only fires on menu changes, and nothing logs
  `SwitchingCamera`.

### b188: a jack probe and a live switch `[compile-verified 2026-10-24]`

- `menu_probe` now dumps the frames around a jack's start (20 after) and end the same way as a menu change
  (`menuprobe ---- jack START seen at frame N ----`), and every line carries `jk=` (jacked), `sw=` (SwitchingCamera, −1 =
  unreadable), `ct=` (BusyCameraType), `cy=` (the controller's Yaw as our hold left it). Dump cap 80 -> 160.
- REFramework menu, "Visceral: hand tuning": tick box **"Ladder / cupboard: hold the view facing the body (untick to
  compare)"**, on by default (bridge slot 537 `S_LADDER_HOLD_OFF`). Off = our hold writes nothing; the game's own camera
  runs the jacks (as REFramework alone would). So Tefa can climb the same ladder twice, with and without.
- Reading the dump: if the flick frame has `sw=1` and the camera turns while `cy` is steady, it is FirstPerson's
  switching path (b185 incomplete); if it coincides with a `ladder: snap` line and goes away with the box unticked, it
  is our hold's snap, and the cure is to turn gently (drop the "turned in one go" window, `SNAP_WINDOW_S`).

## Also corrected on the board

- b184/b185 were worn on 2026-10-19 20:41–21:14 (this log), not unworn.
