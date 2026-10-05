# Archived native code (tried, did not work; kept for reference)

| File | Tried in | Why it is out |
| --- | --- | --- |
| `subweapon.cpp/.h` | b078, 2026-10-06 | Latched the game's SUPPORT_HOLD input to ready the sub weapon and cleared HOLD/SUPPORT_HOLD bits every frame. BROKE THE GAME: menus dead, play/inventory flicker, LG still readied the grenade. Replaced by putting the sub weapon into the bottom shortcut slot (shortcut.cpp, b079). |
| `shortcut-b080.cpp` | b079-b080, 2026-10-06 | Put the equipped sub weapon into the bottom of the cross with `Inventory.setShortcutSlot(index, Down)` (it showed there), but `equipMainSlot(Down)` refuses a grenade ("game said no"), and clearing it on unequip used `InventoryManager.setShortcutWeaponSlotIndex(DOWN, -1)` (hypothesis). |
| `subhold.cpp/.h` | b080, 2026-10-06 | Left hip latched SUPPORT_HOLD (only "in control") and swapped RT/RG bits at UpdateHID post. BROKE MENUS AGAIN: the inventory and menus kept scrolling DOWN (Tefa suspects the bottom-slot / d-pad-down link); RG at the hip did not draw it; LG still did. Tefa then rolled the whole sub-weapon idea back to b077. |
