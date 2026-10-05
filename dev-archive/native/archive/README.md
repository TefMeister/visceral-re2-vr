# Archived native code (tried, did not work; kept for reference)

| File | Tried in | Why it is out |
| --- | --- | --- |
| `subweapon.cpp/.h` | b078, 2026-10-06 | Latched the game's SUPPORT_HOLD input to ready the sub weapon and cleared HOLD/SUPPORT_HOLD bits every frame. BROKE THE GAME: menus dead, play/inventory flicker, LG still readied the grenade. Replaced by putting the sub weapon into the bottom shortcut slot (shortcut.cpp, b079). |
