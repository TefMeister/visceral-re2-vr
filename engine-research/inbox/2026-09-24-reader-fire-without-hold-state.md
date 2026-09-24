# Where firing is tied to the aim (Hold) state, and the cheapest way to fire without it (2026-09-24, read-only reader beside the live /lm)

Author: reader session (static only: `il2cpp_dump.json`, capstone over `re2.exe` at image base
0x140000000, the 2026-08-29 probe notes, Arcade Controls notes). Nothing launched, nothing hooked,
nothing installed. Every claim tagged; "measured" here means *read from the disassembly or the dump*,
never a live run. Live facts are quoted from the notes with their own dates.

Question (Tefa's idea 7): fire one-handed on RT alone, and ideally two-hand the weapon in a NON-aim
state and still fire on RT. The dossier's 2026-08-29 verdict was "firing is structural to the aim
state, no single check to delete." This drop names the check, and it is not structural — it is one
managed method with a built-in override the game ships.

## 1. Chain of custody for one trigger pull (in-frame order, upstream → downstream)

All addresses static VA in `re2.exe`; all names from the dump. `[inferred-static]` unless marked.

| step | who | what it decides | evidence |
| --- | --- | --- | --- |
| 1 | `app.ropeway.survivor.SurvivorActionOrderer.updatePrecedeBit` @0x140d3b820, then `updatePrecedeOrder` @0x140d3bdc0 | per frame, walks the precede-order priority list; for each order not inhibited, asks `checkOrder(Precede)` (virtual, vtable+0x110); the first "yes" becomes `Precede` (+0x58) | disassembly |
| 2 | **`app.ropeway.survivor.player.PlayerActionOrderer.checkOrder(ActionOrder.Precede)` @0x140db49d0**, case `ATTACK` (=4) at 0x140db515d–0x140db52b2 | **THE decision.** Accepts the ATTACK order only if: `!SurvivorCondition.get_IsForbidAim` **and** `InputSystem.isOn(0x40 HOLD)` (or `isOn(0x80 SUPPORT_HOLD)` when `get_IsSupport`) **and** `InputSystem.isOn(0x100 ATTACK)` **and** `SurvivorCondition.get_EnableAttack()` (+ a DualSense adaptive-trigger check when that manager exists) | disassembly; 08-29 live log saw `get_EnableAttack` fire first on every real shot |
| 3 | `app.ropeway.survivor.SurvivorCondition.get_EnableAttack` @0x1411930c0 | `Equipment != null && EquipType valid && !Equipment.get_WaitChangeWeapon && tags.hasTag(HOLD) && !tags.hasTag(HOLD_START) && !tags.hasTag(TURN, layer 0) && !tags.hasTag(CHANGE_WEAPON, layer 3) && !tags.hasTag(RELOAD, layer 3)`. `tags` = `StateTagHandle` (+0xb8), the tag set of the motion FSM's CURRENT states. Hashes resolved against `app.ropeway.player.tag.StateAttribute`: HOLD 0x6868eff5, HOLD_START 0x97ea326c, TURN 0xc71737fe, CHANGE_WEAPON 0x1637c1fa, RELOAD 0x54ff7911 | disassembly + dump enum values |
| 3b | `SurvivorCondition.get_IsHold` @0x140454ed0 | is literally `StateTagHandle.hasTag(HOLD)` — **IsHold is a read-out of the FSM state's tag, not a switch** | disassembly |
| 4 | the accepted order reaches the motion FSM (`via.motion.MotionFsm2`, `SurvivorCondition` +0x138) and the FSM DATA decides whether the current state has a transition to the Shot state | not decoded; the player `.motfsm2` asset path was not found (2.48 M guessed paths tested against the pak hash tables, 0 hits; motlist paths under `natives/stm/sectionroot/animation/player/pl10/list/…` do resolve, so the tool works) | `[hypothesis]` as to where the transition lives |
| 5 | `app.ropeway.fsmv2.player.FireShot` (FSM action inside the Shot state): `onStart` @0x14033fe10 → if `IsStartFire` (+0x38) → `createShell(IsSupport)` @0x141e0d830 (tail-jumps into `requestFire`); `onUpdate` @0x141e3da60 → if the playing clip's `PlayerFireWeaponTrack.Fire` (+0x10) is set → `Equipment.requestFire()` | xrefs: the only callers of `requestFire` are these two plus `survivor.fsmv2.action.SurvivorShootAction.shoot` @0x14037bd50 (the "unknown wrapper 0x14037bd5b" of the 08-29 note — now named; it is the non-player survivor FSM's twin) |
| 6 | `Equipment.requestFire` @0x140aecbd0 | raises bool triggers on the WEAPON's own FSM variables (`Accessor_Wep_common`: `_Fire` @+0x10, also `_Hold` @+0x48 exists) via `Implement.CommonVariablesHub` (+0x140) | disassembly; **live 2026-08-29: calling it unaimed runs and nothing happens** — the weapon FSM is not in a state that takes `_Fire` |
| 7 | weapon FSM Shot node → `Equipment.enableAttack` @0x1402dd2c0 → `Equipment.executeFire` @0x140aaad00 → `Gun.executeFire` @0x14136c630 (ballistics, ammo, recoil). Both Equipment methods have no direct call sites, only function-table pointers in `.data` (0x1471e8230 / 0x147365cb8 …) — invoked by the FSM/delegate machinery | xrefs; chain order `[verified-live 2026-08-29]` |

