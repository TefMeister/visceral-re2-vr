# Main-menu rain: nearly there (2026-10-03, home PC, VR)

Tefa wants the Story page's rain on the main menu too, seamless in both directions. Two scripts do it:
`visceral_title_one_scene.lua` (the last-save scene behind every title page) and `visceral_title_rain.lua` (the rain).

## What works now `[verified-live 2026-10-03, n=1 launch, Tefa in the headset]`

- **Story -> main menu -> Story is seamless.** Once the game's own rain exists, our close hook keeps it alive
  (clears `MenuStoryBehavior.EffectContainer` before close / the fade-out kill it), and on the next Story open the
  b052 change hands it back to the Story object and kills the fresh copy the game asks for. No blip.
- **Back from a game the menu is no longer black.** One-scene kept "LATEST already showing" across the game and
  skipped the MAIN request, so no scene loaded. Fixed in b053: cleared at title flow state 1 (log line
  `title restarted: remembered scene cleared`). State 1 only appears on the way back from a game, never at boot.

## What is left: the FIRST main menu has no rain, at boot and back from a game

Our own request on the main menu, `ObjectEffectManager.requestEffectInternal(EffectID(0, el, -1), nil, -1)` on the
Story object's `ObjectEffectManagerComponent`, is refused (Invoke throws, `r` not logged):

| launch | result |
| --- | --- |
| 21:31 | refused 10x, accepted at attempt 11 (~5 s); Tefa saw rain on the main menu |
| 21:42 boot | refused every 0.5 s for 33 s until Tefa pressed Story; no rain until then |
| 21:43 back from game | refused again, same |

So the call works sometimes. Rain appears only once the game makes its own (Story open), after which everything
is seamless. `[measured 2026-10-03, n=3]`

## Next (ideas, none tried)

1. Log the exception text of the refused call (`tostring(r)` on every refusal, rate-limited) to see WHY.
2. Hook `requestEffectInternal` while Story opens and log the game's real arguments (EffectID fields, the
   GameObject it passes, the int). We pass `nil` and `-1`; the game may pass its own GameObject.
3. Request it the way open does (call the Story object's own rain request, if it is a separate method) instead
   of building the EffectID ourselves.
4. Order: one-scene swaps MAIN -> LATEST ~20 s after the rain script starts asking (21:42:01 vs 21:42:20). The
   effect may need the LATEST scene loaded; try only after `changeTitleCameraScene MAIN -> LATEST` has finished.

Installed now: both scripts as of b053/b052, on top of the 2026-09-25 package (b055).
