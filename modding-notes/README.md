# Visceral — RE2 VR: modding notes

The readable field notes and progress ledger for **Visceral**, a VR
interaction overhaul for Resident Evil 2 (2019) built on praydog's
[REFramework](https://github.com/praydog/REFramework): one entry per working
session or milestone — what was tried, what worked, what failed, and the
lesson worth keeping. Written for anyone who wants to do the same kind of
work, not just for us.

The notes from the shipped predecessor (case studies included) live in
[arcade-controls-re2-vr-modding-notes](https://github.com/TefMeister/arcade-controls-re2-vr/tree/main/modding-notes)
(frozen, study material) — Visceral builds on that knowledge, not that code.

## The folders for Visceral — RE2 VR

Everything for this project lives in six folders, each with one job — so
you always know where to look. You are in **`modding-notes/`**.

| Folder | What lives here |
| --- | --- |
| [`mod/`](../mod/) | The mod itself — releases only. |
| [`dev-archive/`](../dev-archive/) | Full development history — snapshots, probes, dead ends, raw recon. |
| **`modding-notes/`** ← you are here | Readable field notes / progress ledger. |
| [staging/visceral-re2-vr](https://github.com/TefMeister/staging/tree/main/visceral-re2-vr) 🔒 | **Private** — unverified WIP builds, cross-machine handoff. |
| [`engine-research/`](../engine-research/) | Distilled engine reference (dossier) + reusable VR RE playbook. |
| [`external-research/`](../external-research/) | Ongoing public-research leads, gathered separately from hands-on modding work. |

## Setting up the same environment

[`TOOLCHAIN.md`](TOOLCHAIN.md) lists exactly which versions of REFramework and
the optional DLSS upscaler stack this project is built against, where to
download each one, and how to install them by hand. It also explains why the
newest REFramework build is not always the one you want.

## Credits & policy

See [`CREDITS.md`](CREDITS.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
Non-commercial fan project; requires an owned copy of the game; no original
game files are stored here.
