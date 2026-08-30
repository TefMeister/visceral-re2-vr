# RE2's weapon-equipped state: the public class surface, and how REFramework changes behavior in it

**Status of every claim here: `[reported]`** — read out of public source code (REFramework's
`FirstPerson.cpp`, praydog), never yet exercised by us in Visceral. Verify each name live via the
Object Explorer / Console before building on it.

## What this is

The Visceral roadmap keeps landing on the same underlying question: *how does the game know a
weapon is out, and where do you intervene to change what the game does while it is?* Our own
record already holds two hard-won pieces — the aim stance is raised by the
`app.ropeway.InputDefine.Kind.HOLD` input bit (latched via `InputSystem:setForce`, see
modding-notes `2026-08-27-no-grip-to-shoot-proven-in-one-evening.md`) and the stance is readable
as `app.ropeway.survivor.SurvivorCondition.get_IsHold`. This topic adds the rest of the publicly
documented surface, from the one first-party codebase that manipulates it heavily: praydog's
REFramework, whose `FirstPerson.cpp` is written specifically for RE2/RE3.

## The equipped-weapon object itself

- The player carries an **`app.ropeway.survivor.Equipment`** component; the currently equipped
  weapon is its **`<EquipWeapon>k__BackingField`** (the compiler-generated backing field of an
  `EquipWeapon` property — reading the field directly skips any property-side effects).
- From that weapon object, `get_GameObject` reaches the weapon's GameObject, and its **type
  decides behavior**: REFramework distinguishes guns from melee by testing the type definition
  against **`implement.Gun`** vs **`implement.Melee`** (the `app.ropeway.implement.*` weapon
  implement classes). So "is a firearm equipped" is one component read plus one type check — no
  inventory walking.
- The gun's muzzle is a plain skeleton joint on the weapon's own transform, named
  **`vfx_muzzle1`** (dossier §4 route 1 — joints, not children or components).

## State flags read alongside it

- `SurvivorCondition.get_IsHold` — aiming stance (we already use this).
- **`SurvivorCondition.get_IsReload`** — reload in progress. New to us as a named flag; directly
  useful for manual-reload work (roadmap 11) and for gating any pose correction during reloads.
- `app.ropeway.gui.GUIMaster` exposes a `GuiState` enum (PAUSE, INVENTORY, …) — the clean "is
  the player actually in gameplay" gate.
- `app.ropeway.camera.CameraControlType` — REFramework checks for the PLAYER camera type before
  applying first-person behavior (a cutscene/ladder gate at the camera level, distinct from our
  holster script's cinematic detection).

## The flagship behavior change: where bullets come from

The single best public example of "change what the game does while a weapon is equipped":

- RE2 normally fires bullets **from the camera** — fine flat, wrong in VR where the gun is in
  your hand. The choice is a per-shell enum, **`app.ropeway.weapon.shell.ShellDefine.FireBulletType`**,
  with (at least) the variants **`Camera`** and **`AlongMuzzle`**.
- While motion controllers are active, REFramework sets the fire type to `AlongMuzzle`, so shots
  originate at the weapon's `vfx_muzzle1` joint; when controllers are off it restores `Camera`.
  The game's own code supports both paths — the mod only flips the selector. This is the same
  "narrow selector the engine already honors" shape as our TargetBankType find (dossier §8).
- Pattern worth internalizing for Visceral: before building a mechanism (projectile redirection,
  custom aim), check whether the game ships a **dormant enum variant** that already does it.

## Other armed-state behavior branches in FirstPerson.cpp

- **Two-hand grip tracking**: a `m_was_gripping_weapon` flag; while gripping and aiming, the left
  hand is solved *relative to the right hand* (grip position from right-hand rotation), not from
  its own controller — the hands become one rigid system. Elbow/shoulder joints are reset toward
  T-pose alignment *before* the IK solve so arms extend properly (same write-before-the-reader
  discipline as our spine fix).
- **Flashlight**: its transform-apply hook returns early (leaves vanilla behavior) when a
  two-handed weapon grip is detected, and otherwise re-targets the light to the muzzle joint —
  i.e. the same hook branches on the armed state.

## Why it matters for Visceral

Roadmap items 1 (aim/walk pose), 6 (grip chording), 8 (3D ammo counter — needs the equipped gun
object), 11 (manual reloads — needs IsReload and the weapon implement), 16 (weapon reshuffle)
all start from "get the equipped weapon and know the stance." This page is that starting kit.

## Sources

- REFramework `FirstPerson.cpp` (praydog) — https://github.com/praydog/REFramework
  (`src/mods/FirstPerson.cpp`; read via the repo's own viewer 2026-08-29. The file is large —
  the read covered roughly its first ~1,750 lines; the hook list at the bottom of the file was
  not fully covered and may hold more).
- Our own modding-notes: `2026-08-27-no-grip-to-shoot-proven-in-one-evening.md`,
  `2026-08-27-spine-twist-mechanism-study.md` (the in-house half of this picture).

## Concrete next step

In a modding session, Console-verify the chain
`PlayerManager → CurrentPlayer → Equipment component → <EquipWeapon>k__BackingField → implement type`
and dump `ShellDefine.FireBulletType`'s full enum values. Each verified name upgrades from
`[reported]` to `[verified-live]` in the dossier.
