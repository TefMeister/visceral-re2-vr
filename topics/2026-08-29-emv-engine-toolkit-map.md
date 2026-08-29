# EMV Engine mapped: which of its tools actually help study the armed state

**Status: `[reported]`** — from the EMV-Engine repo's own README/source viewers, 2026-08-29.

## What this is

EMV Engine (alphaZomega / alphazolam, MIT) is already in our EXTERNAL-RESOURCES as a technique
reference, but only for one borrowed hook-timing trick. This is the fuller map, focused on the
session's question: *what in EMV helps us study or change the weapon-equipped state?* Short
answer: **EMV contains no weapon/equipment code at all** — its value here is as
instrumentation for finding the game's own.

Repo: https://github.com/alphazolam/EMV-Engine · maintained fork:
https://github.com/SilverEzredes/EMV-Engine-SILVER (use when upstream lags a game update; RE2
still supported there).

## What's in the box (verified against the repo tree)

One shared library everything `require`s — `EMV Engine/init.lua` (~450 KB; most functions
documented in comments inside it; exported globally as the `EMV` table) — plus separate autorun
tools: **Console** (Lua REPL with tab-autocomplete, object search via `/`, `folders` /
`transforms` listings), **Enhanced Model Viewer** (animation/cutscene control incl. an RE2
resource list), **Gravity Gun**, **Enemy Spawner**, **Hooked Method Inspector**, and the **RE
Engine Resource Editor** (PFB/SCN/USER/MDF2 editing, ships a native rszparser DLL). Poser,
Action Monitor, Material Manager and Chain Controller are features inside the EMV/Model Viewer
scripts, not separate files.

- Player detection is generic, not per-game: `sdk.get_managed_singleton(sdk.game_namespace("PlayerManager"))`
  with fallbacks for other titles — in RE2 that resolves to `app.ropeway.PlayerManager`, the
  same singleton the CAF dodge uses. No EquipmentManager/weapon handling anywhere in EMV or in
  alphaZomega's companion `_ScriptCore` library (checked its `Functions.lua` — generic
  GameObject/field/raycast/JSON helpers only).

## How each tool maps to armed-state work

- **Console** — live-verify every `[reported]` name from the weapon-state topic
  (`Equipment` component, `<EquipWeapon>k__BackingField`, `FireBulletType`) without writing a
  probe script per question. Its autocomplete doubles as TDB discovery.
- **Action Monitor / the motion-FSM view** — watch which motion states/banks change when the
  weapon is drawn vs holstered vs aimed: the observational complement to our dossier §8
  TargetBankType finding, and the fast way to map the armed locomotion states for roadmap 1.
- **Hooked Method Inspector** — attach to a TDB method and see its live args/returns; the
  right tool for "what actually reads the equipped state" questions (e.g. who consumes
  `IsHold`), replacing blind hook-and-log scripts.
- **Poser** — the source of the borrowed re-apply-before-each-IK-write timing technique already
  credited in the dossier; relevant again for aim/walk pose work.
- **Enhanced Model Viewer's RE2 resource list** (`enhanced_model_viewer_RE2_resources.lua`) —
  a ready reference of RE2 motion-bank paths and ID conventions: players `pl00` (Leon), `pl10`
  (Claire), `pl20` (Ada), `pl30` (Sherry); weapons `wp****`; banks under
  `sectionroot/animation/player/<char>/bank/*.motbank`. Useful vocabulary for roadmap 11/15/16
  asset work.

## Why it matters

Visceral's next fronts (aim/walk matching, manual reloads, dodge) all begin with observation
sessions. EMV turns those from "write a probe, relaunch, read logs" into interactive work. Per
the ground-up rule: EMV is MIT and could be required as a dependency, but Visceral's pattern so
far is to use it as a **dev-time instrument only**, keeping the shipped mod dependency-free.

## Sources

- EMV-Engine repo (alphaZomega) + README + file tree; `_ScriptCore` repo; EMV-Engine-SILVER
  fork (SilverEzredes) — all read via the repos' own viewers, 2026-08-29. Note: `init.lua` is
  too large for full remote reading (~first quarter reviewed; its comment-documented function
  list is best read in a human browser when needed).

## Concrete next step

Install EMV Console + Hooked Method Inspector on the dev install for the next observation
session; first exercise: draw/holster each weapon with the Action Monitor open and record which
FSM states and bank selections flip — that inventory is the map roadmap item 1 needs.
