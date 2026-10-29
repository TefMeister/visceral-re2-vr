# 2026-10-29 (morning) — the ladder flick is the head's turn counted twice for three frames (home PC, Fable, desk, no game)

## The question
Tefa: *"can you also work on the re2 ladders flicker issue, i think you fixed the inventory close one it does not
flicker anymore. for some reason the main menu picture is back to the default one, not the save game one"*.

## What the 10-28 log says (b188's jack probe, seven jacks, 23:51 to 00:04)
Per frame, the yaw of the game's camera transform at `LockScene.post` (what the frame is drawn from), the headset's
yaw relative to its centre, and the player camera controller's yaw as our hold wrote it:

| jack | head turn | camera before | camera at the jack frame | change | frames until our snap | after the snap |
| --- | --- | --- | --- | --- | --- | --- |
| 23:51:38 | +8.6 | -88.3 | -80.1 | **+8.2** | 3 | -89.1 |
| 23:51:43 | -33.0 | -89.8 | -124.4 | **-34.6** | 3 | -90.8 |
| 23:55:24 | +1.4 | -67.3 | -65.6 | +1.7 | 1 | (the body turns to the ladder, 23 deg over 9 frames) |
| 23:58:37 | +1.4 | 99.8 | 101.9 | +2.1 | 2 | (body turn) |
| 23:59:50 | -6.3 | 89.8 | 86.0 | -3.8 (+2.2 snapped already) | 3 | 91.1 |
| 00:04:24 | -5.9 | -89.2 | -83.3 | +5.9 (the other way) | no snap; back by itself after 4 frames | -88.0 |
| 23:54:40 | +8.4 | -89.0 | -95.3 | a snap taken from a stale reading one frame after the capture over-turned by ~7; the servo crawled back over 15 frames | | |

So `[measured 2026-10-29, 5 of 7 jacks]`: **at the frame a jack takes the camera, the camera transform gains the
headset's own turn once more** (in normal play the transform holds the body direction and the headset's turn is added at
render; during the jack the transform = body + head, so the drawn view = body + 2 x head). Our hold's servo measures the
rendered view against the body, but through a reading that is a frame or two behind, so it noticed three frames later and
turned the controller by minus the head's turn in one go ("snap"). **Those three frames are the flick.** Tefa's words
fit: *"points it at the centered hmd direction for just a frame or 2"*. b185's switching-frame reading was not the cause
(the probe never found `SwitchingCamera`: `sw=-1` every frame; FirstPerson was in use throughout, `fp=1`). Which line of
FirstPerson/the game adds the turn is not pinned `[hypothesis]`: the cure below does not need it.

The 00:04 jack went the other way (+5.9 with the head at -5.9) and came back by itself: one jack in seven does not fit;
with the cure below it would get a flick it did not have, bounded by the servo as before.

## The cure, built as b194 (visceral_native 52235986, installed; board b194, builds repo Ladder b181)
- `ladder.cpp` `capture_hold`: after the measured aim, `rotate_hold(-(hmd0 + offext) * vsign)`: the head's turn relative to
  its recentred zero is taken out of the hold at the capture, which reaches the camera the same frame the game adds it
  (a controller write at frame N shows in the transform at N+1; the capture happens the frame before the first `jk=1`).
  Logged on the `HOLD captured` line as `pre-turned N deg`.
- No snap in a jack's first three frames (`SNAP_FIRST_WAIT_FRAMES`, was 1): the 23:54 over-turn came from a reading that
  had not yet seen the capture.
- `pre-turn check: view N deg off the body, M ms in (pre-turned P; under 2 = no snap needed)`: logged once per jack at
  the first measurement after the wait. **Under 2 deg = the guess was right and no flick is expected; near P = the guess
  was wrong (the servo then snaps as before).**
- `JACK_PRE_TURN` in `settings.h` switches it off (a rebuild); the b188 tick box still switches the whole hold off.

## The main menu picture
`visceral_title_one_scene.json` held `"enabled": false`, written 2026-10-28 20:35. The script toggled on NUM9 through
`reframework:is_key_down`, which sees the key in ANY window, so a 9 typed on the numpad anywhere on the PC while the
game ran switched the save-game picture off. The hotkey is gone (the tick box "Use the last-save scene everywhere" stays),
the file is back to `true`. Old copies in `D:/Visceral build versions/_backup-2026-10-29-before-title-num9/`.

## Not established
- Nothing worn. The check line decides: climb a ladder with the head turned 30 deg or so, then read `pre-turn check`.
- Whether the flick Tefa sees is exactly these three frames (the probe measures the transform, not the headset picture).
- Why one jack in seven adds the turn the other way.
