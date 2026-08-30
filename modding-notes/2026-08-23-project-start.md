# 2026-08-23 — Project start: the ground-up rebuild begins

Today Arcade Controls for RE2 VR was closed for good — final release v1.5.0 on
Nexus, every shipped zip archived, and the last working development state
frozen privately as the reference build ("ACVR final unfinished"). Visceral is
its successor, and these five repositories are its home.

## The one rule that defines this project

**Every line is written from scratch.** Visceral is not a fork of
RE2VRMODRELOADED, and it does not reuse code from our own Arcade Controls
scripts either. Anything that publicly exists may be studied — played,
read, taken apart to see what makes it tick — but nothing is copied. What
carries over from the predecessor is knowledge only: the engine dossier, the
case studies, and the lessons in its modding notes.

## Scope (from the predecessor's feature set, to be rebuilt fresh)

- A stable core: the interaction layer RE2VRMODRELOADED pioneered — motion
  controller weapon handling, two-handing, body IK and posture.
- Manual reloads; hand-pose slide racking and pump-action handling.
- Holsters and body-anchored inventory interactions.
- The backlog inherited as *ideas* (not code): the ladder-climb camera hold
  (re-examined for a fully snap-free version), menu-camera hold, door push,
  head shadow, and the "more gore" concept that fits the name.

## Starting assets

- The engine dossier and playbook (copied into `-engine-research` — our own
  documents, carried forward).
- The frozen Arcade Controls repositories as study material.
- The reference build "ACVR final unfinished" in the predecessor's private
  staging repository.

Next session: design pass — what the stable core looks like when it is ours
from the first line.
