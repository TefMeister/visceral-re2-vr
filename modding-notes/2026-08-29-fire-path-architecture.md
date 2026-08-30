# The RE2 fire path, mapped end to end (2026-08-29, home PC — probe v4.x + static analysis)

One afternoon of probe rounds (v4 → v4.4) plus ghidrust decompilation of `re2.exe`
answered the design-spec question "who consumes ATTACK and what gates it on the
stance?" ([2026-08-29-always-ready-design-spec.md], [2026-08-29-doors-items-test-result.md]).

## The chain (bottom-up, all verified live)

1. **`app.ropeway.implement.Gun.executeFire(int arg)`** — the ballistic core.
   Spends ammo, spawns the bullet via the ShellGenerator
   (`app.ropeway.weapon.generator.BulletDefaultExGenerator` on the handgun),
   applies camera recoil. Callable out-of-band from Lua: while aimed it works
   (ammo drains, camera kicks) but the muzzle flash only plays on the first
   call and the gun never animates — **visuals do NOT live here**. Unaimed it
   self-refuses. Real shots always pass `arg=1`.
2. **`app.ropeway.survivor.Equipment.executeFire(2 params)`** — wraps 1.
3. **`app.ropeway.survivor.Equipment.requestFire()`** — 0 params, the "fire
   requested" doorbell. Decompiled (`re2.exe` VA 0x140aecbd0): it only RAISES
   request triggers (null-check chains ending in set-trigger-true helper
   calls, weapon-type special cases at this+0x54). Raised triggers are only
   consumed by the aim-state machinery later in the frame — calling it
   unaimed executes fine and nothing happens (probe v4.4, witnessed).
4. **The wrapper at VA 0x14037bd5b (name unknown)** — the only direct caller
   of requestFire (xref at 0x14037be30). Disassembly shows it is ONE managed
   line: `this._field28._field128 /* Equipment */ .requestFire()` — every
   other instruction is compiler-inserted null-check + throw-helper
   (FUN_141f7a0d0 with ids 0x38/0x3c = throw, not game logic) and vmctx
   housekeeping ([rcx+0x50]->0x18 checks = runtime exception/safepoint, not a
   stance gate).
5. **Above the wrapper: indirect invocation.** Static xrefs to the wrapper's
   (suspected) entry: none. Working conclusion: it is an action/FSM callback
   invoked by pointer — **the fire action lives inside the aim (HOLD) state
   and simply does not exist outside it.** There is no single "is he aiming"
   if-statement to delete.

## Gate mirrors (all red herrings for un-gating)

- `Gun.enableFire` / `Gun.enableAttack` flip false→true with aim, but the
  game never calls them on a trigger press — they mirror the decision, they
  don't make it. Forcing them true (v4.3 hook override) changed nothing.
- `Gun.get_EnableExecuteFire` is true aimed AND unaimed — not a gate at all.
- `isPossibleFireFromMuzzle` tracks recoil/cooldown, drops false briefly
  after each shot.
- ATTACK input dies upstream of the wrapper on unaimed presses (0 candidate
  calls in the press window across 61 hooked methods).

## The address bridge (reusable technique, big lesson)

REFramework's log prints every hooked method's **runtime VA**
(`[HookManager] Adding hook for 'name' @ 0x...`; the block that also shows
`Attempting to hook <real>-><trampoline>` carries the real body address —
the evenly-spaced `0x7ff860a80xxx` ones are thunks, ~0x18 apart).
`(Get-Process re2).MainModule.BaseAddress` (PowerShell, works without debug
rights) gives the module base → `RVA = runtimeVA − base` →
`staticVA = 0x140000000 + RVA` feeds ghidrust against the on-disk exe.
Denuvo is present (`.bind` section) but `.text` is plaintext — decompiles
fine. Key RVAs this build: Equipment.requestFire **0xAECBD0**,
Equipment.enableAttack 0x2DD2C0, Equipment.executeFire 0xAAAD00,
SurvivorCondition.get_EnableAttack 0x11930C0, wrapper 0x37BD5B.

**Open follow-up:** REFramework → DeveloperTools → ObjectExplorer → "Dump
SDK" writes a full name↔address map (il2cpp_dump.json) — one click in-game,
names every unknown function in this lane forever.

## What this means for the always-ready design

Un-gating fire from normal locomotion = grafting an FSM action across
states, the known-dangerous lane (IsJog freeze). The honest engineering
choice is now between:

- **A. Micro-latch (favored, proven tech):** on ATTACK press while unaimed,
  latch HOLD (`InputSystem.setForce`, 0–4 ms to stance) for just the shot,
  then release. The REAL fire action runs — full visuals, sound, ballistics.
  Spec requirement 4 (no vanilla aim body animation) then becomes its own
  masking problem for that instant, which the roadmap owed us anyway
  (spine-twist study is the prior art).
- **B. FSM surgery:** invoke/enable the fire action from the locomotion
  state. Only if A's animation flash proves unacceptable in play.

## Next

Confirm the wrapper's true entry + indirect invocation (in flight at session
end), get the SDK dump for names, then prototype the micro-latch: measure
how short the latch can be while still firing (latch → wait IsHold → brief
ATTACK force? → unlatch), and what the body visibly does during it.
