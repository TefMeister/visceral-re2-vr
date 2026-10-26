# Gold Digger: what the 20 Ghost Hand runs on RE2 say together

From: `/gd`, 2026-10-26 (shelf `hands`, all 20 `begone-ghost-hand` reports, 2026-10-13 to 2026-10-19, home PC wears).
The board has the first run (2026-10-13 00:04, 33 of 36 racks). Across all twenty:

**Racks: 189 of 282 finished (67 %)** `[measured 2026-10-13..19, n=20 runs]`. The misses the reports list, by kind:

| kind of miss | count |
| --- | --- |
| pull too short | 22 (9 of them at 0.0 cm: the hands did not move apart at all) |
| the hands moved far enough, but the mod measured **0.00** | 16 |
| the hands moved far enough, mod measured part of it (0.29-0.92) | 4 |
| a hand jumped during it | 8 |
| the gun hand moved back with the pull | 4 |

The two that point at the mod rather than at Tefa's hands:

- **"moved far enough, mod measured 0.00" (16):** the controllers really did the pull, but the mod's rack progress
  never left zero. That reads as the grab not being registered as the start of a rack (the pull happened outside
  the grab zone, or the grab began after the hand was already moving), not as a short pull.
- **"pull too short: 0.0 cm" (9):** a rack started and ended with no movement at all: a grab counted as a rack
  where nobody pulled (a brush of the grab zone).

The lag also changed between evenings: 1-2 frames (14-28 ms) on 2026-10-13/14/16, then 5, 12, 5 and 8 frames
(70-168 ms) on 2026-10-17 23:11-23:53 and 2026-10-18 23:53, with 329 calm-hand jumps in the 2026-10-17 23:40 run.
Lag 12 is Ghost Hand's search limit, so read 12 as "could not line up", not as a real 168 ms. Back to 1 frame on
2026-10-19 21:14 (24 of 27 racks).

Proposed: no new row; when the rack is next tuned, start from the 16 "measured 0.00" misses (`begone.py report
ghost-hand` on the 2026-10-13 00:23 and 2026-10-14 00:04 runs have most of them).
