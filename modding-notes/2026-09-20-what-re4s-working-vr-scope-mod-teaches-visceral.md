# What RE4 Remake's VR mod teaches Visceral (2026-09-20, static — nothing was launched)

Tefa installed **RE4 Remake + Talemann's RE4VR mod** on the home PC and asked what transfers to
Visceral. Read statically from files already on this disk: Talemann's `dinput8.dll` (a REFramework
fork with the whole mod compiled in — **no Lua ships with it**), its 63 saved-settings JSON files,
`re4.exe`, and our own `RESIDENT EVIL 2  BIOHAZARD RE2/il2cpp_dump.json` (494 MB).

The companion note, with the scope architecture in full, is
`re-village-scope-vr/dev-archive/recon/2026-09-20-re4r-working-vr-scope-compared/README.md`.

---

## 1. The method is the most reusable thing, and it saves us a dump step

**RE Engine ships its managed type, field and method names as plain ASCII inside the game
executable.** Confirmed on `re4.exe` and `re8.exe` `[measured 2026-09-20]`. So "does RE2 have a class
or field called X" is answerable by `grep`ping the exe or our existing dump — **statically, with no
game running**. Any future "we need a live dump to know" row should be checked this way first.

## 2. RE2 has NO weapon scope system at all — a hard negative, recorded so nobody spends on it

Every `scope`-bearing string in the whole 494 MB RE2 dump is unrelated: `via.sound.GameParameterInfo.Scope`,
`via.pointgraph.ScopeQuery` / `ScopeSphereQuery` / `ScopeAABBQuery`, `PrivateScope`, `Oscilloscope`,
`Disp_PzlOscilloscope`, `ScopeName`. **No `ScopeController`, no `_ScopeCameraObject`, no lens
updater, nothing weapon-related** `[measured 2026-09-20]`.

So RE4R's route — re-pose the scope camera the game already renders — **cannot be ported to RE2 at
all**, any more than to Village. If a scope, sight picture, binoculars or monitor picture is ever
wanted in Visceral, it has to be built, and the thing to port is **Village's mirror stack, not
Talemann's mod**.

## 3. ⭐ And the Village mirror stack DOES have everything it needs in RE2

All present in RE2's dump `[measured 2026-09-20]`: **`via.render.Mirror`** (9), **`updatableMaterial`**
(1), **`CapturePlane`** (1), **`RenderOutput`** (5). That is the complete set of parts the Village
scope picture is made from. ⚠️ Presence is not a working path — the Village project spent weeks on
latch timing, buffer format and crop maths — but it means a Visceral picture-on-a-surface is a port,
not a research project. `[inferred-static 2026-09-20]`

## 4. ⭐ Native aim-wander and spread levers exist in RE2, with setters

- **`set_Diffusion` / `get_Diffusion` / `setDiffusion` / `getDiffusion`** — "diffusion" is RE Engine's
  word for **bullet spread**. It has a setter, i.e. spread looks like something that can be commanded
  rather than compensated for.
- **`CameraTwirler` / `CameraTwirlType` / `CameraTwirlStatusType` / `IsDispTwirlCameraSetting`** —
  "twirl" is RE Engine's word for camera/aim wander. RE2 has the camera side; RE4 additionally has a
  full `requestForceTwirler…` / `stopForceTwirler` command API, RE2 does not (`ForceTwirler`: 0 hits).
- **`HandShake_1` / `HandShake_2` / `HandShake_3`** — the shake animation set, same names as Village.

⚠️ **Unverified.** The symbols exist; whether these are reachable through REFramework reflection and
whether a write survives the game's own per-frame update is not known. Cheap `[PD]` reflection read,
then one flat write test.

## 5. Talemann's mod as a shape, and as a feature checklist

**Shape** — worth noting because it matches our own code-shape rule and our native-first preference:
one compiled DLL holding all the logic, plus **63 small JSON files, one per feature**, each holding
only that feature's numbers. No Lua at all. Nothing like our 5,600-line files.

**Feature checklist**, read straight off the file names — a mature VR mod's full scope of work, useful
as a list of things Visceral has not addressed yet:

holsters at shoulder / pistol / magazine / grenade · holster-tap select · gesture capture and a
gesture table · per-weapon reload tables (one file per weapon class, plus advanced and two-stage
reloads) · bow draw · bolt action · pump action · knife (separate left-hand, flipped-grip and
per-character offset tables) · knife parry · choke/grapple · throwing · melee breakables ·
arm IK chain · hand-mounted HUDs · haptics weighted per hand (*"right alone 100% / right with support
70% / left support 30%"*, with delay, duration and frequency) · capacitive touch · laser sight ·
crosshair · flashlight · recoil · movement and snap options · a killswitch with **zones** ·
controller-brand preferences · per-mode configs (Mercenaries, the minecart, Wild West) · an in-mod
menu editor.

⚠️ **No code was copied and none should be** — it is a third-party non-tool mod, so the rule is study
and reimplement. The value here is the *list*, the JSON-per-feature shape, and the haptic weighting
idea, which is the same "stack the hands" thinking Tefa already asked for.

Credit: **Talemann** (RE4VR, read statically), **praydog** (REFramework).
