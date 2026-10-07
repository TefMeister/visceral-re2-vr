# A round fired with NO aim state: three of the game's own switches do it (2026-10-07 evening, home PC, Fable, flat)

**Tefa's detour step 2:** fire without holding the right grip, and no latch of the aim stance if at all possible.
Four flat launches, Leon's save at the RPD reception typewriter, handgun with 7 rounds, Visceral native plugin b096 installed,
VR launcher file (`openxr_loader.dll`) parked beside itself for the run and put back after. Probe:
`dev-archive/reframework/autorun/visceral_fire_vars_probe.lua` (numpad keys; nothing runs by itself). Log of the last
launch: `dev-archive/recon/2026-10-07-fire-without-aim-log.txt`. The earlier launches' lines are quoted below from the
session; REFramework starts a fresh log per launch.

## The result `[verified-live 2026-10-07, n=1 shot]`

With the gun lowered (`IsHold=0`, no aim input at all), one press of the game's own ATTACK input while three switches were
held the game's way: **`bullets 7 -> 6`, `Equipment.requestFire` -> `Gun.executeFire` -> `Equipment.executeFire` all
called, the recoil variable (`FireSlur`) kicked 0.80 -> 0.90, layer 4 played the real `pl00_1100_..._Hold_Shoot` clip.**
A real shot, no aim stance beforehand. `IsHold` read 1 for frames 5-9 after the press (the shot state itself carries the
HOLD tag), then 0 again.

## The three switches, and what each one alone did (same save, same gun, one shot each)

| # | the game's own switch | without it | `[tag]` |
| --- | --- | --- | --- |
| 1 | **the order:** `SurvivorActionOrderer.setForcePrecede(true, ATTACK=4)` while the fire input is on (false on release). `checkOrder` honours its Force bits before any input or tag test (dossier 8k). | the press dies in `checkOrder`: `Precede` stays 0, nothing happens | `[verified-live 2026-10-07]` (and 09-25) |
| 2 | **the trigger:** `SurvivorUserVariablesUpdater.set_Fire(true)` once at the press (the player's motion-FSM variable the updater raises for an aimed shot). | `Precede` reads 4 for one frame and nothing follows: the FSM never enters the shot state | `[verified-live 2026-10-07]` |
| 3 | **the gun's question:** `Equipment.enableAttack(WeaponType)` answered YES while our shot is in flight (hooked; it is `checkHold(type) && !checkEmpty(type)`, so unaimed it says no). `Gun.executeFire` begins with this question inlined and returns at once on no. | the FSM plays the real shoot clip, `requestFire` and `Gun.executeFire` run -- and no round is spent | `[verified-live 2026-10-07]` |

Also seen: with 1+2 the FSM first showed `pl00_1120_HG_Hold_Shoot_NoAmmo` for one frame, then the real shoot clip -- so the
09-25 "dry fire" reading was the first frame of the same thing, not a separate branch `[inferred 2026-10-07]`.
Writing the weapon's own `Hold` variable (`Arm.set_CommonVariablesHold`) is overwritten by the game the same frame; the
player's `HoldUp` variable sticks but changes nothing `[verified-live 2026-10-07, n=1 each]`. Neither is needed.

## How the switches were found

The RE2 type dump (`il2cpp_dump.json`, kept in the 2026-09-25 full backup) names the variables the player's and the
weapon's motion FSMs read: `SurvivorUserVariablesUpdater` (Fire, FireSlur, HoldUp, Relax, Jog, ...) and
`Accessor_Wep_common` / `Arm.CommonVariables*` (Hold, Fire, Empty, NoEmptyFire, Equip, ...). The probe read all of them
around an aimed and an unaimed shot; the aimed one showed `Fire=1` for one frame and the unaimed forced one did not, which
named switch 2. The gun's refusal was read from the disassembly of `Gun.executeFire` (0x14136c630) and
`Equipment.enableAttack` (0x1402dd2c0), which named switch 3. Tools: `dev-archive/tools/re-engine/xrefs.py`,
`disasm2.py`, `owner.py` (paths now the Steam install + the backup's dump).

## What this means

- **No latch of the aim stance.** The character never enters the hold state before the shot; the three switches are
  exactly what the game itself does during an aimed shot, applied for the press only. This is detour step 2's "exhaust
  the no-latch possibilities" answered: there is one, and it works.
- **Build next (new native plugin, `src/fire.cpp`):** on the right trigger (bridge `S_RTRIG`) while the right grip is NOT
  held: switch 1 for the frames the trigger is down, switch 2 once at the press edge, switch 3 answered yes from the press
  until the shot's `Equipment.executeFire` has run (or 20 frames). Prove its own effect: log `bullets before/after`,
  the three doorbells, and `IsHold` at the press. Leave the aimed path untouched (RG + RT fires as before).
- **Open, for the headset:** what the body does in the 5 frames the shot state carries the HOLD tag (the relaxed splice
  made the hold clips relaxed, so possibly nothing); automatic weapons / rapid fire (`ResetRapidShot`, `checkEnableRapidFire`);
  the empty-gun case (switch 3 as hooked also says yes on an empty gun -- answer `checkHold` instead of `enableAttack`, or
  AND it with `!checkEmpty`); grenades/knife (sub weapons go through other actions).

## Housekeeping

- The probe is out of the game folder again (kept in `dev-archive/reframework/autorun/`); `openxr_loader.dll` is back.
- OBS: the recorder could not start -- OBS's WebSocket server is off on this PC (Tools -> WebSocket Server Settings). No
  recording of this run. RE2's music volume lives in the in-game options (save data), not in a file; still not muted.
- Flat launch notes: the first relaunch after a close died before the title (no window, log stopped at "Hooked DirectX 12");
  the second try was fine. ENTER at the title needed two presses once (~30 s after launch the first was swallowed).
