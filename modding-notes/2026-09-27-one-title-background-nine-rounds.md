# One title background: nine rounds, parked (2026-09-27, home PC, VR)

Idea 5010 (Tefa): the last-save scene (the moving view that normally appears after pressing **Story**) should be
the only background, from launch, behind every menu. Script: `dev-archive/reframework/autorun/visceral_title_one_scene.lua`
(builds b018-b026 in the test copy; taken out again, copy back to b013).

## What the game does `[verified-live 2026-09-27, n=1 per round]`

| Moment | Calls seen |
| --- | --- |
| launch | title flow 1-5, `MainFlowManager.changeTitleCameraScene(MAIN=0, cb)`, `TitleBackgroundScene.start(0 open)`, flow 6 |
| press Story | `TitleBackgroundScene.start(1 decide)`, flow 11, `changeTitleCameraScene(LATEST=11)` |
| back out | `changeTitleCameraScene(MAIN)`, flow 10, `TitleBackgroundScene.start(2 back)` |

- The game never names the place: it asks for `LATEST` and resolves it itself. `TitleCameraController`'s
  own `set/get_TitleSceneValue` are never called.
- `GUIMaster.openSelectBackground` is never called on these menus.
- **`TitleBackgroundScene`'s GameObject has no `via.gui.GUI` component** (round 9 log): it is not a flat
  picture. Its timeline (open 0-90, decide 200-210, back 300-315) drives the 3D title scene. Decide fades to
  black; back returns the camera to the MAIN view.
- The flow **waits on `changeTitleCameraScene`'s callback** when backing out. Asking for the scene already
  showing never completes (menu left without text). Invoking the callback inside the hook is too early; invoking
  it **one frame later works** (round 7).

## What worked

- Launch → last-save scene behind the main menu: redirect `MAIN` → `LATEST` at launch (rounds 3-8).
- Menus always working: skip a same-scene request and invoke its callback a frame later (round 7).

## What did not

- Jumping the decide timeline to its end frame: the move's end event is skipped and the Story menu never comes.
- Hiding the layer by `DrawSelf` or a GUI view: it is not a GUI.
- Dropping the game's own `LATEST` request after Story: it is what fades the scene back in, so the screen stays black.

## Next try `[hypothesis]`

Let the three timeline moves and the game's Story request run as the game wants. After **back**, treat the camera as
being on `MAIN` (the back move put it there) and, once the move has ended, issue a real `changeTitleCameraScene(LATEST)`,
so the camera actually cuts back. The fade on Story is then the game's own; removing it means finding what in the
decide timeline fades (a post-effect or fade track), possibly by lowering its play speed to zero over the fade frames.

## Rounds 10-11: it works `[reported 2026-09-27, Tefa]`

- **Round 10 (b028):** launch MAIN -> LATEST + open -> decide; Story untouched; back = MAIN skipped with the
  callback a frame later, then a real LATEST cut 0.5 s after the back move. Everything worked, but the old
  main-menu view showed during the Story and back moves.
- **Round 11 (b029), the working version:** the Story (decide) and back moves are also skipped while LATEST
  shows, with their callbacks handed over a frame later, and the game's LATEST request right after Story is skipped
  the same way. Tefa: "the switch is now seamless".

Still open:
- On the main menu the falling rain is invisible (only splashes on the ground and roofs); after Story it rains
  properly. The gameplay `RainZoneManager` does not look involved; what switches the title rain on at flow
  state 11 is unknown. `[hypothesis]` a Story-state effect or a menu dim layer.
- The Models page (Bonuses) keeps its own blue background (Tefa: fine if it cannot change).

## The missing rain: six probe rounds, parked `[verified-live 2026-09-27]`

Probe `dev-archive/reframework/archive/visceral_title_rain_probe.lua` (b030-b035), each round Tefa on the main
menu -> Story -> back -> NUM8:
1. Snapshot diff main menu vs Story: Story switches on a handful of lamps (`M810lmA_*`, `M1500lm_*`, `P1500lm_*`)
   and a `LocalCubemap`, and has one extra effect player, `effect_GUI_MenuStory`. The lamps differ run to run.
2. Holding those lamps + every effect player on, every frame: no rain.
3. Detaching `effect_GUI_MenuStory` from the Story menu: the grab returned nothing, never ran.
4. `MenuStoryBehavior` has an `ObjectEffectManagerComponent` + `effectID` (0/2/-1), but its `open`/`close` never
   call `ObjectEffectManager.requestEffect`; requesting it ourselves returned nothing.
5-6. Keeping the Story screen (`GUI_MenuStory`) drawing and updating behind the main menu, its Panel and
   selectors hidden (round 5 had a state-check bug, round 6 really ran): no rain.

So the falling rain is none of these. Unexplored: `MenuStoryBehavior.toSubCamera`/`CameraState` (the Story menu
switches to a "sub camera"; rain may belong to that camera), and the `IsCheckLatestLocation` flag.
A mouse cursor appears when a numpad key is pressed (the key moves the pointer); harmless.
