# The motlist dead end was the wrong door — and the data pass is de-risked by a mod already on this disk (2026-09-05 evening, home PC)

Supersedes: 2026-08-30-aim-pose-and-foot-grounding-solved.md § "What did NOT work" (the
animation/motlist swap line only — everything else in that note stands)

## How this came up

Tefa, on being told the Blender track was the open PD work, said: *"there is a mod that gives
Claire a body posture from RE3, so must be doable!"* The immediate reading — that a posture
mod proves mesh round-tripping — is wrong, because posture is animation and a neck is
geometry. But the instinct was pointing at something real, and looking for the mod turned up
a different one already sitting in `C:\Users\TD3KX\Downloads\` that answers more.

**Specimen:** `Claire A V2.6.1-2240-C-A-2-6-1-1777794971.zip` — *Claire A v2.6.1*, category
"Fix", by **Shulian01 (Dragonleo)**, Nexus mod 2240, plus a bundled add-on *Restore Mr.x dead
for Claire*. Fluffy Mod Manager loose-file layout (`modinfo.ini` + `natives/stm/...`).
Fluffy itself is in the same Downloads folder.

⚠️ **Read-only specimen.** Nothing from this archive, and nothing derived from its files, goes
into our repos — it is third-party work containing modified original game data. We take the
*path conventions and the mechanism*, and build our own. Per the source rule this is
third-party non-tool code: study the method, reimplement subtly, never reproduce.

## Finding 1 — our "motlist swap is dead" note was about the RUNTIME route only

`2026-08-30-aim-pose-and-foot-grounding-solved.md` lists among confirmed dead ends:

> *"Animation swap / motlist swap / weapon-category spoof — the game rejects it (also failed
> in AC historically)."*

That was established by trying to swap motion at **runtime, from a REFramework script**. This
mod ships **replaced `.motlist.524` files on disk**, including player ones — `pl1000` (Leon)
and `pl3000` (Claire), alongside `pl1050`/`pl1070`/`pl3050`/`pl3070` variants and enemy
motlists `em6200`/`em6250`/`em6270`/`em7000`/`em7100` `[inferred-static 2026-09-05]`.

Those are two different mechanisms sharing a name:

| Route | What it is | Our evidence |
| --- | --- | --- |
| Runtime swap | script tells the motion component to resolve a different bank/list mid-play | rejected `[verified-live 2026-08-30]` |
| **File replacement** | the `.motlist` on disk is different before the engine ever loads it | **a shipped mod does it** `[inferred-static 2026-09-05]` |

**The dead-end note is not wrong — it is narrower than it reads.** It should be read as "the
runtime route is dead", and the file route was never tried.

⚠️ Note the trap this sits next to: the runtime route failing tells us nothing about the file
route, and until a file-replaced motlist is seen loading in-game this is still
`[inferred-static]`. Filenames in an archive are strong evidence of what a file governs; they
are not proof that replacing it works, and the motlists here are **cutscene-scoped**
(`sectionroot/cutscene/cf/cf520/...`), which is a softer case than gameplay locomotion.

### What this reopens

- **Roadmap v2 item 22 (aim posture round two — walk normally while aiming).** The file route
  would mean replacing the aim-walk motions on disk with the normal-walk ones. **Static** — it
  cannot be conditional on the aim state, so it changes the animation always, not only while
  aiming. For this particular feature that may be exactly right: the whole point is that
  aiming should not look different. Needs a flat run to prove.
- **Roadmap v2 item 23 (poisoned walk matching the normal walk).** Same mechanism, same
  caveat, and a cleaner test case than 22 because the poisoned state is rarer and a permanent
  change matters less.

## Finding 2 — the data pass is de-risked by existence

Roadmap v2 phase B was ordered around one unanswered question: *can we edit RE2's placed
scene data at all?* Item 5 (RPD main-hall lights) was put first purely as the cheapest probe
of it. **A shipped, working mod already does this**, and the archive names the paths
`[inferred-static 2026-09-05]`:

| Roadmap item | What the specimen contains |
| --- | --- |
| 7 more enemies | `objectroot/scene/scenario/scenariono/rpd_b1/enemy/{claire_,}s02_0500.scn.20`, `wastewater/enemy/s04_0050.scn.20` |
| 8 item placement | `rpd_b1/item/common.scn.20`, `wastewater/item/{claire_,}common.scn.20` |
| 5 RPD lights | `objectroot/scene/location/*/environments/*.scn.20` and `level_100/environments/*/gimmick.scn.20` — the environment scenes are where placed lights would live |
| 10 Mr. X persistence | the add-on *Restore Mr.x dead for Claire* edits `sectionroot/cutscene/cf/cf520/` (scene + motlists + `cf520_effect.scn.20`) — a worked example of editing a Mr. X scripted beat |
| — (doors) | `scene/location/rpd/gmk_door.scn.20` |
| 36/37 Blender track | `sectionroot/environment/.../st1_611_0_terrain_00ms.mesh.2109108288` — a **replaced mesh** ships and presumably loads, so the mesh pipeline is not theoretical |
| — (text/tables) | `message/mes_item/mes_item_{name,detail}.msg`, `mes_file_27.msg`, `userdata/purposelist/purposelistuserdata.user.2`, `ropewayglobalvariables.uvar.2` |

`ropewayglobalvariables.uvar.2` is worth its own line: `app.ropeway` is RE2's own namespace
(the same one §8d needs for the move-speed accessor), and a mod editing its **global
variables** as a data file is a lead for anything currently being chased through script.

## What changes on the roadmap

Nothing is reordered. Two things get cheaper and one gets better aimed:

1. **Item 5 stays first in phase B**, but it is no longer a blind probe — we now know the path
   shape to look in (`objectroot/scene/location/rpd/.../environments/`) and the packaging
   format to deliver it in.
2. **Items 22 and 23 gain a second route** that was previously written off. Not a promotion —
   the runtime work in flight is still the priority — but the ceiling on item 22 is higher
   than the 2026-08-30 note implied.
3. **The mesh-tooling gate softens.** Last turn's position was "prove the round-trip before
   modelling anything". A shipped replaced mesh does not prove *our* round-trip works, but it
   proves the destination accepts replaced meshes, which was the part genuinely in doubt.

## Next, all of it PD (nothing needs to run)

- Unpack the specimen to a scratch folder and read the `.scn.20` structure — enough to know
  whether these are whole-scene replacements or diffs, and what a placed light/enemy/item
  entry looks like. **Read only; nothing derived goes in a repo.**
- Find out what tooling reads/writes `.scn.20` and `.motlist.524` (the RE Engine file-format
  community tools). That, not modelling, is the real first obstacle on every item in phase B.
- Establish where RE2 keeps the **RPD main-hall lights** specifically — the archive shows the
  folder shape but does not contain the RPD environment scene itself.
- Read the Jill specimen (finding 3) against our own `base_hdg_hold` — what is actually inside
  a `.motlist.85`, and can we author one rather than transplant one.

## Finding 3 — the posture mod itself, and it is three files

Tefa downloaded it mid-session (21:44): **"Jill's Animations for Claire" v1.1, Nexus 2516, by
`xJustAdam`** — *"Replaces Claire's movement animations with Jill's from RE3R."* Fluffy
loose-file layout. `C:\Users\TD3KX\Downloads\Jill's Animations for Claire 2516 1.1 ….rar`.

