# New native plugin, holster step H1 (2026-10-05 late, home PC, Opus)

**Unrun when written.** Built clean `[compile-verified 2026-10-05]`, installed as build **b074** (GOLDEN 2 + two files).

## Why a new plugin

Tefa, 2026-10-05: only new code; the old `visceral_core.dll` (and its RELOADED port branch) was built in the
spine-straightener era. The old code and RELOADED 1.0.1 are read for ideas only. The new code lives in
`dev-archive/native/` (small files, every number in `src/settings.h`).

## What Tefa asked for (the holster layout)

| Spot | Hand + button | Holds |
| --- | --- | --- |
| upper left of the head | left, LG | flashlight |
| left hip | left, LG | ammo pouch (for manual reloads, later) |
| right hip | right, RG | handguns, magnum, every one-handed weapon except Ada's EMF visualizer |
| right shoulder | right, RG | normal long weapons |
| left shoulder | right, RG | special weapons (flamethrower, minigun, rocket launchers, spark shot) and the EMF visualizer |
| left hip | right, RG | sub weapon (knife or grenades) |

Sub weapon rules (for the step that equips it): RG at the left hip puts the knife or grenade in the right hand;
holding RT and releasing it mid-throw lets the grenade go; RG does nothing while a sub weapon is held.
Reload animations: C++, not Lua (Tefa: Lua is janky and glitchy).

**Rule (Tefa): the zones are bound to the HEADSET, never to the character's body** ("so reaching for something
will always be in one spot"). Positions are the headset and controllers in VR tracking space (the real room); side
and forward offsets turn with the headset's yaw only.

## What b074 does

- `lua/visceral_bridge.lua` (fresh; the only Lua): every frame copies the headset pose, both controller positions
  and the grip/trigger/A/B buttons into a shared float array, and fires rumble the plugin asks for. Handed over
  through the mailbox hook on `RagdollControlZoneManager.set_AccessMutex` (the idea that ran live on 2026-10-03).
- `visceral_native.dll` step H1: the six zones; a short buzz when the right hand enters one it uses; a stronger
  one and a log line on a grip press ("would PUT AWAY / TAKE OUT"); every weapon held is logged with its WP number,
  name and the holster it sorts into. **Nothing is equipped or put away yet** (step H2).

## Lines the first run must print (in `re2_framework_log.txt`)

1. `[visceral-native] loaded: step H1 ...`
2. `bridge mailbox hook in`, then `bridge array: elements at +0x..`, then `BRIDGE ATTACHED`
3. `weapon types read from the game: N` and `weapon: WP.... <name> -> <holster>` on every weapon change
4. With the headset on: `status: head ... | left hand ... | right hand ...` every ~10 s, and `ENTERED <zone>` lines

## Unknown / to settle by wearing

- The offsets are first guesses `[hypothesis]`. Whether the zones should turn with the head's yaw (as now) or stay
  facing one way is Tefa's call after feeling it.
- The knife, grenade and EMF visualizer WP numbers: read from the log the first time each is held.
- Weapon names come from RELOADED's data; the GM 79 appears there as both WP1300 and WP4100.

## b075 worn (2026-10-05 ~23:30): every zone buzzes `[verified-live 2026-10-05, n=1]`

The log showed the bridge attached, zone entries, grip presses and weapons: the hand grenade is **WP6200**
`[verified-live]`, confirming the forum ID mapping. The flashlight zone was entered about once a minute during
ordinary play (no grip), so it may sit where the left hand passes naturally; watch it.

## Step H2, b076 (installed, unworn): the holsters ARE the game's shortcut cross

Tefa's redesign: each spot holds whatever is in one slot of the game's own 4-way weapon shortcut, so the player
chooses with the game's own menu. Right = right hip, down = left hip, left = left shoulder, up = right shoulder.
A grenade or knife must be in the cross to be reached. Spots turn left/right with the head, never tilt (Tefa).

- Take out: `survivor.Inventory.equipMainSlot(EquipmentDefine.Shortcut)` (the game's own d-pad call).
- Put away (when the held weapon is that slot's): `unequipEquipedWeapon(WeaponType)`, `Equipment.requestHolster` fallback.
- Slot contents: `Inventory.get_ShortcutSlots()` list, read by direction index `[hypothesis: index = Shortcut value
  Up 0 / Down 1 / Left 2 / Right 3]`; the log prints `shortcut up/down/left/right: <weapon>` to check against the HUD.
- Knife + grenades in the cross: post-hook on `EquipmentDefine.enableShortcut(WeaponType)` answers yes for WP4500,
  4510, 6200, 6300. The first 8 calls are logged with their argument to confirm the hook reads it right.
- Grenade rule for later (Tefa): hold RT, release it while the hand moves = throw; release with a still hand =
  drops at the feet. RG does nothing while a sub weapon is held.
