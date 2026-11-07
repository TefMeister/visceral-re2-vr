# /gr 2026-11-07: a second public RE2 VR overhaul exists (h1nt36r2's RE2IVR)

From: `/gr` estate sweep, CHECK-IN pass. For the modding lane; nothing here was tested.

- **What:** h1nt36r2's *Resident-Evil-2-Remake-VR* (MIT, REFramework + SteamVR/OpenVR, 0.8.9 of 2026-10-26):
  hand reload, belt zones that draw whatever the game's own quick slots hold, grenade pin-and-throw, flat overlay
  belt `[reported 2026-11-07, the author's README]`. Topic:
  `external-research/topics/2026-11-07-a-second-public-re2-vr-overhaul-re2ivr.md`.
- **For the board:** it overlaps Visceral RE2's holster and reload work. Whether that changes anything (watch it,
  borrow ideas, carry on) is Tefa's decision; this lane only reports it.
- **One fact worth keeping:** its author cites REFramework issue #400 for the player's body vanishing in the second
  eye with ray-traced costumes, and works around it with AFR `[reported]`. If that look ever shows up in our wear,
  it is a known REFramework fault, not ours `[hypothesis]`.
- **REFramework:** master moved again (#1885, nightly 01438, 2026-11-04: hook callbacks wrapped so a Lua error
  cannot cross into the game, the first float argument's register kept, hook add/remove made thread-safe)
  `[reported]`; pd-upscaler, which our build uses, is unchanged since 2026-10-11 `[measured 2026-11-07, GitHub API]`.
