# Visceral — RE2 VR: engine research

The distilled engine reference for **Visceral**, a VR interaction overhaul for
Resident Evil 2 (2019) built on praydog's
[REFramework](https://github.com/praydog/REFramework):

- [`ENGINE-DOSSIER.md`](ENGINE-DOSSIER.md) — the consolidated current truth
  about RE2's engine as seen through REFramework: the RE Engine reflection
  model (`via.*` / `app.ropeway.*` / TDB), hooks, cameras, IK, and the
  hard-won gotchas. Carried forward from our Arcade Controls project — the
  same game and engine, our own document.
- [`PLAYBOOK.md`](PLAYBOOK.md) — the engine-agnostic flat-to-VR playbook
  shared by all our projects (North Star: the game in a headset with head
  tracking).
- [`EXTERNAL-RESOURCES.md`](EXTERNAL-RESOURCES.md) — public references used,
  with links and property status.
- [`templates/`](templates/) — the per-engine research template.

Blow-by-blow history lives in the `-dev-archive` and `-modding-notes`
repositories; the predecessor's frozen research is at
[arcade-controls-re2-vr-engine-research](https://github.com/TefMeister/arcade-controls-re2-vr-engine-research).

## The six repositories for Visceral — RE2 VR

Everything for this project lives in six repositories, each with one job — so
you always know where to look. You are in **visceral-re2-vr-engine-research**.

| Repository | What lives here |
| --- | --- |
| [visceral-re2-vr-mod](https://github.com/TefMeister/visceral-re2-vr-mod) | The mod itself — releases only. |
| [visceral-re2-vr-dev-archive](https://github.com/TefMeister/visceral-re2-vr-dev-archive) | Full development history — snapshots, probes, dead ends, raw recon. |
| [visceral-re2-vr-modding-notes](https://github.com/TefMeister/visceral-re2-vr-modding-notes) | Readable field notes / progress ledger. |
| [visceral-re2-vr-staging](https://github.com/TefMeister/visceral-re2-vr-staging) 🔒 | **Private** — unverified WIP builds, cross-machine handoff. |
| **visceral-re2-vr-engine-research** ← you are here | Distilled engine reference (dossier) + reusable VR RE playbook. |
| [visceral-re2-vr-external-research](https://github.com/TefMeister/visceral-re2-vr-external-research) | Ongoing public-research leads, gathered separately from hands-on modding work. |

## Credits & policy

See [`CREDITS.md`](CREDITS.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
Non-commercial fan project; requires an owned copy of the game; no original
game files are stored here.
