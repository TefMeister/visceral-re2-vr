# A second public RE2 VR overhaul appeared in late October: h1nt36r2's "RE2IVR" (MIT, 0.8.9)

**Status:** 🆕 new · **Priority:** medium — prior art for the belt, reload and grenade work, and a project that
overlaps ours (the "watch it, don't rebuild it" rule: Tefa's call, not this lane's).

Read online from the repository's README and its release list (GitHub viewer and API) on 2026-11-07. Nothing was
downloaded. Everything below is `[reported 2026-11-07]` from the author's own text unless tagged otherwise.

## What it is

- **h1nt36r2, *Resident-Evil-2-Remake-VR*** ("RE2 VR / RE2IVR"), MIT licence, created 2026-10-24, last push
  2026-10-26, one star `[measured 2026-11-07, GitHub API]`. Written in Russian with an English summary.
- Built on praydog's REFramework, aimed at Quest 3 through **SteamVR / OpenVR**; a one-click `Install.cmd` that
  fetches REFramework itself, `Play.cmd` that starts a Windows helper (input and belt overlay) and then the game.
- Pre-releases 0.8.7, 0.8.8, 0.8.9 on 2026-10-25/26. The author says repeatedly that headset testing is still owed
  for most features.

## What it does (author's list)

- Right grip holds the gun (let go = put away); belt zones at the right hip and behind the right shoulder that draw
  whatever the GAME's own quick slots hold (a slot can be assigned from the game's inventory with L3 + right stick).
- Physical reload, on by default since 0.8.8: right grip holds the gun, B opens it, left grip at the ammo takes a
  magazine, insert from below, then grab, pull and release the slide; also cylinder, pump and single-shell loading.
  Real ammo is spent from the game's inventory.
- Grenades: right grip at the pouch, right trigger pulls the pin, the throw follows the hand's speed (replaces
  REFramework's own grenade script so two throws cannot happen).
- The belt is flat OpenVR overlay images placed in space, not game models.
- Ada's EMF scanner keeps REFramework's stock control.
- **Ray-traced costumes:** it switches REFramework to AFR as an experimental workaround for the body vanishing in
  the second eye, citing REFramework issue #400 as the known cause (`Restore-Stereo.cmd` undoes it).

## Why it matters here

1. It is the second public REFramework add-on doing hand reload and holsters for RE2 (the first is Andyalpa's
   pack, topic 2026-10-11). Ours stays different in kind (native C++, the game's own finger movements, guns
   built without hands) `[inferred-static]`, but players will compare.
2. Its belt reads the game's quick slots instead of a fixed gun per spot: a cheap answer if our holsters ever
   need to follow what the player owns `[hypothesis]`.
3. The RT-costume right-eye body loss is a known REFramework issue (#400), not something to hunt in our code if it
   ever shows up `[reported]`.

## Sources

- https://github.com/h1nt36r2/Resident-Evil-2-Remake-VR (README, releases v0.8.7 to v0.8.9)
- https://github.com/praydog/REFramework/issues/400 (as cited by the README; not re-read this pass)
