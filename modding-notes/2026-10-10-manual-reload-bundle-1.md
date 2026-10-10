# Manual magazine reload, bundle 1 of the RELOADED port (2026-10-10, home PC, /ms, Opus)

**Build:** `manual reloads/v0.2.0-b132`, installed, unworn. Tefa's plan: build, then check every weapon in VR. Bundled
in three so one early fault cannot hide under the rest: (1) game reload off + sounds + dry fire + magazine reloads,
(2) slide rack + pump, (3) revolvers, single shells, grenade launcher, flamethrower canister.

Spec: Andyalpa's RE2VRMODRELOADED 1.0.1 `ext_1` (magazine state machine) and `reload.lua` B.5 (suppression), read
from `D:/Visceral RE2 archive (old, 2026-10-04)/.../RELOADED-1.0.1-extracted/` and rewritten in C++; his per-weapon
numbers are data (`src/reload_data.h`), his sound pack is installed as data (`reframework/data/custom_sfx/`, not in
this repo). Credit: Andyalpa. Everything below is `[compile-verified 2026-10-10]` and nothing has run.

## The guns it covers

Matilda, M19, JMB Hp3, Glock 17, MUP, Broom Hc, MQ 11, LE 5, Lightning Hawk, Samurai Edge. Every other gun keeps the
game's own reload until its bundle.

## How it plays (headset only; without the headset the game reloads as always)

| step | what happens | log line |
| --- | --- | --- |
| right B | magazine slides out 0.19 s to its exit point, falls 1.2 m in 0.5 s, disappears; `mag_drop`, `mag_floor` sounds | `reload: ... seated -> sliding out (right B)` |
| trigger with it out | no shot, `dry_fire` click once per pull | `block: shot stopped, the magazine is out` |
| left grip at the left-hip pouch spot | a magazine appears in the left hand (`mag_grab`, buzz). No ammo at all: a long buzz | `reload: ... out -> in the left hand` |
| bring it to the magwell | within max(his distance, 6 cm) of the exit point it slides in (`mag_insert`) | `reload: insert at X m from the magwell` |
| inserted | the gun is topped up from the ammo carried: `Inventory.reloadMainSlot(min(space, carried))`, fallback `Equipment.executeReload` | `reload: WPxxxx topped up by ...: loaded a -> b (capacity c), carried d -> e` |
| left grip let go with it in hand | it falls; take another | `... in the left hand -> dropped from the hand` |
| empty but seated gun | `dry_fire` click | — |

The rounds in a dropped magazine never leave the gun's count (the gun only refuses to fire while it is out), so a swap
or a save mid-reload loses nothing: the magazine is simply put back (`magazine put back as it was`).

## What keeps the game's reload off (`src/reload_block.cpp`), only for those guns, never in a menu

RELOAD input bit cleared at UpdateBehavior pre; `Equipment.requestReload`, both `Equipment.executeReload`,
`Gun.executeReload`, `Inventory.reloadMainSlot` skipped (our own top-up passes inside `Commit`); `Equipment.requestFire`
skipped while the magazine is out; HUD `getMainWeaponRemainingBullet` reads 0 while out. While a reload is under way
the SUPPORT_HOLD bit is cleared and `suppress.cpp` does not force it, so the left grip takes a magazine instead of
readying the knife or grenade. Combining ammo with a gun in the inventory still reloads it the game's way.

## Ours, not his

- Insert distance floored at 6 cm (`cfg::RELOAD_DOCK_MIN_M`): his are 2-10 cm, tuned on his hands; first test.
- The pouch is the left-hip spot holster.cpp already had ("ammo pouch", does nothing yet), headset-relative like our
  holsters, not his hip-joint anchor.
- Not yet: his finger pose for the magazine hand, the slide rack after an empty reload (bundle 2), calibration.
- `Gun.endChamberClear` does not exist in this build (type database), so only `executeEndReload` is called.

## What would prove each part (Tefa, headset, any magazine gun)

1. B drops the magazine and it is gone from the gun; the trigger clicks, no shot; the HUD shows 0.
2. Left grip at the left hip: a magazine in the hand, in a sensible grip. Let go: it falls.
3. To the magwell: it goes in; the next shot fires; the HUD count is full (or what was carried).
4. The game's own reload never plays on these guns; on a shotgun or revolver it still does.
5. Sounds play at a sensible volume.
