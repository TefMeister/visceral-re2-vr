# 2026-09-12 — Zombie variety, step 1: the face pool is data, and a brand-new face loads

Home PC, 10:00–11:00, hands-on session (`/lm` claim). Tefa in the morning: *"build it, put it on the
visceral board please. the goal is tu just have a bigger variety of zombies with different clothing …
the bigger job would be the different heads and faces, that i would like to add at least 20 of, at
least."* Idea settled in `mod-ideas/decisions.md` the same morning; three rows on the board.

## What was found (static, before touching anything)

Zombies are assembled from five parts by a **wardrobe table** the game reads from data files, not
code. Full write-up: `engine-research/ENGINE-DOSSIER.md` §7d. The headline numbers: 15 male faces on
disk, 9 in everyday use, seven face slots in the code with nothing in them, and a per-part "same
mesh, different skin" mechanism the game uses for bodies and clothes but never for faces.

## What was built

- `dev-archive/tools/zombies/montage_rsz.py` — reads and writes the nine montage `.user.2` files.
  Round-trips every shipped file byte for byte `[verified-numerically 2026-09-12, n=9]`; that test is
  the gate before any edit is trusted.
- `dev-archive/tools/zombies/make_zombie_pack.py` — builds the overlay from the player's own archive:
  re-deals the 55 everyday male outfits evenly over 12 faces (adjacent IDs always differ), adds
  FACE10 (bald, shipped but only used after chapter 2) to the everyday pool, and creates **FACE08** in
  an empty slot: Face00's prefab renamed, its mesh copied, its material re-pointed, NRMR/ATOS copied,
  ALBM re-tinted (sallow, paler, greener — the first tint was far too strong and posterised the skin).
  `--deploy` copies into the game with a manifest and `.pre-zombie` backups; `--undeploy` reverts.
- `dev-archive/reframework/autorun/visceral_zombie_probe.lua` — asks the live montage manager what it
  thinks, and asks the engine to load the new face, so the loose-file log answers without walking to
  a zombie. Installed in the game's autorun; one shot per save load; NUM9 re-runs it.

## What the two flat launches proved

Both launches were driven end to end by `re2drive.py` (title → Story → Continue → PLAYER BOUND, closed
with WM_CLOSE). The Continue save is the B1F save room next to the morgue — no zombie in view.

1. **The edited tables are accepted.** All three em0000 montage files were served as loose files and
   the game reached gameplay with no error `[verified-live 2026-09-12, n=2]`.
2. **The re-deal is what the game uses.** The live manager reports 68 outfits and `ID004 → FACE08`,
   `ID009 → FACE10`, exactly as written `[verified-live 2026-09-12, n=1]`.
3. **A new face in an empty slot loads.** `getFacePrefab("FACE08")` returns our prefab; standby →
   ready, and all six Face08 files (prefab, mesh, material, three textures) were served loose
   `[verified-live 2026-09-12, n=1]`.

Not yet done: **seeing** a FACE08 zombie rendered (texture bound, not grey). That is the remaining
`[FLAT]` look, and it is a look rather than a test — everything the loader can prove is proven.

Side reading: without a headset awake, REFramework logs `XR_ERROR_FORM_FACTOR_UNAVAILABLE` and falls
back to flat cleanly, so flat automation can run with Virtual Desktop's streamer up.

## Step 2 is now open (the 20+ faces)

Plan on the board: skin variants of the 10 everyday heads using the game's own `_00_nn` naming (one
launch tests whether the seed picker spreads them — the probe can read the variant it resolves), mesh
variants by proportional deformation in Blender (RE Mesh Editor round-trip, rig and UVs untouched),
and a handful of outfit-row edits for more police / sewer / lab zombies (a spawn's outfit is chosen by
level data, so rows are edited, not added `[inferred-static]`).

## Housekeeping

- Inbox drained by name: `2026-09-12-mod-zombie-montage-system.md` (→ dossier §7d) and the `/gs`
  drop about the line-wrapped undated tag (→ fixed in the dossier's IK paragraph).
- Logs of both runs are the game's own `re2_framework_log.txt` / `reframework_accessed_files.txt` /
  `reframework_loose_files.txt`; pre-run copies kept beside them as `*.pre-zombie-2026-09-12`.
