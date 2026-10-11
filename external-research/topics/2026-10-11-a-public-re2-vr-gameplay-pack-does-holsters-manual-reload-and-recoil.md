# A public RE2 VR gameplay pack already does holsters, manual reload and recoil, as a REFramework add-on

**Status:** 🆕 new · **Priority:** medium — prior art for the jobs Visceral RE2 and its RE3 port are
building now (holster, rack, reload), and a likely comparison for players.

## What it is

Nexus Mods, Resident Evil 2 (2019), mod **2483**: a VR "gameplay pack" that runs on top of praydog's
REFramework VR `[reported 2026-10-11, search-engine summary of the Nexus page]`. Its page lists:

- **manual reload** on most weapons, chosen per weapon;
- **physical holsters** at hip, shoulder and chest;
- a **head flashlight zone**;
- **weapon recoil and haptics**, with a global intensity slider plus one slider per weapon, and fire and
  melee haptics switchable separately;
- melee swing sounds and an OpenXR height setting;
- every feature toggled in REFramework's Script Generated UI.

Install: REFramework VR first, then the pack's `reframework` folder by hand or through Fluffy Mod
Manager. The author writes that they want to move on to other projects, so updates may stop.

## ⚠️ What is NOT known yet

- ~~**The author.**~~ **Settled 2026-10-11 by Tefa:** mod 2483 is **Andyalpa's** mod `[reported 2026-10-11,
  Tefa, who opened the page]`, i.e. the RE2VRModReloaded line this repo already tracks as prior art
  (CREDITS.md). The page refused the automated fetch (HTTP 403), which is why this needed a person.
- **Whether it is a Lua script add-on or native code.** The `reframework` folder suggests scripts
  `[hypothesis]`.
- **Whether an RE3 version exists.** None was found `[checked 2026-10-11, two searches]`.

## Why it matters here

- Visceral's own holster (`holster.cpp`), rack and reload work covers the same ground. If the two mods
  are ever installed together, both would answer the same controller gestures.
- Its per-weapon recoil and haptics sliders are a settings shape players of RE2 VR already know.
- For the RE3 port: no public RE3 equivalent turned up, so these features would be new there.

## Next step

None for the author question (settled above). This feature list is Andyalpa's current one, worth
comparing against Visceral's holster, rack and reload work. No code is copied from it.

## Sources

- Nexus Mods, Resident Evil 2 (2019) mod 2483 — <https://www.nexusmods.com/residentevil22019/mods/2483>
- REFramework (praydog) — <https://github.com/praydog/REFramework>