So the "binding" is at **step 2**, and there are TWO gates in it, not one: the *HOLD input bit* and the
*HOLD state tag* (via `get_EnableAttack`). That is why the 08-29 experiments could not un-gate it:
forcing `Gun.enableFire/enableAttack` (step 7 mirrors) and calling `requestFire` (step 6) were both
downstream of the decision, and hooking `get_EnableAttack` alone (v4.4 list) would still have died on
`isOn(HOLD)` in the same function.

## 2. The lever the game ships: the orderer's own Force bits

`checkOrder(Precede)` starts (0x140db4a16–0x140db4a89) with: **if `(PrecedeBits.Force & order) != 0` →
return true** — before any of the input/tag tests. `Force` is `ActionOrderBits.<Force>` @+0x14 of
`SurvivorActionOrderer.PrecedeBits` (+0xa0). It is set by:

- **`SurvivorActionOrderer.setForcePrecede(bool Flag, uint OrderBits)` @0x140d1bad0** — `Flag=true` ORs the
  bits in, `false` ANDs them out; plain field write, no side effects `[inferred-static]`.
- `setForce(bool, ActionOrder.Precede)` (the typed twin; the dump gives both `setForce` overloads the same
  address 0x1403b7c80, so prefer `setForcePrecede`), and the per-clip `SurvivorForcePrecedeOrdersTrack`
  (the game's own use of the same bits — some clips force orders).
- Nothing in `updatePrecedeBit` clears `Force` (only `Acceptable` +0x18 is zeroed each frame), so it
  latches until cleared `[inferred-static]`.

**So `Condition.ActionOrderer.setForcePrecede(true, 4)` while our trigger is down, `false` on release,
makes the ATTACK order accepted with no aim input and no HOLD tag — no hook at all.** Reachable from
Lua today: `PlayerManager.get_CurrentPlayerCondition()` → field `<ActionOrderer>k__BackingField` (+0x108)
→ `:call("setForcePrecede", true, 4)`. Arcade Controls already used the sibling `setInhibitPetient`
(dossier §"Nothing chambers a round…", the load-bearing misspelling), so the orderer API is proven
callable from Lua `[verified-live, AC era]`.

Two things upstream of `checkOrder` can still say no, and the test must log them:
- `Inhibit` bits (+0x10) — `updatePrecedeOrder` skips an inhibited order before ever calling
  `checkOrder`; the per-clip `SurvivorRejectPrecedeOrdersTrack._Reject` ORs into them. A locomotion clip
  carrying "reject ATTACK" would block the force lever; `setInhibitPrecede(false, 4)` @0x140d1bf20 is the
  counter, but tracks re-apply each frame.
- `enablePrecedeOrder()` (vtable+0x120, PlayerActionOrderer @0x140120630) — if false, no precede order
  is processed that frame.

## 3. Ranked candidates (most upstream first) — PROTOCOL §11 "which step runs last"

| rank | lever | step it changes | verdict |
| --- | --- | --- | --- |
| A | edit the player `.motfsm2` (add a locomotion→Shot transition / retag states) | 4 (data) | the only lever that is upstream of *everything*; asset path unknown, format not decoded — not for tonight |
| **B1** | **`setForcePrecede(true, ATTACK)` on the player's `SurvivorActionOrderer` while RT is down** | 2, at its very first test | **the pick.** Game-shipped override, no hook, two Lua calls. Sits exactly at the decision (step K = 2), so it is a fix, not an annotation. Unknown left: whether step 4's FSM data has a Shot transition from the locomotion states at all |
| B2 | hook `PlayerActionOrderer.checkOrder(Precede)` @0x140db49d0, post-hook returns true when arg==4 and RT down | 2 | same power as B1, one hook instead of a field write; use only if B1 is refused by a reject track or Inhibit |
| C | hook `SurvivorCondition.get_EnableAttack` → true | 3 | **insufficient alone**: `checkOrder` still needs `isOn(HOLD)`, so it needs `InputSystem.setForce(HOLD)` too, which raises the whole stance — that is the 08-27 latch, not idea 7 |
| D | call `Equipment.requestFire()` | 6 | `[disproved 2026-08-29]` unaimed: runs, nothing fires (weapon FSM not listening) |
| E | call `Gun.executeFire(1)` | 7 | `[verified-live 2026-08-29]` ballistics only while aimed, self-refuses unaimed, never any muzzle flash/animation |
| F | the 08-27 micro-latch (`InputSystem.setForce(64,true)` around the shot) | 0 (input) | works `[verified-live 2026-08-27]` but drags the entire stance bundle (doors/items lock, speed cap, aim body) — the fallback if B1 proves the FSM has no Shot path outside HOLD |

## 4. What one launch proves (flat, handgun, unaimed, one script)

Script (own file, e.g. `visceral_fire_force_probe.lua`, NUM-key armed, nothing at boot):
1. On RT/LMB press while `get_IsHold()==false`: `orderer:call("setForcePrecede", true, 4)`; on release `false`.
2. Each frame log: `orderer:get_field("<Precede>k__BackingField")`, `PrecedeBits` fields `Inhibit`(+0x10) /
   `Force`(+0x14) / `Accept`(+0x1c), `get_IsHold`, `get_EnableAttack`, layer 4 motion name, gun bullet count.
3. Observe-only hooks on `fsmv2.player.FireShot.onStart` (@0x14033fe10, in `re2.exe`, hookable) and
   `Equipment.requestFire`.

Read-out, one line each:
- `Precede` never becomes 4 and `Inhibit&4 != 0` → a reject track on the locomotion clip blocks it → try B2
  or `setInhibitPrecede(false,4)` each frame *after* `updatePrecedeBit`.
- `Precede == 4`, `FireShot.onStart` never fires, layer 4 empty → **the FSM has no Shot transition outside
  HOLD** (step 4 is the real wall, data-only) → fallback F, or lever A later.
- `Precede == 4`, `FireShot.onStart` fires, bullets drop, `HG_Hold_Shoot` on layer 4, muzzle flash seen →
  **idea 7 is one field write** and every hold-state lever in §8g.3 becomes optional for firing.
- Bullets drop with no flash → weapon FSM `_Hold` gate (step 6/7) still closed: next lever is
  `Arm.set_CommonVariablesHold(true)` @0x14048ef30 while RT is down (`[hypothesis]`, untested).

## 5. Laser dot vs the aim state (Tefa's "go through the Arcade Controls notes")

- AC's ten-round laser case study (`arcade-controls-re2-vr/modding-notes/case-studies/2026-08-06-laser-sight-drift-investigation.md`)
  is about the dot's *position* (a native arm-IK solve running six times a frame vs a spine fix written
  once); it never found a hold gate for the dot's *visibility*, and its crosshair script has no IsHold read.
