# 2026-09-12 (afternoon) — one guard was freezing the camera, and the head hider with it

Home PC, `/lm visceral-re2-vr`, from 13:53. Six flat launches, all driven end to end. A background
reader worked the camera row while this session drove the game.

## The headline

`update_camera()` was called **once per session** and never again, because its only live caller was
guarded by the very flag it sets. Every consumer — the head hider's reveal gate, the dock's re-basing —
was measuring against the camera pose of the first frame. Removing the guard fixed both. Numbers and
the superseded theory: dossier §5b.4.

Before: `cam=(-11.50 -3.20 4.20)` frozen, reveal distance 19.68 m, head never hidden.
After: `cam` tracks the player, reveal distance **0.11 m**, `head=1 hid=1/5` — it hides.
`[verified-live 2026-09-12, n=1 flat walk]`

⚠️ Still open on the head hider: it finds only five meshes and they are our own bracelets and neck
plug plus `Transceiver` and `FlashLight`. So it now hides *reliably* — the wrong thing. Mesh discovery
is the remaining defect.

## Two things that were nearly free and are worth more than they cost

- **⭐ Camera control on RE2 is PROVEN.** Relative mouse movement through `SendInput` turns the view:
  40 small steps moved it measurably, 160 steps turned it around, and the walk that followed navigated
  a save room, a pump room and a corridor `[verified-live 2026-09-12, n=1 session]`. The automation
  profile has carried "character+camera ⚠️ not exercised" since 2026-09-04; that gap is closed. Helper
  script kept at `dev-archive/tools/mouse_look.py`.
- **⚠️ With the Quest connected, RE2 launches into VR and the desktop window stops repainting**, so
  BitBlt captures come back black and a session can waste launches thinking the game is broken. Parking
  `openxr_loader.dll` as `openxr_loader.dll.flat-run-2026-09-12` forces a flat, capturable run
  `[verified-live 2026-09-12, n=4 launches]`. **Restore that file before any VR work on RE2.**

## Zombie variety: the census, and why it found nothing

`dev-archive/reframework/autorun/visceral_zombie_census.lua` lists every live zombie with the montage
ID, the part keys its group resolved to, and the material on its face mesh — so "our face is on a real
zombie" can be read from the log instead of hunted for on foot.

It reported no zombies in a save room, a pump room and a corridor. Two lessons, both recorded so the
next session does not re-learn them: `findComponents(System.Type)` matches the **exact** type, so asking
for `Em0000SimpleMontageBase` finds nothing and the derived `Em0000/0100/0200SimpleMontage` must be
asked for by name (the script now asks for all four); and the Continue save sits in an underground area
with no zombies nearby, so **the FACE08 look wants a save with zombies in the room**, not more walking.

## Not established

Nothing was seen of a FACE08 zombie. The face pool work from this morning is still proven only at the
loader and the manager, not on a rendered zombie.

## Later the same session — 25 new zombie faces, and the pool turned out to be uncapped

Tefa's bar was "at least 20 of, at least". The pack now carries **25 new faces and a pool of 36**,
and the reason it can keep growing is in dossier §7e: the runtime looks faces up by **string**, so
keys past the end of the game's own enum (`FACE20`, `FACE29`, `FACE37`) resolve exactly like the
seven empty enum slots do. All three were confirmed live — `getFacePrefab` returned our prefab, the
engine loaded it, and all six files of each were served from our loose folder
`[verified-live 2026-09-12, n=1 launch]`. The re-dealt outfits use them: `ID004 → FACE30`,
`ID009 → FACE29`, `ID201 → FACE75`, `ID305 → FACE21`.

**How a face is made.** Each is derived at build time from the player's own archive: an existing
head's prefab (renamed), its mesh (copied), its material (three texture slots re-pointed with RE Mesh
Editor's own MDF writer), and a **new albedo** built by tinting that head's own texture. The recipe
per face is five numbers — hue toward green, saturation, brightness, hair greying, and a ruddy term
for a kill that is still flushed — so "long dead", "waxy and bloated" and "fresh and flushed" are
three settings of the same dial rather than three pieces of hand-painted art. All 25 recipes are in
`dev-archive/tools/zombies/face_roster.py`, which is the file to edit to add more.

**What is still not shown:** a rendered zombie wearing one. That needs a save with zombies in the
room, not more walking — see the board row.
