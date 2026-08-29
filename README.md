# Visceral — RE2 VR

A VR interaction overhaul for **Resident Evil 2 (2019)** in VR (praydog's
[REFramework](https://github.com/praydog/REFramework)) — motion-controller
weapon handling, manual reloads, hand-pose slide racking and pump action,
holsters, body IK and posture, and more: the game's guns and body handled
with your own hands.

> **Status: v0.1.0 released — the first slice (body & posture).** This build fixes
> how your character's body behaves in VR with a weapon out: torso straightening,
> feet planted on the ground while aiming, and cutscene/grab safety. Weapon
> handling, reloads and holsters are still to come. Full list in
> [`CHANGELOG.md`](CHANGELOG.md).

## What's in this release (0.1.0)

- **Torso straightening** — removes the twisted weapon-hold posture; the upper
  body faces forward while normal gait/breathing motion is kept.
- **Feet on the ground while aiming** — kills the ~10 cm body float of the VR aim
  pose (feet plant, knees bend); your hands, gun and where bullets land are
  untouched. Tunable (default 0.175 m).
- **Faster aim-walk** — while aiming you move at a normal pace instead of the
  vanilla aim-crawl, done by amplifying the game's own movement so walls still stop
  you and it stays analog. Aiming only; tunable (default 1.3×).
- **Cutscene & grab safety** — the body adjustments switch off during cutscenes,
  camera events, and the enemy-grab third-person camera.

Each has an in-game panel in the REFramework menu (toggles + sliders, usable with
the VR controller pointer). Tested on Leon and Claire.

## Install

Visceral is a set of Lua scripts that run **on top of** praydog's REFramework — it
does **not** include REFramework or any game files.

1. Install **[REFramework](https://github.com/praydog/REFramework)** for RE2 with
   its VR support (its `dinput8.dll` in the game folder, plus the VR runtime dll —
   `openxr_loader.dll` or `openvr_api.dll` — for your headset). Confirm VR works
   first.
2. Download this release and copy its **`reframework/`** folder into your RE2 game
   directory, merging with the one REFramework created. The four scripts land in
   `reframework/autorun/`.
3. Launch in VR and open the REFramework menu to find the **Visceral** panels; the
   features are on by default.

To uninstall, delete the `visceral_*.lua` files from `reframework/autorun/`.

## Where this comes from

Visceral is the successor to our shipped mod
[ARCADE CONTROLS for RE2 VR](https://www.nexusmods.com/residentevil22019/mods/2640)
(final release v1.5.0), which grew out of Andyalpa's RE2VRMODRELOADED. That
lineage taught us everything — and it is exactly why this rebuild exists:
**every line of Visceral is written from scratch.** It is not a fork of
RE2VRMODRELOADED, and it does not reuse code from our own Arcade Controls
either. Prior art — including our own old mod — is studied and credited, never
copied; the Arcade Controls repositories stay frozen as study material. What
carries over is the knowledge: the engine dossier, the field notes, and the
lessons written down along the way.

## What you will need

- Your own legitimate copy of **Resident Evil 2 (2019)** (this mod contains
  **no** game files).
- [REFramework](https://github.com/praydog/REFramework) (the RE2 build, with
  its VR support).
- A PC VR headset via SteamVR/OpenXR (Quest over Link/Virtual Desktop works).

## The six repositories for Visceral — RE2 VR

Everything for this project lives in six repositories, each with one job — so
you always know where to look. You are in **visceral-re2-vr-mod**.

| Repository | What lives here |
| --- | --- |
| **visceral-re2-vr-mod** ← you are here | The mod itself — releases only. |
| [visceral-re2-vr-dev-archive](https://github.com/TefMeister/visceral-re2-vr-dev-archive) | Full development history — snapshots, probes, dead ends, raw recon. |
| [visceral-re2-vr-modding-notes](https://github.com/TefMeister/visceral-re2-vr-modding-notes) | Readable field notes / progress ledger. |
| [visceral-re2-vr-staging](https://github.com/TefMeister/visceral-re2-vr-staging) 🔒 | **Private** — unverified WIP builds, cross-machine handoff. |
| [visceral-re2-vr-engine-research](https://github.com/TefMeister/visceral-re2-vr-engine-research) | Distilled engine reference (dossier) + reusable VR RE playbook. |
| [visceral-re2-vr-external-research](https://github.com/TefMeister/visceral-re2-vr-external-research) | Ongoing public-research leads, gathered separately from hands-on modding work. |

## Credits, scope, and legality

Non-commercial fan project; requires an owned copy; redistributes no original
assets. We credit everyone whose work this builds on — see
[`CREDITS.md`](CREDITS.md) — and we honour correction/removal requests from
rights holders promptly.

## Contributing & policy

See [CONTRIBUTING.md](CONTRIBUTING.md) — how we credit and link sources, our
**study-everything-public but write-our-own-code** rule (we copy no one else's
source code or files, any license or price), the terms for reusing our work
(free, with credit), and how to request a correction or removal.
