# 2026-10-28/29 — the head shadow worn, the hair strand, the blue in the dark, and contrast

Home PC, Tefa in the headset, Opus; the head shadow started on Fable the evening before. Builds b189 to b193.

## What Tefa saw, in order

1. **The head shadow shows** (b179's shadow-only copy of the face mesh): *"the head shadow is actually showing! awesome"*.
   But *"like a mannequin head on a stick ... carved out from the lower neck and is just too high up"*.
2. **A hair-like piece in the left eye, far right.** Live look: the body has only neck_0, neck_1, head between neck
   and head; Leon's HAIR mesh (`SurvivorCostumeChanger.get_Hair()`, 31 joints of its own) was still fully drawn.
   `set_DrawDefault(false)` on it, live: *"yes it's gone!"*. The shadow was the same with the hair on or off (the
   hair casts none while the head joint is hidden).
3. **The automatic brightness was still on**, although the b180 log said it was kept off. From outside the
   component `getAutoExposure` read Enable most of the time: the game's ToneMapController writes Enable in its late
   update and our write lands only at BeginRendering, so the renderer kept adapting.
4. **The blue in the dark**: the game's per-area colour grading, a colour table per place (st4: `ColorCube_15`
   in a bright spot, `ColorCube_13` in a dark one), applied by `via.render.LDRColorCorrect` and re-enabled every
   frame by `app.ropeway.posteffect.ColorCorrectController`. Both switched off live: *"the blue is gone, looks much
   better, so much better"*.
5. **Contrast** 1.0 -> 1.2, live: *"contrast looks right"*.
6. **Claire**: her head shadow was there, then gone. The log had both copies in mode 2 following her head with
   `DrawShadowCast 0`: the game had switched it off after our one write.

## What was built

- **b189** `shadowhead.cpp`: `reframework/data/visceral_shadowhead_offset.txt`, re-read live. 4 cm *"still a bit too
  high"*, **10 cm down looked right on Leon**.
- **b190**: the hair gets the head's treatment: a second shadow-only copy (the hair's own mesh + material holders),
  the real hair hidden from view in play and shown again in cutscenes (camera not the PLAYER camera), both copies
  off in cutscenes so the shadow is not doubled. Leon's shadow *"has hair now and the strand is gone"*.
- **b191**: in a cutscene the game hid the hair again every frame and we showed it again every frame; now shown
  once when a cutscene starts. The mode file is watched (mode 3 = the copy visible, from the desk).
- **b192** `exposure.cpp`: the colour grading kept off every frame (NUM6 brings the game's back to compare); the
  adaptation's limits `AutoExposureMinEV/MaxEV` pinned to the area's own EV every frame (the controller never writes
  them; a live 4/4 write held), so the adaptation has no room whoever switches it on.
- **b193**: the copies' draw flags held every frame (the game flips them; the first flips are logged); contrast 1.2
  written every frame; the offset per character (`leon 10 0`, `claire 5 0`, bare `10 0` for everyone else; Claire:
  *"her head is too low, needs to be 5cm maybe higher"* -> *"height looks right"*).

## Open

- Claire's head shadow with b193 (the flags held) is not yet seen worn; her hair in the shadow *"I think"*.
- Ada and Hunk: the bare 10 cm line until tried.
- Other non-player cameras (ladders, switches) count as cutscenes for the hair, so it may show in view there.
- Does the light still drift between dark and bright rooms with the limits pinned? One walk tells.

Tags: colours and contrast `[verified-live 2026-10-28, headset, n=1]`; the shadow heights `[verified-live, n=1 each]`;
the flag-flip fix `[compile-verified]`.
