# Toolchain: what to download, which versions, and how to install it by hand

This is the exact setup **Visceral** is developed against, written so that
someone who has never touched an RE Engine mod can reproduce it. Everything is
installed manually — no mod manager needed, and nothing here is automated away
from you.

Two things are covered:

1. **REFramework** — required. Visceral is a REFramework mod; without it nothing
   loads.
2. **DLSS upscaling** — entirely optional, and the genuinely confusing part. It
   is confusing because the piece named "TemporalUpscaler" inside REFramework is
   only a *front end*: the thing that actually does the upscaling is a separate
   plugin by a different author. Section 3 untangles that.

Nothing here is our own work. It is other people's tools, credited in
[`CREDITS.md`](CREDITS.md), documented here because the version situation is a
trap that costs people an afternoon.

---

## 1. The short answer

| Piece | What we use | Required? |
| --- | --- | --- |
| Resident Evil 2 (2019), Steam | DirectX 12 build (the ray-tracing update), app id `883710` | yes |
| REFramework | see section 2 — **which build depends on whether you want DLSS** | yes |
| `openvr_api.dll` / `openxr_loader.dll` | from REFramework's separate `VR.zip` | only for VR |
| PureDark's upscaler plugin (`PDPerfPlugin.dll`) | Nexus RE2 mod `2069` | only for DLSS |
| UpscalerBasePlugin backend | version **1.1.2** | only for DLSS |
| `nvngx_dlss.dll` | any current DLSS 4.x DLL | only for DLSS |

Test machine for reference: RTX 5080 (driver 32.0.16.1074), Quest 3 over Virtual
Desktop.

---

## 2. REFramework (required)