The whole mod is three files plus a cover image `[inferred-static 2026-09-05]`:

```
natives/x64/sectionroot/animation/player/pl10/list/hdg/base_hdg_hold.motlist.85
natives/x64/sectionroot/animation/player/pl10/list/hdg/base_hdg_move.motlist.85
natives/x64/sectionroot/animation/player/pl10/list/stg/base_stg_move.motlist.85
```

Reading the path: `pl10` is Claire's gameplay animation root (inferred from the mod's own
description, not yet confirmed against the game); **`hdg` is the handgun bank and `stg` the
shotgun bank**; `base_hdg_hold` is **the HOLD bank** and `base_*_move` the weapon-out walk.

### This is item 22, done, by a route we ruled out

Compare directly with the 2026-08-30 dead-end list. Our "bank poison" attempt tried to make
bank **resolution** fall through to CMN unarmed, and failed for the reason recorded there:

> *"the legs are bound to the weapon-HOLD bank (2000) whenever the weapon is up"*

We read that as the wall. **This mod does not touch resolution at all — it leaves the legs
bound to the HOLD bank and replaces the contents of the bank.** Redirect the pointer versus
change the destination. Ours was the harder problem and it was the wrong one.

### Why the route suits VR better than flat

`2026-08-30-aim-pose-and-foot-grounding-solved.md` establishes that the hands, arms, gun and
therefore the muzzle are **VR-controller-pinned and unaffected by the body skeleton** —
bullets confirmed true while the pelvis-drop is active. So for us the weapon-hold body pose is
**cosmetic**. A flat player swapping the HOLD animation risks their aim reading wrongly; we
cannot, because our aim does not come from the body at all. The static-ness of file
replacement — the objection raised against this route above — is also not a cost here, since
item 22's goal is precisely that aiming should *never* look different.

