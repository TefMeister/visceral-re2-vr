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
| REFramework Book (Lua API documentation) | cursey | https://cursey.github.io/reframework-book/ |
| RE2VRMODRELOADED (VR interaction layer, prior art this project is measured against) | Andyalpa | https://www.nexusmods.com/residentevil22019/users/Andyalpa |
| RE Engine temporal upscaler plugin (PDPerfPlugin.dll / UpscalerBasePlugin) | PureDark | https://www.nexusmods.com/residentevil22019/mods/2069 |
| `pd-upscaler` REFramework fork build | gmankab | https://github.com/gmankab/reframework-pd-upscaler-build |
| DLSS Swapper | beeradmoore | https://github.com/beeradmoore/dlss-swapper |
| EMV Engine (REFramework Lua toolkit — Console, Action Monitor, Hooked Method Inspector, Poser; technique reference only) | alphaZomega (alphazolam) | https://github.com/alphazolam/EMV-Engine |
| EMV-Engine-SILVER (maintained fork of EMV Engine) | SilverEzredes | https://github.com/SilverEzredes/EMV-Engine-SILVER |
| _ScriptCore (REFramework Lua utility/hotkey library) | alphaZomega (alphazolam) | https://github.com/alphazolam/_ScriptCore |
| RE2R Custom Animation Framework (CAF) — custom animation system + RE3-style dodge for RE2, with engine docs | godlock2000-eng (NonRTX) | https://github.com/godlock2000-eng/ResidentEvil2_CustomAnimationFramework_NonRTX |
| Better Movement Speed (RE9 original of the RE2 port; layer-speed + move-speed-hook technique) | Junh2x | https://github.com/Junh2x/RE9-Movement-Speed-Mod |
| REFramework (its published `FirstPerson.cpp` is the source of the settle-bug diagnosis; read online, nothing copied) | praydog and REFramework contributors | https://github.com/praydog/REFramework |
| `re2_smooth_movement.lua` (transform-write locomotion route, MIT) | praydog and REFramework contributors | https://github.com/praydog/REFramework/blob/master/scripts/re2_smooth_movement.lua |

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
