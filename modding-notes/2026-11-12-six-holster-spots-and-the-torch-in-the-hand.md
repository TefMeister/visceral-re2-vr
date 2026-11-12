# 2026-11-12: six holster spots and the torch in the hand (RE2 first, then RE3 and Village)

Developer PC, Opus, no game launched. Built and compiled for RE2 (not installed, never run).

## Tefa's design, verbatim

> "for re2, re3 and re8, manual flashlight, an actual flashlight one can hold in hand. re2 has it, re3 and re8 don't.
> flashlight itself i would like to be able to swap the position, either holding it up above [tactical overhand picture]
> [ordinary grip picture] like so. Jill's on-chest flashlight needs to be removed if possible, so they all have an actual
> flashlight to show around. and Jill - her chest still keeps pushing forward whenever standing still."

> "i want there to be 6 holster spots on all my charachters. hips, shoulders, back, 1 set on left and 1 set on right, in
> exact same positions for both weapons and medicine and so on, just LG and RG in these spots do different stuff. for
> example flashlight is in the left shoulder spot with LG, same shoulder spot with RG is the weapon in this slot.
> flashlight - LG + LT flips the hold, LT switches it on/off in both hand and shoulder spot. long weapons cannot be two
> handed while flashlight is held - it has to go back to shoulder spot before putting anouther hand on the gun, but can
> reload weapons with the flashight out"

> "LG is not required to hold the flashlight, just like the weapon tap LG in the shoulder holster to bring it out, tap LG
> again to put it back. i thought while flashlight is just out, then LG + LT to flip it, but if wrist angle is reliable it
> is way better option." Grip swap: wrist twist. RE3 / Village torch: our own model, made in Blender together.

> "LG + right shoulder - blank, LG + right hip - blank. Second LG tap puts the flashlight back on holster spot only. the
> holster spots have to be as big as possible so they won't interfere with each other but also don't get in the way of
> gameplay."

## The six spots (all three games)

| spot | LG (left hand) | RG (right hand) |
| --- | --- | --- |
| left shoulder | the torch: tap out, tap back; LT lights it | weapon slot left |
| right shoulder | blank | weapon slot up |
| left hip | magazines (the pouch) | weapon slot down |
| right hip | blank | weapon slot right |
| back left | medicine (a later step; buzzes for now) | blank (not decided) |
| back right | blank (not decided) | knife / sub weapon |

Size: reach 20 cm (was 15), leave at 26 cm; a hand is in the NEAREST spot within reach, so overlapping spots never both
answer. Back left is the mirror of the old back spot (now back right, unchanged).

## What was built for RE2 (b206 when installed)

- `src/body/holster.cpp`: the table above; nearest-spot-wins per hand. `holster::left_hand_at_torch_spot()`.
- `src/body/flashlight.cpp`: the torch lives in the left shoulder spot. LG tap there = out / back. LT = light on/off,
  anywhere while in the hand (not with LG held, not while a reload has the left hand), or in the spot. In the spot: the
  2026-10-11 light from the left upper chest, torch hidden. In the hand: the game's own torch mesh shown and placed on
  `l_arm_wrist` (`TORCH_HOLD_*`, first guesses), the beam out of its lens; the grip follows the wrist (the lens goes to
  whichever end of the fist points forward: thumb end = ordinary, little-finger end = overhand; 0.25 hysteresis, 0.15 s
  ease). Both placed every frame at PrepareRendering (b152's "comes and goes" was applyTransform running only some
  frames). The FlashLight object is held with a reference so it is never a dangling pointer.
- Shared with RE3 (`src/core/hand_state.h`, `src/guns/left_hand.cpp`): with the torch in the hand and no rack or reload
  claiming the left hand, the grip is ours with no socket, so the loader does not dock the left hand on a long gun; Hand
  Poser holds the left fingers while the torch is held.
- `[compile-verified 2026-11-12]` only. Unknowns for the first run: whether the torch mesh draws when the game's own
  flashlight state is off; the hold numbers; the lens axis; whether Hand Poser's fingers look like a grip on a torch.

## Next, in order

1. RE2 first run (flat or simulator; the left hand is needed, so the simulator): the log's wrist axes, tune
   `TORCH_HOLD_POS / YPR / LENS_AXIS`, see the beam leave the lens; then the headset.
2. RE3: the same body files (Jill: hide her chest light), the torch model from Blender.
3. Blender with Tefa: our torch, styled on the two pictures (black tactical zoom torch, glowing lens, parts apart).
4. Village: our own six-spot holster code in place of the borrowed script (it is at its size limit), the torch.
5. Jill's chest pushing forward when standing still: compare with RE2's body (2026-10-10 probe: the body slides ~8 cm
   forward under the head at a stop).

## First run, simulator (12:36-12:45, Claire, the main hall, Minigun in hand) `[verified-live 2026-11-12, simulator, n=1]`

- b206: LG tap at the left shoulder took the torch out; the game's own torch mesh DRAWS in the left hand even with the
  game's flashlight state off; LT lit it from the lens (light on vs off: a faint change in the lit hall, mean +1 grey
  level, 1,568 pixels brighter by 20+); "left hand: the torch (no two-handed grip) has it"; the fist raised with the
  little-finger end forward flipped the grip ("OVERHAND ... -0.34"), back in front flipped it back; LG tap at the shoulder
  put it back and the light went on shining from the left chest.
- The torch's origin is its TAIL: at 8 cm the tail sat at the knuckles and the torch ran ahead of the hand. b207: tail just
  past the wrist, lens 24 cm out: the fist now closes on the rear half, head forward. Wrist +Z runs along the hand.
- One "get_GameObject" game exception at the title -> play hand-over, before our FlashLight was found; it came from the
  title rain script's line ("title left"), not ours `[inferred]`.
- Left for the headset: the hold numbers, how bright the beam is in a dark room, how the overhand grip feels (the
  simulator's arm did not reach eye height), Hand Poser's fingers round the torch.

## RE3 (13:00-13:16, Jill, the radio room) `[verified-live 2026-11-12, simulator, n=1]`

- The torch code moved to `src/hands/flashlight.cpp` (shared with RE3 by `sync_shared.py`; type names through
  `game::ns()`). RE3's FlashLight is the same type (`offline.FlashLight`, applyTransform, `<IESLight>`, `<Condition>`).
- Jill's lamp is the FlashLight game object's own mesh (children FakeGI, IESLight, LightShaft; parent pl2000). Switched to
  drawn (DrawSelf was 0), DrawDefault 1, mesh on, placed at the wrist: never seen. Read: it is bound to her body, not to its
  game object `[inferred]`. So `TORCH_GAME_MESH_IN_HAND = false` for RE3: hidden in play (her chest lamp is gone, Tefa's
  ask), the beam from the empty left hand until our Blender torch. RE2 keeps `true`.
- The beam could not be seen on the simulator in that room either way (light on vs off looked alike); for the headset.
- A start-up froze once inside REFramework's own hook install (the known RE2 hang); the window close did nothing; ended.
- RE3's window close works from play only with the window in front (KNOWN.md).
- Builds: RE3 b018 installed (`torch in the hand/v0.1.0-b018`). RE2 compiled with the shared changes, b207 stays installed.
