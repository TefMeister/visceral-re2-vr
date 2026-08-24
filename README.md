# Visceral — RE2 VR: modding notes

The readable field notes and progress ledger for **Visceral**, a VR
interaction overhaul for Resident Evil 2 (2019) built on praydog's
[REFramework](https://github.com/praydog/REFramework): one entry per working
session or milestone — what was tried, what worked, what failed, and the
lesson worth keeping. Written for anyone who wants to do the same kind of
work, not just for us.

The notes from the shipped predecessor (case studies included) live in
[arcade-controls-re2-vr-modding-notes](https://github.com/TefMeister/arcade-controls-re2-vr-modding-notes)
(frozen, study material) — Visceral builds on that knowledge, not that code.

## The five repositories for Visceral — RE2 VR

Everything for this project lives in five repositories, each with one job — so
you always know where to look. You are in **visceral-re2-vr-modding-notes**.

| Repository | What lives here |
| --- | --- |
| [visceral-re2-vr-mod](https://github.com/TefMeister/visceral-re2-vr-mod) | The mod itself — releases only. |
| [visceral-re2-vr-dev-archive](https://github.com/TefMeister/visceral-re2-vr-dev-archive) | Full development history — snapshots, probes, dead ends, raw recon. |
| **visceral-re2-vr-modding-notes** ← you are here | Readable field notes / progress ledger. |
| [visceral-re2-vr-staging](https://github.com/TefMeister/visceral-re2-vr-staging) 🔒 | **Private** — unverified WIP builds, cross-machine handoff. |
| [visceral-re2-vr-engine-research](https://github.com/TefMeister/visceral-re2-vr-engine-research) | Distilled engine reference (dossier) + reusable VR RE playbook. |

## Setting up the same environment

[`TOOLCHAIN.md`](TOOLCHAIN.md) lists exactly which versions of REFramework and
the optional DLSS upscaler stack this project is built against, where to
download each one, and how to install them by hand. It also explains why the
newest REFramework build is not always the one you want.

## Credits & policy

See [`CREDITS.md`](CREDITS.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
Non-commercial fan project; requires an owned copy of the game; no original
game files are stored here.
