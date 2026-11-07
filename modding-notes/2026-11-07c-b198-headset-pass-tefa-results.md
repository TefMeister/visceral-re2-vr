# 2026-11-07 evening: Tefa's headset pass on RE2 b198 (and RE3 b008), results as reported

Tefa ran the tests in another session and reported here. **Info only: nothing is to be acted on until Tefa
has tested all of RE2 and RE3** (Tefa: "all of what i'm writing is just info actually, not to be acted on
until i have tested all of it both re2 and re3").

Every line below is Tefa's observation `[reported]`; per the standing rule that IS the observation.

## RE2 b198, point 1 (the body checks): DONE

- **Loading Leon's save from inside Claire's game hung on a black screen.** On the next launch the same save
  loaded fine. (The other session kept a recon folder: `dev-archive/recon/2026-11-07-claire-to-leon-load-black/`.)
- **Picture:** OK.
- **Title rain:** present.
- **Item pick-up:** the game world stays visible (good), but the NEW ITEM icon is missing, possibly some other
  text too. Correction from Tefa: the item is BEHIND the player (found by looking around), not where you'd
  expect it, so the pick-up view is placed facing the wrong way rather than missing.
- **Lightning Hawk from the holster: came out with the slide at the back.** It still fired, and the slide did
  not go forward even after firing.
- **Ladders: fixed** as far as Tefa can tell; the small 5-degree nudge is "nothing".
  - Board idea (Tefa): either block LS during a climb, or make the climb manual: hold LS up/down to climb,
    stop on release. Right now the input is messy: going up and pressing up (or left) sometimes sends the
    player back down.
- **Holsters:** work.
- **Grenade:** comes out of the back (works).
- **Right hand to the LEFT hip holster did NOT bring out the d-pad-down gun (the shotgun).**
- **Running with a shotgun:** works.

## Still to test (resume list)

RE2 b198:
2. A pistol magazine: LG on it + B into the hand, back in with LT; B alone drops it; a fresh one from the left hip.
3. Fire a pistol dry, new magazine: the trigger clicks until the slide is racked.
4. The LE 5: LG on the magazine + LT pulls it out.
5. The W-870: two shells from the hip, dock at the port, LT; pump with LG + LT, let LT go at the bottom.
6. A magazine out, swap guns, swap back: still out.
7. The smoothing sliders in REFramework's menu: do they change anything with a gun in hand?

RE3 b008:
1. Body: menus over the world, pick-up, holsters, knife, running, RT without aiming, ladder.
2. G19 / MUP / G18 magazines: LG + B out into the hand, LT back in, fresh one from the left hip.
3. Fire dry, new magazine, the trigger clicks until racked.
4. M3: two shells at the port + LT, then pump.
5. CQBR: LG + LT pulls it out, LT puts it back.
6. New on this loader: knife and grenade buttons, menus closing cleanly, smoothing on guns only (never the knife).
7. Skip the MGL (still the game's own reload).

Animation & Timing (Village; the other session builds Village b080 + the window first):
1. Take the Wolfsbane. 2. OUR CLIPS, the drum-turn clip. 3. HOLD A FRAME, step to where the left hand sits in
the drum. 4. Drag the WHOLE HAND sliders until it sits on the drum. 5. Step on, let go, turn the drum for real.
Answers wanted: what the hand did; did the sliders move it live.
