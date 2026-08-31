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
- **The REFramework VR community** — public documentation of REFramework's VR
  behaviour, limitations, and techniques.
- **[PureDark](https://www.nexusmods.com/residentevil22019/mods/2069)** — the
  RE Engine temporal upscaler plugin (`PDPerfPlugin.dll` /
  UpscalerBasePlugin) that provides the actual DLSS / FSR2 / XeSS
  implementation REFramework's upscaler UI drives. Used as a tool during
  development; no part of it is included or modified here.
- **[gmankab](https://github.com/gmankab/reframework-pd-upscaler-build)** —
  maintaining the `pd-upscaler` REFramework fork build that exposes that
  upscaler, which is the build this project is developed against.
- **[beeradmoore](https://github.com/beeradmoore/dlss-swapper)** — DLSS
  Swapper, used to obtain and update the DLSS library for testing.
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
