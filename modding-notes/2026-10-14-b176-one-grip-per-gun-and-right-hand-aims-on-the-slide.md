# b176: one grip spot per pump gun, and the right hand alone aims while the left is on a pistol's slide (2026-10-14 night, home PC, Fable, desk)

**Build:** `visceral_native.dll` `0aeb22ef…` + a rebuilt VR layer `dinput8.dll` (REFramework `76298bd` + the cumulative patch
`reframework-patch/2026-10-14-re2-one-grip-per-gun-no-steer-on-the-slide.patch`). The b174 files are in
`D:\Visceral build versions\_backup-2026-10-14-before-b176\`. UNWORN; the game was not launched.

## What Tefa saw (b174, 2026-10-14 23:20) `[reported 2026-10-14]`

- Shotgun: *"the grip ending up in 2 places, both can be used, that was the strange part - the wrist spot and the actual
  hand, depending on where i point the gun before pressing LG."* (The elbow-pad-through-the-forearm screenshot goes with
  the wrist spot.) The pistol was fine.
- *"can you also look at stopping left hand from moving the pistol when hand is on the slider?"*
- And: the empty-gun gunshot is gone (b174) `[verified-live 2026-10-14]`.

## Why there were two grip spots

Two docks exist for the left hand, in two programs:
1. **The VR layer's own two-handed grip** (`FirstPerson.cpp`, our 10-10 patch): with LG held within 15 cm of the gun's
   AID JOINT, the left WRIST is pinned onto that joint and the gun is turned to follow the left hand (two-handed aim).
2. **The plugin's rack dock** (`rack.cpp`): with LG (pistol: near the slide joint; pump: LG+LT along the barrel) the HAND
   is put on the slide / fore-end through the socket the plugin hands to the VR layer (`visceral_dock_socket`), with the
   tuned offsets.

With the socket inactive the VR layer fell back to dock 1. On a pump gun both spots lie near the fore-end, so which one
a press landed in depended on where the controller was relative to the two joints at that moment -- which moves with
the gun's angle. Hence "depending on where I point the gun".

## The change

The socket the plugin hands over grew two words (version 2; a version-1 reader behaves as before):
- `owned` = 1 for pump guns (`is_pump`): the VR layer never docks the hand on the aid joint for that gun, and never
  falls back to a latched aid-joint socket either. One grip spot: the plugin's, tunable with the "Hand on the slide /
  pump" sliders.
- `steer` = 0 for pistols on the slide: while the hand sits on the plugin's socket the VR layer skips its "turn the gun so
  the socket meets the left hand" step, so the right hand alone aims and racking does not drag the pistol around. Pump
  guns keep `steer` = 1 (the two-handed aim stays).

Log lines in `re2_framework_log.txt` (the VR layer's): `[Visceral] weapon type N: the grip is the plugin's alone (no
aid-joint dock)` on a pump gun, `... the aid joint's again` when it goes away.

## What Tefa's wear tells

- Shotgun: LG near the fore-end always gives the same hand-on-fore-end grip, wherever the gun points; no wrist-spot
  grip any more. If the elbow pad still shows through, it is this one grip's roll: the pump's "Hand on the slide /
  pump" roll slider.
- Pistol: with the hand on the slide (LG near it on an empty / locked gun), moving the left hand racks the slide but no
  longer turns the pistol; the pistol follows the right hand only.
- Ordinary two-handed pistol aim (LG near the gun, not on the slide) is unchanged: that is still the aid-joint grip.
