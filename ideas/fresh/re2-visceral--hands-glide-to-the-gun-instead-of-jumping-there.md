# Hands glide to the gun instead of jumping there

Order: 5008
From: the ideas repo, `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-10-18

`[raw]` · `[looks doable]` — *reasoning only, nothing checked against the game*

> "be able to make hands animate - for example not teleport to the gun when pressing LG in the activation zone,
> but move smoothly from motion controller position to the gun handle or slider"

Verbatim record: [`inbox/2026-10-16-re2-hands-glide-to-the-gun.md`](../inbox/2026-10-16-re2-hands-glide-to-the-gun.md)

Today pressing LG near the gun snaps the left hand onto the grip or slide in one frame. The idea: the hand
travels there over a short moment, and back again on release. Came up while designing the stand-alone hand
poser (2026-10-16), which could carry the glide time as one of its sliders.

**What it'd take:** the dock already knows both ends (where the controller is, where the grip is), so the glide
is a blend between the two over a set time, eased in and out. The finger pose would blend the same way.

---