REFramework by [praydog](https://github.com/praydog/REFramework) is the mod
loader, Lua scripting platform and VR layer for every RE Engine game. It installs
as a single proxy DLL — `dinput8.dll` in the game folder — which the game loads on
startup.

### 2a. Which build?

**This is the part people get wrong.** There are two lineages, and the newer one
has *fewer* features:

| Build | Revision | Date | In-framework DLSS upscaler | Maintained |
| --- | --- | --- | --- | --- |
| praydog mainline nightly | `684ca77` (nightly 01397) | 2026-08-20 | **no** | yes |
| `pd-upscaler` fork build | `76298bd` | 2026-03-11 | **yes** | no — see below |

praydog's mainline has **never** shipped the temporal upscaler. We verified this
the hard way: we downloaded seventeen nightlies spanning March to August 2026
(builds 1294, 1300, 1306, 1313, 1318, 1320, 1331, 1336, 1362, 1366, 1370, 1381,
1391, 1394, 1395, 1396, 1397) and counted the `TemporalUpscaler` symbols in each
`dinput8.dll`. Every single one: zero.

So if you install "the latest REFramework" expecting to find a DLSS option, you
will not find one, and nothing is broken — it was never there.

The upscaler lives only on a `pd-upscaler` branch in a fork of REFramework
([`gmankab/reframework-pd-upscaler-build`](https://github.com/gmankab/reframework-pd-upscaler-build)).
That build identifies itself at runtime: its log's first lines include
`Branch: pd-upscaler`.

Be aware of what you are choosing there: that fork publishes **no releases**, its
newest source commit is from 2026-03-27, and its only CI runs (2026-08-06) ended
in `failure` and `cancelled`. A newer binary does not exist, so the March build is
the newest usable one, and if you want anything past it you must compile the fork
yourself.

**Our choice for Visceral:** the `pd-upscaler` build (`76298bd`), because we want
the upscaler available while testing. If you do not care about DLSS, take
praydog's latest nightly instead — it is maintained, and it is what your users
will have.

### 2b. Where to get it

- Stable releases: <https://github.com/praydog/REFramework/releases>
- Nightly builds (newer, and what most guides mean by "the latest"):
  <https://github.com/praydog/REFramework-nightly/releases>
- The DLSS-capable fork's source:
  <https://github.com/gmankab/reframework-pd-upscaler-build> (branch
  `pd-upscaler`)

Since April 2026 the nightlies are **one monolithic download**
(`REFramework.zip`) that covers every supported RE Engine game. The old per-game
`RE2.zip` files no longer exist. VR runtime DLLs are now a **separate** `VR.zip`,
which turns out to be very convenient — see below.

### 2c. Manual install

Your game folder is wherever `re2.exe` lives, e.g.
`C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\` (note the double
space in that folder name — it is genuinely there).

1. Download `REFramework.zip`.
2. Extract **`dinput8.dll` only**, into the game folder next to `re2.exe`.
3. That is the whole installation. REFramework creates its own `reframework\`
   folder, `re2_fw_config.txt` and a log on first launch.

**Do not extract anything else from the zip unless you know you want it.** The
release notes say the same thing, and the reason matters: pulling in the VR DLLs
turns REFramework's VR mode on, and if you only wanted flat-screen modding you
will suddenly be fighting a VR runtime you never asked for.

For flat-screen development, that is exactly why the separate `VR.zip` is useful:
**just do not extract it.** Flat stays flat with no configuration at all.

For VR, extract `VR.zip` too — `openvr_api.dll` for SteamVR, or delete that and
keep `openxr_loader.dll` for OpenXR.

### 2d. Verifying it works

Launch the game and press **Insert** — REFramework's overlay should open. If it
does not, check `re2_framework_log.txt` in the game folder; it is created on
every launch and its first lines report the build and branch.

---

## 3. DLSS (optional, and the confusing part)

### 3a. Why this is confusing

Three facts, which together explain every confused forum thread on the subject:

1. **RE2 has no DLSS of its own.** The ray-tracing update added ray tracing, not
   DLSS. There is no `nvngx_dlss.dll` in a clean install and no DLSS switch in
   `re2_config.ini` — only `PCRaytracingReflection` and `PCRaytracingAO`.
2. **REFramework's "TemporalUpscaler" is only a front end.** It draws the UI and
   hooks the renderer. It contains no upscaling code and no NVIDIA SDK.
3. **The actual upscaling is a separate plugin by a different author** —
   PureDark's, loaded by REFramework as `PDPerfPlugin.dll`. Without it the front
   end does nothing.

So "installing DLSS" for RE2 means installing *three* things from two different
projects, and the failure mode for a missing piece is a UI that appears but does
nothing, or does not appear at all.

### 3b. What you need

| File | What it is | Where it goes | Where to get it |
| --- | --- | --- | --- |
| `dinput8.dll` | a `pd-upscaler` REFramework build (section 2) | game folder | see 2b |
| `PDPerfPlugin.dll` | PureDark's upscaler plugin — the actual backend | `reframework\plugins\` | Nexus RE2 mod [`2069`](https://www.nexusmods.com/residentevil22019/mods/2069) |
| UpscalerBasePlugin, **v1.1.2** | the backend's shared runtime | game folder (per its own readme) | same mod page |
| `nvngx_dlss.dll` | NVIDIA's DLSS library itself | game folder | [DLSS Swapper](https://github.com/beeradmoore/dlss-swapper), or any game that ships one |

The **1.1.2** pin is not arbitrary. Newer UpscalerBasePlugin releases and the
RE2 build of PD-Upscaler have drifted out of sync, which produces a plugin that
loads but reports no backend; REFramework
[discussion #1170](https://github.com/praydog/REFramework/discussions/1170)
collects the reports and 1.1.2 is the version people land on.

### 3c. Manual install

1. Install the `pd-upscaler` REFramework build (section 2c) and launch the game
   once, so `reframework\plugins\` exists.
2. Put `PDPerfPlugin.dll` in `reframework\plugins\`.
3. Put the UpscalerBasePlugin files where that mod's own readme says — the game
   folder, alongside `re2.exe`.
4. Put `nvngx_dlss.dll` in the game folder. On an RTX card, DLSS Swapper is the
   tidiest way to fetch and later update it.
5. Launch, press **Insert**, and look for the **TemporalUpscaler** section.

### 3d. Reading the log when it does not work

`re2_framework_log.txt` tells you exactly which piece is missing. The two lines
worth knowing:

```
[TemporalUpscaler] Could not load PDPerfPlugin.dll, TemporalUpscaler will not work
```

The front end loaded; the backend plugin is absent. You are missing step 2 —
`reframework\plugins\PDPerfPlugin.dll`.

```
Backend is not loaded, TemporalUpscaler will not work.
```

The plugin was found but its backend was not. This is the version-mismatch case:
UpscalerBasePlugin missing, wrong version (try 1.1.2), or `nvngx_dlss.dll`
absent for the DLSS backend specifically.

You may also see `[Streamline] Failed to get sl.dlss_g.dll module handle`. That
is DLSS Frame Generation's Streamline library, a different feature again, and it
is harmless if you are not using frame generation.

**Honest status, 2026-08-24:** on our machine the front end loads and reports
`Could not load PDPerfPlugin.dll` — we have documented the requirement and the
diagnostics, but we have **not** yet completed the backend install, so we cannot
claim to have seen DLSS running in RE2 ourselves. Section 3 is a map, not a
testimonial. Sections 1, 2 and 4 are verified.

---

## 4. The folder layout, all together

```
RESIDENT EVIL 2  BIOHAZARD RE2\
    re2.exe
    dinput8.dll              <- REFramework (the only required file)
    openvr_api.dll           <- VR only, from VR.zip
    openxr_loader.dll        <- VR only, from VR.zip (pick one)
    nvngx_dlss.dll           <- DLSS only
    re2_fw_config.txt        <- REFramework writes this itself
    re2_framework_log.txt    <- REFramework writes this itself
    natives\                 <- where file-replacement mods land; absent when clean
    reframework\
        autorun\             <- Lua mods load from here
        data\                <- mods' saved settings
        plugins\
            PDPerfPlugin.dll <- DLSS only
```

A clean, mod-free install has **no** `natives\` and **no** `reframework\` folder
at all. If you are ever unsure whether a game is vanilla, that is the thing to
check: delete both, plus `dinput8.dll`, and the game is stock again. Everything
REFramework and its mods create lives in those three places, which is what makes
RE Engine modding easy to fully undo.

## 5. Versions this project is developed against

| | |
| --- | --- |
| Game | Resident Evil 2 (2019), Steam `883710`, DX12 / ray-tracing build |
| REFramework | `pd-upscaler` build, revision `76298bd`, 2026-03-11 |
| Mainline reference | praydog nightly 01397, revision `684ca77`, 2026-08-20 |
| VR runtime | OpenXR (`openxr_loader.dll`), Quest 3 over Virtual Desktop |
| DLSS backend | not installed yet — see 3d |

Recorded here so that a bug report against Visceral can say which of these it was
seen on, and so that "it works on my machine" has a definition.

---

Credits for every tool named here are in [`CREDITS.md`](CREDITS.md); policy in
[`CONTRIBUTING.md`](CONTRIBUTING.md). This is a non-commercial fan project and
requires a legitimately owned copy of the game. No original game files are
stored in this repository.
