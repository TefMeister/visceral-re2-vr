# Third person for ladders, switches and pushing furniture — the plan B

Order: 5002
From: mod-ideas `games/re2-visceral.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/re2-visceral.md>), copied 2026-09-23

`[raw]` · `[looks doable]` — ⚠️ **NOT CHECKED against the game.** The verdict rests on what the mod
already does elsewhere, not on a test.

> "if ladder climbing does not submit to our modding, and keeps on turning the player head while
> switching switches, climbing ladders or pushing heavy cupboards, maybe game can switch to 3rd person
> for these encounters, the same as being grabbed by zombies"

Verbatim record: [`inbox/2026-09-13-re2-third-person-for-ladders-switches-and-pushing.md`](../inbox/2026-09-13-re2-third-person-for-ladders-switches-and-pushing.md)

**Why it's filed here:** the problem is the game turning your head for you in the headset — a comfort
issue that only exists in VR.

**How it relates to the board:** this is the **fallback** for an item already being built — roadmap
v1 item 2, "ladder-climb camera 100%" (the older arcade-controls mod also had a ladder body-turn fix,
and a "snap-free ladder camera hold" idea). If that works, this idea is not needed. If it does not,
this is the answer. It does not become its own board row unless Tefa settles it.

**What it'd take:**
- ✅ **The switch itself already happens.** The game (through the VR framework) already drops to third
  person when a zombie grabs you, and the mod already *detects* that moment — the head hider built on
  2026-09-06 watches for "grabbed" and "first-person is off". `[inferred-static]`
- ⚙️ **What is not known — my job with the game running, not a question for you:** whether the mod can *force* that same third-person view on for a moment, and
  how to tell when a ladder, a switch or a furniture push has started and ended. Each of the three is
  probably its own "the player is doing X" state in the game, to be found once with the game running.
- **Rough cost:** a flat-screen session to find the three "busy doing X" signals and try forcing the
  view; then one headset check that the cut in and out is not worse than the head-turning it replaces.

⚠️ **One thing to watch:** a hard cut to third person and back is itself a comfort hit. A short fade
to black on each cut is the usual cure, and cheap.

---

Chosen by Tefa: 2026-09-24
