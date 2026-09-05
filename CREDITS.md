# Credits

This project would not exist without other people's work, tools, and direct
inspiration. Listed regardless of how large or small the contribution — even a
single technique, a document, or something used purely for testing counts.

- **[praydog](https://github.com/praydog)** —
  [REFramework](https://github.com/praydog/REFramework): the mod framework,
  its VR support for Resident Evil 2, the FirstPerson mode, and the Lua
  scripting API this mod is built on. praydog's sources are studied as
  technique references, never copied.
- **[Andyalpa](https://www.nexusmods.com/residentevil22019/users/Andyalpa)** —
  RE2VRMODRELOADED, the VR interaction layer that made RE2 VR playable with
  motion controllers and the direct ancestor of this work: our earlier mod,
  ARCADE CONTROLS for RE2 VR, began as a fork of it. Visceral is written from
  scratch, but his mod remains the prior art it is measured against.
- **[cursey](https://github.com/cursey)** — the
  [REFramework Book](https://cursey.github.io/reframework-book/), the Lua API
  documentation used to establish what scripting can and cannot do.
- **[xJustAdam](https://www.nexusmods.com/residentevil22019/mods/2516)** and
  **[Angeltitilxyz](https://www.nexusmods.com/residentevil22019/mods/2529)** —
  *Jill's Animations for Claire* and its Ray Tracing port. Studied as specimens,
  and they corrected a mistake of ours: we had recorded weapon-animation
  swapping as impossible, having only ever tried to redirect the game's bank
  resolution at runtime. Their mod leaves resolution alone and replaces the
  contents of the bank instead. The two variants side by side also told us that
  the `x64`/`stm` split marks the pre-RT and RT builds. The technique is what we
  learned from them; none of their files or animation data are used or
  redistributed.
- **[Shulian01 (Dragonleo)](https://www.nexusmods.com/residentevil22019/mods/2240)** —
  *Claire A*, studied as a specimen for how RE2's placed scene data — enemy,
  item and environment placement — is delivered as loose files. Structure
  studied only; nothing of his is used or redistributed.
- **The REFramework VR community** — public documentation of REFramework's VR
  behaviour, limitations, and techniques.
- **Capcom** — Resident Evil 2 (2019) and the RE Engine. This is a
  non-commercial fan project; it requires a legitimately owned copy of the
  game and redistributes no original game files or assets.

Our own predecessor,
[ARCADE CONTROLS for RE2 VR](https://github.com/TefMeister/arcade-controls-re2-vr/tree/main/mod)
(final release v1.5.0), is frozen and kept as study material — Visceral reuses
its knowledge, not its code.

If you believe your work is used here and is not credited, or is credited
incorrectly, please open an issue — it will be fixed promptly and without
argument.
