# The rounds seen in a handgun's magazine (RE2 and RE3), 2026-11-11

Tefa 2026-11-11: *"can you see if you can get the bullet showing in re2 and re3 in handguns, like Lemi now has in re8 - i
mean in the mag when it's taken out logically so if one is left in the chamber and the mag is taken out, it shows empty,
the amount of bullets left would be epicly immersive but even if just one bullet shows when it's expected to that is cool.
then gun is empty, it cannot still show a bullet inside when the slide is pulled back."* (RE2 to-do item 4 of 2026-11-09.)

## What the games offer

Village's guns say themselves which mesh parts are "the rounds" (`switchVanishEmptyPartsDisp`, magdisp.cpp). RE2's and
RE3's `Gun` has no such field `[measured 2026-11-11, live, both games]`: nothing on the gun speaks of parts for the
rounds. So the answer was read from the model files (each handgun's `.mesh.2109108288` pulled with `pak_pull.py`, read
with NSACloud's RE Mesh Editor parser outside Blender: per part and per joint the vertex count and box)
`[measured 2026-11-11]`, and the parts each gun really has were read live (`getPartsEnableIndices`).

| Game | Gun | Magazine | Rounds of their own |
| --- | --- | --- | --- |
| RE3 | WP0000 G19 | standard = part 1 on joint `_04`; extended = part 4 on joint `_05` | part 7 (standard), part 8 (extended), 1190 vertices each |
| RE3 | WP0100 G18 | standard = part 2, extended = part 1, both on `_04` | part 7 (430 v), part 8 (1190 v) |
| RE3 | WP0200 G18 | in part 0, on `_04` | part 7 (430 v) |
| RE3 | WP0300 Samurai Edge | part 2 on `_04` | part 7 (1530 v, a tall column) |
| RE3 | WP0600 MUP | in part 0, on `_04` | part 7 (112 v: one top round) |
| RE3 | WP3000 magnum | part 3 on `_04` | part 7 (300 v) |
| RE2 | WP0700 Broom Hc (file `hsc`) | in part 0, on `_04` | part 1 (40 v: one top round) |
| RE2 | WP3000 Lightning Hawk | in part 0, on `_04` | part 5 (300 v); the game leaves it OFF |
| RE2 | WP0000 Matilda | part 5 (long magazine: part 6), on `_04` | NONE: the magazine is one piece |
| RE2 | WP0100 M19, WP0400 (file `glock`), WP0600 MUP (file `usp`), WP7000 Samurai Edge | inside the gun's single part | NONE |
| RE2 | WP0200 JMB Hp3 | part 5 (long: part 6) | NONE |

Live check of the Matilda `[verified-live 2026-11-11, simulator, n=1]`: with part 5 off the whole magazine is gone, so the
part is the magazine, not a round.

## What is built (RE2 b201, RE3 b017; shared code, RE2 owns it)

`src/guns/magrounds.cpp` + `MagRounds` in `gun_data.h` + each game's table in `src/game/guns_table.cpp`. The rounds part is
on while the MAGAZINE holds a round and off when it holds none (`magazine::rounds_in_mag()`):

- seated: the loaded count minus the one in the chamber (a slide gun; not minus one while a rack is still due);
- out of the gun (in the hand, falling, on the floor, put away with the gun): what left with it = loaded minus the
  chambered round that stays in the gun;
- a fresh one from the pouch: what is carried.

So one round left = it is in the chamber: the magazine shows empty, seated or in the hand. An empty gun shows none with the
slide back. Guns with two magazines switch only the rounds of the magazine the gun wears.

Simulator `[verified-live 2026-11-11, simulator, n=1 each]`: RE3 G19 seated with 33 / 5 / 2 loaded = shown, 1 / 0 = hidden;
magazine dropped with 5 loaded = stays shown (4 in it). RE2 Broom Hc 5 loaded = shown, 1 = hidden; Lightning Hawk 5 = shown
(part 5 read back on), 0 = hidden. NOT seen by eye at headset sharpness (the simulator's picture is too small to tell a
round apart); the out-of-the-gun cases with 1 and 0 loaded are compile-checked arithmetic, not driven `[compile-verified]`.

## Open

1. **RE2's other handguns (Matilda, M19, JMB Hp3, WP0400, MUP, Samurai Edge) need the round cut out of the model.**
   Their magazine is one piece; whether a round is modelled at its top was not looked at yet (island check of the
   magazine's vertices, then Blender). A changed gun model is a modified game file the mod needs (allowed since 2026-09-27).
   Blender work: together with Tefa.
2. **The number of rounds left.** Each gun has ONE rounds part (a fixed stack), so only "some / none" can be shown without
   new models. A stack that shrinks needs rounds of our own (RE3 has `wp0001_HandgunCartridge`, a single-round model).
3. **RE3 G19 with the extended magazine: the magazine hangs on joint `_05`, our table moves `_04`** `[inferred-static]`
   (the save used here wears the extended one: parts 4 and 8 on). To check in the headset: does the long magazine come out
   in the hand, or stay in the gun?
4. RE2: the first start right after installing the new build ended by itself within a minute (a 286-byte crash note, no dump worth
   reading); the next start ran normally `[measured, n=1]`. Watch for it.
