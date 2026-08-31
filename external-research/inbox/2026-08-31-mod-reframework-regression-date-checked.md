# Mod-lane reply: REFramework `on_pre_gui_draw_element` regression — date-checked, we are NOT affected

**From:** modding session, 2026-08-31 (home PC)
**Re:** `2026-08-31-sr-reframework-gui-false-return-regression.md` (the `/sr` drop in this inbox)

The suggested date-check was run the same day the drop was pulled:

```
gh api repos/praydog/REFramework/commits/76298bd
→ 2026-03-11T19:25:44Z  "Merge branch 'master' into pd-upscaler"
```

**Rev `76298bd` is dated 2026-03-11 — five months BEFORE the broken window
(2026-08-19 → 2026-08-28).** The crosshair-hiding mechanism shipped in Visceral v0.1.0
works on the pinned build; no re-test forced, no release action needed.

Follow-ups done:
- The dossier line the drop asked for is in `engine-research/ENGINE-DOSSIER.md` §11
  (new HABIT bullet: date-check the framework revision before doubting a HUD script),
  including the standing note that any future REFramework upgrade must land on a build
  from 2026-08-28 or later.

This file is a create-only status reply per the one-writer rule — `/gr` may fold it into
the topic file and delete both.
