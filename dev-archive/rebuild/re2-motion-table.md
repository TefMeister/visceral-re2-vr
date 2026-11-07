# RE2's reload motions, from the 2026-11-02 finger takes (Claire) `[measured 2026-11-07, n=1 take per gun]`

Made by `dev-archive/native/tools/clips/motion_table.py` (plan Task 11 step 1). Motion ids and timings only.

What it says, against Village's (which the clip cutter was written for):
- **One motion per reload, on player layer 3, bank 2000, id 1200**, for every gun. Village plays its reloads on layer 1 as a
  chain (2420 → 2511 → 2512 x N → 2513, or 2500). RE2 has no chain and **no per-round cycle**: the frame counter never starts
  over inside a reload (restarts = 0 everywhere), the W-870 included (one 3.06 s motion for all its shells).
- **The Quickdraw Army alone has two**: 2000:1199 (2.40 s, the left hand: the gate open, cases out) then 2000:1200 (4.23 s,
  the right hand: the rounds in).
- **Which hand works**: the left on every magazine gun, the W-870, the GM 79, the flamethrower and the Spark Shot; the RIGHT on
  the SLS 60 (61 deg against 19) and in the Quickdraw's second part.
- Every handgun's reload lasts 1.62-1.63 s (the same length for eight guns).
- The rocket launcher (4600), the ATM-4 (4400) and the minigun (4700) have no reload.

For the cutter (Task 11): `mag_out` / `mag_in` split by Village's grasp-and-open rule inside 2000:1200 works as it is. A
`round_in` cannot be cut by motion cycles in RE2: it is cut by the loading hand's curl (one grasp-push-open per round inside
the one motion), counted against the rounds the take loaded. That is new signal work, not a port: try it on the W-870 first;
if two tries do not give one clean cycle per shell, it becomes a `MODEL: FABLE` row.

| gun | reload (s) | layer | motion (bank:id) | from (s) | lasts (s) | restarts | left curl range | right curl range |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| wp0000 | 1.63 | player.3 | 2000:1200 | +0.00 | 1.63 | 0 | 30 | 0 |
| wp0100 | 1.62 | player.3 | 2000:1200 | +0.00 | 1.62 | 0 | 30 | 0 |
| wp0200 | 1.62 | player.3 | 2000:1200 | +0.00 | 1.62 | 0 | 30 | 0 |
| wp0300 | 6.63 | player.3 | 2000:1199 | +0.00 | 2.40 | 0 | 42 | 12 |
| wp0300 | 6.63 | player.3 | 2000:1200 | +2.40 | 4.23 | 0 | 15 | 41 |
| wp0400 | 1.63 | player.3 | 2000:1200 | +0.00 | 1.63 | 0 | 30 | 0 |
| wp0600 | 1.62 | player.3 | 2000:1200 | +0.00 | 1.62 | 0 | 30 | 0 |
| wp0700 | 1.62 | player.3 | 2000:1200 | +0.00 | 1.62 | 0 | 30 | 0 |
| wp0800 | 3.93 | player.3 | 2000:1200 | +0.00 | 3.93 | 0 | 19 | 61 |
| wp1000 | 3.06 | player.3 | 2000:1200 | +0.00 | 3.06 | 0 | 52 | 11 |
| wp2000 | 2.34 | player.3 | 2000:1200 | +0.00 | 2.34 | 0 | 67 | 7 |
| wp2200 | 2.86 | player.3 | 2000:1200 | +0.00 | 2.86 | 0 | 58 | 1 |
| wp3000 | 1.63 | player.3 | 2000:1200 | +0.00 | 1.63 | 0 | 30 | 6 |
| wp3200 | 1.62 | player.3 | 2000:1200 | +0.00 | 1.62 | 0 | 30 | 6 |
| wp4100 | 2.51 | player.3 | 2000:1200 | +0.00 | 2.51 | 0 | 48 | 9 |
| wp4200 | 2.48 | player.3 | 2000:1200 | +0.00 | 2.48 | 0 | 34 | 1 |
| wp4300 | 1.79 | player.3 | 2000:1200 | +0.00 | 1.79 | 0 | 23 | 6 |
| wp4400 | no reload in the take | | | | | | | |
| wp4600 | no reload in the take | | | | | | | |
| wp4700 | no reload in the take | | | | | | | |
| wp7000 | 1.62 | player.3 | 2000:1200 | +0.00 | 1.62 | 0 | 30 | 0 |
