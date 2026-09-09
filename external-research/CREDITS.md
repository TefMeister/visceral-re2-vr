# Credits & Attribution

This project is a reverse-engineering and modding effort built on the public
research, tools, and documentation of many people who came before us. None of
this would be possible without their work. We list every source we've drawn
on below — including work that helped only as inspiration — by name or
handle, as accurately as we could verify it.

## The game itself

This mod modifies, at runtime, **Resident Evil 2** (2019) by **Capcom**
(https://www.capcom.com), via praydog's REFramework. The game, its engine,
and all of its assets are Capcom's, and the game is the entire reason this
project exists. **No game files, code, or assets are distributed in any of
this project's repositories** — only code, notes, and tools we wrote
ourselves, plus third-party components whose licenses permit redistribution
(noted below).

## Prior art, tools, and research this repo draws on

| Source / Work | Creator(s) | Link |
|---|---|---|
| REFramework (mod framework, VR support for RE2, FirstPerson mode, Lua API) | praydog | https://github.com/praydog/REFramework |
| `re-engine-trainer` (public RE Engine trainer; its Requiem module independently corroborated the layer-speed + `getMoveSpeed` pairing) | Namsku | https://github.com/Namsku/re-engine-trainer |
| REFramework PR #1809 — restored `on_pre_gui_draw_element`'s `false` return after the 2026-08-19→28 silent regression (PR #1503) | ErwinGunsmith, and praydog as maintainer | https://github.com/praydog/REFramework/pull/1809 |
| REFramework Book (Lua API documentation) | cursey | https://cursey.github.io/reframework-book/ |
| RE2VRMODRELOADED (VR interaction layer, prior art this project is measured against) | Andyalpa | https://www.nexusmods.com/residentevil22019/users/Andyalpa |
| RE Engine temporal upscaler plugin (PDPerfPlugin.dll / UpscalerBasePlugin) | PureDark | https://www.nexusmods.com/residentevil22019/mods/2069 |
| `pd-upscaler` REFramework fork build | gmankab | https://github.com/gmankab/reframework-pd-upscaler-build |
| DLSS Swapper | beeradmoore | https://github.com/beeradmoore/dlss-swapper |
| EMV Engine (REFramework Lua toolkit — Console, Action Monitor, Hooked Method Inspector, Poser; technique reference only) | alphaZomega (alphazolam) | https://github.com/alphazolam/EMV-Engine |
| EMV-Engine-SILVER (maintained fork of EMV Engine) | SilverEzredes | https://github.com/SilverEzredes/EMV-Engine-SILVER |
| _ScriptCore (REFramework Lua utility/hotkey library) | alphaZomega (alphazolam) | https://github.com/alphazolam/_ScriptCore |
| RE2R Custom Animation Framework (CAF) — custom animation system + RE3-style dodge for RE2, with engine docs | godlock2000-eng (NonRTX) | https://github.com/godlock2000-eng/ResidentEvil2_CustomAnimationFramework_NonRTX |
| Better Movement Speed (RE9 original of the RE2 port; layer-speed + move-speed-hook technique, incl. the `getLayer(0)`/enemy-context-list approach studied in detail) — **and, found 2026-09-05, the two diagnostic scripts shipped in the same repo: `re9_layer0_diag.lua` (motion-layer property dumper, source of the `get_Weight` / `getLayerCount` / `via.motion.Motion` `PlaySpeed` findings) and `re9_character_diag.lua`.** No licence file on the repo — studied only, nothing copied | Junh2x | https://github.com/Junh2x/RE9-Movement-Speed-Mod |
| REFramework (its published `FirstPerson.cpp` is the source of the settle-bug diagnosis; read online, nothing copied) | praydog and REFramework contributors | https://github.com/praydog/REFramework |
| `re2_smooth_movement.lua` (transform-write locomotion route, MIT) | praydog and REFramework contributors | https://github.com/praydog/REFramework/blob/master/scripts/re2_smooth_movement.lua |
| RE Engine 010 Editor templates — `RE_Engine_motlist.bt` / `RE_Engine_motbank.bt`; source of the v486/524 motlist header and 72-byte collection-entry layout, the `Switch` u16 at +0x0A, and the confirmation that murmur3 hashing in these files is for **bone** names only (2026-09-07) | alphaZomega (alphazolam) | https://github.com/alphazolam/RE-Engine-010-Templates |
| Motlist-Tool (MaxScript motlist import/export) — one of the two independent writers showing `motSize` is emitted as 0 for every entry in the 486/524 generation (2026-09-07) | alphaZomega (alphazolam) | https://github.com/alphazolam/Motlist-Tool |
| MMDK (RE Engine moveset development kit) — documents `MotionKey` → numeric **MotionID** within a numeric **bankID**, the basis of the "lookup is by number, not by name hash" finding (2026-09-07) | alphaZomega (alphazolam) | https://github.com/alphazolam/MMDK |
| CAF's engine documentation specifically — `docs/actor_motion_systems.md` (`changeMotion(bankID, motionID, …)`; `findMotionBankByNameHash` being **file**-level) and `docs/motlist_format_guide.md` (motions resolved by index position, no motion-name hash field, `motSize` populated only in RE2 v65) (2026-09-07) | godlock2000-eng (NonRTX) | https://github.com/godlock2000-eng/ResidentEvil2_CustomAnimationFramework_NonRTX |
| RevilLib (RE Engine format library) — cited for its supported-version list only | PredatorCZ (Lukas Cone) | https://github.com/PredatorCZ/RevilLib |
| RE-Engine-Hash-tool | TrikzMe / devilsnake88 | https://github.com/TrikzMe/RE-Engine-Hash-tool |
| REEngine-Modding-Documentation | Havens-Night | https://github.com/Havens-Night/REEngine-Modding-Documentation |
| The ray-tracing-patch format-changes thread — source of the `natives/x64` → `natives/stm` loose-file root move happening in the same patch as the motlist 99 → 524 bump (2026-09-07) | the residentevilmodding community | https://residentevilmodding.boards.net/thread/16547/information-format-changes-tracing-patch |
| RE-Engine-Lib (`RTexFile.cs` — the public `.rtex` descriptor layout, which names `widthRate`/`heightRate` and confirms `0x0C` is a raw DXGI enum) and REE-Lib-Resources (the per-game file-extension version table) (2026-09-07) | kagenocookie | https://github.com/kagenocookie |
| REE.PAK.Tool — its published file lists let the sibling project pull the shipped RE8 `.rtex` set by name hash without unpacking; the `.rtex.5` descriptor decode this project inherits rests on it (2026-09-07) | Ekey | https://github.com/Ekey/REE.PAK.Tool |
| H3VR — the developer's own public refusal of per-weapon second-hand offsets ("incredibly time consuming … an extra entire set of manual poses") and the global `use gun rig mode` that ships instead, incl. its lever-action caveat (2026-09-07) | Anton Hand / RUST LTD, and "[RUST]Grumplestiltskin" | https://steamcommunity.com/app/450540/discussions/0/3183345176717342122/ |
| The H3VR player whose request made that developer reasoning public (2026-09-07) | Knifie_Sp00nie | https://steamcommunity.com/app/450540/discussions/0/3183345176717342122/ |
| Anomaly VR — the two-handed grip with secondary-hand anti-occlusion, its `F11 → VR Tools` calibration tabs and MCM section (the published interface, as against the unconfirmed per-weapon LTX table) (2026-09-07) | Andrey "MarsyApp" | https://ap-pro.ru/forums/topic/14575-anomaly-vr/ |
| Onward — Virtual Gunstock Mode, the "stop reading the rear hand" family | Downpour Interactive; reported by UploadVR | https://www.uploadvr.com/onward-inside-out-tracking-update/ |
| Blade & Sorcery — the named second-hand attach-transform config shape (`FirearmSecondaryHandle`, handle IDs, `weaponHoldPositionOffset`); lower confidence, via search summary rather than a fetched page (2026-09-07) | WarpFrog | https://www.bladeandsorcery.com/ |
| H3VR "Far ForeGrip" (a scalar foregrip grab distance via the Sodalite Mod Panel) and Accessibility Options | NGA; Okkim | https://thunderstore.io/c/h3vr/p/NGA/Far_ForeGrip/ |
| "Tracking Technology Explained: LED Matching" — occlusion named among worst-case controller-tracking scenarios, with no published coast-time figure | Meta | https://developers.meta.com/horizon/blog/ |
| The ap-pro.ru, stalkerportaal.ru and stalker-mods.clan.su STALKER communities, and h3vr.fandom.com contributors and Thunderstore — descriptive mod pages read online; nothing downloaded | various, credited individually as sourced | — |
| RE Mesh Editor — its RE2/RE2RT/RE3RT/RE4/RE9 material presets carry the **shipped `pl1000_Jacket_Mat` detail parameter set** (`DetailMap`, `DetailMaskMap`, `Detail_UVScale`, `Detail_Normal_Intensity`, `Detail_AO_Intensity`) that corrected two of our parameter names; also `modules/mdf/file_re_mdf.py` and `modules/tex/file_re_tex.py` for the `.mdf2` v21 and `.tex` v34 layouts (2026-09-07) | NSACloud | https://github.com/NSACloud/RE-Mesh-Editor |
| The canonical MDF editing tutorial thread — the public statement that a black `DetailMask` region stops the `DetailMap` being applied, what a DetailMap is for, and that null textures mark unused map slots (2026-09-07) | alphaZomega (alphazolam) | https://residentevilmodding.boards.net/thread/12456/template-editing-materials-tutorial-updated |
| `fmt_RE_MESH` Noesis plugin — its format table is the source of `tex .34` / `mdf2.21` being the **RERT** generation (2026-09-07) | alphaZomega (alphazolam) | https://github.com/alphazolam/fmt_RE_MESH-Noesis-Plugin |
| Original MDF structure research, credited by both public MDF tooling projects | Che, Darkness | — |
| MDF-Manager, and the point that a material's texture bindings and properties are fixed by its `.mmtr` shader | Silvris / SilverEzredes | https://github.com/Silvris/MDF-Manager |
| REEngine-Modding-Documentation wiki — texture channel-packing reference (`NRRC`, `ATOC`), the corpus that shows the detail-mask channel is genuinely undocumented rather than merely unfound (2026-09-07) | Havens-Night | https://github.com/Havens-Night/REEngine-Modding-Documentation/wiki/Textures |
| "The individual texture maps and their meaning" — ALBM/NRMR/ATOS channel layouts | Riot1986 | https://residentevilmodding.boards.net/thread/13115/individual-texture-maps-meaning |
| RE4R MDF template thread | terenceyao, smkquanchi | https://residentevilmodding.boards.net/thread/17585/mdf-template-editing-re4r-materials |

Our own sibling project [re-village-scope-vr](https://github.com/TefMeister/re-village-scope-vr)
decoded the `.rtex.5` render-target descriptor byte-for-byte on RE8 (2026-09-06); this project
inherits that decode rather than repeating it.

Our own predecessor, [ARCADE CONTROLS for RE2 VR](https://github.com/TefMeister/arcade-controls-re2-vr/tree/main/mod)
(final release v1.5.0), is frozen and kept as study material — Visceral
reuses its knowledge, not its code.

Development on this project is AI-assisted: much of the research, code, and
documentation was produced with **Claude (Anthropic)** (https://claude.com)
working alongside the project owner.

## Missing from this list?

If you — or someone whose work you know — contributed to, influenced, or
even just inspired anything used in this project and you aren't credited
here, please **open a GitHub issue on this repo** and we'll correct it as
soon as possible. We would much rather over-credit than leave anyone out.

## Respecting creators

This project exists because other people generously shared their
reverse-engineering research, tools, and modding know-how in public — we've
tried to credit every one of them by name or handle above, as accurately as
we could verify. If you are the creator or rightful owner of anything
credited or used here and you'd rather your work not be referenced in this
repo, or you want specific content removed or no longer used by the mod,
please tell us: **open a GitHub issue on this repo**. We'll act on that
request promptly — no argument, no delay — and we'll find another way to get
the job done that doesn't rely on your material. This is your work; we're
just grateful to have learned from it.
