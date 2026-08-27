# No-grip-to-shoot: proven feasible in one evening (2026-08-27, home PC)

The user's challenge for the night: *"see if a whole core system of a game can be
changed"* — specifically, remove RE2's requirement to hold RG (the aim input) before
the trigger will fire. Either prove it impossible or spend however long it takes.
It took one evening, two probe versions, zero crashes.

## The mechanism

The game never checks the physical button when firing — it checks whether the
character is in the aiming stance, and that stance is driven by one bit in the
input abstraction: `app.ropeway.InputDefine.Kind.HOLD` (= 64). Flat RMB/LT and the
VR grip all funnel into that same bit. The fire input is `Kind.ATTACK` (= 256 —
note the frozen AC code carried a wrong-but-never-used fallback of 4, which is
actually WALK).

`app.ropeway.InputSystem:setForce(Kind, bool)` — the same method the AC era used
to hard-block empty fire — turns out to be a **clean latching switch**:

- `setForce(HOLD, true)` once → aim stance raises on its own,
  `SurvivorCondition.get_IsHold` flips true, and the trigger fires with the aim
  button untouched. The force **persists indefinitely** (survived its pulse ending
  by 40+ seconds; no per-frame reassert needed).
- `setForce(HOLD, false)` once → stance drops, **vanilla aiming works normally
  immediately after**, and the fire gate returns (trigger alone refuses again).
- So the bool means "start/stop forcing this input active" — not "force to this
  value". Two one-line calls, no per-frame work, no state-machine writes (the
  IsJog freeze lesson said to stay away from those), no races against native
  recomputation (the force lives above the per-frame ButtonBits rebuild that
  doomed the old SUPPORT_HOLD bit-clearing attempt).

Probe: `visceral_noaim_fire_probe.lua` in `-dev-archive` (v1 `33de98a`, v2 result
`49b2b4c`). Verified flat; the input abstraction is shared, so VR should follow,
but that's still to be verified in-headset.

## The design this enables (user's vision, sharpened by the result)

Latch HOLD on **trigger-touch/press**, unlatch on release. Because the latch is
instant and clean, the character only aims while a finger is actually on the
trigger — which sidesteps most of the livability problems the user predicted
before testing:

- slow aim-walk / no running → only applies during the moment you're shooting
  (AC-era alternative if needed: raise aim-walk speed ~1.3, but that desynced
  footstep audio — a known open problem, not solved here);
- item interaction while aiming → untested yet, but under trigger-touch latching
  you're not touching the trigger while grabbing items;
- separate backlog item either way: left-hand two-handed grip should require LG
  held, not proximity auto-grip.

## Open before it's a feature

1. VR verification (does the trigger reach ATTACK without REFramework's own
   grip-gating interfering; does trigger-touch exist as a bindable action).
2. Stance-raise latency from latch to fireable — decides touch vs press latching.
3. Livability sweep while latched: pickups, doors, inventory, cutscene entry.
4. Edge cases: does the game itself ever call setForce and stomp/inherit our
   latch (cutscenes, level loads, menus)?
