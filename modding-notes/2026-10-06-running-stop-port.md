# Running stops at once: ported to C++ (b087, 2026-10-06)

**Build:** `Running stop/v0.2.0-b087 - running stops at once, first C++ port` (feature folder; saved with the Lanes
plugin's `builds.py snap --feature`). Installed in the Steam game; unworn. Holds b086's ladder hold and b085's
holsters too.

**Tefa's ask (2026-10-06):** *"Running and then letting go of LS to stop running immediately, same with pressing the
run button again to stop running and start walking."*

## What it is

`dev-archive/native/src/run.cpp`, a C++ re-write of Arcade Controls' `re2_vr_run_toggle_fix.lua` (shipped from
2026-08-09, worn `[verified-live 2026-08-09]` in Arcade Controls):

- **Left stick click** = run. **Stick back to the middle** (amount <= 0.05) or **a second click** = walk at once.
- How: the game sets `PlayerActionOrderer.set_JogMode(bool)` every frame while it wants a jog. A pre-hook replaces the
  value with ours on every call. Arcade Controls proved the alternatives fail: blocking the JOG order does nothing,
  writing the animation flag `IsJog` freezes movement, and writing after one update method was not reliably last.
- Only while the headset is live; flat play is left alone.
- As in Arcade Controls, a click while standing still does nothing (the stick is in the middle, so it walks at once):
  push the stick, then click.
- Arcade Controls advised the game's own **Options > Controls > Run Type = Hold** (not required, but Toggle has its
  own stuck-latch).

Bridge: two new slots, `S_LCLICK` (37) and `S_LSTICK_MAG` (38), written with the buttons at UpdateHID.

## Proving it (`re2_framework_log.txt`)

- `run: set_JogMode hook in` once.
- `run: RUN` / `run: walk` at each change.
- `run: game wanted jog=X, we set Y, the flag reads Z` for the first 10 changes: **Z must equal Y**, or the write does
  not land. `run: set_JogMode argument is not a bool` means the argument slot is wrong (the hook then touches nothing).

## How to take it out

Put back b086 (`Ladder climb/v0.2.0-b086 ...`): `builds.py restore visceral-re2-vr 86 --yes`.
