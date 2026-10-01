# Title rain: the Story page's sub camera is NOT what switches the rain on (2026-10-01)

**Question.** The Story page of the title menu shows falling rain; the main menu does not (Tefa,
2026-09-30). The Story page enters a "sub camera" when it opens (`MenuStoryBehavior.open` →
`toSubCamera`, camera state `TO_SUB`) and leaves it when it closes (`outSubCamera`, `OUT_SUB`). Is that
mode what brings the rain?

**Test.** `visceral_title_subcam_probe.lua` (in the test copy since b039) calls `toSubCamera()` on the
Story menu object from the MAIN menu on NUM8, and `outSubCamera()` on the next NUM8. Tefa ran it flat on
2026-10-01 23:58, headset connected but not needed.

**Result** `[verified-live 2026-10-01, n=1]`: NUM8 fades the screen to black and comes back on the same
main-menu view, **no rain**. The log confirms the call went through and the state changed:

```
23:58:11.924 toSubCamera called ... (camera state was OUT_SUB)
23:58:11.924 toSubCamera done (camera state now TO_SUB)
23:58:11.924 NUM8: toSubCamera() sent; camera state TO_SUB; look for falling rain
23:58:12.378 camera state TO_SUB  IsCheckLatestLocation=true
23:58:19.952 NUM8: outSubCamera() sent; camera state OUT_SUB
```

So `toSubCamera` on its own, including the two calls with the constant 11 (LATEST) that the static read
found inside it, does not switch the rain on. The fade-to-black-and-back looks like a camera-scene request
that resolved to the scene already showing.

**What this leaves.** The rain must come from something else `MenuStoryBehavior.open` (or the Story page's
own screen flow) does. Seven rounds have now ruled out: lamps, effect players, the menu's own effect, the
Story screen itself (09-27, six rounds), and the sub camera mode (tonight). `IsCheckLatestLocation` was true
throughout, so it is not a flag that the Story page flips.

**Next, two ways, pick one:**

1. **Live diff (preferred):** a probe that lists every active GameObject under the title scene (name,
   component types, active flag) once on the main menu and once with the Story page open, and logs only the
   differences. Whatever appears only on the Story page is the rain's owner, or its switch. Needs the game;
   an `/lm` session can run it alone.
2. **Static:** decompile `MenuStoryBehavior.open` fully (not only the `toSubCamera` call) and list every
   other call it makes; then the same for the Story page's screen-open flow.

**Installed now:** the probe stays in the test copy tonight only because a build snapshot takes minutes at
midnight; the first session tomorrow takes it out (its source is in `dev-archive/reframework/autorun/`) and
snapshots b040.
