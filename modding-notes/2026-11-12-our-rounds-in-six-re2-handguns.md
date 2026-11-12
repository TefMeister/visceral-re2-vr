# Our rounds in six RE2 handguns' magazines, and the M19's chamber round (2026-11-12)

Tefa 2026-11-12 (home PC, Opus): *"can you do blender work for re2 and re3, the bullets in guns and mags, i'll do the fine
tune work with you in the headset when i have a bit more free time."* So this was done in background Blender without Tefa
watching (an exception to the "Blender together" rule, by Tefa's own ask); the look is to be tuned together in the headset.
Follows `2026-11-11-the-rounds-seen-in-the-magazine.md` (open item 1) and the board's to-do item 4 (the round in the chamber
of an empty gun).

## What each model had `[measured 2026-11-12, model files]`

The magazine joint's (`_04`) pieces, split by faces into islands and rendered alone (`dev-archive/tools/blender/rounds/`):

| Gun | Magazine | What was there | Done | Part(s) |
| --- | --- | --- | --- | --- |
| WP0400 (`wp0400_glock`) | inside part 40 | two rounds modelled at the top, separate pieces | cut out | 41 |
| MUP (`wp0600_usp`) | inside part 60 | one round's top (nose and case) | cut out | 61 |
| Matilda (`wp0000_vp70`) | parts 5 / 6 (long) | an empty follower at the very top (also seen with the colour texture: plain dark grey) | two rounds added, follower lowered 12 mm | 7 / 8 |
| JMB Hp3 (`wp0200_browningh`) | parts 5 / 6 (long) | an empty follower | two rounds added, follower lowered 12 mm | 7 / 8 |
| M19 (`wp0100_m1911`) | inside part 10 | an open magazine, follower already 18 mm down | two rounds added at .45 size (x1.12) | 11 |
| Samurai Edge (`wp7000_samuraiedge`) | inside part 0, submesh 1 | a closed box, no follower | top plate lowered 12 mm (opens the top), two rounds added (x0.97, set 3 mm back so the noses stay inside the slanted front) | 1 |

The rounds added are the WP0400's own two (cut out above), so they carry its material: `wp0400_Glock_Mat` (same master
material, `Weapon_Default.mmtr`) is copied into each gun's material file (`.mdf2.21`; the JMB has two identical ones,
`wp0200_browningh` and `wp0200`, both changed). The game's single-round model `wp0001_handguncartridge` was no use: it is a
case without a bullet, 19.4 mm long.

**The chamber round** `[measured 2026-11-12]`: of the six, only the M19 has a round modelled in the chamber: 168 vertices on
the frame joint `_00` (the same shape as `wp0001_handguncartridge`'s main piece), so it showed in an empty gun with the slide
back. Cut out as part 12. RE3's six handguns have no piece shaped like a chambered round (`chamber_rounds.py`), and every one
already has its magazine rounds as a part (RE3 b017), so RE3 needed no model work.

## Code

- `MagRounds` gains `chamber` (default -1, so RE3's table is unchanged): a part shown only while a round is chambered.
- `magazine::chambered()`: seated = loaded, a slide gun, no rack due; out = the round kept in the chamber.
- `magrounds.cpp` writes the chamber part like the rounds parts and logs `the chamber holds a round / is empty`.
- RE2's table: WP0400 41, MUP 61, Matilda 5->7 / 6->8, M19 11 (+ chamber 12), JMB Hp3 5->7 / 6->8, Samurai Edge 1.
- Shared code synced to RE3 and compiled there; RE3 not reinstalled (it behaves the same). `sync_shared.py` stopped first on
  seven files although RE3's copies were identical to RE2's saved ones (FAULTS row); ran with `--force`.

## Builds (RE2, all installed, none seen in the game)

- b202: WP0400 and MUP. b203: + Matilda, M19, JMB Hp3, Samurai Edge. b204: + the M19's chamber round.
- Each model read back after export: the new parts on detail levels 0-4 (5-7 are far views, left as they were). Exports
  are larger (about 1.2-2 MB instead of 0.3-0.4) because the exporter writes levels 0-4 separately instead of shared.
- `[compile-verified 2026-11-12]` only. Not launched: a broken model could stop the game when that gun is drawn.

## For the headset session with Tefa

Look at each of the six with the magazine out: several rounds = rounds at the top; one round loaded = empty magazine;
empty gun, slide back = no round (the M19: also none in the chamber). Tune: how high the rounds sit, how far forward,
their size, and the lowered followers (an empty magazine now shows its follower 12 mm below the lips on the Matilda,
JMB Hp3 and Samurai Edge). The numbers are in `dev-archive/tools/blender/rounds/specs/`; re-run `add_rounds.py`.

## Simulator check (b204, then b205), 2026-11-12 11:3x

`[verified-live 2026-11-12, simulator, n=1 each]`: each of the six given into the hand one after another (the test script
`zz_test_short2.lua`, gift; taken out of the game folder after): the game stayed up, no load error. WP0400 given empty:
`magrounds: WP0400 the magazine holds 0 (seated): its rounds HIDDEN`; MUP, Matilda, JMB Hp3, Samurai Edge with 10 and the
M19 with 7: `holds 9` / `holds 6 (seated): its rounds SHOWN`. Not looked at by eye (the simulator picture is too small).
The M19's chamber part had nothing to change (loaded and chambered: on), so no chamber line was logged.

Found on the way and fixed in b205: `visceral_options_book.lua` read its command file before it existed and REFramework
logged a JSON error about 4 times a second (613 in a few minutes); it now writes the file at the start (0 after a fresh
start). Also learnt: RE2 ignores a window close in play; the quit route (Esc > Quit > Up + Enter, then close at the title)
is recorded in ai-game-control-profiles.