### What is still unproven

- That `pl10` is Claire, and what Leon's root is.
- Whether these files load at all on our install alongside REFramework and the plugin.
- Whether the replacement can be **authored** (build our own motlist from the normal walk)
  rather than **transplanted** (lift RE3R's). Authoring is the version we would ship; a
  transplant of another game's animation data is not something we can redistribute.

## Finding 4 — `x64` vs `stm` and `.85` vs `.524` are the pre-RT and RT builds, and OUR INSTALL IS RT

Tefa downloaded the second variant three minutes after the first, which turned the pair into a
controlled comparison and answered the root/suffix question outright.

**RT variant:** *"Jill´s Animation For Claire" v2.0 RT*, Nexus **2529**, by **Angeltitilxyz &
xJustAdam** — a port of 2516 to the Ray Tracing build.

| | non-RT (2516, xJustAdam) | RT (2529, Angeltitilxyz & xJustAdam) |
| --- | --- | --- |
| natives root | `natives/x64/` | `Natives/STM/` |
| format suffix | `.motlist.85` | `.motlist.524` |
| files | `hdg` hold + move, **`stg` move** | `hdg` hold + move only — **no shotgun** |

So `x64`/`.85` and `stm`/`.524` are not alternative spellings, they are **the two game builds**
`[inferred-static 2026-09-05]`. This also retro-explains the Claire A specimen, which is
`natives/stm/` + `.motlist.524` throughout: it is an RT-build mod, and the pattern is
consistent across both specimens.

**Our install is the RT build** `[inferred-static 2026-09-05]` — read from the game's own
config rather than observed running, so it is as strong as static evidence gets without a
launch: `re2_config.ini` carries
`TargetPlatform=DirectX12`, `PCDXver=DirectX12`, `PCRaytracingReflection=1`, and `D3D12/
D3D12Core.dll` is present. Steam buildid 11636119.

Consequences:
- **The RT variant is the applicable specimen**; 2516's files would not have matched our tree
  at all. Anything we author targets `Natives/STM/…/*.motlist.524`.
- **The RT port covers less** — the shotgun move list is missing from it. If item 22 wants the
  shotgun walk too, that is ours to author, with no ported reference to compare against.
- Any future specimen must be checked for which build it targets **before** concluding anything
  from its paths.

### Bonus: REFramework has its own loose-file loader

`reframework_accessed_files.txt` in the game folder contains one line —
`[LooseFileLoader] [info] LooseFileLoader constructed` (2026-09-05 10:10) — so **REFramework
itself loads loose `natives/` files**; Fluffy Mod Manager is not necessarily required to
deliver this kind of change `[inferred-static 2026-09-05]`. That matters for packaging: it
would let a motlist ship inside the Visceral mod folder rather than as a separate Fluffy mod.
The log recorded no filenames, so per-file access logging is either off or needs enabling —
worth finding out, because a log of the paths the game *actually requests* would remove the
guesswork from every path question above, including where the RPD lights live.

⚠️ **Redistribution line:** studying this is fine and the mechanism is ours to reuse. Shipping
RE3R animation data, or xJustAdam's files, is not. If the technique ends up in Visceral it is
with our own authored motlists — and **xJustAdam is credited either way**, per Tefa
(2026-09-05): *"we'll just credit the modder if we use it in any way or form."* Shulian01
(Dragonleo) likewise for the Claire A specimen.
