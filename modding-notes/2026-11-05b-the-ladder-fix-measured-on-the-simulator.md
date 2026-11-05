# The ladder fix measured on the simulator: five lever pulls with the head turned (2026-11-05, Fable, home PC)

Tefa: *"can you look at the ladder fix too?"* b194 (2026-10-29) takes the head's turn out of the hold the moment a jack
takes the camera, so the three-frame flick (the full head turn, snapped away late) should be gone. Unworn until now.

## How a jack was made without a ladder or a player at the keys `[verified-live 2026-11-05, simulator, n=5]`

RE2 on the OpenXR Simulator with VR Bench (`tools/re2_sim_boot.py`), the head yaw set by the simulator. The RPD hall's
east shutter lever (`sm42_162_ShutterLever01A_gimmick`, a `GimmickToggleSwitch`) is a jack. By script, from the probe's
`lua` command: its control (`get_MyGimmickBodyObject` names the lever) `tryOccupy()` + `startGimmick()` +
`requestStartSoloGimmick()`, and on the body `requestTurnToggle(not get_IsToggle())`: the jack starts (`ladder: jacked ->
1`), Leon is carried to the lever, pulls it, the body turns about 80 deg, release. The toggle flips each time, so it
repeats. (Walking to it and pressing A never found the prompt; the goddess statue's A opens the use-item screen, which
neither B nor Esc closed: `NewInventoryBehavior.close()` did.)

## What the hold logged, first measurement after the three-frame wait (or the first snap)

| head yaw | pre-turned | first error | snap? |
| --- | --- | --- | --- |
| 0 | 0.0 | -2.2 | yes (2.2) |
| +15 | -15.0 | -0.9 | none |
| -15 | +15.0 | +2.1 | yes (2.1) |
| +30 | -30.0 | -5.3 | yes (5.3) |
| -30 | +30.0 | +4.0 | yes (4.0) |

- The head's turn is read right (`hmd0 + offext` = the simulator's yaw, both signs) and taken out whole: without the
  pre-turn the first error would have been the full 15 or 30 deg, as the 10-28 log showed.
- What is left is about -2 deg with the head straight (the lever's own, under the deadband's edge) plus roughly a tenth
  of the head's turn the other way (over-correction, or the one-frame lag of the rendered-view reading during a jack
  that turns the body): at 30 deg about 4-5 deg, snapped away at frame 3. One pull per angle: `[hypothesis]` on the shape.
- A diagnostic fault found: the `pre-turn check` line logged one frame AFTER the first snap had corrected the view, so
  it always said ~0 and hid the real first error (the snap line carried it). Fixed: it now logs the first measurement
  itself, before the snap (build b192 of the RE2 plugin, unworn).

## What it means

The fix does what it was built to do. Whether a 4-5 deg one-frame correction at a 30 deg head turn is still visible is
Tefa's call in the headset: `GATE: VR USER`. If it is, the next step is a scale on the pre-turn (about 0.85) or one more
frame of wait, decided from headset numbers, not these five.