- Statically, visibility is `app.ropeway.LaserSightController.lateUpdate` @0x1414709b0 reading its
  `Owner` (`ILaserSightOwner` = the `Gun`): `Gun.get_EnableLaserSight` @0x1413809b0 = `EquipStatus (+0xe0) == 1`
  **and** a laser-sight weapon part is fitted **and** one yes/no from `Gun.get_OwnerInterface()` (vtable
  slot +0x68 on `IGunOwner`, slot not resolved) `[inferred-static]`. `[hypothesis]` that last call is the
  owner's hold state; if so the dot in a non-aim two-hand state wants a post-hook on
  `Gun.get_EnableLaserSight` → true while two-handed. One-launch check: log `get_EnableLaserSight` aimed
  vs not with the JMB Hp3 or a red-dot part fitted.

## 6. Loose ends, for the dossier

- `hikako.PlayerAdaptiveTriggerController.get_IsAttacking` also reads `get_EnableAttack` (DualSense only).
- `Accessor_Pl_action` (the player FSM's variable hub) carries `b_Fire`, `b_CanRapidFire`, `b_HoldUp`,
  `f_FireSlur`; `PlayerUserVariablesUpdater.doSurvivorUserVariablesUpdaterUpdate` @0x1413689b0 writes
  `CanRapidFire` from `Equipment.get_EnableRapidFire` and the stick/pitch floats; who writes `_Fire` was not
  traced.
- Enum values worth keeping: `ActionOrder.Precede`: TURN 1, QUICK_TURN 2, ATTACK 4, RELOAD 8, CHANGE_WEAPON 16,
  SWITCH_SIGHT 32, SWITCH_LIGHT 64, CHANGE_BULLET 128, STEP_UP 256, STEP_DOWN 512, QUICK_TURN_EX 1024.
  `ActionOrder.Petient`: IDLE 1, LIGHT_WHEEL 2, MOVE 4, JOG 8, HOLD 16, HOLD_WHEEL 32, HOLD_WALK 64 — so the
  aim stance itself is the *Petient* HOLD order, and `setForcePetient(true,16)` is the orderer-level twin
  of `InputSystem.setForce(64,true)` (untested, `[hypothesis]`).
- `PlayerDefine.ForceChangeState` (`SurvivorCondition.setStateAll`) has only IDLE/WALK/JACK/EVENT/REACTION/
  EXTERNAL_OVERWRITE/SAME — no HOLD or SHOT, so it is not a way into the Shot state.
- Tools left in the reader scratchpad (not committed): `xrefs.py` (rel32 + RIP-lea + pointer xrefs with
  dump owner names), `disasm2.py` (capstone with call-target names, no early stop), `owner.py`,
  `dumpls.py`, `pakfind.py`. Worth copying into `dev-archive/tools/re-engine/` by the live session.
