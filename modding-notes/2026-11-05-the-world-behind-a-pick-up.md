# The world behind a pick-up (2026-11-05, Fable, home PC, flat)

Tefa: *"please do the pick up blur, so if at all possible, game world can be visible on item pick up, just like a normal
inventory screen would"*. Board row 6b, open since 2026-10-10: the inventory shows the live world (b129-b131), the get-item
screen showed a grey-green floor under a black sky even with the item camera skipped (b145).

## What hides the world `[measured 2026-11-05, flat, n=2 pick-ups]`

- A probe (`lua/visceral_pickup_probe.lua`, test only) took a snapshot of every mesh outside the menus and again twelve
  frames into a pick-up: 1 mesh appeared (the item model), 649 stopped drawing: lamps, doors, enemies, the player. Each of
  them still had its own `DrawSelf` true but `Draw` false: an ANCESTOR was off.
- Walking up from one door: its game object's chain was fine; its scene FOLDER `gmk_door` read draw false, and the folder
  above it, **`Environments`** (under `Location_RPD`), had **`DrawSelf` false**. So the game switches whole scene folders off
  with `via.Folder.set_DrawSelf(false)` for the pick-up (16 of them in the hall: Effect, Environments, Event, File, Fsm,
  GimmickEnv, Item, LocationFsm_*, LocationResident_RPD, Scenario, SearchText, VFX_RPD), and the grey-green stage is simply
  the empty scene. The camera stays the player's (b145), the post effect is off (b128).
- `set_DrawSelf(true)` on `Environments` brought the hall back at once, and the game did not switch it off again (the next
  frames kept it on; setting draw on the 649 objects themselves did nothing, as expected, since their own flag was never
  off). `set_Draw` on a folder does not exist (`via.Folder` has `set_DrawSelf`, `set_UpdateSelf`, `set_Standby`, `set_Select`).
- Who calls it was not chased: the inventory's `update` / `lateUpdate` and the detail behaviour's did not change the
  counts, so the switch happens once, elsewhere (the open path), and the game restores everything when the screen closes
  (`Environments`, `Character`, `Enemy`, `Player`, `gmk_door` all true again after Next).

## What was built

`lua/visceral_pickup_world.lua` (deployed by build.sh, build 'the world behind a pick-up' in the builds repo): every two
seconds while no menu is open it notes each scene folder's `DrawSelf`; while the inventory's open mode is GetItem (4) or
GetItemShortcut (5), right before rendering, every folder that was on and is now off is switched on again, once per
frame, with one log line naming them. Tick box in the REFramework menu: "Visceral pick-up world". The game puts its own
state back on close, so nothing is written then.

## Seen `[verified-live 2026-11-05, flat, n=2]`

A Blue Herb pick-up opened by script (`GUIMaster.openInventoryGetItemMode(stock, nil)` with a `StockItem` made by
`sdk.create_instance` + `setStock(4, 0, 0, 1)`; no walking to an item needed): the RPD main hall drawn behind the item,
the item lit as the game lights it; Next, then Esc: the hall as before. Not yet seen in the headset: `GATE: VR USER`.

## Open

- The headset look (the VR layer draws the item view over the world as it does the inventory, so it should match).
- Whether the characters should show too (the game hides `Player` / `Enemy` by their objects' own flags, not a folder;
  they stayed hidden): a question for Tefa if a zombie frozen mid-lunge behind the item looks wrong, or right.
- Which routine calls `set_DrawSelf(false)` (a one-line hook would replace the per-frame check).
