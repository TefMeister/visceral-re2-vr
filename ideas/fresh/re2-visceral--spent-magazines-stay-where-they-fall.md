# Spent magazines stay where they fall

Order: 5002
From: the ideas repo, `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-10-14

`[raw]` `[not judged]`

> "spent clips actually stay on the surface they fall on."

Verbatim record: [`inbox/2026-10-14-re2-spent-magazines-stay-where-they-fall.md`](../inbox/2026-10-14-re2-spent-magazines-stay-where-they-fall.md)

Today the magazine the manual reload drops (2026-10-10, bundle 1) slides out, falls about 1.2 m and is then
hidden. The idea: it lands on whatever is under it (floor, table, stairs) and lies there, like the spent
shells do.

**What it'd take** (reasoning, nothing checked against the game): the dropped magazine is our own object
moved by the plugin, not a game item, so "landing" means a downward ray from the magazine to the first
surface under it and stopping there (the game has ray casts the plugin can call), then leaving it lying
with the right tilt. A limit on how many stay (the oldest vanishes) keeps it cheap. Related to "Pumping a
live round throws it out" below, which wants a real pick-up item; this one is only the look.
