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
