# Every menu over the live game world: no tint, no camera jump

Order: 5014
From: the ideas repo, `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-10-10

`[raw]` · `[not judged]` — ⚠️ *nothing checked against the game*

> "IN RE2, this is how the inventory screen looks like when i need to use an item, in this case a key from my inventory. this is our goal, to make all menus and inventorys, item pick up screen, map screen, item box, to look like this seamlessly. no black or blue tint in the background, just game world. the thing with this screen is, it jumps to a pre-selected angled image of the door, so that jump has to be removed too, and just the inventory popping up wherever the player was looking while pressing A on a door to get to this screen that is on the screenshot."

Verbatim record: [`inbox/2026-10-10b-re2-menus-over-the-game-world.md`](../inbox/2026-10-10b-re2-menus-over-the-game-world.md) (with Tefa's screenshot)

The game already does this once: the "use an item" screen at a locked door shows the inventory over
the real game world, with no dark or blue cover. The goal is for **every** menu to look like that:
inventory, item pick-up, map, item box. Two parts:
- **No tint.** The black or blue cover behind each menu goes; you just see the world.
- **No camera jump.** At a door, the view jumps to a set angle of the door. That jump goes; the menu
  simply appears where you were already looking when you pressed A.

**What it'd take** (reasoning, nothing looked at yet): the tint is probably a separate layer each menu
switches on, which may be easy to switch off; the door's camera jump is a set camera the game cuts to,
which has to be found and skipped. In VR the jump matters more than on a flat screen, because a sudden
camera cut is uncomfortable in a headset.
🔗 Reads with [One background everywhere](#one-background-everywhere-the-last-save-scene) just above,
which is about the title menus.

---

_Other categories appear as ideas arrive. Nothing is missing; they just haven't been needed yet._

**To add one:** `[re2] your idea` — anywhere, any time.

🔗 Ideas for **all four** Resident Evil games at once — they share an engine and a modding framework — go on [`resident-evil-all.md`](resident-evil-all.md) (`[re games] your idea`).
