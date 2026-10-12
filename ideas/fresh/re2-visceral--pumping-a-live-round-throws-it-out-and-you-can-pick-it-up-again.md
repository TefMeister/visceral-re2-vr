# Pumping a live round throws it out, and you can pick it up again

Order: 5012
From: the ideas repo, `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-10-13

`[raw]` `[not judged]`

> "is there a way to make the actual rounds fly out, take 1 shell off the ammo count, and then be later possible to pick it up from the floor? no that is too much change i think. something to save for a future idea maybe."

Verbatim record: [`inbox/2026-10-13-re2-live-shells-eject-and-pick-up.md`](../inbox/2026-10-13-re2-live-shells-eject-and-pick-up.md)

Like a real pump gun: working the pump with a live round in the chamber throws that round out, the gun
has one less, and the round lies on the floor where it can be picked up again. For now the shotgun works
the Arcade Controls way (a shell flies out only after a shot; a pump with a live round is just for show).

**What it'd take** (reasoning, nothing checked against the game): the flying shell is easy (the game
already throws spent ones); taking one off the count is a small change; the hard part is a real item on
the floor that the game lets you pick up, which means spawning a pick-up object at runtime.
