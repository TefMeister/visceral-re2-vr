# 2026-11-01: Village's gun work brought to RE2 and RE3, and a full check of both games

All on the OpenXR Simulator with VR Bench's test hands (no headset). Nothing here has been worn yet.

## What came across from Village

| Piece | RE2 | RE3 |
| --- | --- | --- |
| Empty click = a short hammer cock (the game is silent of its own on an empty pull) | yes (b186) | yes (RE3 b001) |
| A rack still due after a weapon swap | yes (b186) | yes (RE3 b001) |
| Hand Poser: a gun's own pose loads by itself; the left fingers held only while the left hand is on the gun | yes (b187) | gun name yes; left-hand slot built, proven only in RE2 |
| Left hand on the magazine of a long gun (LE 5: LG on it, LT pulls it out into the hand) | already there, checked | not checked |
| GM 79 break action: LG + LT on the front grip opens and shuts, an upward flick shuts, the barrel swings 40 deg, no firing while open, one round from the pouch only while open | yes (b188) | (no GM 79) |
| Left forearm held, right hand still with no gun | not needed (checked) | not needed |
| Grip spheres | RE2 has none | none |

## The launchers

* RE2 GM 79: the game's own reload turns joint `_01` 40 deg (the hinge; turned by hand the barrel drops open), `_06`
  50 deg (the latch), and moves `_04`, `_100`, `_101` (the rounds). Whole cycle on the simulator: fire, open with
  LG + LT on the front grip, a round from the pouch into the chamber (loaded 0 -> 1, carried 4 -> 3), shut, fire.
  A slow turn and a downward flick leave it open; an upward flick shuts it (speed measured across 60 ms of the
  controller, because the drawn gun jumps thousands of deg/s on equipping and firing).
  While the left hand carries a round it is free of the game's two-handed grip, or it could never reach the chamber.
* RE3 MGL: its model has NO hinge. The game's reload only turns the cylinder (`_01`, 100 deg about its length) and moves
  the rounds (`_06`..`_09`, `_101`). So there is no barrel to swing open; left as the game has it until Tefa decides.

## The full gun sweep (gunsweep.py: give, in hand?, Hand Poser's name, fire to empty, the hammer)

RE3: G19, G18 (burst), G18, MUP, M3, CQBR, Lightning Hawk: all pass (hammer on empty). No hammer, because the mod does
not manage them yet (the game's own empty behaviour): Samurai Edge, MGL. The "(other)" spare entries, the rocket launcher
and the 4200/4400 specials read no ammo count.

RE2: Matilda, M19, JMB Hp3, Glock 17, MUP, Broom Hc, Lightning Hawk, Samurai Edge, W-870, MQ 11, LE 5, GM 79: all pass.
No hammer (the game's own reload and click): the three revolvers (Quickdraw Army, SLS 60, Chief Irons) and the special
weapons. The Remington 870, M3 and Lightning Hawk (shotgun) entries read no ammo count (extra-mode copies).

Every gun in both games was named correctly to Hand Poser.

## Open

* Revolvers in RE2/RE3 still use the game's own reload: the Village revolver loading (LG + LT, rounds by LT) is a new
  build there, as the GM 79 was.
* RE3's Samurai Edge is not in its reload table.
* Everything above wants a headset check.
