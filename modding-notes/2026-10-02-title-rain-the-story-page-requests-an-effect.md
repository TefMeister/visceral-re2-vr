# Story-page rain: `open` DOES request an effect — through the private `requestEffectInternal`, chosen by the latest save's location (reader, 2026-10-02, static)

Same route as the two earlier reader drops today (reader tools against `D:\RE2 test copy\re2.exe`, base 0x140000000, the 2026-09-25
backup `il2cpp_dump.json`); nothing launched. Read first: `modding-notes/2026-10-01-title-rain-the-sub-camera-is-not-it.md` and the
09-27 note's six rounds. **Supersedes** the 09-27 note's round-4 line *"its `open`/`close` never call `ObjectEffectManager.requestEffect`"*.

## 1. Everything `MenuStoryBehavior.open` @0x141875970 does (911 instructions, read to the `ret`) `[measured 2026-10-02, disassembly]`

In order: `RopewayGuiBehaviorRoot.enableObject` (the GUI object on) · seven `GUIPath` look-ups (`_main_.c_main_story / c_sub_story
→ c_header / sl_list`, `c_extra → c_scenario → c_menulist → lost_title / c_list_lost`, `c_bg_parts → c_bg → c_bg_tex`) binding `Panel`,
`SelectorMain`, `SelectorSub` · `initList` · `Control.get_PlayState` · `selectionChangedEvent` · `fadeinListSub` · **`toSubCamera`**
(0x141876067, the one the 10-01 probe replayed) · stores the two callbacks · **the effect block (§2)** · `InputSystem.requestShowMouseCursor`
(the cursor the probes kept seeing). No scene/level/area flag, no weather or environment value, no `via.timeline`, no event post, no
other `GameObject` enable, no `Rain`/`Weather`/`Env`-named thing. Pure GUI except the camera and the effect. `GUIMaster.openMenuStory`
@0x141ad6360 (the level above) only resolves the `GameObjectRef` and calls `open`, then does the same for Records/Option pages.

## 2. The effect block — the only non-GUI thing besides the camera `[measured 2026-10-02]`

- `if (!IsCheckLatestLocation)` (+0xb0): `SaveDataManager.getLastTimeStampSlotIndex` → `getGameDataLocation(slot)`
  (`app.ropeway.gamemastering.Location.ID`) → **`effectID` (+0xc0) = `new EffectID(containerID 0, elementID N)`** with
  **RPD(16) → 2**, GasStation2(23) → 0, OrphanAsylum(24) → 4 unless `getLatestGameDataSurvivorType == 3` (then none),
  OrphanApproach(25) → 3, Opening3(30) → 1, any other location → **null (no effect)**; then `IsCheckLatestLocation = 1`. (The ctor @0x14142c010
  writes DataContainerIndex −1, ContainerID r8, ElementID r9 — the "0/2/−1" seen on 09-27 is RPD's `(0, 2)`.)
- Then, if `ObjectEffectManagerComponent` (+0xb8) is set, `effectID` non-null and **`EffectContainer` (+0xc8) is null**:
  **`EffectContainer = ObjectEffectManagerComponent.requestEffectInternal(effectID, Parent = null, externalIndex = −1)`** @0x141783b40
  (private; `getStandardDataElements` → `EPVStandard.requestEffect` → `getDataContainerObjFromIndex` → `CreatedEffectContainer`).
- The kill: `close(callback, killEffect)` @0x14182d0a0 with `killEffect` true sets **`EffectContainer.KillAllRequest` (+0x20) = 1** and
  drops the reference; `update` @0x14189cfe0 does the same when `TimelineEnd && FadeOutEnd` (after `enableObject(false)` and the
  fade-out callback). Nothing else in `update` touches it (the rest is input, `outSubCamera`, a Wwise trigger).
- **Why the 09-27 hooks saw nothing** `[inferred-static]`: the probe hooked the public `requestEffect(EffectID, GameObject, Int32)`
  @0x1404c4710, which the game never uses here — `open` calls the private `requestEffectInternal` directly (the public one is a thin
  wrapper around it, so hooking the wrapper misses the game's own call). **Why round 4's own request "returned nothing"**: it called
  `requestEffect(eid, main_menu_go, 0)` — Parent = the main-menu object and **externalIndex 0** (an external data container slot);
  the game passes **null and −1** (its own container). `[hypothesis]` index 0 is what returned null.

## 3. Could it own the rain? `[inferred-static 2026-10-02]` — yes, and it fits every observation

One effect, present only while the Story page is open (requested at `open`, killed at `close`/fade-out), picked by the latest
save's location — RPD gives element 2, and Tefa's saves are RPD-era (`[hypothesis]`; a non-listed location would show NO rain on the
Story page, a cheap check). The 09-27 snapshot's one extra effect player on the Story page, `effect_GUI_MenuStory`, is this container's
player `[hypothesis]`. The sub camera (10-01) and lamps (09-27) were real differences but not the rain; the Story screen kept drawing
(round 5/6) without rain because `open` — and thus this request — never ran on the main menu.

## 4. The one method to hook / call live

**Call it, don't hook it:** on the main menu, `story:get_field("ObjectEffectManagerComponent"):call(
"requestEffectInternal(via.effect.script.EffectID, via.GameObject, System.Int32)", eid, nil, -1)` with `eid` = the Story object's
`effectID` field (set after one Story visit) or `sdk.create_instance("via.effect.script.EffectID")` + `.ctor(System.UInt32, System.UInt32)`
(0, 2); keep the returned `CreatedEffectContainer`; to stop, set its `KillAllRequest` true. If a hook is wanted for proof first:
**`via.effect.script.ObjectEffectManager.requestEffectInternal(via.effect.script.EffectID, via.GameObject, System.Int32)`** — post-hook,
log `this`'s GameObject name, `id.ContainerID/ElementID`, and the return; expect exactly one call on main menu → Story with (0, 2).
Read-out: rain appears on the main menu after the call ⇒ this is it (then the title script requests it at flow state 10 and kills it
on leaving); returns nil ⇒ try `requestEffect` with the same `(eid, nil, -1)`, then check `IsCheckLatestLocation`/`effectID` were set.


## Built and seen, same evening (flat, three launches) `[verified-live 2026-10-02, n=2 launches]`

`dev-archive/reframework/autorun/visceral_title_rain.lua` (in the test copy, b044): finds the title's `MenuStoryBehavior`
through the scene (`findComponents`), builds `EffectID(0, 2)` as a managed object (`sdk.create_instance(..., true)`, fields
`ContainerID`/`ElementID`/`DataContainerIndex=-1`; a `ValueType` is refused) and calls
`ObjectEffectManagerComponent.requestEffectInternal(id, nil, -1)`. **The first call is often refused ("Invoke threw") while the
effect data is still loading — one launch accepted the first press, two refused it — so it retries every 0.5 s; attempt 2
was accepted on the run that shipped.** Rain drops run down the main-menu "lens" in the captures. On Story open it kills
ours (the game requests its own); on close it clears `EffectContainer` before close/fade-out can kill it and adopts the
game's container, so backing out keeps the rain (log + capture). ESC backs out of the Story page (a keyboard Return).
Probe `visceral_title_rain_probe.lua` archived (in git; taken out of the test copy). Not yet seen in the headset.
